#include "NetworkTransport.h"
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

NetworkTransport::NetworkTransport(AudioRingBuffer& ringBuf, JitterBuffer& jitBuf)
    : Thread("AnarackAudioRecv"), audioBuffer(ringBuf), jitterBuffer(jitBuf)
{
}

NetworkTransport::~NetworkTransport()
{
    disconnect();
}

// --- Raw UDP connection (LAN) ---

void NetworkTransport::connect(const juce::String& host, int midiPort, int audioPort)
{
    disconnect();
    useWireGuard = false;

    // Resolve hostname to numeric IP
    struct addrinfo hints = {}, *res = nullptr;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    int err = getaddrinfo(host.toRawUTF8(), nullptr, &hints, &res);
    if (err == 0 && res != nullptr)
    {
        char ipBuf[INET_ADDRSTRLEN];
        auto* addr = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
        inet_ntop(AF_INET, &addr->sin_addr, ipBuf, sizeof(ipBuf));
        serverHost = ipBuf;
        freeaddrinfo(res);
    }
    else
    {
        serverHost = host;
    }
    serverMidiPort = midiPort;
    serverAudioPort = audioPort;

    // Create send socket
    rawSendFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (rawSendFd < 0) return;

    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons((uint16_t)serverMidiPort);
    inet_pton(AF_INET, serverHost.toRawUTF8(), &serverAddr.sin_addr);

    // Create receive socket
    rawRecvFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    {
        struct sockaddr_in recvAddr;
        std::memset(&recvAddr, 0, sizeof(recvAddr));
        recvAddr.sin_family = AF_INET;
        recvAddr.sin_addr.s_addr = INADDR_ANY;
        recvAddr.sin_port = htons((uint16_t)serverAudioPort);

        int reuseAddr = 1;
        setsockopt(rawRecvFd, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr));

        // Increase receive buffer to prevent packet drops from buffer overflow
        int rcvBuf = 1024 * 1024; // 1MB (default is often 256KB)
        setsockopt(rawRecvFd, SOL_SOCKET, SO_RCVBUF, &rcvBuf, sizeof(rcvBuf));

        if (::bind(rawRecvFd, (struct sockaddr*)&recvAddr, sizeof(recvAddr)) < 0)
        {
            recvAddr.sin_port = 0;
            ::bind(rawRecvFd, (struct sockaddr*)&recvAddr, sizeof(recvAddr));
        }
    }

    connected = true;
    packetsReceived = 0;
    midiFifo.reset();
    audioBuffer.reset();

    sendRegistration();
    startThread(juce::Thread::Priority::high);

    sendThread = std::make_unique<SendThread>(*this);
    sendThread->startThread(juce::Thread::Priority::high);
}

// --- WireGuard connection (internet) ---

void NetworkTransport::connectWireGuard(const juce::String& serverEndpoint,
                                         const juce::String& serverPubkey,
                                         const juce::String& privateKey,
                                         const juce::String& tunnelLocalIp,
                                         const juce::String& tunnelRemoteIp,
                                         int midiPort, int audioPort)
{
    disconnect();
    useWireGuard = true;
    serverMidiPort = midiPort;
    serverAudioPort = audioPort;

    // Use provided ephemeral key, or fall back to static test keypair
    auto privKey = privateKey.isNotEmpty()
        ? privateKey
        : juce::String("XTmhMhpKGEhfNtqff4GUQ5cS281pfScf+1x2Cd6aF44=");

    DBG("WG connect: endpoint=" + serverEndpoint
        + " pubkey=" + serverPubkey.substring(0, 16) + "..."
        + " privkey=" + (privateKey.isNotEmpty() ? "ephemeral" : "static")
        + " tunnel=" + tunnelLocalIp + "→" + tunnelRemoteIp);

    wgTunnel = std::make_unique<WgTunnel>();
    if (!wgTunnel->connect(privKey, serverPubkey, serverEndpoint,
                           tunnelLocalIp, tunnelRemoteIp))
    {
        wgTunnel.reset();
        return;
    }

    connected = true;
    packetsReceived = 0;
    midiFifo.reset();
    audioBuffer.reset();

    // Send registration through the tunnel
    sendRegistration();

    // Start receive thread (uses WireGuard path)
    startThread(juce::Thread::Priority::high);

    sendThread = std::make_unique<SendThread>(*this);
    sendThread->startThread(juce::Thread::Priority::high);
}

