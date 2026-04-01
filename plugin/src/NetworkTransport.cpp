#include "NetworkTransport.h"
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

NetworkTransport::NetworkTransport(AudioRingBuffer& ringBuf, JitterBuffer& jitBuf)
    : Thread("AnarackAudioRecv"), audioBuffer(ringBuf), jitterBuffer(jitBuf)
{
    for (auto& e : dataRing) e.valid = false;
    for (auto& e : fecRing) e.valid = false;
}

void NetworkTransport::processPacketWithFec(const uint8_t* data, int size)
{
    if (size < JitterBuffer::HEADER_SIZE + 2)
        return;

    // Parse header
    uint32_t seq, timestamp;
    uint16_t flags;
    std::memcpy(&seq, data, 4);
    std::memcpy(&timestamp, data + 4, 4);
    std::memcpy(&flags, data + 8, 2);

    bool isFec = (flags & 0x0001) != 0;
    int payloadSize = size - JitterBuffer::HEADER_SIZE;
    const uint8_t* payload = data + JitterBuffer::HEADER_SIZE;

    if (isFec)
    {
        // Store FEC packet
        auto& entry = fecRing[fecRingIdx % (FEC_RING_SIZE / 2)];
        entry.seq = seq;
        entry.timestamp = timestamp; // timestamp of first covered packet
        entry.payload.assign(payload, payload + payloadSize);
        entry.valid = true;
        fecRingIdx++;

        // Try to recover a missing packet
        tryFecRecovery(entry);
    }
    else
    {
        // Write data packet to jitter buffer
        jitterBuffer.writePacket(data, size);

        // Record arrival for adaptive jitter estimation
        double nowMs = juce::Time::getMillisecondCounterHiRes();
        jitterEstimator.recordArrival(seq, nowMs);

        // Store in data ring for FEC recovery
        auto& entry = dataRing[dataRingIdx % FEC_RING_SIZE];
        entry.seq = seq;
        entry.timestamp = timestamp;
        entry.payload.assign(payload, payload + payloadSize);
        entry.valid = true;
        dataRingIdx++;

        // Check if any stored FEC packet can now recover a missing partner
        for (int i = 0; i < FEC_RING_SIZE / 2; i++)
        {
            auto& fec = fecRing[i];
            if (fec.valid)
                tryFecRecovery(fec);
        }
    }
}

