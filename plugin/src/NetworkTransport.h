#pragma once
#include <juce_core/juce_core.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "AudioRingBuffer.h"

// Lock-free MIDI message slot for audio thread -> network thread communication.
struct MidiSlot
{
    uint8_t data[3];
    int size;
};

// Handles UDP communication with the Anarack server.
// - Sends MIDI messages (from lock-free FIFO filled by audio thread)
// - Receives audio (int16 PCM from server, converts to float, pushes to ring buffer)
// - Runs on background threads, never blocks the audio thread.
class NetworkTransport : private juce::Thread
{
public:
    NetworkTransport(AudioRingBuffer& audioBuffer);
    ~NetworkTransport() override;

    void connect(const juce::String& host, int midiPort = 5555, int audioPort = 9999, int preallocatedSendFd = -1);
    void disconnect();
    bool isConnected() const { return connected.load(); }

    // Called from audio thread — pushes a MIDI message into the lock-free FIFO.
    void sendMidi(const uint8_t* data, int size);

    // Stats for UI
    int getPacketsReceived() const { return packetsReceived.load(); }
    int getBufferLevel() const { return audioBuffer.getNumReady(); }

private:
    void run() override; // Receive thread
    void sendPendingMidi();
    void sendRegistration();

    AudioRingBuffer& audioBuffer;

    int rawSendFd = -1;
    int rawRecvFd = -1;
    struct sockaddr_in serverAddr {};
    juce::String serverHost;
    int serverMidiPort = 5555;
    int serverAudioPort = 9999;

    std::atomic<bool> connected { false };
    std::atomic<int> packetsReceived { 0 };

    // Lock-free MIDI FIFO (audio thread writes, network thread reads)
    static constexpr int MIDI_FIFO_SIZE = 256;
    juce::AbstractFifo midiFifo { MIDI_FIFO_SIZE };
    MidiSlot midiSlots[MIDI_FIFO_SIZE];

    // Send thread
    std::unique_ptr<juce::Thread> sendThread;

    class SendThread : public juce::Thread
    {
    public:
        SendThread(NetworkTransport& parent) : Thread("AnarackMidiSend"), parent(parent) {}
        void run() override;
    private:
        NetworkTransport& parent;
    };
};
