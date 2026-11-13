#include "SkaterView.h"

using namespace skatetune;

//==============================================================================
SkaterView::SkaterView (SkateTuneAudioProcessor& processorToUse)
    : processor (processorToUse)
{
    startTimeSeconds = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    lastSeenTrickCount = processor.trickEventCounter.load (std::memory_order_relaxed);
    setOpaque (true);
    startTimerHz (40);
}

SkaterView::~SkaterView()
{
    stopTimer();
}

void SkaterView::resized()
{
}

//==============================================================================
void SkaterView::timerCallback()
{
    const int count = processor.trickEventCounter.load (std::memory_order_relaxed);
    if (count != lastSeenTrickCount)
    {
        lastSeenTrickCount = count;
        triggerNewTrick();
    }

    const float rawMeter = processor.correctionMeter.load (std::memory_order_relaxed);
    displayedCorrection += (rawMeter - displayedCorrection) * 0.25f;

    repaint();
}

void SkaterView::triggerNewTrick()
{
    const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;

    if (now - lastTrickWallClock <= comboWindowSeconds)
        ++combo;
    else
        combo = 1;
    lastTrickWallClock = now;

    const auto& tricks = allTricks();
    int next = currentTrickIndex;
    if (tricks.size() > 1)
    {
        while (next == currentTrickIndex)
            next = random.nextInt ((int) tricks.size());
    }
    else
    {
        next = 0;
    }
    currentTrickIndex = next;
    triggerTimeSeconds = now;
}

//==============================================================================
// A 0->1->0 trapezoid: ramps up over [0,riseEnd], HOLDS at 1 over
// [riseEnd,fallStart], then ramps back down over [fallStart,1]. Several
// tricks below are about holding a position (a grind's tilt, a manual's
// balance, a slide staying upside-down) rather than snapping through one
// quick bump the way sin(pi*t) does - this is that "hold" shape.
static float trapezoid (float t, float riseEnd, float fallStart)
{
    const float rise = juce::jlimit (0.0f, 1.0f, t / juce::jmax (0.0001f, riseEnd));
    const float fall = juce::jlimit (0.0f, 1.0f, (1.0f - t) / juce::jmax (0.0001f, 1.0f - fallStart));
    return juce::jmin (rise, fall);
}