void NetworkTransport::tryFecRecovery(const FecEntry& fec)
{
    // FEC covers two packets: timestamp T and timestamp T+128
    uint32_t ts1 = fec.timestamp;
    uint32_t ts2 = ts1 + JitterBuffer::PACKET_SAMPLES;

    // Find which data packets we have
    const FecEntry* pkt1 = nullptr;
    const FecEntry* pkt2 = nullptr;
    for (int i = 0; i < FEC_RING_SIZE; i++)
    {
        auto& d = dataRing[i];
        if (!d.valid) continue;
        if (d.timestamp == ts1) pkt1 = &d;
        if (d.timestamp == ts2) pkt2 = &d;
    }

    // If we have both, no recovery needed
    if (pkt1 && pkt2) return;
    // If we have neither, can't recover
    if (!pkt1 && !pkt2) return;

    // Recover the missing one via XOR
    const FecEntry* have = pkt1 ? pkt1 : pkt2;
    uint32_t missingTs = pkt1 ? ts2 : ts1;

    if (have->payload.size() != fec.payload.size())
        return; // size mismatch, can't recover

    // XOR to recover
    std::vector<uint8_t> recovered(fec.payload.size());
    for (size_t i = 0; i < recovered.size(); i++)
        recovered[i] = fec.payload[i] ^ have->payload[i];

    // Build a packet header for the recovered data and write to jitter buffer
    uint8_t recoveredPacket[JitterBuffer::HEADER_SIZE + JitterBuffer::PAYLOAD_BYTES];
    uint32_t recoveredSeq = 0; // unknown, but writePacket handles late arrivals
    uint16_t noFlags = 0;
    int16_t noChecksum = 0;
    std::memcpy(recoveredPacket, &recoveredSeq, 4);
    std::memcpy(recoveredPacket + 4, &missingTs, 4);
    std::memcpy(recoveredPacket + 8, &noFlags, 2);
    std::memcpy(recoveredPacket + 10, &noChecksum, 2);
    std::memcpy(recoveredPacket + JitterBuffer::HEADER_SIZE, recovered.data(),
                juce::jmin((int)recovered.size(), JitterBuffer::PAYLOAD_BYTES));

    jitterBuffer.writePacket(recoveredPacket, JitterBuffer::HEADER_SIZE + (int)recovered.size());
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

        // JSON message from server (patch name etc.)
        if (bytesRead > 2 && packetBuf[0] == '{')
        {
            auto json = juce::String::fromUTF8((const char*)packetBuf, bytesRead);
            auto parsed = juce::JSON::parse(json);
            if (parsed.isObject())
            {
                auto type = parsed.getProperty("type", "").toString();
                if (type == "patchName" && onPatchName)
                    onPatchName(parsed.getProperty("name", "").toString());
            }
            continue;
        }

        // 3-byte MIDI CC from Rev2 (synth parameter update)
        if (bytesRead == 3 && (packetBuf[0] & 0xF0) == 0xB0)
        {
            if (onSynthCC)
                onSynthCC(packetBuf[1], packetBuf[2]);
        }
        else if ((bytesRead == JitterBuffer::PACKET_BYTES || bytesRead == JitterBuffer::LEGACY_PACKET) && jitterBuffer.isConfigured())
        {
            processPacketWithFec(packetBuf, bytesRead);
        }
        else
        {
            // Legacy fallback: strip header, decode 24-bit or 16-bit
            const uint8_t* audioStart = packetBuf;
            int audioBytes = bytesRead;
            bool hasHeader = (bytesRead == JitterBuffer::PACKET_BYTES || bytesRead == JitterBuffer::LEGACY_PACKET);
            if (hasHeader) {
                audioStart = packetBuf + JitterBuffer::HEADER_SIZE;
                audioBytes = bytesRead - JitterBuffer::HEADER_SIZE;
            }

            int numSamples;
            if (audioBytes == JitterBuffer::PACKET_SAMPLES * 3) {
                // 24-bit packed
                numSamples = audioBytes / 3;
                for (int i = 0; i < numSamples; i++) {
                    int32_t val = (int32_t)(audioStart[i*3] | (audioStart[i*3+1] << 8) | (audioStart[i*3+2] << 16));
                    if (val & 0x800000) val |= 0xFF000000;
                    convBuf[i] = (float)val / 8388608.0f;
                }
            } else {
                // 16-bit
                numSamples = audioBytes / 2;
                auto* int16Data = reinterpret_cast<const int16_t*>(audioStart);
                for (int i = 0; i < numSamples; ++i)
                    convBuf[i] = (float)int16Data[i] / 32768.0f;
            }
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

        // JSON message from server (patch name etc.)
        if (bytesRead > 2 && packetBuf[0] == '{')
        {
            auto json = juce::String::fromUTF8((const char*)packetBuf, bytesRead);
            auto parsed = juce::JSON::parse(json);
            if (parsed.isObject())
            {
                auto type = parsed.getProperty("type", "").toString();
                if (type == "patchName" && onPatchName)
                    onPatchName(parsed.getProperty("name", "").toString());
            }
            continue;
        }

        // 3-byte MIDI CC from Rev2 (synth parameter update)
        if (bytesRead == 3 && (packetBuf[0] & 0xF0) == 0xB0)
        {
            if (onSynthCC)
                onSynthCC(packetBuf[1], packetBuf[2]);
            continue;
        }

        // Audio packets come from the server's audio port
        if (srcPort == (uint16_t)serverAudioPort || bytesRead >= 128)
        {
            if ((bytesRead == JitterBuffer::PACKET_BYTES || bytesRead == JitterBuffer::LEGACY_PACKET) && jitterBuffer.isConfigured())
            {
                processPacketWithFec(packetBuf, bytesRead);
            }
            else
            {
                const uint8_t* audioStart = packetBuf;
                int audioBytes = bytesRead;
                bool hasHdr = (bytesRead == JitterBuffer::PACKET_BYTES || bytesRead == JitterBuffer::LEGACY_PACKET);
                if (hasHdr) {
                    audioStart = packetBuf + JitterBuffer::HEADER_SIZE;
                    audioBytes = bytesRead - JitterBuffer::HEADER_SIZE;
                }
                int numSamples;
                if (audioBytes == JitterBuffer::PACKET_SAMPLES * 3) {
                    numSamples = audioBytes / 3;
                    for (int i = 0; i < numSamples; i++) {
                        int32_t val = (int32_t)(audioStart[i*3] | (audioStart[i*3+1] << 8) | (audioStart[i*3+2] << 16));
                        if (val & 0x800000) val |= 0xFF000000;
                        convBuf[i] = (float)val / 8388608.0f;
                    }
                } else {
                    numSamples = audioBytes / 2;
                    auto* int16Data = reinterpret_cast<const int16_t*>(audioStart);
                    for (int i = 0; i < numSamples; ++i)
                        convBuf[i] = (float)int16Data[i] / 32768.0f;
                }
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
