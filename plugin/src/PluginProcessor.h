#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "AudioRingBuffer.h"
#include "JitterBuffer.h"
#include "NetworkTransport.h"

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
    juce::String currentPatchName;
    std::atomic<bool> patchNameUpdated { false };

    // MIDI Learn: map external controller CCs to synth CCs
    std::atomic<int> learnTargetCC { -1 };       // synth CC waiting for learn (-1 = not learning)
    std::atomic<int> lastLearnedFrom { -1 };     // last controller CC that was learned
    std::atomic<int> lastLearnedTo { -1 };        // last synth CC it was mapped to
    std::atomic<int> mappedSendCount { 0 };      // debug: how many mapped CCs sent

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

private:
    static constexpr double SERVER_SAMPLE_RATE = 48000.0;

    AudioRingBuffer audioRingBuffer;
public:
    JitterBuffer jitterBuffer;
private:
    NetworkTransport transport;
    int underrunCount = 0;
    int driftCounter = 0;

    // Media clock recovery: measure server clock rate vs DAW clock rate
    int64_t clockRecoveryDAWSamples = 0;      // total DAW samples consumed
    int64_t clockRecoveryStartDAW = -1;       // DAW sample count at measurement start
    int clockRecoveryStartFill = 0;           // buffer fill at measurement start
    double recoveredRatio = 1.0;              // server_rate / daw_rate

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
