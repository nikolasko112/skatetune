#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "TrickData.h"
#include "../PluginProcessor.h"

/**
    The "stage": a little skatepark scene with a skater who lands a brand
    new trick every time the processor reports a retune-speed peak. Purely
    cosmetic - it only ever reads the processor's atomics, never touches
    audio-thread state, and never blocks.
*/
class SkaterView final : public juce::Component,
                          private juce::Timer
{
public:
    explicit SkaterView (SkateTuneAudioProcessor& processorToUse);
    ~SkaterView() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void triggerNewTrick();

    struct Pose
    {
        float bodyLean = 0.0f;      // degrees
        float armAngle1 = 0.0f;     // degrees, front arm
        float armAngle2 = 0.0f;     // degrees, back arm
        float legAngle1 = 0.0f;     // degrees, front leg
        float legAngle2 = 0.0f;     // degrees, back leg
        float boardRotation = 0.0f; // degrees
        float bodyBounceY = 0.0f;   // normalised, - is up
        float boardLift = 0.0f;     // normalised, how far the board is off the ground
    };

    static Pose computePose (skatetune::TrickPose pose, float amount, float idlePhase);

    void drawBackground (juce::Graphics& g, juce::Rectangle<float> area) const;
    void drawRamp (juce::Graphics& g, juce::Rectangle<float> area) const;
    void drawSkater (juce::Graphics& g, juce::Rectangle<float> area, const Pose& pose) const;
    void drawHud (juce::Graphics& g, juce::Rectangle<float> area) const;

    SkateTuneAudioProcessor& processor;

    int lastSeenTrickCount = 0;
    int currentTrickIndex = -1;
    double triggerTimeSeconds = -1000.0;
    double startTimeSeconds = 0.0;

    int combo = 0;
    double lastTrickWallClock = -1000.0;

    float displayedCorrection = 0.0f; // smoothed for the on-screen meter only

    juce::Random random;

    static constexpr double trickAnimSeconds = 0.8;
    static constexpr double bannerSeconds = 1.1;
    static constexpr double comboWindowSeconds = 2.5;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkaterView)
};
