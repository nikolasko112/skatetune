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
SkaterView::Pose SkaterView::computePose (TrickPose pose, float phase01, float idlePhase)
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
    const float amount = std::sin (juce::MathConstants<float>::pi * t); // 0 -> 1 -> 0

    switch (pose)
    {
        case TrickPose::AirTuck:
            p.bodyLean      = -15.0f * amount;
            p.armAngle1     = -20.0f - 50.0f * amount;
            p.armAngle2     =  20.0f + 40.0f * amount;
            p.legAngle1     = -10.0f - 60.0f * amount;
            p.legAngle2     =  10.0f + 60.0f * amount;
            p.boardRotation = t * 360.0f;
            p.bodyBounceY   = -0.18f * amount;
            p.boardLift     =  0.22f * amount;
            break;

        case TrickPose::Grind:
            p.bodyLean      = 10.0f * amount;
            p.armAngle1     = -40.0f * amount;
            p.armAngle2     =  30.0f * amount;
            p.legAngle1     = -70.0f * amount;
            p.legAngle2     = -50.0f * amount;
            p.boardRotation =  90.0f * amount;
            p.bodyBounceY   = -0.05f * amount;
            p.boardLift     =  0.05f * amount;
            break;

        case TrickPose::Manual:
            p.bodyLean      = -20.0f * amount;
            p.armAngle1     = -60.0f * amount;
            p.armAngle2     =  60.0f * amount;
            p.legAngle1     =  10.0f * amount;
            p.legAngle2     = -30.0f * amount;
            p.boardRotation = -25.0f * amount;
            break;

        case TrickPose::Boneless:
            p.bodyLean      =   5.0f * amount;
            p.armAngle1     = -90.0f * amount;
            p.armAngle2     =  50.0f * amount;
            p.legAngle1     =  40.0f * amount;
            p.legAngle2     = -80.0f * amount;
            p.boardRotation =  20.0f * amount;
            p.bodyBounceY   = -0.10f * amount;
            p.boardLift     =  0.15f * amount;
            break;

        case TrickPose::Slide:
            p.bodyLean      =  20.0f * amount;
            p.armAngle1     = -70.0f * amount;
            p.armAngle2     =  70.0f * amount;
            p.legAngle1     = -80.0f * amount;
            p.legAngle2     = -40.0f * amount;
            p.boardRotation = 180.0f * amount;
            p.bodyBounceY   = -0.08f * amount;
            p.boardLift     =  0.08f * amount;
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
        pose = computePose (allTricks()[(size_t) currentTrickIndex].pose, phase, idlePhase);
    }
    else
    {
        pose = computePose (TrickPose::AirTuck, -1.0f, idlePhase);
    }

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

    const auto toRad = [] (float deg) { return juce::degreesToRadians (deg); };

    const juce::Colour bodyColour  (0xffff5d8f);
    const juce::Colour limbColour  (0xfffff3b0);
    const juce::Colour helmetColour (0xff39d6c7);
    const juce::Colour boardColour (0xff2e2b45);
    const juce::Colour boardStripe (0xffff5d8f);

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

    // board, centred under the feet, lifted/rotated per pose
    const float boardW = scale * 1.15f;
    const float boardH = scale * 0.13f;
    const float boardCX = (foot1X + foot2X) * 0.5f;
    const float boardCY = (foot1Y + foot2Y) * 0.5f - pose.boardLift * area.getHeight() * 0.6f;

    juce::Path board;
    board.addRoundedRectangle (-boardW * 0.5f, -boardH * 0.5f, boardW, boardH, boardH * 0.45f);
    juce::Path stripe;
    stripe.addRoundedRectangle (-boardW * 0.5f, -boardH * 0.18f, boardW, boardH * 0.36f, boardH * 0.18f);
    auto boardTransform = juce::AffineTransform::rotation (toRad (pose.boardRotation)).translated (boardCX, boardCY);
    board.applyTransform (boardTransform);
    stripe.applyTransform (boardTransform);

    g.setColour (boardColour);
    g.fillPath (board);
    g.setColour (boardStripe);
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
