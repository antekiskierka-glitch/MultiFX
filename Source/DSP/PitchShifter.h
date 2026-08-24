#pragma once
#include <vector>
#include <cmath>

// Simple time-domain granular pitch shifter.
// Two overlapping read heads, half a grain apart, triangular-windowed
// crossfade between them. This is a classic, well-known DSP technique
// (not tied to any commercial product) — good enough for a musical
// effect, but it will produce some warble/artifacts at extreme ratios
// or on very transient material. A phase-vocoder (FFT-based) approach
// would sound cleaner; see the "what to extend" notes.
class PitchShifter
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        bufferSize = (int) (sr * 0.5); // 500ms circular buffer
        buffer.assign((size_t) bufferSize, 0.0f);
        writePos = 0;
        setGrainSizeMs(80.0f);
        readPos = 0.0f;
    }

    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        readPos = 0.0f;
    }

    void setGrainSizeMs(float ms)
    {
        grainSize = std::max(64, (int) (sr * (ms / 1000.0f)));
    }

    // semitones: -24 .. +24 typical musical range
    void setPitchSemitones(float semitones)
    {
        ratio = std::pow(2.0f, semitones / 12.0f);
    }

    float processSample(float input)
    {
        buffer[(size_t) writePos] = input;

        float rp1 = readPos;
        float rp2 = readPos + (float) grainSize * 0.5f;
        if (rp2 >= (float) bufferSize)
            rp2 -= (float) bufferSize;

        float s1 = readInterp(rp1);
        float s2 = readInterp(rp2);

        float pos1 = std::fmod(rp1, (float) grainSize) / (float) grainSize;
        float w1 = 1.0f - std::abs (2.0f * pos1 - 1.0f); // triangle window
        float pos2 = std::fmod(rp2, (float) grainSize) / (float) grainSize;
        float w2 = 1.0f - std::abs (2.0f * pos2 - 1.0f);

        float wsum = w1 + w2 + 1.0e-6f;
        float out = (s1 * w1 + s2 * w2) / wsum;

        writePos = (writePos + 1) % bufferSize;
        readPos += ratio;
        if (readPos >= (float) bufferSize) readPos -= (float) bufferSize;
        if (readPos < 0.0f) readPos += (float) bufferSize;

        return out;
    }

private:
    float readInterp(float pos) const
    {
        int i0 = (int) pos;
        int i1 = (i0 + 1) % bufferSize;
        float frac = pos - (float) i0;
        return buffer[(size_t) i0] * (1.0f - frac) + buffer[(size_t) i1] * frac;
    }

    std::vector<float> buffer;
    int bufferSize = 0, writePos = 0, grainSize = 0;
    float readPos = 0.0f;
    float ratio = 1.0f;
    double sr = 44100.0;
};
