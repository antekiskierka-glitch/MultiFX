#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include "PluginProcessor.h"
#include "FreakLookAndFeel.h"

struct KnobWithLabel : juce::Component
{
    KnobWithLabel(juce::AudioProcessorValueTreeState& a, const juce::String& id, const juce::String& text)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setMouseDragSensitivity(350);
        slider.setComponentID(id);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 20);
        slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxTextColourId, FreakLookAndFeel::cream);
        slider.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        slider.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
        slider.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        slider.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        slider.setColour(juce::TextEditor::textColourId, FreakLookAndFeel::cream);
        addAndMakeVisible(slider);
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(12.0f));
        addAndMakeVisible(label);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(a, id, slider);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(1);
        label.setBounds(b.removeFromTop(17));
        slider.setBounds(b);
    }

    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

struct IlluminatedButton : juce::TextButton
{
    IlluminatedButton() { setClickingTogglesState(true); }

    void paintButton(juce::Graphics& g, bool over, bool down) override
    {
        auto b = getLocalBounds().toFloat().reduced(1.0f);
        const bool off = !isEnabled();
        auto fill = getToggleState() ? juce::Colour(0xff6b3038) : juce::Colour(0xff211d20);
        if (over && !off) fill = fill.brighter(0.10f);
        if (down && !off) fill = fill.darker(0.14f);
        g.setColour(juce::Colour(0xff0c0a0c));
        g.fillRoundedRectangle(b.translated(1.0f, 1.5f), 3.0f);
        g.setColour(off ? fill.withMultipliedSaturation(0.35f).darker(0.35f) : fill);
        g.fillRoundedRectangle(b, 3.0f);
        auto outline = getToggleState() ? juce::Colour(0xffb8aa8d) : juce::Colour(0xff655c5e);
        g.setColour(off ? outline.withAlpha(0.35f) : outline);
        g.drawRoundedRectangle(b, 3.0f, 1.0f);
        auto text = getToggleState() ? juce::Colour(0xffeee0bd) : juce::Colour(0xffa79a82);
        g.setColour(off ? text.withAlpha(0.40f) : text);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawFittedText(getButtonText(), getLocalBounds().reduced(3), juce::Justification::centred, 1);
    }
};

struct EffectSection : juce::Component
{
    static juce::Colour panelFillColour() noexcept { return juce::Colour(0x245b1e29); }
    static juce::Colour panelBorderColour() noexcept { return juce::Colour(0xb08d4a55); }
    static juce::Colour panelHighlightColour() noexcept { return juce::Colour(0x4dcc7b86); }

    EffectSection(juce::AudioProcessorValueTreeState&, const juce::String&, const juce::String&,
        std::vector<std::pair<juce::String, juce::String>>, int = 3,
        const juce::String& extraId = {}, const juce::String& extraText = {});

    void paint(juce::Graphics&) override;
    void resized() override;
    void setPulse(float amount) noexcept { pulse = amount; }
    bool isEffectEnabled() const noexcept { return enable.getToggleState(); }

    int maxColumns;
    float pulse = 0.0f;
    juce::GroupComponent group;
    juce::ToggleButton enable;
    IlluminatedButton extra;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttachment, extraAttachment;
    std::vector<std::unique_ptr<KnobWithLabel>> knobs;
};

// Peak-envelope preview of the live circular buffer or the loaded file, with an
// animated playhead marking the current grain read position.
struct WaveformDisplay : juce::Component
{
    void paint(juce::Graphics&) override;
    void setPeaks(const std::vector<float>& newPeaks) { peaks = newPeaks; }

    // Negative hides the marker. phase drives the pulse so the playhead reads as
    // moving even while parked on sustained material.
    void setPlayhead(float normalisedPosition, float phase)
    {
        playhead = normalisedPosition;
        pulsePhase = phase;
    }

    std::vector<float> peaks;
    float playhead = -1.0f;
    float pulsePhase = 0.0f;
};

struct GranularPage : juce::Component, juce::FileDragAndDropTarget
{
    explicit GranularPage(MultiFXAudioProcessor&);
    ~GranularPage() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Called from the editor's timer: refreshes the preview, playhead and the
    // enabled/disabled state of the file-only controls.
    void refresh(float animationPhase);

    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;

    MultiFXAudioProcessor& processorRef;
    juce::ToggleButton enable;
    IlluminatedButton liveButton, fileButton, loadButton, stopButton, exportButton;
    juce::Label status;
    WaveformDisplay waveform;
    std::vector<std::unique_ptr<KnobWithLabel>> knobs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttachment;
    std::unique_ptr<juce::FileChooser> chooser;

private:
    // Renders and writes the WAV on a background thread so neither the audio
    // callback nor the message thread stalls on a long export. It holds the
    // processor (which outlives the editor) plus a SafePointer for the UI hop,
    // so a closing editor can never leave the render on a dangling page.
    struct ExportJob : juce::Thread
    {
        ExportJob(MultiFXAudioProcessor& p, GranularPage& page, const juce::File& d)
            : juce::Thread("MultiFX Granular Export"), processor(p), owner(&page), destination(d) {}

        void run() override;

        MultiFXAudioProcessor& processor;
        juce::Component::SafePointer<GranularPage> owner;
        juce::File destination;
    };

    void setSourceMode(bool useFile);
    void loadFile(const juce::File&);
    void beginExport();
    void exportFinished(bool succeeded, const juce::String& error, const juce::File&);
    void updateTransportState();

    juce::AudioParameterChoice* sourceParameter = nullptr;
    std::vector<float> previewScratch;
    std::unique_ptr<ExportJob> exportJob;
    juce::String transportMessage;
};

class MultiFXAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MultiFXAudioProcessorEditor(MultiFXAudioProcessor&);
    ~MultiFXAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    enum class Page { horror, fx, granular };
    void showPage(Page);
    void timerCallback() override;

    MultiFXAudioProcessor& processorRef;
    FreakLookAndFeel freakLookAndFeel;
    juce::Image background;
    IlluminatedButton horrorButton, fxButton, granularButton;
    GranularPage granularPage;
    Page currentPage = Page::fx;
    float animationPhase = 0.0f;
    EffectSection distortionSection, cassetteSection, pitchSection, wowSection, chorusSection, flangerSection,
        reverbSection, squeezeSection, ghostSection, driftSection, stutterSection;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiFXAudioProcessorEditor)
};
