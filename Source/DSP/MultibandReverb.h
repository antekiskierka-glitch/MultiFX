#pragma once
#include <juce_dsp/juce_dsp.h>

class MultibandReverb
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        for (auto* filter : { &lowPassLR, &highPassLR })
            filter->prepare(spec);

        lowPassLR.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
        highPassLR.setType(juce::dsp::LinkwitzRileyFilterType::highpass);

        lowReverb.prepare(spec);
        highReverb.prepare(spec);

        lowBuffer.setSize((int)spec.numChannels, (int)spec.maximumBlockSize);
        highBuffer.setSize((int)spec.numChannels, (int)spec.maximumBlockSize);

        roomSizeSmoothed.reset(sampleRate, 0.05);
        dampingSmoothed.reset(sampleRate, 0.05);
        crossoverSmoothed.reset(sampleRate, 0.05);

        roomSizeSmoothed.setCurrentAndTargetValue(roomSize);
        dampingSmoothed.setCurrentAndTargetValue(damping);
        crossoverSmoothed.setCurrentAndTargetValue(crossoverHz);

        updateReverbParams(roomSize, damping);
        setCrossoverNow(crossoverHz);
    }

    void reset()
    {
        lowPassLR.reset();
        highPassLR.reset();
        lowReverb.reset();
        highReverb.reset();
    }

    void setCrossoverFrequency(float hz)
    {
        crossoverHz = std::clamp(hz, 80.0f, 8000.0f);
        crossoverSmoothed.setTargetValue(crossoverHz);
    }

    void setRoomSize(float value)
    {
        roomSize = std::clamp(value, 0.0f, 1.0f);
        roomSizeSmoothed.setTargetValue(roomSize);
    }

    void setDamping(float value)
    {
        damping = std::clamp(value, 0.0f, 1.0f);
        dampingSmoothed.setTargetValue(damping);
    }

    void setLowBandLevel(float value)
    {
        lowLevel = std::clamp(value, 0.0f, 1.0f);
    }

    void setHighBandLevel(float value)
    {
        highLevel = std::clamp(value, 0.0f, 1.0f);
    }

    void setMix(float value)
    {
        mix = std::clamp(value, 0.0f, 1.0f);
    }

    void process(juce::dsp::AudioBlock<float>& block)
    {
        const auto numCh = block.getNumChannels();
        const auto numSamples = block.getNumSamples();

        // Aktualizacja płynnych wartości raz na blok audio.
        const float currentRoomSize = roomSizeSmoothed.skip((int)numSamples);
        const float currentDamping = dampingSmoothed.skip((int)numSamples);
        const float currentCrossover = crossoverSmoothed.skip((int)numSamples);

        updateReverbParams(currentRoomSize, currentDamping);
        setCrossoverNow(currentCrossover);

        lowBuffer.setSize((int)numCh, (int)numSamples, false, false, true);
        highBuffer.setSize((int)numCh, (int)numSamples, false, false, true);
        lowBuffer.clear();
        highBuffer.clear();

        for (size_t ch = 0; ch < numCh; ++ch)
        {
            auto* in = block.getChannelPointer(ch);
            auto* lo = lowBuffer.getWritePointer((int)ch);
            auto* hi = highBuffer.getWritePointer((int)ch);

            for (size_t i = 0; i < numSamples; ++i)
            {
                lo[i] = lowPassLR.processSample((int)ch, in[i]);
                hi[i] = highPassLR.processSample((int)ch, in[i]);
            }
        }

        juce::dsp::AudioBlock<float> lowBlock(lowBuffer);
        juce::dsp::AudioBlock<float> highBlock(highBuffer);

        juce::dsp::ProcessContextReplacing<float> lowContext(lowBlock);
        juce::dsp::ProcessContextReplacing<float> highContext(highBlock);

        lowReverb.process(lowContext);
        highReverb.process(highContext);

        for (size_t ch = 0; ch < numCh; ++ch)
        {
            auto* out = block.getChannelPointer(ch);
            auto* lo = lowBuffer.getReadPointer((int)ch);
            auto* hi = highBuffer.getReadPointer((int)ch);

            for (size_t i = 0; i < numSamples; ++i)
            {
                const float dry = out[i];
                const float wet = lo[i] * lowLevel + hi[i] * highLevel;

                out[i] = dry + wet * mix;
            }
        }
    }

private:
    void setCrossoverNow(float hz)
    {
        lowPassLR.setCutoffFrequency(hz);
        highPassLR.setCutoffFrequency(hz);
    }

    void updateReverbParams(float currentRoomSize, float currentDamping)
    {
        juce::Reverb::Parameters parameters;
        parameters.roomSize = currentRoomSize;
        parameters.damping = currentDamping;
        parameters.wetLevel = 1.0f;
        parameters.dryLevel = 0.0f;
        parameters.width = 1.0f;

        lowReverb.setParameters(parameters);
        highReverb.setParameters(parameters);
    }

    juce::dsp::LinkwitzRileyFilter<float> lowPassLR;
    juce::dsp::LinkwitzRileyFilter<float> highPassLR;

    juce::dsp::Reverb lowReverb;
    juce::dsp::Reverb highReverb;

    juce::AudioBuffer<float> lowBuffer;
    juce::AudioBuffer<float> highBuffer;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> roomSizeSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> dampingSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> crossoverSmoothed;

    float roomSize = 0.5f;
    float damping = 0.5f;
    float crossoverHz = 500.0f;
    float lowLevel = 1.0f;
    float highLevel = 1.0f;
    float mix = 0.3f;

    double sampleRate = 44100.0;
};