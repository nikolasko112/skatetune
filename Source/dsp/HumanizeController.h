#pragma once
#include <cmath>
#include <algorithm>

namespace skatetune
{

/** Antares-style "Humanize": Retune Speed alone forces a global, permanent
    choice between "natural" and "robotic" - a slow speed glides sweetly
    into a new note but also smooths away a singer's own natural vibrato on
    a sustained one, while a fast speed keeps the vibrato but turns every
    note-to-note transition into a hard, robotic step. Humanize breaks that
    tradeoff: it watches how much the *tracked* pitch has actually been
    moving, and only relaxes the effective Retune Speed while a note is
    genuinely being held - the instant the input pitch actually jumps to a
    new note, it snaps straight back to the full user-set Retune Speed, so
    transitions stay just as snappy as with Humanize off.

    This never touches the correction amount itself, only the speed value
    handed to RetuneSmoother, so it layers cleanly on top of the existing
    retune-speed machinery rather than replacing any of it. */
class HumanizeController
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        releaseCoeff = std::exp (-1.0 / (releaseSeconds * sr));
        reset();
    }

    /** amount0to100: 0 = no effect (effective speed always equals the
        user's Retune Speed setting), 100 = maximum relaxation while a note
        is held steady. */
    void setAmount (double amount0to100) { amount = std::clamp (amount0to100, 0.0, 100.0) / 100.0; }

    /** Call once per new pitch-tracker hop (not per sample), with the
        newly tracked note in MIDI-note units, whenever the input is
        voiced. A real note change registers as an immediate jump. */
    void onNewPitchHop (double trackedMidi)
    {
        if (havePrevMidi)
        {
            double jump = std::abs (trackedMidi - prevMidi);
            if (jump > instability)
                instability = jump;
        }
        prevMidi = trackedMidi;
        havePrevMidi = true;
    }

    /** Call once per audio sample - this is what lets "has it been steady
        for a while" decay smoothly in real time - to get the Retune Speed
        value to actually feed RetuneSmoother this sample. */
    double getEffectiveRetuneSpeed (double userRetuneSpeed0to100)
    {
        instability *= releaseCoeff;

        // stability -> 1 the longer the pitch has sat still, -> 0 right after a jump.
        double stability = std::exp (-instability / instabilityScaleSemis);
        double relaxedSpeed = userRetuneSpeed0to100 * (1.0 - amount);
        return userRetuneSpeed0to100 * (1.0 - stability) + relaxedSpeed * stability;
    }

    void reset()
    {
        instability = 0.0;
        havePrevMidi = false;
        prevMidi = 0.0;
    }

private:
    double sr = 44100.0;
    double amount = 0.0;

    double instability = 0.0;
    double prevMidi = 0.0;
    bool havePrevMidi = false;

    // A jump of this many semitones or more counts as "fully unstable" (a
    // real note change); small enough that ordinary vibrato only reads as
    // partially unstable rather than as a note change.
    static constexpr double instabilityScaleSemis = 0.12;
    // How long "just changed note" memory takes to decay back to steady.
    static constexpr double releaseSeconds = 0.18;
    double releaseCoeff = 0.0;
};

} // namespace skatetune
