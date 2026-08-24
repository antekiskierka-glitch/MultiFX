#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>
#include <cmath>

// Single-sideband frequency shifter. This processor intentionally contains no
// tape-wobble modulation: frequency shift and wow are independent effects.
class FrequencyShifter
{
public:
    FrequencyShifter() { createHilbertCoefficients(); }

    void prepare(double newSampleRate)
    {
        sampleRate = std::max(1.0, newSampleRate);
        smoothingCoefficient = std::exp(-1.0 / (sampleRate * 0.02));
        reset();
    }

    void reset()
    {
        delayLine.fill(0.0f);
        writePosition = 0;
        phase = 0.0;
        smoothedShiftHz = targetShiftHz;
    }

    void setFrequencyShiftHz(float value)
    {
        targetShiftHz = std::clamp(value, -200.0f, 200.0f);
    }

    float processSample(float input)
    {
        smoothedShiftHz = smoothingCoefficient * smoothedShiftHz
            + (1.0 - smoothingCoefficient) * targetShiftHz;

        delayLine[(size_t)writePosition] = input;
        float hilbert = 0.0f;
        for (int i = 0; i < numTaps; ++i)
        {
            const int index = (writePosition - i + numTaps) % numTaps;
            hilbert += hilbertCoefficients[(size_t)i] * delayLine[(size_t)index];
        }

        const int delayedIndex = (writePosition - centreTap + numTaps) % numTaps;
        const float delayedInput = delayLine[(size_t)delayedIndex];
        const float output = delayedInput * (float)std::cos(phase)
            - hilbert * (float)std::sin(phase);

        phase += juce::MathConstants<double>::twoPi * smoothedShiftHz / sampleRate;
        constexpr double twoPi = juce::MathConstants<double>::twoPi;
        if (phase >= twoPi || phase <= -twoPi)
            phase = std::fmod(phase, twoPi);

        writePosition = (writePosition + 1) % numTaps;
        return output;
    }

private:
    static constexpr int numTaps = 63;
    static constexpr int centreTap = numTaps / 2;

    void createHilbertCoefficients()
    {
        for (int i = 0; i < numTaps; ++i)
        {
            const int n = i - centreTap;
            if (n == 0 || (n % 2) == 0)
            {
                hilbertCoefficients[(size_t)i] = 0.0f;
                continue;
            }

            const float p = (float)i / (float)(numTaps - 1);
            const float window = 0.42f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * p)
                + 0.08f * std::cos(2.0f * juce::MathConstants<float>::twoPi * p);
            hilbertCoefficients[(size_t)i] = window * 2.0f
                / (juce::MathConstants<float>::pi * (float)n);
        }
    }

    std::array<float, numTaps> delayLine{}, hilbertCoefficients{};
    int writePosition = 0;
    double sampleRate = 44100.0, phase = 0.0, smoothingCoefficient = 0.0;
    float targetShiftHz = 0.0f;
    double smoothedShiftHz = 0.0;
};