SkaterView::Pose SkaterView::computePose (const Trick& trick, float phase01, float idlePhase)
{
    Pose p;

    if (phase01 < 0.0f)
    {
        p.armAngle1 = -15.0f + 5.0f * std::sin (idlePhase);
        p.armAngle2 =  15.0f - 5.0f * std::sin (idlePhase);
        p.legAngle1 = -8.0f;
        p.legAngle2 = 8.0f;
        p.bodyBounceY = 0.02f * std::sin (idlePhase);
        return p;
    }

    const float t = juce::jlimit (0.0f, 1.0f, phase01);
    const float bump = std::sin (juce::MathConstants<float>::pi * t); // 0 -> 1 -> 0, the "quick trick" shape

    switch (trick.pose)
    {
        case TrickPose::AirTuck:
        {
            // How "busy" the spin is drives a bit of extra hang-time/pop -
            // an Ollie (no spin at all) stays modest, a 360 Flip or
            // Impossible (lots of board rotation) or a 900/McTwist (lots of
            // whole-body rotation) gets visibly more air.
            const float spin = std::abs (trick.boardFlipTurns) + std::abs (trick.boardShoveTurns)
                              + std::abs (trick.wholeBodySpinTurns);
            const float bigness = juce::jlimit (0.0f, 1.0f, spin / 2.0f);
            const bool  isBigSpin = trick.wholeBodySpinTurns > 0.01f;

            p.bodyLean    = -15.0f * bump;
            p.armAngle1   = -20.0f - 50.0f * bump;
            // A real 900/McTwist is held in a grab, not a balancing flail -
            // the back hand reaches down toward the board and stays there,
            // rather than swinging wide the way the flip/shove tricks do.
            p.armAngle2   = isBigSpin ? (10.0f + 15.0f * bump)
                                       : (20.0f + 40.0f * bump);
            p.legAngle1   = -10.0f - 60.0f * bump;
            p.legAngle2   =  10.0f + 60.0f * bump;
            p.bodyBounceY = -(0.16f + 0.10f * bigness) * bump;
            p.boardLift   =  (0.20f + 0.14f * bigness) * bump;

            // The actual trick-telling rotations: a kickflip and a
            // heelflip spin the SAME board axis in opposite directions: a
            // shove-it spins a different axis entirely; a varial/hardflip/
            // 360-flip layer both at once. drawSkater() turns these two
            // angles into two independently-squashed axes so each reads
            // as a distinct rotation rather than one generic flat spin.
            p.boardFlipDeg  = trick.boardFlipTurns  * 360.0f * t;
            p.boardShoveDeg = trick.boardShoveTurns * 360.0f * t;

            // 900/McTwist: the whole skater+board silhouette spins together
            // as one rigid unit (drawSkater() applies this about the hip) -
            // unlike boardFlipDeg/boardShoveDeg above, which rotate the
            // board relative to planted feet.
            p.wholeBodySpinDeg = trick.wholeBodySpinTurns * 360.0f * t;
            break;
        }

        case TrickPose::Grind:
            p.bodyLean     = 10.0f * bump;
            p.armAngle1    = -40.0f * bump;
            p.armAngle2    =  30.0f * bump;
            p.legAngle1    = -70.0f * bump;
            p.legAngle2    = -50.0f * bump;
            // Ease into the grind's own held tilt (50-50 flat, 5-0
            // nose-up, Nosegrind nose-down) rather than snapping to it.
            p.boardTiltDeg = trick.grindTiltDeg * trapezoid (t, 0.18f, 0.82f);
            p.bodyBounceY  = -0.05f * bump;
            p.showRail     = t > 0.08f && t < 0.92f;
            p.sparkAmount  = trapezoid (t, 0.15f, 0.85f);
            break;

        case TrickPose::Manual:
        {
            // A manual is a held balance, not a snap - hold near the
            // tipped-back position for most of the animation, with a
            // small wobble to read as "actively balancing" rather than
            // frozen.
            const float hold = trapezoid (t, 0.18f, 0.82f);
            const float wobble = 3.0f * std::sin (t * juce::MathConstants<float>::pi * 5.0f) * hold;
            p.bodyLean     = -22.0f * hold + wobble * 0.3f;
            p.armAngle1    = -55.0f * hold - wobble;
            p.armAngle2    =  55.0f * hold + wobble;
            p.legAngle1    =  12.0f * hold;
            p.legAngle2    = -32.0f * hold;
            p.boardTiltDeg = -28.0f * hold + wobble;
            break;
        }

        case TrickPose::Boneless:
            // The back foot plants on the ground for the middle part of
            // the trick while the front foot keeps the board - so the
            // board should stay anchored near the front foot then, not at
            // the usual both-feet midpoint.
            p.groundReachFoot2 = trapezoid (t, 0.12f, 0.55f);
            p.bodyLean         =   8.0f * bump;
            p.armAngle1        = -90.0f * bump;
            p.armAngle2        =  55.0f * bump;
            p.legAngle1        =  35.0f * bump;
            p.legAngle2        = juce::jmap (p.groundReachFoot2, 0.0f, 1.0f, -80.0f * bump, 8.0f);
            p.boardTiltDeg     = 20.0f * bump;
            p.bodyBounceY      = 0.05f * p.groundReachFoot2 - 0.14f * bump;
            p.boardLift        = 0.16f * bump;
            p.boardAnchorBlend = juce::jmap (p.groundReachFoot2, 0.0f, 1.0f, 0.5f, 0.12f);
            break;

        case TrickPose::Slide:
            p.bodyLean     =  20.0f * bump;
            p.armAngle1    = -70.0f * bump;
            p.armAngle2    =  70.0f * bump;
            p.legAngle1    = -80.0f * bump;
            p.legAngle2    = -40.0f * bump;
            // Flips upside-down fast, HOLDS it (that's the slide), then
            // flips back - a Darkslide rides on the board's underside.
            // This deliberately reuses boardFlipDeg (not boardTiltDeg):
            // a static 180-degree TILT of a left-right-symmetric board
            // shape is pixel-for-pixel identical to not rotating it at
            // all, so it would show nothing happening. Driving it through
            // boardFlipDeg instead passes through the edge-on squash on
            // the way there and triggers the underside colour swap once
            // held past 90 degrees, which actually reads as "flipped over".
            p.boardFlipDeg = 180.0f * trapezoid (t, 0.15f, 0.85f);
            p.bodyBounceY  = -0.08f * bump;
            p.boardLift    =  0.06f * bump;
            p.sparkAmount  = trapezoid (t, 0.2f, 0.8f);
            break;
    }

    return p;
}

