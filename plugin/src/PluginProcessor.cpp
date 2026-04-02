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
    if (connectThread) connectThread->stopThread(3000);
    if (directMidiInput) directMidiInput->stop();
    if (currentSessionId.isNotEmpty())
        sessionClient.endSession(currentSessionId);
    transport.disconnect();
}

// Background thread class for auto-connect
class ConnectThread : public juce::Thread
{
public:
    ConnectThread(AnarackProcessor& p) : Thread("AnarackConnect"), proc(p) {}
    void run() override { proc.autoConnect(); }
private:
    AnarackProcessor& proc;
};

void AnarackProcessor::autoConnect()
{
    if (transport.isConnected()) return;

    connectionState.store((int)ConnState::connecting);

    // Only configure JitterBuffer for WireGuard (LAN uses AudioRingBuffer)
    if (useWireGuard)
    {
        int fixed = fixedBufferMs.load();
        int bufferMs = fixed > 0 ? fixed : 300;
        int bufferSamples = (int)(48000.0 * bufferMs / 1000.0);
        jitterBuffer.configure(bufferSamples, 48000.0);
        setLatencySamples(bufferSamples);
    }

    bool connected = false;

    if (useWireGuard && useSessionApi)
    {
        // Try Session API (P2P direct connection)
        sessionClient.setApiUrl(sessionApiUrl);
        auto session = sessionClient.createSession(piId);
        if (session.valid)
        {
            currentSessionId = session.sessionId;
            auto privKey = sessionClient.getPrivateKey();
            auto endpoint = session.piLocalIp.isNotEmpty()
                ? session.piLocalIp + ":" + juce::String(session.piWgPort)
                : session.relayEndpoint;
            transport.connectWireGuard(endpoint, session.piPubkey,
                                       privKey, "10.0.0.10", "10.0.0.2");
            connected = true;
        }
    }

    if (!connected && useWireGuard)
    {
        // Fall back to static keys via relay
        auto ep = wgEndpoint + ":" + juce::String(wgPort);
        transport.connectWireGuard(ep, wgServerPubkey);
        connected = true;
    }

    if (!connected)
    {
        // LAN mode
        transport.connect(serverHost);
        connected = true;
    }

    // Wait for audio to actually flow (prebuffer fills), not just handshake
    // Timeout after 15 seconds if audio never starts
    for (int i = 0; i < 150 && transport.isConnected(); ++i)
    {
        juce::Thread::sleep(100);
        if (jitterBuffer.isConfigured() && !jitterBuffer.isPrebuffering())
        {
            // Audio is flowing
            connectionState.store((int)ConnState::connected);
            return;
        }
    }
    connectionState.store(transport.isConnected()
        ? (int)ConnState::connected
        : (int)ConnState::disconnected);

    // TODO: Auto buffer detection — needs thread-safe buffer resizing.
    // For now, start at 80ms which testing showed is stable on this connection.
    // User can adjust manually via the buffer dropdown.
}

void AnarackProcessor::disconnectAndCleanup()
{
    transport.disconnect();
    jitterBuffer.reset();       // stop processBlock from reading empty buffer
    resampler.reset();          // clear stale interpolator state
    asrcInitialised = false;
    prebuffering = true;        // reset AudioRingBuffer prebuffer
    updatePrebuffer();          // recalculate prebuffer target for current buffer size
    if (currentSessionId.isNotEmpty())
    {
        sessionClient.endSession(currentSessionId);
        currentSessionId = {};
    }
    connectionState.store((int)ConnState::disconnected);
    asrcDropCount.store(0, std::memory_order_relaxed);
    asrcDupCount.store(0, std::memory_order_relaxed);
}