// --- Shared ---

void NetworkTransport::disconnect()
{
    connected = false;

    if (sendThread)
    {
        sendThread->stopThread(500);
        sendThread.reset();
    }

    stopThread(500);

    if (wgTunnel)
    {
        wgTunnel->disconnect();
        wgTunnel.reset();
    }

    if (rawRecvFd >= 0) { ::close(rawRecvFd); rawRecvFd = -1; }
    if (rawSendFd >= 0) { ::close(rawSendFd); rawSendFd = -1; }
    audioBuffer.reset();
    midiFifo.reset();
    useWireGuard = false;
}

void NetworkTransport::sendRegistration()
{
    uint8_t reg = 0xFE;

    if (useWireGuard && wgTunnel)
    {
        wgTunnel->send(&reg, 1, (uint16_t)serverMidiPort);
    }
    else if (rawSendFd >= 0)
    {
        sendto(rawSendFd, &reg, 1, 0,
               (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    }
}

void NetworkTransport::sendMidi(const uint8_t* data, int size)
{
    if (!connected.load() || size <= 0 || size > 3)
        return;

    int start1, size1, start2, size2;
    midiFifo.prepareToWrite(1, start1, size1, start2, size2);

    if (size1 > 0)
    {
        auto& slot = midiSlots[start1];
        std::memcpy(slot.data, data, (size_t)size);
        slot.size = size;
        midiFifo.finishedWrite(1);
    }
}

void NetworkTransport::sendPendingMidi()
{
    int start1, size1, start2, size2;
    midiFifo.prepareToRead(midiFifo.getNumReady(), start1, size1, start2, size2);

    for (int i = 0; i < size1; ++i)
    {
        auto& slot = midiSlots[start1 + i];
        if (useWireGuard && wgTunnel)
            wgTunnel->send(slot.data, slot.size, (uint16_t)serverMidiPort);
        else if (rawSendFd >= 0)
            sendto(rawSendFd, slot.data, (size_t)slot.size, 0,
                   (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    }
    for (int i = 0; i < size2; ++i)
    {
        auto& slot = midiSlots[start2 + i];
        if (useWireGuard && wgTunnel)
            wgTunnel->send(slot.data, slot.size, (uint16_t)serverMidiPort);
        else if (rawSendFd >= 0)
            sendto(rawSendFd, slot.data, (size_t)slot.size, 0,
                   (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    }

    midiFifo.finishedRead(size1 + size2);
}

void NetworkTransport::handleJsonPacket(const uint8_t* data, int size)
{
    // Parse JSON messages from the server (CC updates, patch names)
    auto json = juce::JSON::parse(juce::String::fromUTF8((const char*)data, size));
    if (json.isVoid()) return;

    auto type = json.getProperty("type", "").toString();
    if (type == "cc" && onSynthCC)
    {
        int cc = (int)json.getProperty("cc", -1);
        int value = (int)json.getProperty("value", 0);
        if (cc >= 0) onSynthCC(cc, value);
    }
    else if (type == "patchName" && onPatchName)
    {
        auto name = json.getProperty("name", "").toString();
        if (name.isNotEmpty()) onPatchName(name);
    }
}

int NetworkTransport::getEstimatedRtt() const
{
    if (wgTunnel)
        return wgTunnel->getStats().estimated_rtt;
    return -1;
}

// --- Audio receive: raw UDP mode ---

void NetworkTransport::run()
{
    if (useWireGuard)
    {
        runWireGuard();
        return;
    }

    constexpr int MAX_PACKET = 1024;
    uint8_t packetBuf[MAX_PACKET];
    float convBuf[MAX_PACKET / 2];

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    setsockopt(rawRecvFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    while (!threadShouldExit() && connected.load())
    {
        if (rawRecvFd < 0) break;
        int bytesRead = (int)recvfrom(rawRecvFd, packetBuf, MAX_PACKET, 0, nullptr, nullptr);

        if (bytesRead <= 0)
            continue;

        // JSON messages from server (CC updates, patch names)
        // Audio packets are exactly 268 bytes (12-byte header + 256-byte payload).
        // JSON messages are variable-length and never 268 bytes.
        if (bytesRead != 268 && bytesRead > 0 && bytesRead < 512 && packetBuf[0] == '{')
        {
            handleJsonPacket(packetBuf, bytesRead);
            continue;
        }

        if (bytesRead == 268 && jitterBuffer.isConfigured())
        {
            // Header packet → JitterBuffer (timestamp-indexed placement)
            jitterBuffer.writePacket(packetBuf, bytesRead);
        }
        else
        {
            // Strip header if present, feed AudioRingBuffer
            const uint8_t* audioStart = packetBuf;
            int audioBytes = bytesRead;
            if (bytesRead == 268) {
                audioStart = packetBuf + 12;
                audioBytes = 256;
            }
            int numSamples = audioBytes / 2;
            auto* int16Data = reinterpret_cast<const int16_t*>(audioStart);
            for (int i = 0; i < numSamples; ++i)
                convBuf[i] = (float)int16Data[i] / 32768.0f;
            audioBuffer.write(convBuf, numSamples);
        }

        lastPacketSize.store(bytesRead, std::memory_order_relaxed);
        packetsReceived.fetch_add(1, std::memory_order_relaxed);
    }
}

// --- Audio receive: WireGuard mode ---

void NetworkTransport::runWireGuard()
{
    constexpr int MAX_PACKET = 1024;
    uint8_t packetBuf[MAX_PACKET];
    float convBuf[MAX_PACKET / 2];

    while (!threadShouldExit() && connected.load())
    {
        if (!wgTunnel) break;

        uint16_t srcPort = 0;
        int bytesRead = wgTunnel->recv(packetBuf, MAX_PACKET, srcPort);

        if (bytesRead <= 0)
        {
            // No data ready — brief sleep to avoid busy spinning
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            continue;
        }

        // JSON messages from server (CC updates, patch names)
        // Audio packets are exactly 268 bytes (12-byte header + 256-byte payload).
        // JSON messages are variable-length and never 268 bytes.
        if (bytesRead != 268 && bytesRead > 0 && bytesRead < 512 && packetBuf[0] == '{')
        {
            handleJsonPacket(packetBuf, bytesRead);
            continue;
        }

        // Audio packets come from the server's audio port
        if (srcPort == (uint16_t)serverAudioPort || bytesRead >= 128)
        {
            if (bytesRead == 268 && jitterBuffer.isConfigured())
            {
                jitterBuffer.writePacket(packetBuf, bytesRead);
            }
            else
            {
                const uint8_t* audioStart = packetBuf;
                int audioBytes = bytesRead;
                if (bytesRead == 268) {
                    audioStart = packetBuf + 12;
                    audioBytes = 256;
                }
                int numSamples = audioBytes / 2;
                auto* int16Data = reinterpret_cast<const int16_t*>(audioStart);
                for (int i = 0; i < numSamples; ++i)
                    convBuf[i] = (float)int16Data[i] / 32768.0f;
                audioBuffer.write(convBuf, numSamples);
            }
        }

        lastPacketSize.store(bytesRead, std::memory_order_relaxed);
        packetsReceived.fetch_add(1, std::memory_order_relaxed);
    }
}

// --- MIDI send thread ---

void NetworkTransport::SendThread::run()
{
    int keepaliveCounter = 0;

    while (!threadShouldExit() && parent.connected.load())
    {
        parent.sendPendingMidi();
        Thread::sleep(1);

        if (++keepaliveCounter >= 5000)
        {
            parent.sendRegistration();
            keepaliveCounter = 0;
        }
    }
}