//==============================================================================
void SkaterView::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    drawBackground (g, area);
    drawRamp (g, area);

    const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    const float idlePhase = (float) ((now - startTimeSeconds) * 1.3);

    Pose pose;
    if (currentTrickIndex >= 0 && (now - triggerTimeSeconds) <= trickAnimSeconds)
    {
        const float phase = (float) ((now - triggerTimeSeconds) / trickAnimSeconds);
        pose = computePose (allTricks()[(size_t) currentTrickIndex], phase, idlePhase);
    }
    else
    {
        // The idle early-return in computePose() never looks at the trick
        // argument, so any valid entry works here - Ollie is as good as any.
        pose = computePose (allTricks()[0], -1.0f, idlePhase);
    }

    drawRailAndSparks (g, area, pose);
    drawSkater (g, area, pose);
    drawHud (g, area);
}

//==============================================================================
void SkaterView::drawBackground (juce::Graphics& g, juce::Rectangle<float> area) const
{
    juce::ColourGradient sky (juce::Colour (0xff2b1055), area.getX(), area.getY(),
                               juce::Colour (0xffff7e5f), area.getX(), area.getBottom(), false);
    sky.addColour (0.55, juce::Colour (0xff6a3093));
    g.setGradientFill (sky);
    g.fillRect (area);

    // sun
    const float sunR = area.getHeight() * 0.16f;
    const float sunX = area.getX() + area.getWidth() * 0.78f;
    const float sunY = area.getY() + area.getHeight() * 0.28f;
    juce::ColourGradient sunGrad (juce::Colour (0xfffff2a8), sunX, sunY,
                                   juce::Colour (0x00fff2a8), sunX, sunY + sunR * 1.6f, true);
    g.setGradientFill (sunGrad);
    g.fillEllipse (sunX - sunR * 1.6f, sunY - sunR * 1.6f, sunR * 3.2f, sunR * 3.2f);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.fillEllipse (sunX - sunR, sunY - sunR, sunR * 2.0f, sunR * 2.0f);

    // distant skyline triangles for a bit of depth
    juce::Random skyline (42);
    g.setColour (juce::Colour (0xff1c0f3d).withAlpha (0.6f));
    float x = area.getX() - 20.0f;
    const float baseY = area.getY() + area.getHeight() * 0.62f;
    while (x < area.getRight() + 20.0f)
    {
        const float w = 40.0f + skyline.nextFloat() * 50.0f;
        const float h = 20.0f + skyline.nextFloat() * area.getHeight() * 0.18f;
        juce::Path tri;
        tri.addTriangle (x, baseY, x + w * 0.5f, baseY - h, x + w, baseY);
        g.fillPath (tri);
        x += w * 0.7f;
    }
}

void SkaterView::drawRamp (juce::Graphics& g, juce::Rectangle<float> area) const
{
    const float rampTop = area.getY() + area.getHeight() * 0.62f;
    const float midY    = area.getY() + area.getHeight() * 0.92f;

    juce::Path ramp;
    ramp.startNewSubPath (area.getX(), rampTop);
    ramp.quadraticTo (area.getX() + area.getWidth() * 0.18f, midY,
                       area.getX() + area.getWidth() * 0.5f, midY + area.getHeight() * 0.05f);
    ramp.quadraticTo (area.getRight() - area.getWidth() * 0.18f, midY,
                       area.getRight(), rampTop);
    ramp.lineTo (area.getRight(), area.getBottom());
    ramp.lineTo (area.getX(), area.getBottom());
    ramp.closeSubPath();

    g.setColour (juce::Colour (0xff23203a));
    g.fillPath (ramp);

    g.setColour (juce::Colour (0xff8ee3f5).withAlpha (0.7f));
    juce::Path copingLeft, copingRight;
    copingLeft.startNewSubPath (area.getX(), rampTop);
    copingLeft.quadraticTo (area.getX() + area.getWidth() * 0.18f, midY,
                             area.getX() + area.getWidth() * 0.5f, midY + area.getHeight() * 0.05f);
    g.strokePath (copingLeft, juce::PathStrokeType (3.0f));

    copingRight.startNewSubPath (area.getRight(), rampTop);
    copingRight.quadraticTo (area.getRight() - area.getWidth() * 0.18f, midY,
                              area.getX() + area.getWidth() * 0.5f, midY + area.getHeight() * 0.05f);
    g.strokePath (copingRight, juce::PathStrokeType (3.0f));
}

