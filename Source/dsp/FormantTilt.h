#pragma once
#include <cmath>
#include <algorithm>

namespace skatetune
{

/** A deliberately simplified, honestly-scoped approximation of a "Formant"
    control.

    True formant-independent pitch correction - what a commercial product
    like Auto-Tune actually does - requires separating a voice's spectral
    envelope from its pitch (typically via LPC or a cepstral/channel-vocoder
    analysis) and re-applying that envelope after pitch shifting. That's a
    substantial DSP subsystem in its own right, and there is no shortcut to
    it from a simple time-domain resampler like GranularPitchShifter: time-
    domain resampling can't separate "pitch" from "formant" at all, because
    both are just "how fast the buffer is read" - cascading two resamples
    always just multiplies into a single plain pitch shift, never an
    independent formant move. Building real formant separation would mean a
    second, much larger DSP subsystem on the order of (likely bigger than)
    the granular shifter itself.

    What this class gives instead is a straightforward spectral TILT -
    brighter when positive, darker when negative - applied only to the
    pitch-corrected signal. It's a real, audible, tunable EQ-style control
    in the same conceptual direction as formant shifting (nudging a
    pitched-up voice back away from sounding thin/chipmunked, or a
    pitched-down voice back away from sounding boomy), but it does not move
    individual formant peaks and is not spectral-envelope-accurate formant
    correction. Labelled "Formant" to match the feature list it's standing
    in for, not because it claims to be the real thing. */
class FormantTilt
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        // One-pole lowpass reference for the tilt, centred roughly where
        // the first vocal formant typically lives.
        const double cutoffHz = 1500.0;
        lpCoeff = std::exp (-2.0 * kPi * cutoffHz / sr);
        lpState = 0.0;
    }

    /** amountMinus100to100: 0 = no change, +100 = brightest tilt,
        -100 = darkest tilt. */
    void setAmount (double amountMinus100to100)
    {
        amount = std::clamp (amountMinus100to100, -100.0, 100.0) / 100.0;
    }

    float processSample (float x)
    {
        lpState = (1.0 - lpCoeff) * (double) x + lpCoeff * lpState;
        double tilted = (double) x + amount * ((double) x - lpState);
        return (float) tilted;
    }

    void reset() { lpState = 0.0; }

private:
    static constexpr double kPi = 3.14159265358979323846;
    double sr = 44100.0;
    double lpCoeff = 0.0;
    double lpState = 0.0;
    double amount = 0.0;
};

} // namespace skatetune
