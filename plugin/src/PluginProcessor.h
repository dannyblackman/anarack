#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "AudioRingBuffer.h"
#include "JitterBuffer.h"
#include "NetworkTransport.h"
#include "SessionClient.h"

class AnarackProcessor : public juce::AudioProcessor, private juce::MidiInputCallback
{
public:
    AnarackProcessor();
    ~AnarackProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Anarack Rev2"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // Public access for the editor
    NetworkTransport& getTransport() { return transport; }
    std::atomic<int> midiInCount { 0 };

    // MIDI Learn: map external controller CCs to synth CCs
    std::atomic<int> learnTargetCC { -1 };       // synth CC waiting for learn (-1 = not learning)
    std::atomic<int> lastLearnedFrom { -1 };     // last controller CC that was learned
    std::atomic<int> lastLearnedTo { -1 };        // last synth CC it was mapped to
    std::atomic<int> mappedSendCount { 0 };      // debug: how many mapped CCs sent

    // ASRC/PLC diagnostics
    std::atomic<int> asrcDropCount { 0 };        // ASRC: samples dropped (buffer overfull)
    std::atomic<int> asrcDupCount { 0 };         // ASRC: samples duplicated (buffer underfull)
    std::atomic<int> plcConcealCount { 0 };      // PLC: unfilled slots concealed
    std::atomic<int> lastBlockSize { 0 };        // actual numOutputSamples from processBlock
    juce::String currentPatchName;               // last patch name from Rev2

    // Preset name ring buffer (for bank scan results)
    static constexpr int PRESET_RING_SIZE = 16;
    struct PresetNameEvent { int bank; int program; char name[24]; };
    PresetNameEvent presetNameRing[PRESET_RING_SIZE];
    std::atomic<int> presetNameWrite { 0 };
    std::atomic<int> presetNameRead { 0 };

    // Ring buffer for mapped CC values to push to UI (audio thread → message thread)
    static constexpr int CC_RING_SIZE = 64;
    struct CCEvent { uint8_t cc; uint8_t val; };
    CCEvent ccRing[CC_RING_SIZE];
    std::atomic<int> ccRingWrite { 0 };
    std::atomic<int> ccRingRead { 0 };
    int ccMap[128];                               // ccMap[controllerCC] = synthCC, -1 = unmapped
    int ccValues[128];                            // current value per synth CC (for relative mode)
    int lastRawVal[128];                          // last raw value per controller CC (for 0/127 direction detection)
    std::atomic<int> encoderSensitivity { 3 };    // multiplier for relative encoder ticks

    // Direct MIDI input (bypasses DAW routing for proper encoder support)
    std::unique_ptr<juce::MidiInput> directMidiInput;
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& msg) override;
    juce::StringArray getAvailableMidiInputs() const;
    void openDirectMidiInput(const juce::String& name);
    void startLearn(int synthCC) { learnTargetCC.store(synthCC); }
    void clearLearn() { learnTargetCC.store(-1); }
    void clearMapping(int controllerCC) { if (controllerCC >= 0 && controllerCC < 128) ccMap[controllerCC] = -1; }

    // DAW-automatable parameters (one per Rev2 CC parameter)
    struct SynthParam { int cc; juce::String name; int defaultVal; int maxVal; };
    static const std::vector<SynthParam>& getSynthParams();
    juce::AudioParameterFloat* paramByCC[128] {};  // quick lookup by CC number
    int lastAutomationVal[128] {};                 // last value sent from DAW automation

    // Fixed buffer mode: user-set buffer size in ms (0 = adaptive/auto)
    std::atomic<int> fixedBufferMs { 300 };
    void setFixedBuffer(int ms);

    juce::String serverHost { "192.168.1.131" };
    juce::String wgEndpoint { "66.245.195.65" };
    juce::String wgServerPubkey { "uX4s7vVGT+B2tJl7+4plM3vO+LceS/LKe+8A8IPH934=" };
    int wgPort = 51820;
    bool useWireGuard = true;

    // Session API for P2P connections
    SessionClient sessionClient;
    juce::String sessionApiUrl { "http://anarack.local:8800" };
    juce::String currentSessionId;
    juce::String piId { "anarack-pi-01" };
    bool useSessionApi = true;  // try session API first, fall back to static keys

    // Connection state (readable from UI)
    enum class ConnState { disconnected, connecting, connected };
    std::atomic<int> connectionState { (int)ConnState::disconnected };
    ConnState getConnState() const { return (ConnState)connectionState.load(); }

    // Connection management
    void autoConnect();
    void reconnect();
    void disconnectAndCleanup();
    std::atomic<int> autoDetectedBufferMs { 0 }; // result of auto buffer detection (0 = not yet detected)

private:
    std::unique_ptr<juce::Thread> connectThread;
    static constexpr double SERVER_SAMPLE_RATE = 48000.0;

    AudioRingBuffer audioRingBuffer;
public:
    JitterBuffer jitterBuffer;
private:
    NetworkTransport transport;
    int underrunCount = 0;

    // ASRC: drift-accumulating crossfade correction
    double smoothedFillError = 0.0;
    double driftAccumulator = 0.0;            // fractional sample drift accumulated
    bool asrcInitialised = false;
    int asrcBlockCount = 0;                   // blocks since last correction
    static constexpr int CROSSFADE_LEN = 32;  // samples for crossfade blend (~0.67ms @ 48kHz)
    float crossfadeBuf[CROSSFADE_LEN] {};     // temp buffer for crossfade overlap

    // Resampling: server sends 48kHz, host may run at 44.1kHz etc.
    juce::LagrangeInterpolator resampler;
    double resampleRatio = 1.0; // serverRate / hostRate
    std::vector<float> resampleInputBuf;

    // Adaptive pre-buffer: scales with network conditions
    int prebufferSamples = 0;
    int targetBufferSamples = 0;
    bool prebuffering = true;
    double baseResampleRatio = 1.0;

    void updatePrebuffer();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnarackProcessor)
};
