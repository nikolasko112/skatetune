#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "dsp/PitchTracker.h"
#include "dsp/ScaleQuantizer.h"
#include "dsp/RetuneSmoother.h"
#include "dsp/GranularPitchShifter.h"
#include "dsp/TrickEventDetector.h"
#include "dsp/HumanizeController.h"
#include "dsp/FormantTilt.h"

/**
    SkateTune - a pitch-correction ("autotune-style") effect. Whenever the
    retune has to work hard and fast to snap a note into key, that is
    treated as a "trick" event: the processor just counts these on an
    atomic so the editor's GUI (a little skater) can show a new trick
    without any locking or audio-thread-unsafe behaviour.
*/
class SkateTuneAudioProcessor final : public juce::AudioProcessor
{
public:
    SkateTuneAudioProcessor();
    ~SkateTuneAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    static constexpr const char* paramKeyRoot      = "keyRoot";
    static constexpr const char* paramScale        = "scale";
    static constexpr const char* paramRetuneSpeed  = "retuneSpeed";
    static constexpr const char* paramMix          = "mix";
    static constexpr const char* paramOutputGain   = "outputGain";
    static constexpr const char* paramHumanize     = "humanize";
    static constexpr const char* paramInputType    = "inputType";
    static constexpr const char* paramTracking     = "tracking";
    static constexpr const char* paramFormant      = "formant";
    static constexpr const char* paramTranspose    = "transpose";
    static constexpr const char* paramModernClassic = "modernClassic";

    // Order must match the choice list given to the paramInputType parameter.
    enum class InputType { SopranoAlto = 0, TenorBaritone, Bass, Instrument };

    juce::AudioProcessorValueTreeState apvts;

    // The audio thread only ever increments these; the editor's Timer polls
    // them on the message thread. No locks, no audio-thread-unsafe calls.
    std::atomic<int>   trickEventCounter { 0 };
    std::atomic<float> correctionMeter   { 0.0f }; // current |smoothed correction| in semitones

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioParameterChoice* keyRootParam      = nullptr;
    juce::AudioParameterChoice* scaleParam        = nullptr;
    juce::AudioParameterFloat*  retuneSpeedParam  = nullptr;
    juce::AudioParameterFloat*  mixParam          = nullptr;
    juce::AudioParameterFloat*  outputGainParam   = nullptr;
    juce::AudioParameterFloat*  humanizeParam     = nullptr;
    juce::AudioParameterChoice* inputTypeParam    = nullptr;
    juce::AudioParameterFloat*  trackingParam     = nullptr;
    juce::AudioParameterFloat*  formantParam      = nullptr;
    juce::AudioParameterFloat*  transposeParam    = nullptr;
    juce::AudioParameterChoice* modernClassicParam = nullptr;

    skatetune::PitchTracker       pitchTracker;
    skatetune::ScaleQuantizer     scaleQuantizer;
    skatetune::RetuneSmoother     retuneSmoother;
    skatetune::RetuneSmoother     trackingSmoother; // same one-pole math, repurposed to smooth the tracked frequency
    skatetune::HumanizeController humanizeController;
    skatetune::TrickEventDetector trickDetector;
    std::vector<skatetune::GranularPitchShifter> shifters;    // one per channel, same ratio
    std::vector<skatetune::FormantTilt>          formantTilts; // one per channel

    double rawCorrectionSemitones = 0.0; // held between pitch-tracker hops, updated every sample downstream
    double rawTrackedFreqHold = 220.0;   // last voiced frequency estimate, held between hops for trackingSmoother
    bool   voicedHold = false;           // held between hops: was the most recent hop voiced?

    static void frequencyRangeForInputType (InputType type, float& minHzOut, float& maxHzOut);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkateTuneAudioProcessor)
};
