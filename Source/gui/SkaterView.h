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

        // Three independent board rotations, each faked with a different
        // 2D trick so a side-on silhouette can still tell them apart:
        float boardFlipDeg  = 0.0f; // about the long (nose-tail) axis -> squashes the drawn HEIGHT, swaps to the underside colour past 180
        float boardShoveDeg = 0.0f; // about the vertical axis         -> squashes the drawn WIDTH
        float boardTiltDeg  = 0.0f; // an ordinary in-plane tilt (grind pitch, manual, slide) - no squash, just rotated

        // Big aerial spins (900, McTwist): the whole skater+board silhouette
        // rotates together as one rigid unit around the hip, unlike the
        // board-only axes above. drawSkater() applies this as a single
        // extra transform wrapped around everything else it draws.
        float wholeBodySpinDeg = 0.0f;

        float bodyBounceY = 0.0f;   // normalised, - is up
        float boardLift   = 0.0f;   // normalised, how far the board is off the ground

        bool  showRail          = false; // Grind: draw a rail/ledge under the skater
        float sparkAmount       = 0.0f;  // Grind/Slide: 0..1 intensity for contact sparks
        float groundReachFoot2  = 0.0f;  // Boneless: 0..1, how far the back foot reaches down to the ground
        float boardAnchorBlend  = 0.5f;  // 0 = board stays under the front foot, 1 = under the back foot, 0.5 = midpoint (the normal riding stance)
    };

    static Pose computePose (const skatetune::Trick& trick, float phase01, float idlePhase);

    void drawBackground (juce::Graphics& g, juce::Rectangle<float> area) const;
    void drawRamp (juce::Graphics& g, juce::Rectangle<float> area) const;
    void drawSkater (juce::Graphics& g, juce::Rectangle<float> area, const Pose& pose) const;
    void drawRailAndSparks (juce::Graphics& g, juce::Rectangle<float> area, const Pose& pose) const;
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
