#include "PluginEditor.h"

//==============================================================================
SkateTuneAudioProcessorEditor::SkateTuneAudioProcessorEditor (SkateTuneAudioProcessor& p)
    : AudioProcessorEditor (p), processorRef (p), skaterView (p)
{
    addAndMakeVisible (skaterView);

    setupCombo (keyBox, keyLabel, "Key");
    const juce::StringArray keyNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    keyBox.addItemList (keyNames, 1);

    setupCombo (scaleBox, scaleLabel, "Scale");
    const juce::StringArray scaleNames { "Chromatic", "Major", "Minor", "Major Pentatonic", "Minor Pentatonic" };
    scaleBox.addItemList (scaleNames, 1);

    setupCombo (inputTypeBox, inputTypeLabel, "Input Type");
    const juce::StringArray inputTypeNames { "Soprano/Alto", "Tenor/Baritone", "Bass", "Instrument (Wide)" };
    inputTypeBox.addItemList (inputTypeNames, 1);

    setupCombo (modeBox, modeLabel, "Mode");
    const juce::StringArray modeNames { "Modern", "Classic" };
    modeBox.addItemList (modeNames, 1);

    setupRotary (retuneSpeedSlider, retuneSpeedLabel, "Retune Speed");
    setupRotary (humanizeSlider, humanizeLabel, "Humanize");
    setupRotary (trackingSlider, trackingLabel, "Tracking");
    setupRotary (transposeSlider, transposeLabel, "Transpose");
    setupRotary (formantSlider, formantLabel, "Formant");
    setupRotary (mixSlider, mixLabel, "Mix");
    setupRotary (outputGainSlider, outputGainLabel, "Output Gain");

    auto& apvts = processorRef.apvts;
    keyAttachment          = std::make_unique<ComboAttachment>  (apvts, SkateTuneAudioProcessor::paramKeyRoot, keyBox);
    scaleAttachment        = std::make_unique<ComboAttachment>  (apvts, SkateTuneAudioProcessor::paramScale, scaleBox);
    inputTypeAttachment    = std::make_unique<ComboAttachment>  (apvts, SkateTuneAudioProcessor::paramInputType, inputTypeBox);
    modeAttachment         = std::make_unique<ComboAttachment>  (apvts, SkateTuneAudioProcessor::paramModernClassic, modeBox);
    retuneSpeedAttachment  = std::make_unique<SliderAttachment> (apvts, SkateTuneAudioProcessor::paramRetuneSpeed, retuneSpeedSlider);
    humanizeAttachment     = std::make_unique<SliderAttachment> (apvts, SkateTuneAudioProcessor::paramHumanize, humanizeSlider);
    trackingAttachment     = std::make_unique<SliderAttachment> (apvts, SkateTuneAudioProcessor::paramTracking, trackingSlider);
    transposeAttachment    = std::make_unique<SliderAttachment> (apvts, SkateTuneAudioProcessor::paramTranspose, transposeSlider);
    formantAttachment      = std::make_unique<SliderAttachment> (apvts, SkateTuneAudioProcessor::paramFormant, formantSlider);
    mixAttachment          = std::make_unique<SliderAttachment> (apvts, SkateTuneAudioProcessor::paramMix, mixSlider);
    outputGainAttachment   = std::make_unique<SliderAttachment> (apvts, SkateTuneAudioProcessor::paramOutputGain, outputGainSlider);

    setSize (820, 620);
}

SkateTuneAudioProcessorEditor::~SkateTuneAudioProcessorEditor() = default;

//==============================================================================
juce::Slider& SkateTuneAudioProcessorEditor::setupRotary (juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffff5d8f));
    slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xfffff3b0));
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (label);

    return slider;
}

juce::ComboBox& SkateTuneAudioProcessorEditor::setupCombo (juce::ComboBox& box, juce::Label& label, const juce::String& text)
{
    addAndMakeVisible (box);

    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (label);

    return box;
}

//==============================================================================
void SkateTuneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff17122a));
}

void SkateTuneAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    skaterView.setBounds (bounds.removeFromTop ((int) ((float) bounds.getHeight() * 0.58f)));

    auto controls = bounds.reduced (12);
    const int rowHeight = controls.getHeight() / 2;

    auto layoutRow = [] (juce::Rectangle<int> row, std::initializer_list<std::pair<juce::Component*, juce::Label*>> items)
    {
        const int colWidth = row.getWidth() / (int) items.size();
        int index = 0;
        for (auto& item : items)
        {
            auto col = row.withX (row.getX() + colWidth * index).withWidth (colWidth);
            item.second->setBounds (col.removeFromTop (18));
            item.first->setBounds (col.reduced (4));
            ++index;
        }
    };

    auto topRow    = controls.removeFromTop (rowHeight);
    auto bottomRow = controls;

    // Choice-style controls on top...
    layoutRow (topRow, {
        { &keyBox, &keyLabel },
        { &scaleBox, &scaleLabel },
        { &inputTypeBox, &inputTypeLabel },
        { &modeBox, &modeLabel }
    });

    // ...continuous knobs on the bottom, roughly in the order a correction
    // conceptually flows: how the pitch is read and corrected, then a
    // static offset, then tone, then the final dry/wet and level.
    layoutRow (bottomRow, {
        { &retuneSpeedSlider, &retuneSpeedLabel },
        { &humanizeSlider, &humanizeLabel },
        { &trackingSlider, &trackingLabel },
        { &transposeSlider, &transposeLabel },
        { &formantSlider, &formantLabel },
        { &mixSlider, &mixLabel },
        { &outputGainSlider, &outputGainLabel }
    });
}
