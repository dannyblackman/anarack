#include "WgTunnel.h"
#include <netdb.h>
#include <cstring>

// Minimal IPv4 header (20 bytes)
struct IpHeader {
    uint8_t  versionIhl;    // version(4) + IHL(4)
    uint8_t  tos;
    uint16_t totalLength;
    uint16_t id;
    uint16_t flagsOffset;
    uint8_t  ttl;
    uint8_t  protocol;      // 17 = UDP
    uint16_t checksum;
    uint32_t srcAddr;
    uint32_t dstAddr;
};

// Minimal UDP header (8 bytes)
struct UdpHeader {
    uint16_t srcPort;
    uint16_t dstPort;
    uint16_t length;
    uint16_t checksum;
};

static uint16_t ipChecksum(const void* data, int len)
{
    auto* p = reinterpret_cast<const uint16_t*>(data);
    uint32_t sum = 0;
    for (int i = 0; i < len / 2; ++i)
        sum += ntohs(p[i]);
    if (len & 1)
        sum += ((const uint8_t*)data)[len - 1] << 8;
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return htons((uint16_t)(~sum));
}

WgTunnel::WgTunnel() = default;

WgTunnel::~WgTunnel()
{
    disconnect();
}

std::pair<juce::String, juce::String> WgTunnel::generateKeypair()
{
    auto priv = x25519_secret_key();
    auto pub = x25519_public_key(priv);
    auto privB64 = x25519_key_to_base64(priv);
    auto pubB64 = x25519_key_to_base64(pub);
    std::pair<juce::String, juce::String> result { privB64, pubB64 };
    x25519_key_to_str_free(privB64);
    x25519_key_to_str_free(pubB64);
    return result;
}

bool WgTunnel::connect(const juce::String& myPrivkeyB64,
                       const juce::String& serverPubkeyB64,
                       const juce::String& serverEndpoint,
                       const juce::String& tunnelLocalIp,
                       const juce::String& tunnelRemoteIp)
{
    disconnect();

    // Parse endpoint "ip:port"
    auto colonIdx = serverEndpoint.lastIndexOfChar(':');
    if (colonIdx < 0) return false;
    auto endpointIp = serverEndpoint.substring(0, colonIdx);
    auto endpointPort = serverEndpoint.substring(colonIdx + 1).getIntValue();

    // Resolve endpoint hostname
    struct addrinfo hints = {}, *res = nullptr;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(endpointIp.toRawUTF8(), nullptr, &hints, &res) == 0 && res)
    {
        serverAddr = *reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
        serverAddr.sin_port = htons((uint16_t)endpointPort);
        freeaddrinfo(res);
    }
    else
    {
        return false;
    }

    // Store tunnel IPs
    inet_pton(AF_INET, tunnelLocalIp.toRawUTF8(), &localIp);
    inet_pton(AF_INET, tunnelRemoteIp.toRawUTF8(), &remoteIp);

    // Create the boringtun tunnel
    tunnel = new_tunnel(myPrivkeyB64.toRawUTF8(),
                        serverPubkeyB64.toRawUTF8(),
                        nullptr,  // no preshared key
                        25,       // keepalive interval (seconds)
                        0);       // index
    if (!tunnel) return false;

    // Create real UDP socket
    udpFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpFd < 0)
    {
        tunnel_free(tunnel);
        tunnel = nullptr;
        return false;
    }

    // Set recv timeout — low for minimal jitter, just enough for clean shutdown
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 1000; // 1ms (was 50ms — added up to 50ms jitter per packet)
    setsockopt(udpFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Increase receive buffer to prevent packet drops
    int rcvBuf = 1024 * 1024; // 1MB
    setsockopt(udpFd, SOL_SOCKET, SO_RCVBUF, &rcvBuf, sizeof(rcvBuf));

    connected = true;

    // Reset recv queue
    recvWriteIdx = 0;
    recvReadIdx = 0;
    for (auto& pkt : recvQueue)
        pkt.ready.store(false);

    // Force initial handshake
    {
        std::lock_guard<std::mutex> lock(sendMutex);
        auto result = wireguard_force_handshake(tunnel, wgSendBuf, sizeof(wgSendBuf));
        processWgResult(result, wgSendBuf);
    }

    // Start background threads
    recvThread = std::make_unique<std::thread>(&WgTunnel::recvLoop, this);
    tickThread = std::make_unique<std::thread>(&WgTunnel::tickLoop, this);

    return true;
}

void WgTunnel::disconnect()
{
    connected = false;

    if (recvThread && recvThread->joinable())
        recvThread->join();
    recvThread.reset();

    if (tickThread && tickThread->joinable())
        tickThread->join();
    tickThread.reset();

    if (udpFd >= 0) { ::close(udpFd); udpFd = -1; }

    if (tunnel)
    {
        tunnel_free(tunnel);
        tunnel = nullptr;
    }
}

int WgTunnel::buildIpUdpPacket(uint8_t* out, int maxLen,
                                const void* payload, int payloadLen,
                                uint16_t srcPort, uint16_t dstPort)
{
    int totalLen = (int)sizeof(IpHeader) + (int)sizeof(UdpHeader) + payloadLen;
    if (totalLen > maxLen) return -1;

    auto* ip = reinterpret_cast<IpHeader*>(out);
    auto* udp = reinterpret_cast<UdpHeader*>(out + sizeof(IpHeader));
    auto* data = out + sizeof(IpHeader) + sizeof(UdpHeader);

    // IP header
    std::memset(ip, 0, sizeof(IpHeader));
    ip->versionIhl = 0x45;  // IPv4, 5 dwords (20 bytes)
    ip->totalLength = htons((uint16_t)totalLen);
    ip->ttl = 64;
    ip->protocol = 17;  // UDP
    ip->srcAddr = localIp;
    ip->dstAddr = remoteIp;
    ip->checksum = ipChecksum(ip, sizeof(IpHeader));

    // UDP header
    udp->srcPort = htons(srcPort);
    udp->dstPort = htons(dstPort);
    udp->length = htons((uint16_t)(sizeof(UdpHeader) + payloadLen));
    udp->checksum = 0;  // Optional for IPv4

    // Payload
    std::memcpy(data, payload, (size_t)payloadLen);

    return totalLen;
}