void SkaterView::drawSkater (juce::Graphics& g, juce::Rectangle<float> area, const Pose& pose) const
{
    const float scale = juce::jmin (area.getWidth(), area.getHeight()) * 0.30f;
    const float anchorX = area.getX() + area.getWidth() * 0.5f;
    const float anchorY = area.getY() + area.getHeight() * 0.60f + pose.bodyBounceY * area.getHeight();

    // Big aerial spins (900, McTwist) rotate the WHOLE skater+board
    // silhouette together as one rigid unit about the hip anchor - the
    // correct 2D fake for a real aerial spin, where the feet stay planted
    // on the board throughout rather than the board moving under them.
    // Scoped so it never leaks into drawRailAndSparks()/drawHud(), which
    // are called separately right after this and must stay unrotated.
    juce::Graphics::ScopedSaveState saveState (g);
    if (std::abs (pose.wholeBodySpinDeg) > 0.01f)
        g.addTransform (juce::AffineTransform::rotation (juce::degreesToRadians (pose.wholeBodySpinDeg), anchorX, anchorY));

    const auto toRad = [] (float deg) { return juce::degreesToRadians (deg); };

    const juce::Colour bodyColour  (0xffff5d8f);
    const juce::Colour limbColour  (0xfffff3b0);
    const juce::Colour helmetColour (0xff39d6c7);
    const juce::Colour boardColour (0xff2e2b45);
    const juce::Colour boardStripe (0xffff5d8f);
    const juce::Colour boardUnderside (0xff9aa0ad); // griptape-side vs wheels/truck-side, for the flip colour swap

    const float limbWidth = scale * 0.11f;

    // hip (anchor) -> neck
    const float torsoLen = scale * 0.55f;
    const float neckX = anchorX + std::sin (toRad (pose.bodyLean)) * torsoLen;
    const float neckY = anchorY - std::cos (toRad (pose.bodyLean)) * torsoLen;

    // legs (hip -> feet)
    const float legLen = scale * 0.5f;
    const float foot1X = anchorX + std::sin (toRad (pose.legAngle1)) * legLen;
    const float foot1Y = anchorY + std::cos (toRad (pose.legAngle1)) * legLen;
    const float foot2X = anchorX + std::sin (toRad (pose.legAngle2)) * legLen;
    const float foot2Y = anchorY + std::cos (toRad (pose.legAngle2)) * legLen;

    // board, anchored under the feet (usually the midpoint - see
    // boardAnchorBlend), lifted/rotated per pose.
    const float boardW = scale * 1.15f;
    const float boardH = scale * 0.13f;
    const float boardCX  = foot1X + (foot2X - foot1X) * pose.boardAnchorBlend;
    const float boardCY0 = foot1Y + (foot2Y - foot1Y) * pose.boardAnchorBlend;
    const float boardCY  = boardCY0 - pose.boardLift * area.getHeight() * 0.6f;

    // Two independent fake-3D rotations, each squashing a different
    // dimension so a kickflip/heelflip (long-axis spin) and a shove-it
    // (vertical-axis spin) read as genuinely different motions rather than
    // one flat spin wearing different names. cos() of the flip angle also
    // decides which "side" is facing us, for the underside colour swap.
    const float flipRad  = toRad (pose.boardFlipDeg);
    const float shoveRad = toRad (pose.boardShoveDeg);
    const float cosFlip  = std::cos (flipRad);
    const float heightSquash = juce::jmax (0.12f, std::abs (cosFlip));   // never fully collapse to an invisible sliver
    const float widthSquash  = juce::jmax (0.12f, std::abs (std::cos (shoveRad)));
    const bool  showUnderside = cosFlip < 0.0f;

    const float drawnW = boardW * widthSquash;
    const float drawnH = boardH * heightSquash;

    juce::Path board;
    board.addRoundedRectangle (-drawnW * 0.5f, -drawnH * 0.5f, drawnW, drawnH, drawnH * 0.45f);
    juce::Path stripe;
    stripe.addRoundedRectangle (-drawnW * 0.5f, -drawnH * 0.18f, drawnW, drawnH * 0.36f, drawnH * 0.18f);
    // boardTiltDeg is an ordinary, non-squashing in-plane rotation (a
    // grind's pitch, a manual's tipped-back angle) - independent of the
    // flip/shove fake-3D spins above, and zero for tricks that don't use it.
    auto boardTransform = juce::AffineTransform::rotation (toRad (pose.boardTiltDeg)).translated (boardCX, boardCY);
    board.applyTransform (boardTransform);
    stripe.applyTransform (boardTransform);

    g.setColour (showUnderside ? boardUnderside : boardColour);
    g.fillPath (board);
    g.setColour (showUnderside ? boardUnderside.darker (0.3f) : boardStripe);
    g.fillPath (stripe);

    // legs
    g.setColour (limbColour);
    g.drawLine (anchorX, anchorY, foot1X, foot1Y, limbWidth);
    g.drawLine (anchorX, anchorY, foot2X, foot2Y, limbWidth);
    g.fillEllipse (foot1X - limbWidth * 0.6f, foot1Y - limbWidth * 0.6f, limbWidth * 1.2f, limbWidth * 1.2f);
    g.fillEllipse (foot2X - limbWidth * 0.6f, foot2Y - limbWidth * 0.6f, limbWidth * 1.2f, limbWidth * 1.2f);

    // torso
    g.setColour (bodyColour);
    g.drawLine (anchorX, anchorY, neckX, neckY, scale * 0.22f);

    // arms (from the neck / shoulder point)
    const float armLen = scale * 0.48f;
    const float hand1X = neckX + std::sin (toRad (pose.armAngle1)) * armLen;
    const float hand1Y = neckY + std::cos (toRad (pose.armAngle1)) * armLen;
    const float hand2X = neckX + std::sin (toRad (pose.armAngle2)) * armLen;
    const float hand2Y = neckY + std::cos (toRad (pose.armAngle2)) * armLen;

    g.setColour (limbColour);
    g.drawLine (neckX, neckY, hand1X, hand1Y, limbWidth * 0.85f);
    g.drawLine (neckX, neckY, hand2X, hand2Y, limbWidth * 0.85f);
    g.fillEllipse (hand1X - limbWidth * 0.5f, hand1Y - limbWidth * 0.5f, limbWidth, limbWidth);
    g.fillEllipse (hand2X - limbWidth * 0.5f, hand2Y - limbWidth * 0.5f, limbWidth, limbWidth);

    // head / helmet
    const float headR = scale * 0.17f;
    const float headX = neckX + std::sin (toRad (pose.bodyLean)) * headR * 1.1f;
    const float headY = neckY - std::cos (toRad (pose.bodyLean)) * headR * 1.1f;
    g.setColour (helmetColour);
    g.fillEllipse (headX - headR, headY - headR, headR * 2.0f, headR * 2.0f);
    g.setColour (boardColour.withAlpha (0.6f));
    g.drawEllipse (headX - headR, headY - headR, headR * 2.0f, headR * 2.0f, 1.5f);
}

