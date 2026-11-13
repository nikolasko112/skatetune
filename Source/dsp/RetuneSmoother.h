#pragma once
#include <cmath>
#include <algorithm>

namespace skatetune
{

/** One-pole smoother for the pitch-correction amount. This is literally
    what "Retune Speed" means on a classic auto-tune effect: fast speed =
    short time constant = the correction snaps on almost instantly (the
    robotic/warbly sound), slow speed = long time constant = the
    correction glides in (a more natural-sounding pitch nudge). */
class RetuneSmoother
{
public:
    void prepare (double sampleRate) { sr = sampleRate; }

    /** speed0to100: 0 = slow glide (~250 ms), 100 = near-instant snap (~0.3 ms). */
    void setRetuneSpeed (double speed0to100)
    {
        double s = std::clamp (speed0to100, 0.0, 100.0) / 100.0;
        double tauMs = 250.0 * std::pow (1.0 - s, 2.0) + 0.3;
        double tauSec = tauMs / 1000.0;
        coeff = std::exp (-1.0 / (tauSec * sr));
    }

    double process (double targetValue)
    {
        current = coeff * current + (1.0 - coeff) * targetValue;
        return current;
    }

    double getCurrent() const { return current; }
    void reset (double value = 0.0) { current = value; }

private:
    double sr = 44100.0;
    double coeff = 0.0;
    double current = 0.0;
};

} // namespace skatetune
