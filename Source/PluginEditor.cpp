#include "PluginEditor.h"
#include "BinaryData.h"
#include <cmath>
static juce::Image img(const char* d, int s) { return juce::ImageFileFormat::loadFrom(d, static_cast<size_t>(s)); }
EffectSection::EffectSection(juce::AudioProcessorValueTreeState& a, const juce::String& t, const juce::String& id, std::vector<std::pair<juce::String, juce::String>> p, int c, const juce::String& e, const juce::String& et) :maxColumns(c) { group.setText(t);addAndMakeVisible(group);enable.setButtonText("ENABLE EFFECT");addAndMakeVisible(enable);enableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(a, id, enable);for (auto& x : p) { auto k = std::make_unique<KnobWithLabel>(a, x.first, x.second);addAndMakeVisible(*k);knobs.push_back(std::move(k)); }if (e.isNotEmpty()) { extra.setButtonText(et);addAndMakeVisible(extra);extraAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(a, e, extra); } }
void EffectSection::paint(juce::Graphics& g) { auto b = getLocalBounds().toFloat().reduced(3.0f);const bool a = enable.getToggleState();const float glow = a ? (0.045f + pulse * 0.055f) : 0.0f;g.setColour(juce::Colours::black.withAlpha(0.34f));g.fillRoundedRectangle(b.translated(1.5f, 2.0f), 8.0f);g.setColour(juce::Colour(0xff10090d).withAlpha(a ? 0.30f : 0.28f));g.fillRoundedRectangle(b, 8.0f);g.setColour(juce::Colour(0xff35141d).withAlpha(a ? 0.12f : 0.08f));g.fillRoundedRectangle(b, 8.0f);if (a) { g.setColour(juce::Colour(0xff813641).withAlpha(glow));g.fillRoundedRectangle(b, 8.0f); }g.setColour((a ? juce::Colour(0xff78313a) : juce::Colour(0xff503338)).withAlpha(a ? 0.80f : 0.52f));g.drawRoundedRectangle(b, 8.0f, a ? 1.35f : 1.0f);g.setColour(juce::Colour(0xffa24751).withAlpha(a ? 0.48f + pulse * 0.17f : 0.30f));g.drawLine(b.getX() + 9.0f, b.getY() + 7.0f, b.getRight() - 15.0f, b.getY() + 7.0f, 1.0f); }
void EffectSection::resized() { auto b = getLocalBounds().reduced(6, 19);group.setBounds(getLocalBounds());enable.setBounds(b.removeFromTop(20));b.removeFromTop(3);const int n = (int)knobs.size();if (n == 0)return;const int c = juce::jmin(n, maxColumns), r = (n + c - 1) / c;for (int i = 0;i < n;++i) { auto x = juce::Rectangle<int>(b.getX() + (i % c) * b.getWidth() / c, b.getY() + (i / c) * b.getHeight() / r, b.getWidth() / c, b.getHeight() / r);knobs[(size_t)i]->setBounds(x.reduced(r > 1 ? 1 : 2)); }if (extraAttachment != nullptr && n == 3 && c == 2) { auto x = juce::Rectangle<int>(b.getCentreX(), b.getCentreY(), b.getWidth() / 2, b.getHeight() / 2);extra.setBounds(juce::Rectangle<int>(36, 36).withCentre(x.getCentre())); } }
void WaveformDisplay::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(2.0f);
    g.setColour(juce::Colour(0xff0d0a0d).withAlpha(0.72f));
    g.fillRoundedRectangle(b, 5.0f);
    g.setColour(juce::Colour(0xff6d3038).withAlpha(0.62f));
    g.drawRoundedRectangle(b, 5.0f, 1.0f);
    g.setColour(juce::Colour(0xff8d4a55).withAlpha(0.35f));
    g.drawHorizontalLine((int)b.getCentreY(), b.getX(), b.getRight());

    if (peaks.empty())
    {
        g.setColour(FreakLookAndFeel::cream.withAlpha(0.45f));
        g.setFont(juce::Font(11.0f));
        g.drawText("NO SIGNAL", getLocalBounds(), juce::Justification::centred);
        return;
    }

    auto inner = b.reduced(4.0f);
    const float half = inner.getHeight() * 0.5f;
    const float step = inner.getWidth() / (float)peaks.size();
    g.setColour(juce::Colour(0xffd8caab).withAlpha(0.78f));

    for (size_t i = 0; i < peaks.size(); ++i)
    {
        const float height = juce::jmax(1.0f, juce::jlimit(0.0f, 1.0f, peaks[i]) * half);
        g.fillRect(inner.getX() + (float)i * step, inner.getCentreY() - height, juce::jmax(1.0f, step - 0.6f), height * 2.0f);
    }
}

