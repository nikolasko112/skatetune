#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SkateTuneAudioProcessor::SkateTuneAudioProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    keyRootParam       = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramKeyRoot));
    scaleParam         = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramScale));
    retuneSpeedParam   = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (paramRetuneSpeed));
    mixParam           = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (paramMix));
    outputGainParam    = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (paramOutputGain));
    humanizeParam      = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (paramHumanize));
    inputTypeParam     = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramInputType));
    trackingParam      = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (paramTracking));
    formantParam       = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (paramFormant));
    transposeParam     = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (paramTranspose));
    modernClassicParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramModernClassic));

    jassert (keyRootParam != nullptr);
    jassert (scaleParam != nullptr);
    jassert (retuneSpeedParam != nullptr);
    jassert (mixParam != nullptr);
    jassert (outputGainParam != nullptr);
    jassert (humanizeParam != nullptr);
    jassert (inputTypeParam != nullptr);
    jassert (trackingParam != nullptr);
    jassert (formantParam != nullptr);
    jassert (transposeParam != nullptr);
    jassert (modernClassicParam != nullptr);
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

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { paramHumanize, 1 }, "Humanize",
        juce::NormalisableRange<float> (0.0f, 100.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // Order must match SkateTuneAudioProcessor::InputType.
    juce::StringArray inputTypeNames { "Soprano/Alto", "Tenor/Baritone", "Bass", "Instrument (Wide)" };
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { paramInputType, 1 }, "Input Type", inputTypeNames, 1));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { paramTracking, 1 }, "Tracking",
        juce::NormalisableRange<float> (0.0f, 100.0f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { paramFormant, 1 }, "Formant",
        juce::NormalisableRange<float> (-100.0f, 100.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { paramTranspose, 1 }, "Transpose",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("st")));

    juce::StringArray modernClassicNames { "Modern", "Classic" };
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { paramModernClassic, 1 }, "Mode", modernClassicNames, 0));

    return { params.begin(), params.end() };
}

//==============================================================================
void SkateTuneAudioProcessor::frequencyRangeForInputType (InputType type, float& minHzOut, float& maxHzOut)
{
    // Deliberately generous, overlapping bands rather than tight ones: the
    // cost of a slightly-too-wide search range is a rare octave error, but
    // the cost of a too-narrow one is losing real signal outright, so each
    // band leans wide.
    switch (type)
    {
        case InputType::SopranoAlto:   minHzOut = 160.0f; maxHzOut = 1200.0f; break;
        case InputType::TenorBaritone: minHzOut = 80.0f;  maxHzOut = 1000.0f; break;
        case InputType::Bass:          minHzOut = 50.0f;  maxHzOut = 400.0f;  break;
        case InputType::Instrument:
        default:                       minHzOut = 40.0f;  maxHzOut = 2000.0f; break;
    }
}

