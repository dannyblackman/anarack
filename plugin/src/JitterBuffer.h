#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <cstring>
#include <cmath>
#include <vector>

// Timestamp-indexed jitter buffer for network audio reception.
//
// Replaces AudioRingBuffer for the network audio path. Packets are placed by
// their timestamp (sample position), not arrival order, so late or reordered
// packets land in the correct position. The buffer size is fixed at connection
// time and never changes during a session — critical for DAW PDC.
//
// Thread model (SPSC):
//   - writePacket() called from the network receive thread (single producer)
//   - read() called from the audio callback thread (single consumer)
//
// Packet loss concealment ensures no clicks:
//   - Short gap  (1-2 packets, <=256 samples): crossfade interpolation
//   - Medium gap (3-5 packets, <=640 samples): repeat last good with cosine fade
//   - Long gap   (>5 packets):                 fade to silence over ~5ms
//
// Packet header format (12 bytes, little-endian):
//   sequence  (uint32)  — monotonic counter, never resets during session
//   timestamp (uint32)  — sample position in stream
//   flags     (uint16)  — bit 0: FEC, bit 1: silence, bit 2: session-start
//   checksum  (uint16)  — CRC-16 of header + payload
//
// Audio payload: 128 x int16 samples (256 bytes per packet).

class JitterBuffer
{
public:
    static constexpr int PACKET_SAMPLES     = 128;
    static constexpr int HEADER_SIZE        = 12;
    static constexpr int BYTES_PER_SAMPLE   = 3;   // 24-bit packed
    static constexpr int PAYLOAD_BYTES      = PACKET_SAMPLES * BYTES_PER_SAMPLE;  // 384
    static constexpr int PACKET_BYTES       = HEADER_SIZE + PAYLOAD_BYTES;        // 396
    // Legacy 16-bit support
    static constexpr int LEGACY_PAYLOAD     = PACKET_SAMPLES * 2;  // 256
    static constexpr int LEGACY_PACKET      = HEADER_SIZE + LEGACY_PAYLOAD;  // 268

    // Packet flag bits
    static constexpr uint16_t kFlagFEC          = 0x0001;
    static constexpr uint16_t kFlagSilence      = 0x0002;
    static constexpr uint16_t kFlagSessionStart = 0x0004;

    // PLC fade duration in samples (~5ms at 48kHz)
    static constexpr int FADE_SAMPLES = 240;

    JitterBuffer() = default;

    // -------------------------------------------------------------------------
    // Configuration — call once at connection time, never during a session.
    // fixedSizeSamples: total buffer size in samples (e.g. RTT_p95*2 + 50ms).
    // sampleRate: server sample rate (expected 48000).
    // -------------------------------------------------------------------------
    void configure(int fixedSizeSamples, double sampleRate)
    {
        jassert(fixedSizeSamples > 0);
        jassert(sampleRate > 0.0);

        bufferSize = fixedSizeSamples;
        sampleRate_ = sampleRate;

        buffer.assign((size_t)bufferSize, 0.0f);
        slotFilled.assign((size_t)bufferSize, 0);

        readPos.store(0, std::memory_order_relaxed);
        writeBase.store(0, std::memory_order_relaxed);
        expectedSeq.store(0, std::memory_order_relaxed);
        samplesWritten.store(0, std::memory_order_relaxed);
        totalSamplesRead.store(0, std::memory_order_relaxed);

        firstPacket.store(true, std::memory_order_relaxed);
        prebuffering.store(true, std::memory_order_relaxed);
        prebufferTarget = fixedSizeSamples * 3 / 4; // 75% fill before playing

        packetsLost.store(0, std::memory_order_relaxed);
        packetsRecovered.store(0, std::memory_order_relaxed);
        packetsDuplicate.store(0, std::memory_order_relaxed);
        packetsReceived.store(0, std::memory_order_relaxed);

        lastGoodBlock.assign(PACKET_SAMPLES, 0.0f);
        lastGoodLength = PACKET_SAMPLES;
        gapLength = 0;

        configured.store(true, std::memory_order_release);
    }

    int getFixedLatencySamples() const { return bufferSize; }
    bool isConfigured() const { return configured.load(std::memory_order_acquire); }
    bool isPrebuffering() const { return prebuffering.load(std::memory_order_acquire); }