GranularPage::GranularPage(MultiFXAudioProcessor& processor) : processorRef(processor)
{
    sourceParameter = dynamic_cast<juce::AudioParameterChoice*>(processor.apvts.getParameter("granularSource"));

    enable.setButtonText("ENABLE GRANULIZER");
    addAndMakeVisible(enable);
    enableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "granularEnabled", enable);

    liveButton.setButtonText("LIVE");
    fileButton.setButtonText("FILE");
    loadButton.setButtonText("LOAD FILE");
    loadButton.setClickingTogglesState(false);
    liveButton.onClick = [this] { setSourceMode(false); };
    fileButton.onClick = [this] { setSourceMode(true); };
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser>("Load a grain source", juce::File(), processorRef.getSupportedFileWildcard());
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();
                if (file.existsAsFile())
                    loadFile(file);
            });
    };

    for (auto* b : { &liveButton, &fileButton, &loadButton })
        addAndMakeVisible(b);

    status.setJustificationType(juce::Justification::centredLeft);
    status.setFont(juce::Font(11.0f));
    status.setColour(juce::Label::textColourId, juce::Colour(0xffc9aaa4));
    addAndMakeVisible(status);
    addAndMakeVisible(waveform);

    for (auto& spec : std::vector<std::pair<juce::String, juce::String>>{
             { "granularAttack", "Attack" }, { "granularHold", "Hold" }, { "granularSpacing", "Spacing" },
             { "granularSpeed", "Speed" }, { "granularPan", "Pan" }, { "granularDepth", "Depth" },
             { "granularRandom", "Random" }, { "granularMix", "Mix" } })
    {
        auto k = std::make_unique<KnobWithLabel>(processor.apvts, spec.first, spec.second);
        addAndMakeVisible(*k);
        knobs.push_back(std::move(k));
    }

    refresh();
}

void GranularPage::setSourceMode(bool useFile)
{
    if (sourceParameter != nullptr)
    {
        const float target = sourceParameter->convertTo0to1((float)(useFile ? 1 : 0));
        sourceParameter->beginChangeGesture();
        sourceParameter->setValueNotifyingHost(target);
        sourceParameter->endChangeGesture();
    }

    refresh();
}

void GranularPage::loadFile(const juce::File& file)
{
    processorRef.loadGranularFile(file);
    status.setText(processorRef.getGranularStatus(), juce::dontSendNotification);
}

bool GranularPage::isInterestedInFileDrag(const juce::StringArray& files)
{
    const auto wildcard = processorRef.getSupportedFileWildcard();

    for (const auto& f : files)
        if (wildcard.containsIgnoreCase("*" + juce::File(f).getFileExtension()))
            return true;

    return false;
}

void GranularPage::filesDropped(const juce::StringArray& files, int, int)
{
    for (const auto& f : files)
    {
        const juce::File file(f);

        if (file.existsAsFile())
        {
            setSourceMode(true);
            loadFile(file);
            return;
        }
    }
}

void GranularPage::refresh()
{
    const bool fileMode = sourceParameter != nullptr && sourceParameter->getIndex() == 1;
    liveButton.setToggleState(!fileMode, juce::dontSendNotification);
    fileButton.setToggleState(fileMode, juce::dontSendNotification);
    status.setText(fileMode ? processorRef.getGranularStatus() : "Live input", juce::dontSendNotification);
    processorRef.copyGranularPreview(previewScratch);
    waveform.setPeaks(previewScratch);
}

void GranularPage::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(3.0f);
    g.setColour(juce::Colours::black.withAlpha(0.34f));
    g.fillRoundedRectangle(b.translated(1.5f, 2.0f), 8.0f);
    g.setColour(juce::Colour(0xff10090d).withAlpha(0.30f));
    g.fillRoundedRectangle(b, 8.0f);
    g.setColour(EffectSection::panelBorderColour());
    g.drawRoundedRectangle(b, 8.0f, 1.2f);
    g.setColour(FreakLookAndFeel::cream.withAlpha(0.85f));
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText("GRANULIZER", getLocalBounds().reduced(14, 8).removeFromTop(18), juce::Justification::topLeft);
}

