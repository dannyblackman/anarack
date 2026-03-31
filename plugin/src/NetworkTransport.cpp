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
                                         int midiPort, int audioPort)
{
    disconnect();
    useWireGuard = true;
    serverMidiPort = midiPort;
    serverAudioPort = audioPort;

    // TODO: ephemeral keys via session API. For now, use static test keypair.
    auto privKey = juce::String("XTmhMhpKGEhfNtqff4GUQ5cS281pfScf+1x2Cd6aF44=");
    auto pubKey = juce::String("arRpxSMBrlWstnbjoHA5sL6ONaVHIeH5pcWAhZPsXEM=");

    wgTunnel = std::make_unique<WgTunnel>();
    // Plugin is 10.0.0.3, Pi studio is 10.0.0.2 (via VPS relay at 10.0.0.1)
    if (!wgTunnel->connect(privKey, serverPubkey, serverEndpoint,
                           "10.0.0.3", "10.0.0.2"))
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

int NetworkTransport::getEstimatedRtt() const
{
    // Prefer measured RTT (from ping/pong), fall back to WireGuard stats
    int measured = measuredRtt.load();
    if (measured > 0) return measured;
    if (wgTunnel) return wgTunnel->getStats().estimated_rtt;
    return -1;
}

int NetworkTransport::getBufferLevel() const
{
    if (jitterBuffer.isConfigured())
        return jitterBuffer.getFillLevel();
    return audioBuffer.getNumReady();
}

int NetworkTransport::getPacketsLost() const
{
    return jitterBuffer.getPacketsLost();
}

void NetworkTransport::measureRtt()
{
    // Send RTT ping: 0xFD + uint64 timestamp
    uint8_t ping[9];
    ping[0] = 0xFD;
    auto now = (uint64_t)juce::Time::getMillisecondCounterHiRes();
    std::memcpy(ping + 1, &now, 8);

    if (useWireGuard && wgTunnel)
        wgTunnel->send(ping, 9, (uint16_t)serverMidiPort);
    else if (rawSendFd >= 0)
        sendto(rawSendFd, ping, 9, 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
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

        // RTT pong: 0xFD byte echoed back from server
        if (bytesRead >= 1 && packetBuf[0] == 0xFD)
        {
            if (bytesRead >= 9) // 0xFD + uint64 timestamp
            {
                uint64_t sendTime;
                std::memcpy(&sendTime, packetBuf + 1, 8);
                auto now = (uint64_t)juce::Time::getMillisecondCounterHiRes();
                measuredRtt.store((int)(now - sendTime));
                rttReady.store(true);
            }
            continue;
        }

        // Detect packet format: exactly 268 bytes = header (12) + payload (256)
        constexpr int HDR = JitterBuffer::HEADER_SIZE;
        constexpr int PAYLOAD = JitterBuffer::PACKET_SAMPLES * 2;
        bool hasHeader = (bytesRead == HDR + PAYLOAD);

        if (hasHeader && jitterBuffer.isConfigured())
        {
            jitterBuffer.writePacket(packetBuf, bytesRead);
        }
        else
        {
            // Strip header if present, feed raw audio to legacy AudioRingBuffer
            const uint8_t* audioData = packetBuf;
            int audioBytes = bytesRead;
            if (hasHeader)
            {
                audioData = packetBuf + HDR;
                audioBytes = bytesRead - HDR;
            }

            int numSamples = audioBytes / 2;
            if (numSamples > 0 && numSamples <= 512)
            {
                auto* int16Data = reinterpret_cast<const int16_t*>(audioData);
                for (int i = 0; i < numSamples; ++i)
                    convBuf[i] = (float)int16Data[i] / 32768.0f;
                audioBuffer.write(convBuf, numSamples);
            }
        }
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

        // RTT pong
        if (bytesRead >= 1 && packetBuf[0] == 0xFD)
        {
            if (bytesRead >= 9)
            {
                uint64_t sendTime;
                std::memcpy(&sendTime, packetBuf + 1, 8);
                auto now = (uint64_t)juce::Time::getMillisecondCounterHiRes();
                measuredRtt.store((int)(now - sendTime));
                rttReady.store(true);
            }
            continue;
        }

        // Audio packets from the server's audio port
        if (srcPort == (uint16_t)serverAudioPort || bytesRead >= 128)
        {
            constexpr int HDR = JitterBuffer::HEADER_SIZE;
            constexpr int PAYLOAD = JitterBuffer::PACKET_SAMPLES * 2;
            bool hasHeader = (bytesRead == HDR + PAYLOAD);

            if (hasHeader && jitterBuffer.isConfigured())
            {
                jitterBuffer.writePacket(packetBuf, bytesRead);
            }
            else
            {
                // Strip header if present
                const uint8_t* audioData = hasHeader ? packetBuf + HDR : packetBuf;
                int audioBytes = hasHeader ? bytesRead - HDR : bytesRead;
                int numSamples = audioBytes / 2;
                if (numSamples > 0 && numSamples <= 512)
                {
                    auto* int16Data = reinterpret_cast<const int16_t*>(audioData);
                    for (int i = 0; i < numSamples; ++i)
                        convBuf[i] = (float)int16Data[i] / 32768.0f;
                    audioBuffer.write(convBuf, numSamples);
                }
            }
        }

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