int WgTunnel::extractPayload(const uint8_t* ipPacket, int ipLen,
                              void* payload, int maxLen, uint16_t& srcPort)
{
    if (ipLen < (int)(sizeof(IpHeader) + sizeof(UdpHeader)))
        return -1;

    auto* ip = reinterpret_cast<const IpHeader*>(ipPacket);
    int ihl = (ip->versionIhl & 0x0F) * 4;
    if (ipLen < ihl + (int)sizeof(UdpHeader))
        return -1;

    auto* udp = reinterpret_cast<const UdpHeader*>(ipPacket + ihl);
    srcPort = ntohs(udp->srcPort);

    int udpDataLen = ntohs(udp->length) - (int)sizeof(UdpHeader);
    if (udpDataLen <= 0 || udpDataLen > maxLen)
        return -1;

    std::memcpy(payload, ipPacket + ihl + sizeof(UdpHeader), (size_t)udpDataLen);
    return udpDataLen;
}

int WgTunnel::send(const void* data, int len, uint16_t dstPort, uint16_t srcPort)
{
    if (!connected.load() || !tunnel) return -1;

    // Build IP+UDP packet wrapping the data
    int ipLen = buildIpUdpPacket(ipBuildBuf, sizeof(ipBuildBuf), data, len, srcPort, dstPort);
    if (ipLen < 0) return -1;

    // Encrypt through WireGuard
    std::lock_guard<std::mutex> lock(sendMutex);
    auto result = wireguard_write(tunnel, ipBuildBuf, (uint32_t)ipLen,
                                  wgSendBuf, sizeof(wgSendBuf));
    processWgResult(result, wgSendBuf);

    return (result.op == WRITE_TO_NETWORK) ? len : -1;
}

int WgTunnel::recv(void* buf, int maxLen, uint16_t& srcPort)
{
    int idx = recvReadIdx.load();
    auto& pkt = recvQueue[idx % RECV_QUEUE_SIZE];
    if (!pkt.ready.load())
        return -1;

    int copyLen = std::min(pkt.len, maxLen);
    std::memcpy(buf, pkt.data, (size_t)copyLen);
    srcPort = pkt.srcPort;
    pkt.ready.store(false);
    recvReadIdx.store(idx + 1);
    return copyLen;
}

void WgTunnel::sendToNetwork(const uint8_t* data, size_t len)
{
    if (udpFd >= 0)
        sendto(udpFd, data, len, 0,
               (struct sockaddr*)&serverAddr, sizeof(serverAddr));
}

void WgTunnel::processWgResult(struct wireguard_result result, uint8_t* buf)
{
    switch (result.op)
    {
        case WRITE_TO_NETWORK:
            sendToNetwork(buf, result.size);
            break;

        case WRITE_TO_TUNNEL_IPV4:
        case WRITE_TO_TUNNEL_IPV6:
        {
            // Decrypted IP packet — extract payload and queue it
            int idx = recvWriteIdx.load();
            auto& pkt = recvQueue[idx % RECV_QUEUE_SIZE];
            if (!pkt.ready.load()) // Don't overwrite unread packets
            {
                pkt.len = extractPayload(buf, (int)result.size,
                                         pkt.data, RECV_BUF_SIZE, pkt.srcPort);
                if (pkt.len > 0)
                {
                    pkt.ready.store(true);
                    recvWriteIdx.store(idx + 1);
                }
            }
            break;
        }

        case WIREGUARD_DONE:
        case WIREGUARD_ERROR:
        default:
            break;
    }
}

void WgTunnel::recvLoop()
{
    uint8_t udpBuf[MAX_WIREGUARD_PACKET_SIZE];

    while (connected.load())
    {
        if (udpFd < 0) break;
        ssize_t bytesRead = ::recvfrom(udpFd, udpBuf, sizeof(udpBuf), 0, nullptr, nullptr);
        if (bytesRead <= 0) continue;

        // Decrypt through WireGuard
        std::lock_guard<std::mutex> lock(sendMutex);
        auto result = wireguard_read(tunnel, udpBuf, (uint32_t)bytesRead,
                                     wgRecvBuf, sizeof(wgRecvBuf));
        processWgResult(result, wgRecvBuf);

        // Drain any queued packets (after handshake there may be multiple)
        while (result.op != WIREGUARD_DONE && result.op != WIREGUARD_ERROR)
        {
            result = wireguard_read(tunnel, nullptr, 0, wgRecvBuf, sizeof(wgRecvBuf));
            processWgResult(result, wgRecvBuf);
        }
    }
}

void WgTunnel::tickLoop()
{
    uint8_t tickBuf[MAX_WIREGUARD_PACKET_SIZE];

    while (connected.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (!connected.load() || !tunnel) break;

        std::lock_guard<std::mutex> lock(sendMutex);
        auto result = wireguard_tick(tunnel, tickBuf, sizeof(tickBuf));
        processWgResult(result, tickBuf);
    }
}

struct stats WgTunnel::getStats() const
{
    if (tunnel)
        return wireguard_stats(tunnel);
    return {};
}