void GranularPage::resized()
{
    auto b = getLocalBounds().reduced(12, 26);
    auto header = b.removeFromTop(24);
    enable.setBounds(header.removeFromLeft(180));
    header.removeFromLeft(10);
    liveButton.setBounds(header.removeFromLeft(58).reduced(0, 1));
    header.removeFromLeft(4);
    fileButton.setBounds(header.removeFromLeft(58).reduced(0, 1));
    header.removeFromLeft(10);
    loadButton.setBounds(header.removeFromLeft(84).reduced(0, 1));

    b.removeFromTop(6);
    waveform.setBounds(b.removeFromTop(juce::jmax(48, b.getHeight() / 4)));
    status.setBounds(b.removeFromTop(18).reduced(4, 0));
    b.removeFromTop(4);

    const int n = (int)knobs.size();
    if (n == 0)
        return;

    const int columns = 4, rows = (n + columns - 1) / columns;

    for (int i = 0; i < n; ++i)
    {
        auto cell = juce::Rectangle<int>(b.getX() + (i % columns) * b.getWidth() / columns,
            b.getY() + (i / columns) * b.getHeight() / rows,
            b.getWidth() / columns, b.getHeight() / rows);
        knobs[(size_t)i]->setBounds(cell.reduced(3));
    }
}

