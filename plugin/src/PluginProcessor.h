#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "AudioRingBuffer.h"
#include "NetworkTransport.h"

class AnarackProcessor : public juce::AudioProcessor
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
    juce::String serverHost { "192.168.1.131" };

private:
    static constexpr double SERVER_SAMPLE_RATE = 48000.0;

    AudioRingBuffer audioRingBuffer;
    NetworkTransport transport;
    int underrunCount = 0;

    // Resampling: server sends 48kHz, host may run at 44.1kHz etc.
    juce::LagrangeInterpolator resampler;
    double resampleRatio = 1.0; // serverRate / hostRate
    std::vector<float> resampleInputBuf;

    // Pre-buffer: wait until we have enough audio before outputting
    static constexpr int PREBUFFER_MS = 20; // ms to accumulate before playing
    static constexpr int TARGET_BUFFER_MS = 20; // target fill level for adaptive ratio
    int prebufferSamples = 0;
    int targetBufferSamples = 0;
    bool prebuffering = true;
    double baseResampleRatio = 1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnarackProcessor)
};
