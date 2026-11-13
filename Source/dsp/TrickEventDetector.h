#pragma once

namespace skatetune
{

/** Fires a "new trick" event whenever the (smoothed) pitch-correction
    amount peaks above a threshold - i.e. whenever the retune is working
    hard and fast on a note - with hysteresis so it doesn't chatter and a
    cooldown so tricks don't spam faster than they can be shown. */
class TrickEventDetector
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        recalcCooldown();
        cooldownCounter = 0;
        above = false;
    }

    void setThresholds (double lowSemitones, double highSemitones)
    {
        lowThreshold = lowSemitones;
        highThreshold = highSemitones;
    }

    void setCooldownSeconds (double seconds)
    {
        cooldownSeconds = seconds;
        recalcCooldown();
    }

    /** Feed the current |correction| in semitones once per sample.
        Returns true exactly on the sample where a new trick should fire. */
    bool update (double absCorrectionSemitones)
    {
        if (cooldownCounter > 0)
            --cooldownCounter;

        bool fired = false;
        if (! above && absCorrectionSemitones >= highThreshold && cooldownCounter == 0)
        {
            above = true;
            fired = true;
            cooldownCounter = cooldownSamples;
        }
        else if (above && absCorrectionSemitones <= lowThreshold)
        {
            above = false;
        }
        return fired;
    }

private:
    void recalcCooldown() { cooldownSamples = (int) (cooldownSeconds * sr); }

    double sr = 44100.0;
    double lowThreshold = 0.15;
    double highThreshold = 0.60;
    double cooldownSeconds = 0.35;
    int cooldownSamples = 0;
    int cooldownCounter = 0;
    bool above = false;
};

} // namespace skatetune
