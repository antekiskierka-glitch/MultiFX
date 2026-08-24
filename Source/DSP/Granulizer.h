#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

// Granular engine with two sources: a live circular buffer fed by the audio
// thread and a static file buffer owned by the processor. Everything the audio
// thread touches is preallocated in prepare(): the live buffer and the grain
// pool are fixed size, so process() never allocates or locks.
class Granulizer
{
public:
    enum class SourceMode { live, file };

    static constexpr int maxGrains = 48;
    static constexpr int previewPoints = 256;
    static constexpr double liveBufferSeconds = 6.0;

    void prepare(double sampleRate, int numChannels)
    {
        sr = std::max(8000.0, sampleRate);
        channels = std::clamp(numChannels, 1, 2);
        liveBuffer.setSize(channels, (int)std::ceil(sr * liveBufferSeconds), false, true, false);
        previewHopSamples = std::max(1, (int)(sr * 0.01));
        reset();
    }

    void reset()
    {
        liveBuffer.clear();
        writePos = 0;
        recordedSamples = 0;
        samplesUntilNextGrain = 0;
        previewCounter = 0;
        previewAccumulator = 0.0f;
        previewWritePos.store(0);

        for (auto& g : grains)
            g.active = false;

        for (auto& p : previewPeaks)
            p.store(0.0f);
    }

    void setActive(bool shouldRenderGrains) noexcept { active = shouldRenderGrains; }
    void setSourceMode(SourceMode mode) noexcept { sourceMode = mode; }
    void setAttackMs(float value) noexcept { attackMs = std::clamp(value, 1.0f, 500.0f); }
    void setHoldMs(float value) noexcept { holdMs = std::clamp(value, 5.0f, 1000.0f); }
    void setSpacingMs(float value) noexcept { spacingMs = std::clamp(value, 5.0f, 500.0f); }
    void setSpeed(float value) noexcept { speed = std::clamp(value, 0.25f, 4.0f); }
    void setPan(float value) noexcept { panSpread = std::clamp(value, 0.0f, 1.0f); }
    void setDepth(float value) noexcept { depth = std::clamp(value, 0.0f, 1.0f); }
    void setRandom(float value) noexcept { randomAmount = std::clamp(value, 0.0f, 1.0f); }
    void setMix(float value) noexcept { mix = std::clamp(value, 0.0f, 1.0f); }

    // Called from the audio thread only; the buffer stays alive for as long as
    // the processor holds its reference, so storing the raw pointer is safe.
    void setFileSource(const juce::AudioBuffer<float>* buffer, double bufferSampleRate) noexcept
    {
        fileBuffer = buffer;
        fileSampleRate = bufferSampleRate > 0.0 ? bufferSampleRate : sr;
    }

    void process(juce::AudioBuffer<float>& block)
    {
        const int liveLength = liveBuffer.getNumSamples();
        const int numSamples = block.getNumSamples();
        const int numOutputChannels = std::min(block.getNumChannels(), channels);

        if (liveLength < 8 || numSamples <= 0 || numOutputChannels <= 0)
            return;

        const juce::AudioBuffer<float>* file = fileBuffer;
        const bool useFile = sourceMode == SourceMode::file;

        // File mode without a usable buffer still records into the live buffer
        // so the preview keeps scrolling; it just renders no grains.
        const bool renderGrains = active && (!useFile || (file != nullptr && file->getNumSamples() >= 4));

        float* liveLeft = liveBuffer.getWritePointer(0);
        float* liveRight = channels > 1 ? liveBuffer.getWritePointer(1) : liveLeft;
        float* outLeft = block.getWritePointer(0);
        float* outRight = numOutputChannels > 1 ? block.getWritePointer(1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            const float inLeft = outLeft[i];
            const float inRight = outRight != nullptr ? outRight[i] : inLeft;

            liveLeft[writePos] = inLeft;
            if (channels > 1)
                liveRight[writePos] = inRight;

            pushPreview(std::max(std::abs(inLeft), std::abs(inRight)));

            float wetLeft = 0.0f;
            float wetRight = 0.0f;

            if (renderGrains)
            {
                if (--samplesUntilNextGrain <= 0)
                {
                    triggerGrain(file);
                    samplesUntilNextGrain = spacingInSamples();
                }

                for (auto& g : grains)
                {
                    if (!g.active)
                        continue;

                    const float* sourceLeft;
                    const float* sourceRight;
                    int sourceLength;

                    if (g.fromFile)
                    {
                        if (file == nullptr || file->getNumSamples() < 4)
                        {
                            g.active = false;
                            continue;
                        }

                        sourceLeft = file->getReadPointer(0);
                        sourceRight = file->getNumChannels() > 1 ? file->getReadPointer(1) : sourceLeft;
                        sourceLength = file->getNumSamples();
                    }
                    else
                    {
                        sourceLeft = liveLeft;
                        sourceRight = liveRight;
                        sourceLength = liveLength;
                    }

                    const float envelope = grainEnvelope(g);
                    wetLeft += readInterpolated(sourceLeft, sourceLength, g.readPos) * envelope * g.gainLeft;
                    wetRight += readInterpolated(sourceRight, sourceLength, g.readPos) * envelope * g.gainRight;

                    g.readPos += g.increment;
                    while (g.readPos >= (double)sourceLength)
                        g.readPos -= (double)sourceLength;

                    if (++g.age >= g.attackSamples + g.holdSamples + g.releaseSamples)
                        g.active = false;
                }
            }

            writePos = writePos + 1 < liveLength ? writePos + 1 : 0;
            if (recordedSamples < liveLength)
                ++recordedSamples;

            if (!renderGrains)
                continue;

            if (outRight != nullptr)
            {
                outLeft[i] = inLeft * (1.0f - mix) + wetLeft * mix;
                outRight[i] = inRight * (1.0f - mix) + wetRight * mix;
            }
            else
            {
                outLeft[i] = inLeft * (1.0f - mix) + (wetLeft + wetRight) * 0.5f * mix;
            }
        }
    }

