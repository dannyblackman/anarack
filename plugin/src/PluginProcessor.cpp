#include "PluginProcessor.h"
#include "PluginEditor.h"

// ── Rev2 parameter definitions for DAW automation ──
const std::vector<AnarackProcessor::SynthParam>& AnarackProcessor::getSynthParams()
{
    static const std::vector<SynthParam> params = {
        // Oscillator 1
        { 20, "Osc1 Freq", 36, 120 },
        { 21, "Osc1 Fine Tune", 50, 100 },
        { 22, "Osc1 Shape", 1, 4 },
        { 30, "Osc1 Pulse Width", 50, 99 },
        { 23, "Osc1 Glide", 0, 127 },
        // Oscillator 2
        { 24, "Osc2 Freq", 36, 120 },
        { 25, "Osc2 Fine Tune", 50, 100 },
        { 26, "Osc2 Shape", 1, 4 },
        { 31, "Osc2 Pulse Width", 50, 99 },
        { 27, "Osc2 Glide", 0, 127 },
        // Mixer
        { 28, "Osc Mix", 0, 127 },
        {  8, "Sub Octave", 0, 127 },
        { 29, "Noise", 0, 127 },
        {  9, "Osc Slop", 0, 127 },
        // Filter
        { 102, "Cutoff", 164, 164 },
        { 103, "Resonance", 0, 127 },
        { 104, "Filter Key Amt", 0, 127 },
        { 105, "Filter Audio Mod", 0, 127 },
        // Filter Envelope
        { 106, "Filter Env Amt", 127, 254 },
        { 107, "Filter Env Vel", 0, 127 },
        { 108, "Filter Env Delay", 0, 127 },
        { 109, "Filter Env Attack", 0, 127 },
        { 110, "Filter Env Decay", 64, 127 },
        { 111, "Filter Env Sustain", 64, 127 },
        { 112, "Filter Env Release", 64, 127 },
        // Amplifier
        { 113, "VCA Level", 0, 127 },
        {  37, "Volume", 127, 127 },
        { 114, "Pan Spread", 0, 127 },
        // Amp Envelope
        { 115, "Amp Env Amt", 127, 127 },
        { 116, "Amp Env Vel", 0, 127 },
        { 117, "Amp Env Delay", 0, 127 },
        { 118, "Amp Env Attack", 0, 127 },
        { 119, "Amp Env Decay", 64, 127 },
        {  75, "Amp Env Sustain", 64, 127 },
        {  76, "Amp Env Release", 64, 127 },
        // Aux Envelope (Env 3)
        {  85, "Aux Env Dest", 0, 52 },
        {  86, "Aux Env Amt", 127, 254 },
        {  87, "Aux Env Vel", 0, 127 },
        {  88, "Aux Env Delay", 0, 127 },
        {  89, "Aux Env Attack", 0, 127 },
        {  90, "Aux Env Decay", 64, 127 },
        {  77, "Aux Env Sustain", 64, 127 },
        {  78, "Aux Env Release", 64, 127 },
        // Effects
        {   3, "FX Type", 0, 13 },
        {  16, "FX On/Off", 0, 1 },
        {  17, "FX Mix", 64, 127 },
        {  12, "FX Param 1", 64, 255 },
        {  13, "FX Param 2", 64, 127 },
        // Clock
        {  14, "BPM", 120, 250 },
        {  15, "Clock Divide", 6, 12 },
        // Arp
        {  34, "Arp Mode", 0, 4 },
        {  33, "Arp On/Off", 0, 1 },
        // Glide
        {  65, "Glide On/Off", 0, 1 },
    };
    return params;
}