void SkaterView::drawRailAndSparks (juce::Graphics& g, juce::Rectangle<float> area, const Pose& pose) const
{
    // Matches drawSkater()'s own anchor calculation so the rail/sparks line
    // up with the feet regardless of the current bounce/lift.
    const float anchorY = area.getY() + area.getHeight() * 0.60f + pose.bodyBounceY * area.getHeight();
    const float contactX = area.getX() + area.getWidth() * 0.5f;
    const float contactY = anchorY + area.getHeight() * 0.08f;

    if (pose.showRail)
    {
        const float railHalfW = area.getWidth() * 0.42f;
        const float railH = area.getHeight() * 0.022f;

        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRoundedRectangle (contactX - railHalfW, contactY - railH * 0.3f, railHalfW * 2.0f, railH, railH * 0.5f);
        g.setColour (juce::Colour (0xffb7c6cf));
        g.fillRoundedRectangle (contactX - railHalfW, contactY - railH * 0.5f, railHalfW * 2.0f, railH, railH * 0.5f);
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.fillRoundedRectangle (contactX - railHalfW, contactY - railH * 0.5f, railHalfW * 2.0f, railH * 0.3f, railH * 0.15f);
    }

    if (pose.sparkAmount > 0.02f)
    {
        // Sparks are meant to flicker randomly frame to frame, so the
        // system random (not the seeded member used for trick choice) is
        // the right source here - there's nothing to reproduce later.
        auto& rnd = juce::Random::getSystemRandom();
        const int numSparks = (int) std::round (7.0f * pose.sparkAmount);
        const float burst = area.getWidth() * 0.055f * pose.sparkAmount;

        for (int i = 0; i < numSparks; ++i)
        {
            const float ang = rnd.nextFloat() * juce::MathConstants<float>::twoPi;
            const float dist = (0.25f + rnd.nextFloat() * 0.75f) * burst;
            const float sx = contactX + std::cos (ang) * dist;
            const float sy = contactY - std::abs (std::sin (ang)) * dist * 0.6f; // mostly up/sideways, not downward
            const float r = area.getWidth() * 0.0055f * (0.5f + rnd.nextFloat());

            g.setColour (juce::Colour (0xffffe08a).withAlpha (pose.sparkAmount * (0.4f + rnd.nextFloat() * 0.6f)));
            g.fillEllipse (sx - r, sy - r, r * 2.0f, r * 2.0f);
        }
    }
}