MultiFXAudioProcessorEditor::MultiFXAudioProcessorEditor(MultiFXAudioProcessor& processor) :AudioProcessorEditor(&processor), processorRef(processor), background(img(BinaryData::background_png, BinaryData::background_pngSize)), granularPage(processor), distortionSection(processor.apvts, "DISTORTION", "distBypass", { {"distMix","Mix"} }), cassetteSection(processor.apvts, "CASSETTE FX", "cassetteEnabled", { {"distCassette","Amount"} }), pitchSection(processor.apvts, "FREQUENCY SHIFT", "pitchBypass", { {"freqShiftHz","Shift Hz"} }), wowSection(processor.apvts, "WOW FX", "wowEnabled", { {"wowAmount","Amount"},{"wowSpeed","Speed"} }), chorusSection(processor.apvts, "CHORUS", "chorusBypass", { {"chorusMix","Mix"} }), flangerSection(processor.apvts, "FLANGER", "flangerEnabled", { {"flangerMix","Mix"} }), reverbSection(processor.apvts, "REVERB", "reverbBypass", { {"reverbCrossover","Crossover"},{"reverbSize","Size"},{"reverbDamping","Damp"},{"reverbLowLevel","Low"},{"reverbHighLevel","High"},{"reverbMix","Mix"} }), squeezeSection(processor.apvts, "LO-FI PUNCHER", "squeezeBypass", { {"squeezeAmount","Amount"},{"squeezeCutoff","Cutoff"},{"squeezeMix","Mix"} }, 2, "squeezeNoiseOnSound", "NOISE ON SOUND"), ghostSection(processor.apvts, "GHOST VOICE", "ghostEnabled", { {"ghostPitch","Pitch"},{"ghostMix","Mix"} }, 2), driftSection(processor.apvts, "RANDOM DRIFT", "driftEnabled", { {"driftAmount","Amount"},{"driftRate","Rate"} }, 2), stutterSection(processor.apvts, "STUTTER", "stutterEnabled", { {"stutterRate","Rate"},{"stutterLength","Length"},{"stutterMix","Mix"} }, 2) { setLookAndFeel(&freakLookAndFeel);horrorButton.setButtonText("HORROR LAB");fxButton.setButtonText("FX");granularButton.setButtonText("GRANULAR");horrorButton.onClick = [this] {showPage(Page::horror);};fxButton.onClick = [this] {showPage(Page::fx);};granularButton.onClick = [this] {showPage(Page::granular);};addAndMakeVisible(horrorButton);addAndMakeVisible(fxButton);addAndMakeVisible(granularButton);addAndMakeVisible(granularPage);for (auto* s : { &distortionSection,&cassetteSection,&pitchSection,&wowSection,&chorusSection,&flangerSection,&reverbSection,&squeezeSection,&ghostSection,&driftSection,&stutterSection })addAndMakeVisible(s);showPage(Page::fx);setSize(840, 440);setResizable(true, true);setResizeLimits(760, 460, 2400, 1400);startTimerHz(30); }
MultiFXAudioProcessorEditor::~MultiFXAudioProcessorEditor() { stopTimer();setLookAndFeel(nullptr); }
void MultiFXAudioProcessorEditor::showPage(Page p) { currentPage = p;horrorButton.setToggleState(p == Page::horror, juce::dontSendNotification);fxButton.setToggleState(p == Page::fx, juce::dontSendNotification);granularButton.setToggleState(p == Page::granular, juce::dontSendNotification);const bool h = p == Page::horror, f = p == Page::fx;for (auto* s : { &distortionSection,&cassetteSection,&chorusSection,&flangerSection,&reverbSection,&squeezeSection })s->setVisible(f);for (auto* s : { &pitchSection,&wowSection,&ghostSection,&driftSection,&stutterSection })s->setVisible(h);granularPage.setVisible(p == Page::granular);if (p == Page::granular)granularPage.refresh();resized();repaint(); }
void MultiFXAudioProcessorEditor::timerCallback() { animationPhase += 0.055f;const float p = 0.5f + 0.5f * std::sin(animationPhase);for (auto* s : { &distortionSection,&cassetteSection,&pitchSection,&wowSection,&chorusSection,&flangerSection,&reverbSection,&squeezeSection,&ghostSection,&driftSection,&stutterSection })if (s->isVisible())s->setPulse(p);freakLookAndFeel.setAnimationPhase(animationPhase);freakLookAndFeel.setPointerJitter((cassetteSection.isEffectEnabled() || wowSection.isEffectEnabled()) ? 0.34f : 0.0f);if (granularPage.isVisible())granularPage.refresh();repaint(); }
void MultiFXAudioProcessorEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xff09070a));if (background.isValid()) { g.setOpacity(0.32f);g.drawImageWithin(background, 0, 0, getWidth(), getHeight(), juce::RectanglePlacement::fillDestination, false); }g.setColour(juce::Colours::black.withAlpha(0.30f));g.fillAll();const int o = ((int)(animationPhase * 97.0f)) % 7;g.setColour(juce::Colour(0xffd1b6ad).withAlpha(0.022f));for (int y = o;y < getHeight();y += 7)g.drawHorizontalLine(y, 0.0f, (float)getWidth());auto c = getLocalBounds().reduced(12).removeFromTop(36).removeFromRight(210);g.setFont(juce::Font(10.5f, juce::Font::bold));g.setColour(juce::Colour(0xffc9aaa4).withAlpha(0.72f));g.drawFittedText("BERSERK VST MADE BY ANTECC", c, juce::Justification::centredRight, 1, 0.72f); }
void MultiFXAudioProcessorEditor::resized() { auto a = getLocalBounds().reduced(12);auto h = a.removeFromTop(36);horrorButton.setBounds(h.removeFromLeft(112).reduced(0, 6));h.removeFromLeft(6);fxButton.setBounds(h.removeFromLeft(52).reduced(0, 6));h.removeFromLeft(6);granularButton.setBounds(h.removeFromLeft(92).reduced(0, 6));if (currentPage == Page::granular) { granularPage.setBounds(a);return; }constexpr int gap = 8;if (currentPage == Page::horror) { const int w = (a.getWidth() - 4 * gap) / 5;for (auto* s : { &pitchSection,&wowSection,&ghostSection,&driftSection }) { s->setBounds(a.removeFromLeft(w));a.removeFromLeft(gap); }stutterSection.setBounds(a);return; }const int w = (a.getWidth() - 3 * gap) / 4;auto split = [gap](juce::Rectangle<int>c, EffectSection& t, EffectSection& b) {t.setBounds(c.removeFromTop((c.getHeight() - gap) / 2));c.removeFromTop(gap);b.setBounds(c);};auto d = a.removeFromLeft(w);a.removeFromLeft(gap);auto cf = a.removeFromLeft(w);a.removeFromLeft(gap);auto r = a.removeFromLeft(w);a.removeFromLeft(gap);split(d, distortionSection, cassetteSection);split(cf, chorusSection, flangerSection);reverbSection.setBounds(r);squeezeSection.setBounds(a); }