    // Scrolling peak envelope of the live buffer, for the editor's preview.
    void copyLivePreview(std::vector<float>& destination) const
    {
        destination.resize((size_t)previewPoints);
        const int start = previewWritePos.load();

        for (int i = 0; i < previewPoints; ++i)
            destination[(size_t)i] = previewPeaks[(size_t)((start + i) % previewPoints)].load();
    }

private:
    struct Grain
    {
        bool active = false;
        bool fromFile = false;
        double readPos = 0.0;
        double increment = 1.0;
        int age = 0;
        int attackSamples = 1;
        int holdSamples = 1;
        int releaseSamples = 1;
        float gainLeft = 0.0f;
        float gainRight = 0.0f;
    };

    static float readInterpolated(const float* data, int length, double position) noexcept
    {
        int index = (int)position;
        const double fraction = position - (double)index;
        index %= length;
        if (index < 0)
            index += length;
        const int next = index + 1 < length ? index + 1 : 0;
        return (float)((double)data[index] + ((double)data[next] - (double)data[index]) * fraction);
    }

    static float grainEnvelope(const Grain& g) noexcept
    {
        float value;

        if (g.age < g.attackSamples)
            value = (float)g.age / (float)g.attackSamples;
        else if (g.age < g.attackSamples + g.holdSamples)
            value = 1.0f;
        else
            value = 1.0f - (float)(g.age - g.attackSamples - g.holdSamples) / (float)g.releaseSamples;

        value = std::clamp(value, 0.0f, 1.0f);
        return value * value * (3.0f - 2.0f * value);
    }

    int spacingInSamples() const noexcept
    {
        return std::max(16, (int)(spacingMs * 0.001f * (float)sr));
    }

    int msToSamples(float milliseconds) const noexcept
    {
        return (int)(milliseconds * 0.001f * (float)sr);
    }

    float nextRandomBipolar() noexcept
    {
        randomState = randomState * 1664525u + 1013904223u;
        return (float)((randomState >> 8) & 0x00ffffffu) / 8388607.5f - 1.0f;
    }

