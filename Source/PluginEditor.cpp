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

    setupRotary (retuneSpeedSlider, retuneSpeedLabel, "Retune Speed");
    setupRotary (mixSlider, mixLabel, "Mix");
    setupRotary (outputGainSlider, outputGainLabel, "Output Gain");

    auto& apvts = processorRef.apvts;
    keyAttachment          = std::make_unique<ComboAttachment>  (apvts, SkateTuneAudioProcessor::paramKeyRoot, keyBox);
    scaleAttachment        = std::make_unique<ComboAttachment>  (apvts, SkateTuneAudioProcessor::paramScale, scaleBox);
    retuneSpeedAttachment  = std::make_unique<SliderAttachment> (apvts, SkateTuneAudioProcessor::paramRetuneSpeed, retuneSpeedSlider);
    mixAttachment          = std::make_unique<SliderAttachment> (apvts, SkateTuneAudioProcessor::paramMix, mixSlider);
    outputGainAttachment   = std::make_unique<SliderAttachment> (apvts, SkateTuneAudioProcessor::paramOutputGain, outputGainSlider);

    setSize (600, 460);
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
    skaterView.setBounds (bounds.removeFromTop ((int) ((float) bounds.getHeight() * 0.68f)));

    auto controls = bounds.reduced (12);
    constexpr int numControls = 5;
    const int colWidth = controls.getWidth() / numControls;

    auto layoutOne = [&controls, colWidth] (juce::Component& comp, juce::Label& label, int index)
    {
        auto col = controls.withX (controls.getX() + colWidth * index).withWidth (colWidth);
        label.setBounds (col.removeFromTop (18));
        comp.setBounds (col.reduced (4));
    };

    layoutOne (keyBox, keyLabel, 0);
    layoutOne (scaleBox, scaleLabel, 1);
    layoutOne (retuneSpeedSlider, retuneSpeedLabel, 2);
    layoutOne (mixSlider, mixLabel, 3);
    layoutOne (outputGainSlider, outputGainLabel, 4);
}
