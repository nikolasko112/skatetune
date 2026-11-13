#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SkateTuneAudioProcessor::SkateTuneAudioProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    keyRootParam     = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramKeyRoot));
    scaleParam       = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramScale));
    retuneSpeedParam = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (paramRetuneSpeed));
    mixParam         = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (paramMix));
    outputGainParam  = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (paramOutputGain));

    jassert (keyRootParam != nullptr);
    jassert (scaleParam != nullptr);
    jassert (retuneSpeedParam != nullptr);
    jassert (mixParam != nullptr);
    jassert (outputGainParam != nullptr);
}

SkateTuneAudioProcessor::~SkateTuneAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SkateTuneAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    juce::StringArray keyNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { paramKeyRoot, 1 }, "Key", keyNames, 0));

    // Order must match skatetune::ScaleType (Chromatic, Major, Minor, MajorPentatonic, MinorPentatonic).
    juce::StringArray scaleNames { "Chromatic", "Major", "Minor", "Major Pentatonic", "Minor Pentatonic" };
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { paramScale, 1 }, "Scale", scaleNames, 1));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { paramRetuneSpeed, 1 }, "Retune Speed",
        juce::NormalisableRange<float> (0.0f, 100.0f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { paramMix, 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { paramOutputGain, 1 }, "Output Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return { params.begin(), params.end() };
}

//==============================================================================
void SkateTuneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    pitchTracker.prepare (sampleRate);

    retuneSmoother.prepare (sampleRate);
    retuneSmoother.reset (0.0);

    trickDetector.prepare (sampleRate);
    trickDetector.setThresholds (0.15, 0.60);
    trickDetector.setCooldownSeconds (0.35);

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    shifters.clear();
    shifters.resize ((size_t) numChannels);
    for (auto& s : shifters)
        s.prepare (sampleRate);

    rawCorrectionSemitones = 0.0;
    trickEventCounter.store (0);
    correctionMeter.store (0.0f);
}

void SkateTuneAudioProcessor::releaseResources()
{
}

bool SkateTuneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOutput = layouts.getMainOutputChannelSet();
    const auto mainInput  = layouts.getMainInputChannelSet();

    if (mainOutput != mainInput)
        return false;

    return mainOutput == juce::AudioChannelSet::mono()
        || mainOutput == juce::AudioChannelSet::stereo();
}

void SkateTuneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    // Key/scale/retune-speed are UI-rate controls; re-reading them every
    // block is negligible cost and keeps this lock-free and listener-free.
    scaleQuantizer.setKeyRoot (keyRootParam->getIndex());
    scaleQuantizer.setScale (static_cast<skatetune::ScaleType> (scaleParam->getIndex()));
    retuneSmoother.setRetuneSpeed ((double) retuneSpeedParam->get());

    const double mixAmount     = (double) mixParam->get() / 100.0;
    const float  outputGainLin = juce::Decibels::decibelsToGain (outputGainParam->get());

    if ((int) shifters.size() < numChannels)
    {
        // Defensive: a host that changes channel count without calling
        // prepareToPlay again should still not crash or read out of bounds.
        auto sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
        auto oldSize = shifters.size();
        shifters.resize ((size_t) numChannels);
        for (size_t i = oldSize; i < shifters.size(); ++i)
            shifters[i].prepare (sr);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        double monoInput = 0.0;
        for (int ch = 0; ch < numChannels; ++ch)
            monoInput += (double) buffer.getSample (ch, i);
        if (numChannels > 0)
            monoInput /= (double) numChannels;

        if (pitchTracker.pushSample ((float) monoInput))
        {
            if (pitchTracker.isVoiced())
            {
                double targetFreqUnused = 0.0;
                scaleQuantizer.process ((double) pitchTracker.getFrequencyHz(),
                                         targetFreqUnused, rawCorrectionSemitones);
            }
            else
            {
                rawCorrectionSemitones = 0.0;
            }
        }

        const double smoothed = retuneSmoother.process (rawCorrectionSemitones);
        const double ratio = std::pow (2.0, smoothed / 12.0);

        correctionMeter.store ((float) std::abs (smoothed), std::memory_order_relaxed);

        if (trickDetector.update (std::abs (smoothed)))
            trickEventCounter.fetch_add (1, std::memory_order_relaxed);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& shifter = shifters[(size_t) ch];
            shifter.setRatio (ratio);

            const float dry   = buffer.getSample (ch, i);
            const float wet   = shifter.processSample (dry);
            const float mixed = (float) (dry * (1.0 - mixAmount) + (double) wet * mixAmount);

            buffer.setSample (ch, i, mixed * outputGainLin);
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* SkateTuneAudioProcessor::createEditor()
{
    return new SkateTuneAudioProcessorEditor (*this);
}

//==============================================================================
void SkateTuneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SkateTuneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin. This function definition must be
// in the correct namespace, and is required for every JUCE plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SkateTuneAudioProcessor();
}