AnarackProcessor::AnarackProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      transport(audioRingBuffer, jitterBuffer)
{
    std::fill(std::begin(ccMap), std::end(ccMap), -1);
    std::fill(std::begin(ccValues), std::end(ccValues), 0);
    std::fill(std::begin(lastRawVal), std::end(lastRawVal), -1);
    std::fill(std::begin(paramByCC), std::end(paramByCC), nullptr);
    std::fill(std::begin(lastAutomationVal), std::end(lastAutomationVal), -1);

    // Register automatable parameters for each Rev2 CC
    for (auto& sp : getSynthParams())
    {
        auto* p = new juce::AudioParameterFloat(
            juce::ParameterID { "cc" + juce::String(sp.cc), 1 },
            sp.name,
            juce::NormalisableRange<float>(0.0f, (float)sp.maxVal, 1.0f),
            (float)sp.defaultVal
        );
        addParameter(p);
        if (sp.cc >= 0 && sp.cc < 128)
            paramByCC[sp.cc] = p;
    }
}

AnarackProcessor::~AnarackProcessor()
{
    if (directMidiInput) directMidiInput->stop();
    transport.disconnect();
}

juce::StringArray AnarackProcessor::getAvailableMidiInputs() const
{
    juce::StringArray names;
    for (auto& dev : juce::MidiInput::getAvailableDevices())
        names.add(dev.name);
    return names;
}

void AnarackProcessor::openDirectMidiInput(const juce::String& name)
{
    if (directMidiInput) { directMidiInput->stop(); directMidiInput.reset(); }
    if (name.isEmpty()) return;
    for (auto& dev : juce::MidiInput::getAvailableDevices())
    {
        if (dev.name == name)
        {
            directMidiInput = juce::MidiInput::openDevice(dev.identifier, this);
            if (directMidiInput) directMidiInput->start();
            return;
        }
    }
}

void AnarackProcessor::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& msg)
{
    // This runs on the MIDI input thread — same handling as processBlock
    midiInCount.fetch_add(1, std::memory_order_relaxed);

    if (msg.isController())
    {
        int inCC = msg.getControllerNumber();
        int val = msg.getControllerValue();

        // MIDI Learn
        int target = learnTargetCC.load();
        if (target >= 0)
        {
            for (int i = 0; i < 128; i++)
                if (ccMap[i] == target) ccMap[i] = -1;
            ccMap[inCC] = target;
            lastLearnedFrom.store(inCC);
            lastLearnedTo.store(target);
            learnTargetCC.store(-1);
            return;
        }

        // Apply CC mapping with relative support
        int synthCC = ccMap[inCC];
        if (synthCC >= 0)
        {
            int absVal;
            lastRawVal[inCC]++;

            if (val == 0 || val == 127)
            {
                if (lastRawVal[inCC] % 2 != 0) return;
                int dir = (val == 127) ? 1 : -1;
                absVal = juce::jlimit(0, 127, ccValues[synthCC] + dir);
            }
            else if (val >= 57 && val <= 71 && val != 64)
            {
                int delta = (val - 64) * encoderSensitivity.load(std::memory_order_relaxed);
                absVal = juce::jlimit(0, 127, ccValues[synthCC] + delta);
            }
            else if (val == 64)
            {
                return;
            }
            else
            {
                absVal = val;
            }

            ccValues[synthCC] = absVal;
            const uint8_t mapped[3] = { 0xB0, (uint8_t)synthCC, (uint8_t)absVal };
            transport.sendMidi(mapped, 3);
            mappedSendCount.fetch_add(1, std::memory_order_relaxed);

            int w = ccRingWrite.load(std::memory_order_relaxed);
            ccRing[w % CC_RING_SIZE] = { (uint8_t)synthCC, (uint8_t)absVal };
            ccRingWrite.store(w + 1, std::memory_order_release);
            return;
        }
    }

    // Forward unmapped (notes etc) to transport
    transport.sendMidi(msg.getRawData(), msg.getRawDataSize());
}

void AnarackProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Ring buffer: 500ms at server sample rate (48kHz)
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
    // Send CCs for DAW-automated parameters — only when the DAW changes them,
    // not when the UI or controller changes ccValues
    for (auto& sp : getSynthParams())
    {
        auto* p = paramByCC[sp.cc];
        if (!p) continue;
        int newVal = (int)p->get();
        if (newVal != lastAutomationVal[sp.cc])
        {
            lastAutomationVal[sp.cc] = newVal;
            ccValues[sp.cc] = newVal;
            int ccVal = sp.maxVal > 127 ? juce::roundToInt(newVal * 127.0f / sp.maxVal) : newVal;
            const uint8_t msg[3] = { 0xB0, (uint8_t)sp.cc, (uint8_t)ccVal };
            transport.sendMidi(msg, 3);
            // Push to UI ring buffer
            int w = ccRingWrite.load(std::memory_order_relaxed);
            ccRing[w % CC_RING_SIZE] = { (uint8_t)sp.cc, (uint8_t)ccVal };
            ccRingWrite.store(w + 1, std::memory_order_release);
        }
    }

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
                lastRawVal[inCC]++;

                // LaunchKey sends alternating 0/127 per encoder tick.
                // Process only every OTHER message (skip the second of each pair).
                // For other values, use offset-binary relative or absolute.
                if (val == 0 || val == 127)
                {
                    if (lastRawVal[inCC] % 2 != 0) continue; // skip every other 0/127
                    // Direction: 127 = increment, 0 = decrement
                    int dir = (val == 0) ? 1 : -1;
                    absVal = juce::jlimit(0, 127, ccValues[synthCC] + dir);
                }
                else if (val >= 57 && val <= 71 && val != 64)
                {
                    absVal = juce::jlimit(0, 127, ccValues[synthCC] + (val - 64));
                }
                else if (val == 64)
                {
                    continue;
                }
                else
                {
                    absVal = val;
                }

                ccValues[synthCC] = absVal;
                const uint8_t mapped[3] = { 0xB0, (uint8_t)synthCC, (uint8_t)absVal };
                transport.sendMidi(mapped, 3);
                mappedSendCount.fetch_add(1, std::memory_order_relaxed);
                // Sync DAW parameter
                if (auto* p = paramByCC[synthCC])
                    p->setValueNotifyingHost(p->convertTo0to1((float)absVal));
                // Push to ring buffer for UI update
                int w = ccRingWrite.load(std::memory_order_relaxed);
                ccRing[w % CC_RING_SIZE] = { (uint8_t)synthCC, (uint8_t)absVal };
                ccRingWrite.store(w + 1, std::memory_order_release);
                continue;
            }
        }

        // Forward unmapped messages (notes, pitchbend, etc.) as-is
        // Force notes to channel 1 for Rev2 compatibility
        if (msg.isNoteOnOrOff())
        {
            auto ch1msg = msg.isNoteOn()
                ? juce::MidiMessage::noteOn(1, msg.getNoteNumber(), msg.getVelocity())
                : juce::MidiMessage::noteOff(1, msg.getNoteNumber(), msg.getVelocity());
            transport.sendMidi(ch1msg.getRawData(), ch1msg.getRawDataSize());
            // Log note to ring: use CC 125 as note marker
            int w = ccRingWrite.load(std::memory_order_relaxed);
            ccRing[w % CC_RING_SIZE] = { 125, (uint8_t)(msg.isNoteOn() ? msg.getNoteNumber() : 0) };
            ccRingWrite.store(w + 1, std::memory_order_release);
        }
        else
        {
            transport.sendMidi(msg.getRawData(), msg.getRawDataSize());
        }
    }
    midiMessages.clear();

    // Read audio from network buffer
    auto numOutputSamples = buffer.getNumSamples();
    auto* outL = buffer.getWritePointer(0);

    if (jitterBuffer.isConfigured())
    {
        // ── JitterBuffer path: fixed latency, loss concealment ──
        if (jitterBuffer.isPrebuffering())
        {
            buffer.clear();
        }
        else
        {
            // Clock drift correction: check every 192 blocks (~0.5s)
            // Correct when buffer drifts >15% from target
            driftCounter++;
            int extra = 0;
            if (driftCounter >= 192)
            {
                driftCounter = 0;
                int fill = jitterBuffer.getFillLevel();
                int target = jitterBuffer.getFixedLatencySamples() / 2;
                int drift = fill - target;
                if (drift > target / 6)
                    extra = 1;
                else if (drift < -(target / 6))
                    extra = -1;
            }

            int samplesToRead = numOutputSamples + extra;
            if (samplesToRead < 1) samplesToRead = 1;

            // Read into temp buffer if size differs from output
            if (extra == 0)
            {
                jitterBuffer.read(outL, numOutputSamples);
            }
            else
            {
                if (samplesToRead > (int)resampleInputBuf.size())
                    resampleInputBuf.resize((size_t)samplesToRead, 0.0f);
                jitterBuffer.read(resampleInputBuf.data(), samplesToRead);
                // Copy to output (drop extra or duplicate last)
                int copyCount = std::min(samplesToRead, numOutputSamples);
                std::memcpy(outL, resampleInputBuf.data(), (size_t)copyCount * sizeof(float));
                if (copyCount < numOutputSamples)
                    outL[numOutputSamples - 1] = resampleInputBuf[(size_t)(copyCount - 1)];
            }
        }
    }
    else
    {
        // ── Legacy AudioRingBuffer path ──
        if (prebuffering)
        {
            if (audioRingBuffer.getNumReady() < prebufferSamples)
            {
                buffer.clear();
                if (buffer.getNumChannels() > 1)
                    buffer.copyFrom(1, 0, buffer, 0, 0, numOutputSamples);
                return;
            }
            prebuffering = false;
        }

        int buffered = audioRingBuffer.getNumReady();
        if (std::abs(baseResampleRatio - 1.0) > 0.001)
        {
            double drift = (double)(buffered - targetBufferSamples) / (double)targetBufferSamples;
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
            if (fixedBufferMs.load() == 0)
                updatePrebuffer();
            if (buffer.getNumChannels() > 1)
                buffer.copyFrom(1, 0, buffer, 0, 0, numOutputSamples);
            return;
        }

        if (std::abs(baseResampleRatio - 1.0) < 0.001)
            audioRingBuffer.read(outL, numOutputSamples);
        else
        {
            if (inputNeeded > (int)resampleInputBuf.size())
                resampleInputBuf.resize((size_t)inputNeeded, 0.0f);
            audioRingBuffer.read(resampleInputBuf.data(), inputNeeded);
            resampler.process(resampleRatio, resampleInputBuf.data(), outL, numOutputSamples);
        }
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

void AnarackProcessor::setFixedBuffer(int ms)
{
    fixedBufferMs.store(juce::jlimit(0, 2000, ms));
    updatePrebuffer();
}

void AnarackProcessor::updatePrebuffer()
{
    int fixed = fixedBufferMs.load();
    int bufferMs;

    if (fixed > 0)
    {
        // Fixed mode: user-set buffer, no adaptation
        bufferMs = fixed;
    }
    else
    {
        // Adaptive mode: scale with RTT
        int rtt = transport.getEstimatedRtt();
        if (rtt <= 0)
            bufferMs = 20;
        else if (rtt < 20)
            bufferMs = 15;
        else
            bufferMs = rtt * 2;
    }

    prebufferSamples = (int)(SERVER_SAMPLE_RATE * bufferMs / 1000.0);
    targetBufferSamples = prebufferSamples;

    // Total latency = buffer + network round-trip
    // Buffer: audio sits in the ring buffer before playback
    // RTT: MIDI takes RTT/2 to reach Rev2, audio takes RTT/2 to come back
    int rtt = transport.getEstimatedRtt();
    int totalLatencyMs = bufferMs + (rtt > 0 ? rtt : 0);
    setLatencySamples((int)(SERVER_SAMPLE_RATE * totalLatencyMs / 1000.0));
}

juce::AudioProcessorEditor* AnarackProcessor::createEditor()
{
    return new AnarackEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AnarackProcessor();
}
