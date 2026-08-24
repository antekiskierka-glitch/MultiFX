#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <memory>
#include <vector>

#include "DSP/Distortion.h"
#include "DSP/FrequencyShifter.h"
#include "HorrorLabDSP.h"
#include "DSP/MultibandReverb.h"
#include "DSP/PitchShifter.h"
#include "DSP/Squeezer.h"
#include "DSP/WowProcessor.h"

class MultiFXAudioProcessor : public juce::AudioProcessor
{
public:
    MultiFXAudioProcessor();
    ~MultiFXAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MultiFX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    std::vector<FrequencyShifter> frequencyShifters;
    std::vector<WowProcessor> wowProcessors;
    std::vector<Squeezer> squeezers;
    std::vector<Distortion> distortions;
    std::vector<GhostVoice> ghostVoices;
    std::vector<RandomDrift> randomDrifts;
    std::vector<Stutter> stutters;

    std::unique_ptr<juce::dsp::Oversampling<float>> distortionOversampling;
    juce::dsp::Chorus<float> chorus, flanger;
    MultibandReverb reverb;
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiFXAudioProcessor)
};