    void triggerGrain(const juce::AudioBuffer<float>* file) noexcept
    {
        Grain* grain = nullptr;

        for (auto& candidate : grains)
        {
            if (!candidate.active)
            {
                grain = &candidate;
                break;
            }
        }

        if (grain == nullptr)
            return;

        const float positionJitter = nextRandomBipolar() * randomAmount;
        const double rate = std::clamp(
            (double)speed * std::pow(2.0, (double)(nextRandomBipolar() * randomAmount) * 7.0 / 12.0),
            0.05, 8.0);

        int attack = std::max(1, msToSamples(attackMs));
        int hold = std::max(0, msToSamples(holdMs));

        if (sourceMode == SourceMode::file)
        {
            if (file == nullptr || file->getNumSamples() < 4)
                return;

            const double length = (double)file->getNumSamples();
            double position = std::fmod((double)depth * length + (double)positionJitter * length * 0.5, length);
            if (position < 0.0)
                position += length;

            grain->fromFile = true;
            grain->readPos = position;
            grain->increment = rate * fileSampleRate / sr;
        }
        else
        {
            const int liveLength = liveBuffer.getNumSamples();

            // Grains may only read audio that has actually been recorded, and
            // depth spans a musical look-back rather than the whole buffer, so
            // it stays useful straight after prepare() and on short material.
            const int maxLookback = std::min(recordedSamples, (int)(sr * maxLookbackSeconds));
            const double window = (double)maxLookback - minMargin - 8.0;

            if (window < 32.0)
                return;

            // The grain drifts away from the write head at |rate - 1| samples
            // per sample, so its length and start offset are both bounded to
            // keep it inside the valid region for its whole life.
            const double drift = std::abs(rate - 1.0);

            if (drift > 1.0e-4)
            {
                const int maxTotal = std::max(8, (int)(window / drift));

                if (attack * 2 + hold > maxTotal)
                {
                    hold = std::max(0, maxTotal - attack * 2);

                    if (attack * 2 + hold > maxTotal)
                    {
                        attack = std::max(1, maxTotal / 2);
                        hold = 0;
                    }
                }
            }

            const double travel = drift * (double)(attack * 2 + hold);
            const double lowest = minMargin + (rate > 1.0 ? travel : 0.0);
            const double highest = minMargin + window - (rate > 1.0 ? 0.0 : travel);
            const double span = std::max(0.0, highest - lowest);
            const double normalised = std::clamp((double)depth + (double)positionJitter * 0.5, 0.0, 1.0);

            double position = (double)writePos - (lowest + normalised * span);
            while (position < 0.0)
                position += (double)liveLength;

            grain->fromFile = false;
            grain->readPos = position;
            grain->increment = rate;
        }

        const float pan = std::clamp(0.5f + nextRandomBipolar() * panSpread * 0.5f, 0.0f, 1.0f);
        const float overlap = std::max(1.0f, (float)(attack * 2 + hold) / (float)spacingInSamples());
        const float gain = 1.0f / std::sqrt(overlap);

        grain->attackSamples = attack;
        grain->holdSamples = hold;
        grain->releaseSamples = attack;
        grain->age = 0;
        grain->gainLeft = std::cos(pan * juce::MathConstants<float>::halfPi) * gain;
        grain->gainRight = std::sin(pan * juce::MathConstants<float>::halfPi) * gain;
        grain->active = true;
    }

    void pushPreview(float magnitude) noexcept
    {
        previewAccumulator = std::max(previewAccumulator, magnitude);

        if (++previewCounter < previewHopSamples)
            return;

        const int position = previewWritePos.load();
        previewPeaks[(size_t)position].store(previewAccumulator);
        previewWritePos.store(position + 1 < previewPoints ? position + 1 : 0);
        previewCounter = 0;
        previewAccumulator = 0.0f;
    }

    double sr = 44100.0;
    int channels = 2;
    bool active = false;
    SourceMode sourceMode = SourceMode::live;

    float attackMs = 20.0f;
    float holdMs = 120.0f;
    float spacingMs = 60.0f;
    float speed = 1.0f;
    float panSpread = 0.35f;
    float depth = 0.5f;
    float randomAmount = 0.25f;
    float mix = 0.5f;

    // Depth sweeps at most this far back, so it stays musically useful instead
    // of mapping across the whole (much longer) circular buffer.
    static constexpr double maxLookbackSeconds = 1.5;
    static constexpr double minMargin = 64.0;

    juce::AudioBuffer<float> liveBuffer;
    int writePos = 0;
    int recordedSamples = 0;
    int samplesUntilNextGrain = 0;

    const juce::AudioBuffer<float>* fileBuffer = nullptr;
    double fileSampleRate = 44100.0;

    std::array<Grain, (size_t)maxGrains> grains;
    uint32_t randomState = 0x2545f491u;

    std::array<std::atomic<float>, (size_t)previewPoints> previewPeaks {};
    std::atomic<int> previewWritePos { 0 };
    int previewCounter = 0;
    int previewHopSamples = 441;
    float previewAccumulator = 0.0f;
};