//==============================================================================
void SkateTuneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    pitchTracker.prepare (sampleRate);

    retuneSmoother.prepare (sampleRate);
    retuneSmoother.reset (0.0);

    trackingSmoother.prepare (sampleRate);
    trackingSmoother.reset (220.0); // A3-ish; harmless starting guess, overwritten the instant input is voiced

    humanizeController.prepare (sampleRate);

    trickDetector.prepare (sampleRate);
    trickDetector.setThresholds (0.15, 0.60);
    trickDetector.setCooldownSeconds (0.35);

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    shifters.clear();
    shifters.resize ((size_t) numChannels);
    for (auto& s : shifters)
        s.prepare (sampleRate);

    formantTilts.clear();
    formantTilts.resize ((size_t) numChannels);
    for (auto& f : formantTilts)
        f.prepare (sampleRate);

    rawCorrectionSemitones = 0.0;
    rawTrackedFreqHold = 220.0;
    voicedHold = false;
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

    // Key/scale/retune-speed and the rest of these are all UI-rate
    // controls; re-reading them every block is negligible cost and keeps
    // this lock-free and listener-free.
    scaleQuantizer.setKeyRoot (keyRootParam->getIndex());
    scaleQuantizer.setScale (static_cast<skatetune::ScaleType> (scaleParam->getIndex()));

    const bool isClassicMode = modernClassicParam->getIndex() == 1;

    // Classic forgoes Humanize entirely - closer to the hard-edged,
    // always-at-the-dialled-in-speed character of first-generation
    // pitch-correction, where Retune Speed was the only speed there was.
    humanizeController.setAmount (isClassicMode ? 0.0 : (double) humanizeParam->get());

    trackingSmoother.setRetuneSpeed ((double) trackingParam->get());

    float inputMinHz = 70.0f, inputMaxHz = 1000.0f;
    frequencyRangeForInputType (static_cast<InputType> (inputTypeParam->getIndex()), inputMinHz, inputMaxHz);
    pitchTracker.setFrequencyRange (inputMinHz, inputMaxHz);

    const double transposeSemitones = (double) transposeParam->get();
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
    if ((int) formantTilts.size() < numChannels)
    {
        auto sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
        auto oldSize = formantTilts.size();
        formantTilts.resize ((size_t) numChannels);
        for (size_t i = oldSize; i < formantTilts.size(); ++i)
            formantTilts[i].prepare (sr);
    }

    for (auto& s : shifters)
        s.setInterpolationQuality (! isClassicMode);
    for (auto& f : formantTilts)
        f.setAmount ((double) formantParam->get());

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
                // A genuinely new onset (nothing voiced the previous hop) has
                // to be judged BEFORE voicedHold is overwritten below.
                const bool isNewOnset = ! voicedHold;
                rawTrackedFreqHold = (double) pitchTracker.getFrequencyHz();
                humanizeController.onNewPitchHop (skatetune::ScaleQuantizer::freqToMidi (rawTrackedFreqHold));

                // trackingSmoother's whole job is to iron out hop-to-hop
                // JITTER while a note is held, not to smear the moment a
                // brand new note starts: left alone, it starts every
                // plugin instance (and, worse, every silence-then-sing
                // onset) climbing from its cold current=0.0 up to the real
                // frequency at the Tracking-knob's time constant - up to
                // 250ms, and a one-pole filter needs several time constants
                // to actually arrive - so for the better part of a second
                // the scale quantizer downstream is fed a continuum of
                // WRONG intermediate frequencies, each snapping to whatever
                // scale note happens to be nearest THAT wrong value. That
                // is a real, confirmed-by-test correctness bug, not a
                // subtle quality tradeoff: it was caught by actually running
                // a sustained note through the full chain and watching the
                // correction amount keep drifting for over a second instead
                // of settling - see dsp_standalone_test.cpp's
                // testTrackingSmootherSnapsOnNewOnset(). Snapping the
                // smoother straight to the new frequency on a real onset
                // (exactly the same "is this a note change or a hold"
                // distinction HumanizeController already makes via
                // onNewPitchHop) keeps the Tracking knob's smoothing for
                // what it's actually for - jitter within a held note -
                // without ever delaying which note is being targeted.
                if (isNewOnset)
                    trackingSmoother.reset (rawTrackedFreqHold);

                voicedHold = true;
            }
            else
            {
                voicedHold = false;
            }
        }

        // Tracking smoothing on the frequency estimate itself, ahead of the
        // quantizer - called every sample (not just at hop boundaries) with
        // its target held between hops, the same pattern RetuneSmoother
        // relies on below: its one-pole coefficient assumes it's driven at
        // the full sample rate, so calling it any less often would silently
        // make the Tracking knob's time constants far slower than they read.
        const double smoothedFreqHz = trackingSmoother.process (rawTrackedFreqHold);

        if (voicedHold)
        {
            double targetFreqUnused = 0.0;
            scaleQuantizer.process (smoothedFreqHz, targetFreqUnused, rawCorrectionSemitones);
        }
        else
        {
            rawCorrectionSemitones = 0.0;
        }

        // Humanize adjusts the *speed* fed to RetuneSmoother, sample by
        // sample, not the correction amount itself - same reasoning as
        // above applies to calling it at full sample rate.
        const double effectiveRetuneSpeed = humanizeController.getEffectiveRetuneSpeed ((double) retuneSpeedParam->get());
        retuneSmoother.setRetuneSpeed (effectiveRetuneSpeed);

        const double smoothed = retuneSmoother.process (rawCorrectionSemitones);

        // Transpose is a deliberate, static shift the user dialled in, not
        // something being corrected - so it's added after smoothing (always
        // instant) and left out of the meter/trick-trigger below, which are
        // about how hard the actual *correction* is working.
        const double finalSemitones = smoothed + transposeSemitones;
        const double ratio = std::pow (2.0, finalSemitones / 12.0);

        correctionMeter.store ((float) std::abs (smoothed), std::memory_order_relaxed);

        if (trickDetector.update (std::abs (smoothed)))
            trickEventCounter.fetch_add (1, std::memory_order_relaxed);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& shifter = shifters[(size_t) ch];
            shifter.setRatio (ratio);

            const float dry      = buffer.getSample (ch, i);
            const float shifted  = shifter.processSample (dry);
            const float wet      = formantTilts[(size_t) ch].processSample (shifted);
            const float mixed    = (float) (dry * (1.0 - mixAmount) + (double) wet * mixAmount);

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
