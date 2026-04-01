#pragma once
#include <juce_core/juce_core.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "AudioRingBuffer.h"
#include "JitterBuffer.h"
#include "JitterEstimator.h"
#include "WgTunnel.h"

// Lock-free MIDI message slot for audio thread -> network thread communication.
struct MidiSlot
{
    uint8_t data[3];
    int size;
};

// Handles communication with the Anarack server.
// Two modes:
//   - Raw UDP: for LAN connections (direct IP, no encryption)
//   - WireGuard: for internet connections (encrypted tunnel via boringtun)
class NetworkTransport : private juce::Thread
{
public:
    NetworkTransport(AudioRingBuffer& audioBuffer, JitterBuffer& jitterBuffer);
    ~NetworkTransport() override;

    // Raw UDP connection (LAN)
    void connect(const juce::String& host, int midiPort = 5555, int audioPort = 9999);

    // WireGuard connection (internet)
    void connectWireGuard(const juce::String& serverEndpoint,
                          const juce::String& serverPubkey,
                          int midiPort = 5555, int audioPort = 9999);

    void disconnect();
    bool isConnected() const { return connected.load(); }
    bool isWireGuard() const { return useWireGuard; }

    // Called from audio thread — pushes a MIDI message into the lock-free FIFO.
    void sendMidi(const uint8_t* data, int size);

    // Stats for UI
    int getPacketsReceived() const { return packetsReceived.load(); }
    int getBufferLevel() const { return audioBuffer.getNumReady(); }
    int getEstimatedRtt() const;
    int getLastPacketSize() const { return lastPacketSize.load(); }

    // Callback for incoming CC from the Rev2 (synth → plugin → UI)
    std::function<void(int cc, int value)> onSynthCC;
    // Callback for patch name from server
    std::function<void(const juce::String&)> onPatchName;

private:
    void run() override; // Receive thread (raw UDP mode)
    void runWireGuard();  // Receive loop (WireGuard mode)
    void sendPendingMidi();
    void sendRegistration();

    AudioRingBuffer& audioBuffer;
    JitterBuffer& jitterBuffer;
public:
    JitterEstimator jitterEstimator;
private:

    // FEC recovery
    static constexpr int FEC_RING_SIZE = 16;
    struct FecEntry { uint32_t seq; uint32_t timestamp; std::vector<uint8_t> payload; bool valid; };
    FecEntry dataRing[FEC_RING_SIZE];   // recent data packets
    FecEntry fecRing[FEC_RING_SIZE / 2]; // recent FEC packets
    int dataRingIdx = 0;
    int fecRingIdx = 0;
    void processPacketWithFec(const uint8_t* data, int size);
    void tryFecRecovery(const FecEntry& fec);

    // Connection mode
    bool useWireGuard = false;
    std::unique_ptr<WgTunnel> wgTunnel;

    // Raw UDP (LAN mode)
    int rawSendFd = -1;
    int rawRecvFd = -1;
    struct sockaddr_in serverAddr {};

    juce::String serverHost;
    int serverMidiPort = 5555;
    int serverAudioPort = 9999;

    std::atomic<bool> connected { false };
    std::atomic<int> packetsReceived { 0 };
    std::atomic<int> lastPacketSize { 0 };

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
