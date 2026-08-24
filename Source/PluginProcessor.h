#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <memory>
#include <vector>

#include "DSP/Distortion.h"
#include "DSP/FrequencyShifter.h"
#include "DSP/Granulizer.h"
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

    // Wildcard list for the formats the current JUCE build registered,
    // e.g. "*.wav;*.aiff;*.mp3".
    juce::String getSupportedFileWildcard() const;

    // Message thread only. Decodes the file, then swaps it into the engine
    // under the callback lock so the audio thread never sees a partial buffer.
    bool loadGranularFile(const juce::File&);
    juce::String getGranularStatus() const { return granularStatus; }

    // Peak envelope of the loaded file, or of the live buffer in live mode.
    void copyGranularPreview(std::vector<float>& destination) const;

private:
    juce::AudioFormatManager formatManager;
    juce::AudioBuffer<float> granularFileBuffer;
    double granularFileSampleRate = 44100.0;
    juce::String granularStatus { "No file loaded" };
    std::vector<float> filePreview;
    Granulizer granulizer;

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
