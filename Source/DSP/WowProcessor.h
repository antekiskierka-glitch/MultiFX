#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>
#include <vector>

// Tape-style wow made with a short, smoothly modulated delay line. Changing
// delay time changes playback rate, giving genuine pitch wobble rather than
// modulating a frequency-shifter oscillator.
class WowProcessor
{
public:
    void prepare(double newSampleRate)
    {
        sampleRate = std::max(1.0, newSampleRate);
        const auto maximumDelaySamples = (int)std::ceil(sampleRate * maximumDelaySeconds) + 4;
        delayLine.assign((size_t)maximumDelaySamples, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill(delayLine.begin(), delayLine.end(), 0.0f);
        writePosition = 0;
        phase = 0.0;
    }

    void setAmount(float value) { amount = std::clamp(value, 0.0f, 1.0f); }

    // Tempo-synchronised divisions can exceed the former 8 Hz UI limit at
    // fast host tempos, so keep the DSP limit above the supported musical range.
    void setSpeed(float value) { speedHz = std::clamp(value, 0.01f, 20.0f); }

    float processSample(float input)
    {
        if (delayLine.empty())
            return input;

        const float modulationSamples = amount * maximumDepthSeconds * (float)sampleRate
            * (float)std::sin(phase);
        const float delaySamples = std::max(1.0f, centreDelaySeconds * (float)sampleRate
            + modulationSamples);

        delayLine[(size_t)writePosition] = input;

        float readPosition = (float)writePosition - delaySamples;
        const float lineLength = (float)delayLine.size();
        while (readPosition < 0.0f)
            readPosition += lineLength;

        const int index0 = (int)readPosition;
        const int index1 = (index0 + 1) % (int)delayLine.size();
        const float fraction = readPosition - (float)index0;
        const float output = delayLine[(size_t)index0]
            + fraction * (delayLine[(size_t)index1] - delayLine[(size_t)index0]);

        phase += juce::MathConstants<double>::twoPi * speedHz / sampleRate;
        if (phase >= juce::MathConstants<double>::twoPi)
            phase = std::fmod(phase, juce::MathConstants<double>::twoPi);

        writePosition = (writePosition + 1) % (int)delayLine.size();
        return output;
    }

private:
    static constexpr float centreDelaySeconds = 0.015f;
    static constexpr float maximumDepthSeconds = 0.007f;
    static constexpr float maximumDelaySeconds = centreDelaySeconds + maximumDepthSeconds;

    std::vector<float> delayLine;
    int writePosition = 0;
    double sampleRate = 44100.0, phase = 0.0;
    float amount = 0.5f, speedHz = 1.0f;
};
