#pragma once

#include <algorithm>
#include <cmath>

class Distortion
{
public:
    void prepare(double newSampleRate)
    {
        sampleRate = std::max(1.0, newSampleRate);
        reset();
    }

    void reset() { cassetteSmoother = 0.0f; }

    void setDrive(float d) { drive = clampFinite(d, 0.0f, 1.0f, 0.0f); }
    void setOutputLevel(float o) { outputLevel = clampFinite(o, 0.0f, 2.0f, 0.7f); }
    void setMix(float m) { mix = clampFinite(m, 0.0f, 1.0f, 0.0f); }
    void setCassette(float c) { cassette = clampFinite(c, 0.0f, 1.0f, 0.0f); }
    void setDistortionEnabled(bool enabled) { distortionEnabled = enabled; }
    void setCassetteEnabled(bool enabled) { cassetteEnabled = enabled; }

    float processSample(float input)
    {
        if (!std::isfinite(input))
            return 0.0f;

        float output = input;

        if (distortionEnabled && mix > 0.0f)
        {
            const float preGain = 1.0f + drive * 24.0f;
            const float distorted = std::tanh(input * preGain) * outputLevel;
            output = input * (1.0f - mix) + distorted * mix;
        }

        if (cassetteEnabled && cassette > 0.0f)
        {
            // Tape compression is independent of the distortion controls. Normalising
            // against tanh(drive) keeps full-scale material near its original level.
            const float tapeDrive = 1.0f + cassette * 8.0f;
            const float saturated = std::tanh(output * tapeDrive) / std::tanh(tapeDrive);

            // Deliberately dark tape response: Amount moves the one-pole cutoff from
            // about 16 kHz to 1.9 kHz. This is a low-pass, never a high-pass response.
            const float cutoffHz = 16000.0f * std::pow(0.12f, cassette);
            const float smoothing = 1.0f - std::exp(-2.0f * pi * cutoffHz / (float)sampleRate);
            cassetteSmoother += smoothing * (saturated - cassetteSmoother);
            const float tapeOutput = output + cassette * (cassetteSmoother - output);

            // No noise is added. Resolution falls mainly in the upper Amount range,
            // reaching six bits at maximum for an obvious but bounded cassette crunch.
            const float bitDepth = 16.0f - 10.0f * cassette * cassette;
            const float quantizationSteps = std::pow(2.0f, bitDepth - 1.0f);
            const float crushed = std::round(tapeOutput * quantizationSteps) / quantizationSteps;
            const float crushMix = smoothStep(0.45f, 0.95f, cassette);
            output = tapeOutput + (crushed - tapeOutput) * crushMix;
        }

        return std::isfinite(output) ? output : 0.0f;
    }

private:
    static constexpr float pi = 3.14159265358979323846f;

    static float clampFinite(float value, float minimum, float maximum, float fallback)
    {
        return std::clamp(std::isfinite(value) ? value : fallback, minimum, maximum);
    }

    static float smoothStep(float edge0, float edge1, float value)
    {
        const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    double sampleRate = 44100.0;
    float drive = 0.3f, outputLevel = 0.7f, mix = 1.0f, cassette = 0.0f;
    float cassetteSmoother = 0.0f;
    bool distortionEnabled = false;
    bool cassetteEnabled = false;
};
