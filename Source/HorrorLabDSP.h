#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "DSP/PitchShifter.h"

class GhostVoice
{
public:
    void prepare(double sampleRate)
    {
        shifter.prepare(sampleRate);
    }

    void reset()
    {
        shifter.reset();
    }

    void setPitch(float semitones)
    {
        shifter.setPitchSemitones(std::clamp(semitones, -24.0f, 12.0f));
    }

    void setMix(float value)
    {
        mix = std::clamp(value, 0.0f, 1.0f);
    }

    float processSample(float input)
    {
        const float ghost = shifter.processSample(input);
        return input * (1.0f - mix) + ghost * mix;
    }

private:
    PitchShifter shifter;
    float mix = 0.25f;
};

class RandomDrift
{
public:
    void prepare(double sampleRate)
    {
        sr = std::max(8000.0, sampleRate);
        buffer.assign((size_t)std::ceil(sr * 0.04), 0.0f);
        reset();
    }

    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        phase = 0.0f;

        // Start every reset from a known, non-zero modulation position. This
        // makes drift audible on the first processed sample instead of waiting
        // for the first rate-driven target update.
        state = initialSeed;
        const float initialOffset = nextNonZeroRandom() * amount;
        target = initialOffset;
        current = initialOffset;
    }

    void setAmount(float value)
    {
        amount = std::clamp(value, 0.0f, 1.0f);
    }

    void setRate(float value)
    {
        rate = std::clamp(value, 0.03f, 2.0f);
    }

    float processSample(float input)
    {
        if (buffer.empty())
            return input;

        phase += rate / (float)sr;

        if (phase >= 1.0f)
        {
            phase -= std::floor(phase);
            target = nextNonZeroRandom() * amount;
        }

        // Smooth each new target. The smoothing follows modulation rate: slow
        // drift stays gradual, while faster drift can still reach its targets.
        const float smoothingHz = std::clamp(rate * 4.0f, 0.12f, 8.0f);
        const float smoothing = 1.0f
            - std::exp(-2.0f * 3.14159265358979323846f * smoothingHz / (float)sr);
        current += (target - current) * smoothing;

        buffer[(size_t)writePos] = input;

        const float maxDelay = (float)buffer.size() - 2.0f;
        const float delay = 2.0f
            + (std::clamp(current, -1.0f, 1.0f) + 1.0f) * 0.5f * maxDelay * 0.65f;
        float read = (float)writePos - delay;

        while (read < 0.0f)
            read += (float)buffer.size();

        const int i0 = (int)read;
        const int i1 = (i0 + 1) % (int)buffer.size();
        const float out = buffer[(size_t)i0]
            + (buffer[(size_t)i1] - buffer[(size_t)i0]) * (read - (float)i0);

        writePos = (writePos + 1) % (int)buffer.size();
        return out;
    }

private:
    float nextNonZeroRandom()
    {
        state = state * 1664525u + 1013904223u;
        const float unit = (float)((state >> 8) & 0x00ffffff) / 16777215.0f;
        const float sign = (state & 1u) == 0u ? -1.0f : 1.0f;

        // Keep every target away from zero so non-zero amount always produces
        // an actual delay offset, including the deterministic initial target.
        return sign * (0.25f + unit * 0.75f);
    }

    static constexpr uint32_t initialSeed = 0x9e3779b9u;

    double sr = 44100.0;
    std::vector<float> buffer;
    int writePos = 0;
    float amount = 0.25f;
    float rate = 0.25f;
    float phase = 0.0f;
    float target = 0.0f;
    float current = 0.0f;
    uint32_t state = initialSeed;
};

class Stutter
{
public:
    void prepare(double sampleRate)
    {
        sr = std::max(8000.0, sampleRate);
        buffer.assign((size_t)std::ceil(sr * 1.0), 0.0f);
        reset();
    }

    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        samplesUntilTrigger = 0;
        loopRemaining = 0;
        loopRead = 0;
    }

    void setRate(float value)
    {
        rate = std::clamp(value, 0.25f, 16.0f);
    }

    void setLength(float value)
    {
        length = std::clamp(value, 0.02f, 0.5f);
    }

    void setMix(float value)
    {
        mix = std::clamp(value, 0.0f, 1.0f);
    }

    float processSample(float input)
    {
        if (buffer.empty())
            return input;

        buffer[(size_t)writePos] = input;

        if (loopRemaining <= 0)
        {
            if (--samplesUntilTrigger <= 0)
            {
                const int loopSamples = std::min(
                    (int)buffer.size() - 2,
                    std::max(16, (int)(length * sr)));

                loopRemaining = loopSamples;
                loopRead = writePos - loopSamples;

                if (loopRead < 0)
                    loopRead += (int)buffer.size();

                samplesUntilTrigger = std::max(loopSamples, (int)(sr / rate));
            }

            writePos = (writePos + 1) % (int)buffer.size();
            return input;
        }

        const float repeated = buffer[(size_t)loopRead];
        loopRead = (loopRead + 1) % (int)buffer.size();
        --loopRemaining;
        writePos = (writePos + 1) % (int)buffer.size();

        return input * (1.0f - mix) + repeated * mix;
    }

private:
    double sr = 44100.0;
    std::vector<float> buffer;
    int writePos = 0;
    int samplesUntilTrigger = 0;
    int loopRemaining = 0;
    int loopRead = 0;
    float rate = 2.0f;
    float length = 0.12f;
    float mix = 0.5f;
};