void AnarackProcessor::reconnect()
{
    // Stop old connect thread FIRST to avoid race with reset
    if (connectThread)
    {
        connectThread->signalThreadShouldExit();
        connectThread->stopThread(3000);
        connectThread.reset();
    }
    disconnectAndCleanup();
    connectThread = std::make_unique<ConnectThread>(*this);
    connectThread->startThread();
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
    // Resampling ratio: how many input (48kHz) samples per output sample
    baseResampleRatio = SERVER_SAMPLE_RATE / sampleRate;
    resampleRatio = baseResampleRatio;

    // Only reset audio buffers if not currently streaming
    // (DAW may call prepareToPlay multiple times; resizing while streaming corrupts audio)
    if (!transport.isConnected())
    {
        int bufferSize = (int)(SERVER_SAMPLE_RATE * 0.5);
        audioRingBuffer.resize(bufferSize);
        resampler.reset();
        asrcInitialised = false;
        updatePrebuffer();
        prebuffering = true;
        setLatencySamples(prebufferSamples);
    }

    // Pre-allocate buffer for resampler input (worst case: full block at highest ratio)
    int maxInputSamples = (int)(samplesPerBlock * resampleRatio + 16);
    resampleInputBuf.resize((size_t)maxInputSamples, 0.0f);

    DBG("prepareToPlay: sampleRate=" + juce::String(sampleRate)
        + " samplesPerBlock=" + juce::String(samplesPerBlock)
        + " baseResampleRatio=" + juce::String(baseResampleRatio));

    // Auto-connect on first prepareToPlay (background thread)
    if (!transport.isConnected() && !connectThread)
    {
        connectThread = std::make_unique<ConnectThread>(*this);
        connectThread->startThread();
    }
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
    lastBlockSize.store(numOutputSamples, std::memory_order_relaxed);
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
            // ASRC: drift-accumulating crossfade correction
            // Instead of continuously resampling (which causes jitter on int16 audio),
            // we read samples directly and track buffer drift. When drift accumulates
            // to ±1 sample, we smoothly insert or remove a sample using a crossfade.
            int currentFill = jitterBuffer.getFillLevel();
            int targetFill = jitterBuffer.getFixedLatencySamples() / 2;

            if (!asrcInitialised)
            {
                smoothedFillError = 0.0;
                driftAccumulator = 0.0;
                asrcInitialised = true;
                asrcBlockCount = 0;
            }

            ++asrcBlockCount;

            // Low-pass filter the fill error — block-size independent
            // Target: ~5 second time constant regardless of block size
            // alpha = blockSize / (sampleRate * timeConstant) = N / (48000 * 5)
            double alpha = (double)numOutputSamples / (48000.0 * 5.0);
            double fillError = (double)(currentFill - targetFill) / (double)juce::jmax(1, targetFill);
            smoothedFillError = smoothedFillError * (1.0 - alpha) + fillError * alpha;

            // Don't accumulate drift during startup (let buffer stabilize first)
            // ~4 seconds regardless of block size
            int startupBlocks = (int)(4.0 * 48000.0 / juce::jmax(1, numOutputSamples));
            if (asrcBlockCount > startupBlocks)
            {
                // Accumulate drift — block-size independent
                // Max correction rate: ~7 corrections/sec regardless of block size
                double driftIncrement = smoothedFillError * 0.005 * numOutputSamples;
                double maxDrift = 7.0 * (double)numOutputSamples / 48000.0;
                driftIncrement = juce::jlimit(-maxDrift, maxDrift, driftIncrement);
                driftAccumulator += driftIncrement;
            }

            // Normal case: read exactly numOutputSamples
            int samplesToRead = numOutputSamples;
            bool doDropSample = false;
            bool doDupSample = false;

            // When drift exceeds ±1 sample, apply correction —
            // but only if the buffer actually needs it (prevents over-correction)
            if (driftAccumulator >= 1.0 && currentFill > targetFill)
            {
                doDropSample = true;
                driftAccumulator -= 1.0;
                samplesToRead = numOutputSamples + 1;
                asrcDropCount.fetch_add(1, std::memory_order_relaxed);
            }
            else if (driftAccumulator <= -1.0 && currentFill < targetFill)
            {
                doDupSample = true;
                driftAccumulator += 1.0;
                samplesToRead = numOutputSamples - 1;
                asrcDupCount.fetch_add(1, std::memory_order_relaxed);
            }
            // If accumulator wants to correct but buffer disagrees, just skip
            // this block — don't reset the accumulator, so drift tracking is preserved.

            // Clamp drift accumulator to prevent runaway
            driftAccumulator = juce::jlimit(-2.0, 2.0, driftAccumulator);

            // Ensure we have a temp buffer large enough
            if (samplesToRead + CROSSFADE_LEN > (int)resampleInputBuf.size())
                resampleInputBuf.resize((size_t)(samplesToRead + CROSSFADE_LEN), 0.0f);

            // Read from jitter buffer
            jitterBuffer.read(resampleInputBuf.data(), samplesToRead);

            if (doDropSample)
            {
                // We read numOutputSamples+1 samples, need to output numOutputSamples.
                // Linear interpolation: map N output samples across N+1 input samples.
                // Each output sample is gently stretched — no splice point, no click.
                double step = (double)(samplesToRead - 1) / (double)(numOutputSamples - 1);
                for (int i = 0; i < numOutputSamples; ++i)
                {
                    double srcPos = i * step;
                    int idx = (int)srcPos;
                    float frac = (float)(srcPos - idx);
                    int idx1 = juce::jmin(idx + 1, samplesToRead - 1);
                    outL[i] = resampleInputBuf[idx] * (1.0f - frac)
                            + resampleInputBuf[idx1] * frac;
                }
            }
            else if (doDupSample)
            {
                // We read numOutputSamples-1 samples, need to output numOutputSamples.
                // Linear interpolation: stretch N-1 input samples across N output samples.
                int readSamples = numOutputSamples - 1;
                double step = (double)(readSamples - 1) / (double)(numOutputSamples - 1);
                for (int i = 0; i < numOutputSamples; ++i)
                {
                    double srcPos = i * step;
                    int idx = (int)srcPos;
                    float frac = (float)(srcPos - idx);
                    int idx1 = juce::jmin(idx + 1, readSamples - 1);
                    outL[i] = resampleInputBuf[idx] * (1.0f - frac)
                            + resampleInputBuf[idx1] * frac;
                }
            }
            else
            {
                // No correction needed — direct copy
                std::memcpy(outL, resampleInputBuf.data(), (size_t)numOutputSamples * sizeof(float));
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
    state.setProperty("fixedBufferMs", fixedBufferMs.load(), nullptr);
    state.setProperty("useWireGuard", useWireGuard, nullptr);
    juce::MemoryOutputStream stream(destData, false);
    state.writeToStream(stream);
}

void AnarackProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto state = juce::ValueTree::readFromData(data, (size_t)sizeInBytes);
    if (state.hasType("AnarackState"))
    {
        serverHost = state.getProperty("serverHost", "anarack.local").toString();
        fixedBufferMs.store((int)state.getProperty("fixedBufferMs", 300));
        useWireGuard = (bool)state.getProperty("useWireGuard", true);
    }
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
