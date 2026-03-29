#include "PluginProcessor.h"
#include "PluginEditor.h"

AnarackProcessor::AnarackProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      transport(audioRingBuffer)
{
    // (network sockets created in NetworkTransport::connect)
}

AnarackProcessor::~AnarackProcessor()
{
    transport.disconnect();
}

void AnarackProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Ring buffer: ~200ms at server sample rate (48kHz)
    int bufferSize = (int)(SERVER_SAMPLE_RATE * 0.2);
    audioRingBuffer.resize(bufferSize);

    // Resampling ratio: how many input (48kHz) samples per output sample
    baseResampleRatio = SERVER_SAMPLE_RATE / sampleRate;
    resampleRatio = baseResampleRatio;
    resampler.reset();
    prebufferSamples = (int)(SERVER_SAMPLE_RATE * PREBUFFER_MS / 1000.0);
    targetBufferSamples = (int)(SERVER_SAMPLE_RATE * TARGET_BUFFER_MS / 1000.0);
    prebuffering = true;

    // Pre-allocate buffer for resampler input (worst case: full block at highest ratio)
    int maxInputSamples = (int)(samplesPerBlock * resampleRatio + 16);
    resampleInputBuf.resize((size_t)maxInputSamples, 0.0f);
}

void AnarackProcessor::releaseResources()
{
    transport.disconnect();
}

void AnarackProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // Forward MIDI from Logic to the server via UDP
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        transport.sendMidi(msg.getRawData(), msg.getRawDataSize());
    }
    midiMessages.clear();

    // Read audio from the network ring buffer, resampling from 48kHz to host rate
    auto numOutputSamples = buffer.getNumSamples();
    auto* outL = buffer.getWritePointer(0);

    // Pre-buffer: output silence until we have enough audio buffered
    if (prebuffering)
    {
        if (audioRingBuffer.getNumReady() < prebufferSamples)
        {
            buffer.clear();
            return;
        }
        prebuffering = false;
    }

    // Adaptive resample ratio: nudge slightly to keep buffer at target fill level
    // This prevents slow drift between server and host clocks
    int buffered = audioRingBuffer.getNumReady();
    if (std::abs(baseResampleRatio - 1.0) > 0.001)
    {
        double drift = (double)(buffered - targetBufferSamples) / (double)targetBufferSamples;
        // Gently adjust: if buffer is fuller than target, consume slightly faster
        resampleRatio = baseResampleRatio * (1.0 - drift * 0.002);
    }

    int inputNeeded;
    if (std::abs(baseResampleRatio - 1.0) < 0.001)
        inputNeeded = numOutputSamples;
    else
        inputNeeded = (int)(numOutputSamples * resampleRatio + 2);

    if (buffered < inputNeeded)
    {
        buffer.clear();
        prebuffering = true;
        return;
    }

    if (std::abs(baseResampleRatio - 1.0) < 0.001)
    {
        audioRingBuffer.read(outL, numOutputSamples);
    }
    else
    {
        if (inputNeeded > (int)resampleInputBuf.size())
            resampleInputBuf.resize((size_t)inputNeeded, 0.0f);

        audioRingBuffer.read(resampleInputBuf.data(), inputNeeded);

        // Resample: ratio = inputRate / outputRate
        resampler.process(resampleRatio, resampleInputBuf.data(), outL, numOutputSamples);
    }

    // Duplicate mono to right channel if stereo
    if (buffer.getNumChannels() > 1)
        buffer.copyFrom(1, 0, buffer, 0, 0, numOutputSamples);
}

bool AnarackProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    auto mainOut = layouts.getMainOutputChannelSet();
    return mainOut == juce::AudioChannelSet::stereo() ||
           mainOut == juce::AudioChannelSet::mono();
}

void AnarackProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ValueTree state("AnarackState");
    state.setProperty("serverHost", serverHost, nullptr);
    juce::MemoryOutputStream stream(destData, false);
    state.writeToStream(stream);
}

void AnarackProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto state = juce::ValueTree::readFromData(data, (size_t)sizeInBytes);
    if (state.hasType("AnarackState"))
        serverHost = state.getProperty("serverHost", "anarack.local").toString();
}

juce::AudioProcessorEditor* AnarackProcessor::createEditor()
{
    return new AnarackEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AnarackProcessor();
}
