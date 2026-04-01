#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <cmath>

/**
 * JitterEstimator — histogram-based adaptive jitter buffer sizing.
 *
 * Tracks inter-arrival delay of packets and computes the 95th percentile
 * of the jitter distribution. The target buffer depth is P95 + safety margin.
 *
 * Thread model:
 *   - recordArrival() called from network receive thread
 *   - getTargetBufferSamples() called from audio thread
 */
class JitterEstimator
{
public:
    static constexpr int NUM_BINS = 64;          // 64 bins, 1ms each (0-64ms)
    static constexpr int SAFETY_PACKETS = 2;     // extra margin in packets
    static constexpr int MIN_TARGET_PACKETS = 4; // minimum buffer depth
    static constexpr float FORGET_FACTOR = 0.999f; // per-packet decay (~2.7s half-life at 375 pkt/s)
    static constexpr double EXPECTED_SPACING_MS = 128.0 / 48000.0 * 1000.0; // ~2.667ms

    void recordArrival(uint32_t seq, double arrivalTimeMs)
    {
        if (lastSeq > 0 && seq == lastSeq + 1)
        {
            // Compute inter-arrival jitter
            double spacing = arrivalTimeMs - lastArrivalMs;
            double jitterMs = spacing - EXPECTED_SPACING_MS;
            if (jitterMs < 0) jitterMs = 0;

            // Add to histogram
            int bin = (int)jitterMs;
            if (bin >= NUM_BINS) bin = NUM_BINS - 1;

            // Decay all bins (forgetting factor)
            for (int i = 0; i < NUM_BINS; i++)
                histogram[i] *= FORGET_FACTOR;

            // Increment the bin
            histogram[bin] += 1.0f;
            totalWeight = totalWeight * FORGET_FACTOR + 1.0f;

            // Compute P95
            float threshold = totalWeight * 0.95f;
            float cumulative = 0.0f;
            int p95Bin = 0;
            for (int i = 0; i < NUM_BINS; i++)
            {
                cumulative += histogram[i];
                if (cumulative >= threshold)
                {
                    p95Bin = i;
                    break;
                }
            }

            // Target = P95 jitter + safety margin, clamped to minimum
            int p95Samples = (int)(p95Bin * 48.0); // ms to samples at 48kHz
            int safetySamples = SAFETY_PACKETS * 128;
            int minSamples = MIN_TARGET_PACKETS * 128;
            int target = juce::jmax(minSamples, p95Samples + safetySamples);

            targetBufferSamples.store(target, std::memory_order_relaxed);
            p95JitterMs.store((float)p95Bin, std::memory_order_relaxed);
        }

        lastArrivalMs = arrivalTimeMs;
        lastSeq = seq;
    }

    int getTargetBufferSamples() const
    {
        return targetBufferSamples.load(std::memory_order_relaxed);
    }

    float getP95JitterMs() const
    {
        return p95JitterMs.load(std::memory_order_relaxed);
    }

private:
    double lastArrivalMs = 0;
    uint32_t lastSeq = 0;
    float histogram[NUM_BINS] {};
    float totalWeight = 0.0f;
    std::atomic<int> targetBufferSamples { MIN_TARGET_PACKETS * 128 };
    std::atomic<float> p95JitterMs { 0.0f };
};
