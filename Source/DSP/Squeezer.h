#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

class Squeezer
{
public:
    void prepare(double sampleRate)
    {
        sr = std::max(8000.0, sampleRate);
        reset();
        noiseState = 0x12345678u;
        updateFilterCoefficient();
        updateLowBandCoefficient();
    }

    void reset()
    {
        filterState = 0.0f;
        envelope = 0.0f;
        lowBandState = 0.0f;
    }

    void setAmount(float value)
    {
        amount = std::clamp(value, 0.0f, 1.0f);
    }

    void setCutoff(float value)
    {
        cutoffHz = std::clamp(value, 200.0f, 18000.0f);
        updateFilterCoefficient();
    }

    void setMix(float value)
    {
        mix = std::clamp(value, 0.0f, 1.0f);
    }

    // true: noise is injected only while the input signal is above the threshold.
    void setNoiseOnSound(bool enabled)
    {
        noiseOnSound = enabled;
    }

    // 0 disables bitcrushing; 1 is intentionally still subtle.
    void setBitcrush(float value)
    {
        bitcrush = std::clamp(value, 0.0f, 1.0f);
    }

    float processSample(float input)
    {
        if (!std::isfinite(input))
            return 0.0f;

        const float level = std::abs(input);
        envelope = std::max(level, envelope * 0.995f);

        const float excess = std::max(0.0f, envelope - 0.35f);
        const float compression =
            1.0f / (1.0f + amount * excess * 5.0f);

        noiseState = noiseState * 1664525u + 1013904223u;
        const float noise =
            (static_cast<float>((noiseState >> 8) & 0x00ffffff) / 8388607.5f) - 1.0f;

        // Short release keeps noise natural on drum hits rather than abruptly cutting it off.
        const float noiseGate = noiseOnSound
            ? std::clamp((envelope - noiseThreshold) / (1.0f - noiseThreshold), 0.0f, 1.0f)
            : 1.0f;

        const float drive = 1.0f + amount * 22.0f;
        const float dirtySignal = input + noise * amount * 0.025f * noiseGate;
        const float saturated = std::tanh(dirtySignal * drive) * compression;

        // Preserve bass: split at ~180 Hz, crush only the high band, then recombine.
        lowBandState = (1.0f - lowBandCoefficient) * saturated
            + lowBandCoefficient * lowBandState;
        const float highBand = saturated - lowBandState;

        const float steps = 32.0f + (1.0f - bitcrush) * 480.0f;
        const float crushedHighBand = std::round(highBand * steps) / steps;
        const float bitcrushed = lowBandState
            + highBand * (1.0f - bitcrush)
            + crushedHighBand * bitcrush;

        // Filter remains after saturation and bitcrush so Cutoff controls the final texture.
        filterState = (1.0f - filterCoefficient) * bitcrushed
            + filterCoefficient * filterState;

        return input * (1.0f - mix) + filterState * mix;
    }

private:
    void updateFilterCoefficient()
    {
        filterCoefficient = std::exp(
            -2.0f * juce::MathConstants<float>::pi
            * cutoffHz / static_cast<float>(sr));
    }

    void updateLowBandCoefficient()
    {
        constexpr float lowBandHz = 180.0f;
        lowBandCoefficient = std::exp(
            -2.0f * juce::MathConstants<float>::pi
            * lowBandHz / static_cast<float>(sr));
    }

    double sr = 44100.0;
    float amount = 0.5f;
    float cutoffHz = 8000.0f;
    float mix = 0.5f;
    float bitcrush = 0.0f;
    bool noiseOnSound = false;
    float noiseThreshold = 0.015f;
    float filterState = 0.0f;
    float lowBandState = 0.0f;
    float envelope = 0.0f;
    float filterCoefficient = 0.0f;
    float lowBandCoefficient = 0.0f;
    uint32_t noiseState = 0x12345678u;
};
