#include "PluginProcessor.h"
#include "PluginEditor.h"

MultiFXAudioProcessor::MultiFXAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)), apvts(*this, nullptr, "PARAMS", createParameterLayout()) {
    formatManager.registerBasicFormats();
}

juce::AudioProcessorValueTreeState::ParameterLayout MultiFXAudioProcessor::createParameterLayout()
{
    using P = juce::AudioParameterFloat; using B = juce::AudioParameterBool;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto f = [&](const char* i, const char* n, float a, float b, float d) { p.push_back(std::make_unique<P>(i, n, juce::NormalisableRange<float>(a, b, .001f), d)); };
    auto q = [&](const char* i, const char* n, float a, float b, float step, float d) { p.push_back(std::make_unique<P>(i, n, juce::NormalisableRange<float>(a, b, step), d)); };
    auto t = [&](const char* i, const char* n) { p.push_back(std::make_unique<B>(i, n, false)); };
    t("pitchBypass", "Frequency Shift Enabled"); q("freqShiftHz", "Frequency Shift (Hz)", -200, 200, .1f, 0);
    t("wowEnabled", "Wow Enabled"); f("wowAmount", "Wow Amount", 0, 1, .5f); f("wowSpeed", "Wow Speed", .1f, 8, 2);
    t("distBypass", "Distortion Enabled"); f("distMix", "Distortion Mix", 0, 1, 1);
    t("cassetteEnabled", "Cassette Enabled"); f("distCassette", "Cassette Amount", 0, 1, 0);
    t("chorusBypass", "Chorus Enabled"); f("chorusMix", "Chorus Mix", 0, 1, .3f);
    t("flangerEnabled", "Flanger Enabled"); f("flangerMix", "Flanger Mix", 0, 1, .5f);
    t("reverbBypass", "Reverb Enabled"); q("reverbCrossover", "Reverb Crossover (Hz)", 80, 8000, 1, 500); f("reverbSize", "Reverb Size", 0, 1, .5f); f("reverbDamping", "Reverb Damping", 0, 1, .5f); f("reverbLowLevel", "Reverb Low Band Level", 0, 1, .5f); f("reverbHighLevel", "Reverb High Band Level", 0, 1, 1); f("reverbMix", "Reverb Mix", 0, 1, .3f);
    t("squeezeBypass", "Lo-Fi Puncher Enabled"); f("squeezeAmount", "Squeeze Amount", 0, 1, .5f); q("squeezeCutoff", "Squeezer Cutoff", 200, 18000, 1, 8000); t("squeezeNoiseOnSound", "Noise On Sound"); f("squeezeMix", "Squeeze Mix", 0, 1, .5f);
    t("ghostEnabled", "Ghost Voice Enabled"); q("ghostPitch", "Ghost Pitch", -24, 12, .01f, -12); f("ghostMix", "Ghost Mix", 0, 1, .25f);
    t("driftEnabled", "Random Drift Enabled"); f("driftAmount", "Random Drift Amount", 0, 1, .25f); q("driftRate", "Random Drift Rate", .03f, 2, .01f, .25f);
    t("stutterEnabled", "Stutter Enabled"); q("stutterRate", "Stutter Rate", .25f, 16, .01f, 2); q("stutterLength", "Stutter Length", .02f, .5f, .001f, .12f); f("stutterMix", "Stutter Mix", 0, 1, .5f);
    t("granularEnabled", "Granular Enabled"); p.push_back(std::make_unique<juce::AudioParameterChoice>("granularSource", "Granular Source", juce::StringArray{ "Live", "File" }, 0));
    q("granularAttack", "Grain Attack (ms)", 1, 500, .1f, 20); q("granularHold", "Grain Hold (ms)", 5, 1000, .1f, 120); q("granularSpacing", "Grain Spacing (ms)", 5, 500, .1f, 60);
    q("granularSpeed", "Grain Speed", .25f, 4, .001f, 1); f("granularPan", "Grain Pan Spread", 0, 1, .35f); f("granularDepth", "Grain Depth", 0, 1, .5f); f("granularRandom", "Grain Random", 0, 1, .25f); f("granularMix", "Granular Mix", 0, 1, .5f);
    return { p.begin(), p.end() };
}

juce::String MultiFXAudioProcessor::getSupportedFileWildcard() const
{
    return formatManager.getWildcardForAllFormats();
}