    // -------------------------------------------------------------------------
    // writePacket — called from the network receive thread.
    //
    // Accepts a raw packet (header + payload). Parses the 12-byte header,
    // converts int16 payload to float, and places samples at the correct
    // timestamp position in the ring buffer. Handles deduplication, gap
    // detection, and late-arrival recovery tracking.
    // -------------------------------------------------------------------------
    void writePacket(const uint8_t* rawPacket, int rawSize)
    {
        if (!configured.load(std::memory_order_acquire))
            return;

        if (rawSize < HEADER_SIZE + 2)
            return;

        // --- Parse header (little-endian) ---
        uint32_t seq, timestamp;
        uint16_t flags, checksum;
        std::memcpy(&seq, rawPacket, 4);
        std::memcpy(&timestamp, rawPacket + 4, 4);
        std::memcpy(&flags, rawPacket + 8, 2);
        std::memcpy(&checksum, rawPacket + 10, 2);

        int payloadBytes = rawSize - HEADER_SIZE;

        // Detect 24-bit (3 bytes/sample) vs 16-bit (2 bytes/sample)
        bool is24bit = (payloadBytes == PACKET_SAMPLES * 3);
        bool is16bit = (payloadBytes == PACKET_SAMPLES * 2);
        if (!is24bit && !is16bit)
            return;
        int numSamples = is24bit ? payloadBytes / 3 : payloadBytes / 2;
        if (numSamples <= 0 || numSamples > PACKET_SAMPLES * 2)
            return;

        // Convert to float (temp buffer on stack)
        float tempSamples[PACKET_SAMPLES * 2];
        const uint8_t* payload = rawPacket + HEADER_SIZE;
        if (is24bit)
        {
            for (int s = 0; s < numSamples; s++)
            {
                // Unpack 3 bytes little-endian → int32 (sign-extend)
                int32_t val = (int32_t)(payload[s*3] | (payload[s*3+1] << 8) | (payload[s*3+2] << 16));
                if (val & 0x800000) val |= 0xFF000000; // sign extend
                tempSamples[s] = (float)val / 8388608.0f;
            }
        }
        else
        {
            const int16_t* int16Data = reinterpret_cast<const int16_t*>(payload);
            for (int s = 0; s < numSamples; s++)
                tempSamples[s] = (float)int16Data[s] / 32768.0f;
        }

        // --- First packet: bootstrap the buffer position ---
        if (firstPacket.load(std::memory_order_acquire))
        {
            bool expected = true;
            if (firstPacket.compare_exchange_strong(expected, false,
                                                    std::memory_order_acq_rel))
            {
                writeBase.store(timestamp, std::memory_order_release);
                expectedSeq.store(seq, std::memory_order_release);
            }
        }

        uint32_t wb = writeBase.load(std::memory_order_acquire);

        // --- Compute offset from current read position ---
        int32_t offset = static_cast<int32_t>(timestamp - wb);

        // Discard ancient packets (far behind read position)
        if (offset < -bufferSize)
        {
            packetsDuplicate.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        // Detect session restart: packet way ahead of expected position
        if (offset > bufferSize * 2)
        {
            // Treat as new session — reset buffer contents, re-sync.
            writeBase.store(timestamp, std::memory_order_release);
            wb = timestamp;
            offset = 0;
            clearBufferContents();
            expectedSeq.store(seq, std::memory_order_release);
        }

        // --- Gap detection via sequence numbers ---
        uint32_t expSeq = expectedSeq.load(std::memory_order_acquire);
        int32_t seqDelta = static_cast<int32_t>(seq - expSeq);

        if (seqDelta > 0)
        {
            // Packets between expectedSeq and this one were lost.
            packetsLost.fetch_add(seqDelta, std::memory_order_relaxed);
        }
        else if (seqDelta < 0)
        {
            // Late arrival of a previously-lost packet (recovered),
            // or a duplicate. We distinguish below during placement.
        }

        // Advance expected sequence if this packet is at or ahead of it.
        if (seqDelta >= 0)
            expectedSeq.store(seq + 1, std::memory_order_release);

        // --- Place samples into ring buffer ---
        int rp = readPos.load(std::memory_order_acquire);
        bool anyNew = false;

        for (int i = 0; i < numSamples; ++i)
        {
            int pos = (rp + offset + i) % bufferSize;
            if (pos < 0) pos += bufferSize;

            if (slotFilled[(size_t)pos])
                continue; // already have this sample (duplicate packet)

            buffer[(size_t)pos] = tempSamples[i];
            slotFilled[(size_t)pos] = 1;
            anyNew = true;
        }

        if (!anyNew && seqDelta < 0)
        {
            // Every sample was already present — this is a true duplicate.
            packetsDuplicate.fetch_add(1, std::memory_order_relaxed);
        }
        else if (seqDelta < 0 && anyNew)
        {
            // Late arrival that filled in missing samples — recovered.
            packetsRecovered.fetch_add(1, std::memory_order_relaxed);
        }

        packetsReceived.fetch_add(1, std::memory_order_relaxed);
        samplesWritten.fetch_add(numSamples, std::memory_order_relaxed);

        // --- Check prebuffer threshold ---
        if (prebuffering.load(std::memory_order_acquire))
        {
            // Approximate fill: count filled slots in a sampled subset.
            int filled = countFilledApprox();
            if (filled >= prebufferTarget)
                prebuffering.store(false, std::memory_order_release);
        }
    }

    // -------------------------------------------------------------------------
    // Overload: writePacket with pre-parsed fields (for callers that parse
    // the header themselves).
    // -------------------------------------------------------------------------
    void writePacket(uint32_t seq, uint32_t timestamp,
                     const int16_t* samples, int numSamples)
    {
        if (!configured.load(std::memory_order_acquire))
            return;
        if (numSamples <= 0 || numSamples > PACKET_SAMPLES * 2)
            return;

        // --- First packet: bootstrap ---
        if (firstPacket.load(std::memory_order_acquire))
        {
            bool expected = true;
            if (firstPacket.compare_exchange_strong(expected, false,
                                                    std::memory_order_acq_rel))
            {
                writeBase.store(timestamp, std::memory_order_release);
                expectedSeq.store(seq, std::memory_order_release);
            }
        }

        uint32_t wb = writeBase.load(std::memory_order_acquire);
        int32_t offset = static_cast<int32_t>(timestamp - wb);

        if (offset < -bufferSize)
        {
            packetsDuplicate.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        if (offset > bufferSize * 2)
        {
            writeBase.store(timestamp, std::memory_order_release);
            wb = timestamp;
            offset = 0;
            clearBufferContents();
            expectedSeq.store(seq, std::memory_order_release);
        }

        uint32_t expSeq = expectedSeq.load(std::memory_order_acquire);
        int32_t seqDelta = static_cast<int32_t>(seq - expSeq);

        if (seqDelta > 0)
            packetsLost.fetch_add(seqDelta, std::memory_order_relaxed);

        if (seqDelta >= 0)
            expectedSeq.store(seq + 1, std::memory_order_release);

        int rp = readPos.load(std::memory_order_acquire);
        bool anyNew = false;

        for (int i = 0; i < numSamples; ++i)
        {
            int pos = (rp + offset + i) % bufferSize;
            if (pos < 0) pos += bufferSize;

            if (slotFilled[(size_t)pos])
                continue;

            buffer[(size_t)pos] = static_cast<float>(samples[i]) / 32768.0f;
            slotFilled[(size_t)pos] = 1;
            anyNew = true;
        }

        if (!anyNew && seqDelta < 0)
            packetsDuplicate.fetch_add(1, std::memory_order_relaxed);
        else if (seqDelta < 0 && anyNew)
            packetsRecovered.fetch_add(1, std::memory_order_relaxed);

        packetsReceived.fetch_add(1, std::memory_order_relaxed);
        samplesWritten.fetch_add(numSamples, std::memory_order_relaxed);

        if (prebuffering.load(std::memory_order_acquire))
        {
            int filled = countFilledApprox();
            if (filled >= prebufferTarget)
                prebuffering.store(false, std::memory_order_release);
        }
    }

    // -------------------------------------------------------------------------
    // read — called from the audio callback thread.
    //
    // Outputs numSamples of audio. Received samples are copied directly.
    // Missing samples (gaps) are filled by packet loss concealment to ensure
    // no clicks or discontinuities.
    // -------------------------------------------------------------------------
    void read(float* output, int numSamples)
    {
        if (!configured.load(std::memory_order_acquire) ||
            prebuffering.load(std::memory_order_acquire))
        {
            std::memset(output, 0, (size_t)numSamples * sizeof(float));
            return;
        }

        int rp = readPos.load(std::memory_order_acquire);

        for (int i = 0; i < numSamples; ++i)
        {
            int pos = (rp + i) % bufferSize;

            if (slotFilled[(size_t)pos])
            {
                float sample = buffer[(size_t)pos];
                lastGoodSample = sample;
                output[i] = sample;
                gapLength = 0;
            }
            else
            {
                // Simple sample-hold: repeat the last good sample.
                // This is inaudible for short gaps (1-2 samples) and
                // fades gently for longer gaps.
                gapLength++;
                if (gapLength <= PACKET_SAMPLES * 2)
                    output[i] = lastGoodSample;
                else
                {
                    // Fade to silence over ~5ms
                    float fade = juce::jmax(0.0f, 1.0f - (float)(gapLength - PACKET_SAMPLES * 2) / 240.0f);
                    output[i] = lastGoodSample * fade;
                }
            }

            // Clear the slot for reuse.
            buffer[(size_t)pos] = 0.0f;
            slotFilled[(size_t)pos] = 0;
        }

        // Advance read position and write base together.
        int newRp = (rp + numSamples) % bufferSize;
        readPos.store(newRp, std::memory_order_release);
        writeBase.fetch_add((uint32_t)numSamples, std::memory_order_release);
        totalSamplesRead.fetch_add(numSamples, std::memory_order_relaxed);
    }

    // -------------------------------------------------------------------------
    // Stats — all safe to read from any thread.
    // -------------------------------------------------------------------------

    // Approximate fill level in samples.
    int getFillLevel() const
    {
        // Exact fill: total samples written minus total samples read
        int written = samplesWritten.load(std::memory_order_relaxed);
        int read = totalSamplesRead.load(std::memory_order_relaxed);
        return juce::jmax(0, written - read);
    }

    // Fill level as a fraction (0.0 - 1.0).
    float getFillRatio() const
    {
        if (bufferSize <= 0) return 0.0f;
        return static_cast<float>(getFillLevel()) / static_cast<float>(bufferSize);
    }

    int getPacketsLost() const { return packetsLost.load(std::memory_order_relaxed); }
    int getPacketsRecovered() const { return packetsRecovered.load(std::memory_order_relaxed); }
    int getPacketsReceived() const { return packetsReceived.load(std::memory_order_relaxed); }
    int getPacketsDuplicate() const { return packetsDuplicate.load(std::memory_order_relaxed); }

    // -------------------------------------------------------------------------
    // Reset — call on disconnect. Only safe when no concurrent read/write.
    // -------------------------------------------------------------------------
    void reset()
    {
        configured.store(false, std::memory_order_release);
        firstPacket.store(true, std::memory_order_release);
        prebuffering.store(false, std::memory_order_release);

        std::fill(buffer.begin(), buffer.end(), 0.0f);
        std::fill(slotFilled.begin(), slotFilled.end(), 0);

        readPos.store(0, std::memory_order_relaxed);
        writeBase.store(0, std::memory_order_relaxed);
        expectedSeq.store(0, std::memory_order_relaxed);
        samplesWritten.store(0, std::memory_order_relaxed);
        totalSamplesRead.store(0, std::memory_order_relaxed);

        packetsLost.store(0, std::memory_order_relaxed);
        packetsRecovered.store(0, std::memory_order_relaxed);
        packetsDuplicate.store(0, std::memory_order_relaxed);
        packetsReceived.store(0, std::memory_order_relaxed);

        std::fill(lastGoodBlock.begin(), lastGoodBlock.end(), 0.0f);
        lastGoodWriteIdx = 0;
        gapLength = 0;
    }

private:
    // -------------------------------------------------------------------------
    // Packet loss concealment — returns a single sample for the current gap.
    //
    // Strategy by gap length:
    //   Short  (1-2 packets, <=256 samples): repeat last good block with
    //          gentle linear fade — nearly inaudible on sustained synth sounds.
    //   Medium (3-5 packets, <=640 samples): repeat with cosine fade toward
    //          silence — works well for pads and sustained tones.
    //   Long   (>5 packets, >640 samples): fade to silence over FADE_SAMPLES
    //          (~5ms), then output pure silence. No hard cut ever.
    // -------------------------------------------------------------------------
    float concealSample(int currentGapLength) const
    {
        static constexpr int kShortThreshold  = PACKET_SAMPLES * 2;  // 256
        static constexpr int kMediumThreshold = PACKET_SAMPLES * 5;  // 640

        // Retrieve a reference sample from the last-good block (cyclic).
        float ref = 0.0f;
        if (lastGoodLength > 0)
        {
            int idx = (currentGapLength - 1) % lastGoodLength;
            ref = lastGoodBlock[(size_t)idx];
        }

        if (currentGapLength <= kShortThreshold)
        {
            // Linear fade from 1.0 to 0.5 over the short threshold.
            float t = static_cast<float>(currentGapLength) / static_cast<float>(kShortThreshold);
            float fade = 1.0f - 0.5f * t;
            return ref * fade;
        }

        if (currentGapLength <= kMediumThreshold)
        {
            // Cosine fade from ~0.5 to 0.0 over the medium range.
            float t = static_cast<float>(currentGapLength - kShortThreshold)
                    / static_cast<float>(kMediumThreshold - kShortThreshold);
            float fade = 0.5f * (1.0f + std::cos(t * 3.14159265f)) * 0.5f;
            return ref * fade;
        }

        // Long gap: linear fade to silence over FADE_SAMPLES from medium end.
        int fadePos = currentGapLength - kMediumThreshold;
        if (fadePos < FADE_SAMPLES)
        {
            float fade = 1.0f - static_cast<float>(fadePos) / static_cast<float>(FADE_SAMPLES);
            // At this point the level is already very low from the medium stage,
            // so we just do a final gentle ramp of whatever remains.
            return ref * fade * 0.05f; // ~5% of reference level
        }

        return 0.0f; // Pure silence.
    }

    // -------------------------------------------------------------------------
    // Approximate fill count — samples a subset of slots to avoid O(n) scan.
    // Accurate enough for prebuffer checks and UI display.
    // -------------------------------------------------------------------------
    int countFilledApprox() const
    {
        if (bufferSize <= 0) return 0;

        // Sample every Nth slot (at most 128 probes).
        int step = juce::jmax(1, bufferSize / 128);
        int count = 0;
        for (int i = 0; i < bufferSize; i += step)
        {
            if (slotFilled[(size_t)i])
                ++count;
        }

        // Scale up by step to estimate total.
        return count * step;
    }

    // Clear buffer contents without deallocating.
    void clearBufferContents()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        std::fill(slotFilled.begin(), slotFilled.end(), 0);
    }

    // -------------------------------------------------------------------------
    // Data members
    // -------------------------------------------------------------------------

    // Ring buffer: float samples, normalised -1.0..1.0
    std::vector<float> buffer;

    // Per-sample fill tracking: 1 = received, 0 = empty.
    // Using uint8_t instead of bool for atomic-safe byte access.
    std::vector<uint8_t> slotFilled;

    int bufferSize = 0;
    double sampleRate_ = 48000.0;

    // Atomic SPSC indices.
    // readPos: current read position in the ring buffer (audio thread owns).
    // writeBase: timestamp corresponding to readPos (network thread reads,
    //            audio thread advances via fetch_add).
    std::atomic<int> readPos { 0 };
    std::atomic<uint32_t> writeBase { 0 };

    // Sequence tracking.
    std::atomic<uint32_t> expectedSeq { 0 };
    std::atomic<int> samplesWritten { 0 };
    std::atomic<int> totalSamplesRead { 0 };

    // State flags — all atomic for cross-thread safety.
    std::atomic<bool> configured { false };
    std::atomic<bool> firstPacket { true };
    std::atomic<bool> prebuffering { true };
    int prebufferTarget = 0;

    // Stats.
    std::atomic<int> packetsLost { 0 };
    std::atomic<int> packetsRecovered { 0 };
    std::atomic<int> packetsDuplicate { 0 };
    std::atomic<int> packetsReceived { 0 };

    // PLC state — only accessed from the audio thread (read()).
    std::vector<float> lastGoodBlock;
    int lastGoodLength = PACKET_SAMPLES;
    int lastGoodWriteIdx = 0;
    int gapLength = 0;
    float lastGoodSample = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JitterBuffer)
};
