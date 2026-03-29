#include "NetworkTransport.h"
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

NetworkTransport::NetworkTransport(AudioRingBuffer& ringBuf)
    : Thread("AnarackAudioRecv"), audioBuffer(ringBuf)
{
}

NetworkTransport::~NetworkTransport()
{
    disconnect();
}

void NetworkTransport::connect(const juce::String& host, int midiPort, int audioPort, int)
{
    disconnect();

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
    if (rawSendFd < 0)
        return;

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
            // Fall back to ephemeral port
            recvAddr.sin_port = 0;
            ::bind(rawRecvFd, (struct sockaddr*)&recvAddr, sizeof(recvAddr));
        }
    }

    connected = true;
    packetsReceived = 0;
    midiFifo.reset();
    audioBuffer.reset();

    // Send registration byte to trigger audio return from server
    sendRegistration();

    // Start receive thread
    startThread(juce::Thread::Priority::high);

    // Start send thread
    sendThread = std::make_unique<SendThread>(*this);
    sendThread->startThread(juce::Thread::Priority::high);
}

void NetworkTransport::disconnect()
{
    connected = false;

    if (sendThread)
    {
        sendThread->stopThread(500);
        sendThread.reset();
    }

    stopThread(500);

    if (rawRecvFd >= 0) { ::close(rawRecvFd); rawRecvFd = -1; }
    if (rawSendFd >= 0) { ::close(rawSendFd); rawSendFd = -1; }
    audioBuffer.reset();
    midiFifo.reset();
}

void NetworkTransport::sendRegistration()
{
    if (rawSendFd >= 0)
    {
        uint8_t reg = 0xFE; // Active Sensing — triggers server audio return
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

    if (rawSendFd < 0) { midiFifo.finishedRead(size1 + size2); return; }

    for (int i = 0; i < size1; ++i)
    {
        auto& slot = midiSlots[start1 + i];
        sendto(rawSendFd, slot.data, (size_t)slot.size, 0,
               (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    }
    for (int i = 0; i < size2; ++i)
    {
        auto& slot = midiSlots[start2 + i];
        sendto(rawSendFd, slot.data, (size_t)slot.size, 0,
               (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    }

    midiFifo.finishedRead(size1 + size2);
}

// Audio receive thread
void NetworkTransport::run()
{
    constexpr int MAX_PACKET = 1024;
    uint8_t packetBuf[MAX_PACKET];
    float convBuf[MAX_PACKET / 2];

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 50000; // 50ms timeout for shutdown check
    setsockopt(rawRecvFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    while (!threadShouldExit() && connected.load())
    {
        if (rawRecvFd < 0) break;
        int bytesRead = (int)recvfrom(rawRecvFd, packetBuf, MAX_PACKET, 0, nullptr, nullptr);

        if (bytesRead <= 0)
            continue;

        int numSamples = bytesRead / 2;
        auto* int16Data = reinterpret_cast<const int16_t*>(packetBuf);
        for (int i = 0; i < numSamples; ++i)
            convBuf[i] = (float)int16Data[i] / 32768.0f;

        audioBuffer.write(convBuf, numSamples);
        packetsReceived.fetch_add(1, std::memory_order_relaxed);
    }
}

// MIDI send thread
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
