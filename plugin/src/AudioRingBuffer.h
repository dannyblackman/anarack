#pragma once
#include <juce_core/juce_core.h>

// Lock-free single-producer single-consumer ring buffer for audio samples.
// Producer: network receive thread (pushes float samples converted from int16).
// Consumer: audio thread processBlock (reads float samples into output buffer).
class AudioRingBuffer
{
public:
    AudioRingBuffer(int capacity = 4800) // ~100ms at 48kHz
        : fifo(capacity), buffer(capacity)
    {
    }

    void resize(int newCapacity)
    {
        fifo.setTotalSize(newCapacity);
        buffer.resize(newCapacity);
        fifo.reset();
    }

    // Called from network thread. Writes float samples into the ring buffer.
    int write(const float* data, int numSamples)
    {
        int start1, size1, start2, size2;
        fifo.prepareToWrite(numSamples, start1, size1, start2, size2);

        if (size1 > 0)
            std::memcpy(buffer.data() + start1, data, (size_t)size1 * sizeof(float));
        if (size2 > 0)
            std::memcpy(buffer.data() + start2, data + size1, (size_t)size2 * sizeof(float));

        fifo.finishedWrite(size1 + size2);
        return size1 + size2;
    }

    // Called from audio thread. Reads float samples from the ring buffer.
    // Returns number of samples actually read. Caller should zero-fill remainder.
    int read(float* dest, int numSamples)
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead(numSamples, start1, size1, start2, size2);

        if (size1 > 0)
            std::memcpy(dest, buffer.data() + start1, (size_t)size1 * sizeof(float));
        if (size2 > 0)
            std::memcpy(dest + size1, buffer.data() + start2, (size_t)size2 * sizeof(float));

        fifo.finishedRead(size1 + size2);
        return size1 + size2;
    }

    int getNumReady() const { return fifo.getNumReady(); }
    int getFreeSpace() const { return fifo.getFreeSpace(); }
    void reset() { fifo.reset(); }

private:
    juce::AbstractFifo fifo;
    std::vector<float> buffer;
};