bool MultiFXAudioProcessor::loadGranularFile(const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

    if (reader == nullptr)
    {
        granularStatus = "Unsupported: " + file.getFileName();
        return false;
    }

    const int numSamples = (int)juce::jmin(reader->lengthInSamples, (juce::int64)(reader->sampleRate * 30.0));

    if (numSamples < 4)
    {
        granularStatus = "Too short: " + file.getFileName();
        return false;
    }

    juce::AudioBuffer<float> loaded((int)juce::jmin<juce::uint32>(reader->numChannels, 2u), numSamples);

    if (!reader->read(&loaded, 0, numSamples, 0, true, loaded.getNumChannels() > 1))
    {
        granularStatus = "Read failed: " + file.getFileName();
        return false;
    }

    const int numChannels = loaded.getNumChannels();
    std::vector<float> preview((size_t)Granulizer::previewPoints, 0.0f);
    const int hop = juce::jmax(1, numSamples / Granulizer::previewPoints);

    for (int i = 0; i < Granulizer::previewPoints; ++i)
    {
        const int start = juce::jmin(i * hop, numSamples - 1);
        const int length = juce::jmin(hop, numSamples - start);
        float peak = 0.0f;

        for (int c = 0; c < numChannels; ++c)
            peak = juce::jmax(peak, loaded.getMagnitude(c, start, length));

        preview[(size_t)i] = peak;
    }

    // Swap by move so the audio thread is only blocked for a pointer exchange;
    // the previous buffer is released after the lock is dropped.
    {
        const juce::ScopedLock callbackLock(getCallbackLock());
        std::swap(granularFileBuffer, loaded);
        granularFileSampleRate = reader->sampleRate;
        granulizer.setFileSource(&granularFileBuffer, granularFileSampleRate);
    }

    filePreview = std::move(preview);
    granularStatus = file.getFileName() + "  |  "
        + juce::String(numSamples / reader->sampleRate, 2) + "s  "
        + juce::String((int)reader->sampleRate) + "Hz  "
        + juce::String(numChannels) + "ch";
    return true;
}

void MultiFXAudioProcessor::copyGranularPreview(std::vector<float>& destination) const
{
    const bool fileMode = apvts.getRawParameterValue("granularSource")->load() >= 0.5f;

    if (fileMode && filePreview.size() == (size_t)Granulizer::previewPoints)
        destination = filePreview;
    else
        granulizer.copyLivePreview(destination);
}

void MultiFXAudioProcessor::prepareToPlay(double sr, int bs) { sampleRate = sr; auto c = (juce::uint32)getTotalNumInputChannels(); frequencyShifters.clear(); frequencyShifters.resize(c); for (auto& x : frequencyShifters) x.prepare(sr); wowProcessors.clear(); wowProcessors.resize(c); for (auto& x : wowProcessors) x.prepare(sr); squeezers.clear(); squeezers.resize(c); for (auto& x : squeezers) x.prepare(sr); distortions.clear(); distortions.resize(c); for (auto& x : distortions) x.prepare(sr * 2); ghostVoices.clear(); ghostVoices.resize(c); randomDrifts.clear(); randomDrifts.resize(c); stutters.clear(); stutters.resize(c); for (auto& x : ghostVoices) x.prepare(sr); for (auto& x : randomDrifts) x.prepare(sr); for (auto& x : stutters) x.prepare(sr); juce::dsp::ProcessSpec s{ sr, (juce::uint32)bs, c }; chorus.prepare(s); flanger.prepare(s); reverb.prepare(s); distortionOversampling = std::make_unique<juce::dsp::Oversampling<float>>(c, 1, juce::dsp::Oversampling<float>::FilterType::filterHalfBandPolyphaseIIR, false, true); distortionOversampling->initProcessing((size_t)bs); setLatencySamples(juce::roundToInt(distortionOversampling->getLatencyInSamples())); granulizer.prepare(sr, (int)c); granulizer.setFileSource(granularFileBuffer.getNumSamples() > 0 ? &granularFileBuffer : nullptr, granularFileSampleRate); }
void MultiFXAudioProcessor::releaseResources() {}
bool MultiFXAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const { return (l.getMainOutputChannelSet() == juce::AudioChannelSet::mono() || l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()) && l.getMainOutputChannelSet() == l.getMainInputChannelSet(); }

void MultiFXAudioProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals n; const int ch = b.getNumChannels(), ns = b.getNumSamples(); auto v = [&](const char* id) { return apvts.getRawParameterValue(id)->load(); };
    if (v("distBypass") || v("cassetteEnabled")) { const float mix = v("distMix"); for (auto& x : distortions) { x.setDistortionEnabled(v("distBypass")); x.setDrive(mix); x.setMix(mix); x.setCassetteEnabled(v("cassetteEnabled")); x.setCassette(v("distCassette")); } juce::dsp::AudioBlock<float> z(b); auto u = distortionOversampling->processSamplesUp(z); for (size_t c = 0; c < u.getNumChannels(); ++c) for (size_t i = 0; i < u.getNumSamples(); ++i) u.getChannelPointer(c)[i] = distortions[c].processSample(u.getChannelPointer(c)[i]); distortionOversampling->processSamplesDown(z); }
    if (v("pitchBypass")) for (int c = 0; c < ch; ++c) { frequencyShifters[c].setFrequencyShiftHz(v("freqShiftHz")); auto* s = b.getWritePointer(c); for (int i = 0; i < ns; ++i) s[i] = frequencyShifters[c].processSample(s[i]); }
    if (v("wowEnabled")) for (int c = 0; c < ch; ++c) { wowProcessors[c].setAmount(v("wowAmount")); wowProcessors[c].setSpeed(v("wowSpeed")); auto* s = b.getWritePointer(c); for (int i = 0; i < ns; ++i) s[i] = wowProcessors[c].processSample(s[i]); }
    auto mod = [&](juce::dsp::Chorus<float>& e, const char* on, float mix, float centreDelay, bool useFeedback) { if (!v(on)) return; e.setRate(0.05f + 4.95f * mix); e.setDepth(mix); e.setFeedback(useFeedback ? (-0.95f + 1.90f * mix) : 0.0f); e.setMix(mix); e.setCentreDelay(centreDelay); juce::dsp::AudioBlock<float> z(b); juce::dsp::ProcessContextReplacing<float> q(z); e.process(q); };
    mod(chorus, "chorusBypass", v("chorusMix"), 7.0f, false); mod(flanger, "flangerEnabled", v("flangerMix"), 2.5f, true);
    if (v("reverbBypass")) { reverb.setCrossoverFrequency(v("reverbCrossover")); reverb.setRoomSize(v("reverbSize")); reverb.setDamping(v("reverbDamping")); reverb.setLowBandLevel(v("reverbLowLevel")); reverb.setHighBandLevel(v("reverbHighLevel")); reverb.setMix(v("reverbMix")); juce::dsp::AudioBlock<float> z(b); reverb.process(z); }
    if (v("squeezeBypass")) for (int c = 0; c < ch; ++c) { auto& x = squeezers[c]; x.setAmount(v("squeezeAmount")); x.setCutoff(v("squeezeCutoff")); x.setMix(v("squeezeMix")); x.setNoiseOnSound(v("squeezeNoiseOnSound") >= .5f); x.setBitcrush(.18f); auto* s = b.getWritePointer(c); for (int i = 0; i < ns; ++i) s[i] = x.processSample(s[i]); }
    if (v("ghostEnabled")) for (int c = 0; c < ch; ++c) { auto& x = ghostVoices[c]; x.setPitch(v("ghostPitch")); x.setMix(v("ghostMix")); auto* s = b.getWritePointer(c); for (int i = 0; i < ns; ++i) s[i] = x.processSample(s[i]); }
    if (v("driftEnabled")) for (int c = 0; c < ch; ++c) { auto& x = randomDrifts[c]; x.setAmount(v("driftAmount")); x.setRate(v("driftRate")); auto* s = b.getWritePointer(c); for (int i = 0; i < ns; ++i) s[i] = x.processSample(s[i]); }
    if (v("stutterEnabled")) for (int c = 0; c < ch; ++c) { auto& x = stutters[c]; x.setRate(v("stutterRate")); x.setLength(v("stutterLength")); x.setMix(v("stutterMix")); auto* s = b.getWritePointer(c); for (int i = 0; i < ns; ++i) s[i] = x.processSample(s[i]); }
    // Runs even when disabled so the live buffer and preview stay current and
    // enabling the effect grains recent audio instead of silence.
    granulizer.setActive(v("granularEnabled") >= .5f); granulizer.setSourceMode(v("granularSource") >= .5f ? Granulizer::SourceMode::file : Granulizer::SourceMode::live);
    granulizer.setAttackMs(v("granularAttack")); granulizer.setHoldMs(v("granularHold")); granulizer.setSpacingMs(v("granularSpacing")); granulizer.setSpeed(v("granularSpeed"));
    granulizer.setPan(v("granularPan")); granulizer.setDepth(v("granularDepth")); granulizer.setRandom(v("granularRandom")); granulizer.setMix(v("granularMix")); granulizer.process(b);
}

juce::AudioProcessorEditor* MultiFXAudioProcessor::createEditor() { return new MultiFXAudioProcessorEditor(*this); }
void MultiFXAudioProcessor::getStateInformation(juce::MemoryBlock& d) { auto s = apvts.copyState(); std::unique_ptr<juce::XmlElement> x(s.createXml()); copyXmlToBinary(*x, d); }
void MultiFXAudioProcessor::setStateInformation(const void* d, int n) { std::unique_ptr<juce::XmlElement> x(getXmlFromBinary(d, n)); if (x && x->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*x)); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MultiFXAudioProcessor(); }
