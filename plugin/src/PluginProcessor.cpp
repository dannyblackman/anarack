#include "PluginProcessor.h"
#include "PluginEditor.h"

AnarackProcessor::AnarackProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      transport(audioRingBuffer)
{
    std::fill(std::begin(ccMap), std::end(ccMap), -1);
    std::fill(std::begin(ccValues), std::end(ccValues), 0);
}

AnarackProcessor::~AnarackProcessor()
{
    transport.disconnect();
}

void AnarackProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Ring buffer: ~500ms at server sample rate (48kHz) — enough for high-latency connections
    int bufferSize = (int)(SERVER_SAMPLE_RATE * 0.5);
    audioRingBuffer.resize(bufferSize);

    // Resampling ratio: how many input (48kHz) samples per output sample
    baseResampleRatio = SERVER_SAMPLE_RATE / sampleRate;
    resampleRatio = baseResampleRatio;
    resampler.reset();
    updatePrebuffer();
    prebuffering = true;
    setLatencySamples(prebufferSamples);

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
    // Forward MIDI from DAW to the server, with CC learn/mapping
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        midiInCount.fetch_add(1, std::memory_order_relaxed);

        if (msg.isController())
        {
            int inCC = msg.getControllerNumber();
            int val = msg.getControllerValue();

            // MIDI Learn: if we're waiting for a controller CC, capture it
            int target = learnTargetCC.load();
            if (target >= 0)
            {
                // Remove any old mapping to this synth CC
                for (int i = 0; i < 128; i++)
                    if (ccMap[i] == target) ccMap[i] = -1;
                ccMap[inCC] = target;
                lastLearnedFrom.store(inCC);
                lastLearnedTo.store(target);
                learnTargetCC.store(-1);
                // Don't send the learn trigger value — it's usually a relative tick
                continue;
            }

            // Apply CC mapping if one exists
            int synthCC = ccMap[inCC];
            if (synthCC >= 0)
            {
                int absVal;
                // Match browser client logic exactly:
                // Values 57-71 (excluding 64) = relative offset from 64
                // Values 0 or 127 = ignore (LaunchKey toggle noise)
                // Everything else = absolute
                if (val != 0 && val != 127 && val >= 57 && val <= 71 && val != 64)
                {
                    int offset = val - 64; // e.g. 65=+1, 63=-1
                    absVal = juce::jlimit(0, 127, ccValues[synthCC] + offset);
                }
                else if (val == 0 || val == 127)
                {
                    continue; // drop toggle noise
                }
                else
                {
                    absVal = val; // absolute from normal controllers
                }

                ccValues[synthCC] = absVal;
                const uint8_t mapped[3] = { 0xB0, (uint8_t)synthCC, (uint8_t)absVal };
                transport.sendMidi(mapped, 3);
                mappedSendCount.fetch_add(1, std::memory_order_relaxed);
                // Push to ring buffer for UI update
                int w = ccRingWrite.load(std::memory_order_relaxed);
                ccRing[w % CC_RING_SIZE] = { (uint8_t)synthCC, (uint8_t)absVal };
                ccRingWrite.store(w + 1, std::memory_order_release);
                continue;
            }
        }

        // Forward unmapped messages (notes, pitchbend, etc.) as-is
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
        updatePrebuffer(); // Re-evaluate based on current RTT
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

void AnarackProcessor::updatePrebuffer()
{
    // Scale pre-buffer with RTT: low latency = small buffer, high latency = larger buffer
    int rtt = transport.getEstimatedRtt();
    int bufferMs;
    if (rtt <= 0)
        bufferMs = 20;          // No RTT data yet — conservative default
    else if (rtt < 20)
        bufferMs = 15;          // LAN / nearby VPS — keep it tight
    else
        bufferMs = rtt * 2;     // Internet — buffer at 2x RTT to absorb jitter

    prebufferSamples = (int)(SERVER_SAMPLE_RATE * bufferMs / 1000.0);
    targetBufferSamples = prebufferSamples;
    setLatencySamples(prebufferSamples);
}

juce::AudioProcessorEditor* AnarackProcessor::createEditor()
{
    return new AnarackEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AnarackProcessor();
}