void SkaterView::drawHud (juce::Graphics& g, juce::Rectangle<float> area) const
{
    const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;

    // trick name + score banner
    if (currentTrickIndex >= 0)
    {
        const double elapsed = now - triggerTimeSeconds;
        if (elapsed >= 0.0 && elapsed <= bannerSeconds)
        {
            const float alpha = juce::jlimit (0.0f, 1.0f, (float) (1.0 - elapsed / bannerSeconds));
            const float yOffset = (float) (elapsed / bannerSeconds) * 36.0f;
            const auto& trick = allTricks()[(size_t) currentTrickIndex];

            auto bannerArea = area.withHeight (area.getHeight() * 0.22f)
                                   .withY (area.getY() + area.getHeight() * 0.08f - yOffset);

            g.setColour (juce::Colours::white.withAlpha (alpha));
            g.setFont (juce::Font (juce::FontOptions ((float) area.getHeight() * 0.11f, juce::Font::bold)));
            g.drawText (trick.name, bannerArea, juce::Justification::centred, true);

            g.setColour (juce::Colour (0xfffff3b0).withAlpha (alpha));
            g.setFont (juce::Font (juce::FontOptions ((float) area.getHeight() * 0.06f, juce::Font::bold)));
            g.drawText ("+" + juce::String (trick.score),
                        bannerArea.translated (0.0f, area.getHeight() * 0.1f),
                        juce::Justification::centred, true);
        }
    }

    // combo counter
    if (combo >= 2 && (now - lastTrickWallClock) <= comboWindowSeconds)
    {
        auto comboArea = area.removeFromTop (area.getHeight() * 0.14f).removeFromRight (area.getWidth() * 0.32f);
        g.setColour (juce::Colour (0xff39d6c7));
        g.setFont (juce::Font (juce::FontOptions (comboArea.getHeight() * 0.55f, juce::Font::bold)));
        g.drawText ("COMBO x" + juce::String (combo), comboArea, juce::Justification::centredRight, true);
    }

    // retune meter
    {
        auto meterArea = area.removeFromTop (area.getHeight()).removeFromLeft (area.getWidth() * 0.06f)
                              .withSizeKeepingCentre (area.getWidth() * 0.035f, area.getHeight() * 0.5f)
                              .withY (area.getY() + area.getHeight() * 0.25f);

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (meterArea, 4.0f);

        const float fillAmount = juce::jlimit (0.0f, 1.0f, displayedCorrection / 2.0f);
        auto fillArea = meterArea.withHeight (meterArea.getHeight() * fillAmount)
                                  .withY (meterArea.getBottom() - meterArea.getHeight() * fillAmount);
        g.setColour (juce::Colour (0xffff5d8f));
        g.fillRoundedRectangle (fillArea, 4.0f);

        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.drawRoundedRectangle (meterArea, 4.0f, 1.0f);
    }
}
