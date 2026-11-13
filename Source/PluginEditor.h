#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "gui/SkaterView.h"

class SkateTuneAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SkateTuneAudioProcessorEditor (SkateTuneAudioProcessor&);
    ~SkateTuneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider&   setupRotary (juce::Slider& slider, juce::Label& label, const juce::String& text);
    juce::ComboBox& setupCombo  (juce::ComboBox& box, juce::Label& label, const juce::String& text);

    SkateTuneAudioProcessor& processorRef;

    SkaterView skaterView;

    juce::ComboBox keyBox, scaleBox, inputTypeBox, modeBox;
    juce::Slider   retuneSpeedSlider, mixSlider, outputGainSlider;
    juce::Slider   humanizeSlider, trackingSlider, formantSlider, transposeSlider;

    juce::Label keyLabel, scaleLabel, retuneSpeedLabel, mixLabel, outputGainLabel;
    juce::Label inputTypeLabel, modeLabel, humanizeLabel, trackingLabel, formantLabel, transposeLabel;

    using SliderAttachment  = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment   = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<ComboAttachment>  keyAttachment, scaleAttachment, inputTypeAttachment, modeAttachment;
    std::unique_ptr<SliderAttachment> retuneSpeedAttachment, mixAttachment, outputGainAttachment;
    std::unique_ptr<SliderAttachment> humanizeAttachment, trackingAttachment, formantAttachment, transposeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkateTuneAudioProcessorEditor)
};
