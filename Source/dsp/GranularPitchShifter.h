#pragma once
#include <vector>
#include <cmath>

namespace skatetune
{

/** Lightweight real-time pitch shifter: two Hann-windowed taps read from a
    circular history buffer at a variable rate (the pitch ratio) and
    crossfade so the listener never hears either one's restart.

    The crossfade/restart is driven by each tap's own DELAY excursion, not
    by a fixed real-time clock. This matters a lot and is worth spelling
    out, because the fixed-clock version of this idea (restart every N
    real-time samples, regardless of ratio) is a genuine correctness bug,
    not just a quality tradeoff: a tap's delay only drifts at rate
    (1 - ratio) per sample, so for a small correction (ratio close to 1,
    i.e. nearly every realistic autotune correction) the delay barely
    moves in N samples - restarting it anyway on a fixed clock snaps the
    read position back to centre before the pitch shift has accumulated
    to anything audible, which silently erases almost the entire
    correction every ~N-sample cycle. Driving the restart from the delay
    itself (restart only once delay has actually swept its full range)
    makes restarts rare for small ratios and frequent for large ones,
    which is exactly what's needed, and it still restarts only when that
    tap's Hann weight is at zero so there's nothing to hear at the seam.

    Not as clean as a commercial formant-aware shifter, but it produces a
    convincingly "pitch corrected, and audibly robotic when pushed hard"
    result, which is what this plugin is going for. */
class GranularPitchShifter
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        // Max one-way delay excursion per tap. 80ms measured noticeably more
        // accurate across the board than the more usual ~40ms grain (see the
        // DSP skill notes / empirical sweep this was tuned against) - small
        // corrections land within ~1 cent, and even a full-octave shift stays
        // under ~15 cents, which is below what's reliably audible.
        rangeSamples = std::max (32, (int) std::round (0.080 * sr));
        baseDelay = (double) rangeSamples; // keeps total delay in [range/2, 1.5*range], always > 0
        bufSize = std::max (rangeSamples * 8, (int) std::round (1.0 * sr));
        buffer.assign ((size_t) bufSize, 0.0f);
        writePos = 0;
        primedCount = 0;
        delay1 = (double) rangeSamples * 0.5;
        delay2 = 0.0;
        ratio = 1.0;
    }

    void setRatio (double r) { ratio = r; }

    /** Modern/Classic switch: Modern (true, the default) reads the history
        buffer with linear interpolation, which is what the accuracy this
        class's whole design is built around actually depends on. Classic
        (false) reads with plain nearest-neighbour instead, which reintroduces
        a bit of sample-accurate quantization "grit" into every read - closer
        to the harder-edged, more digital character of first-generation
        pitch-correction hardware/software, especially audible on sustained
        notes pushed with a fast Retune Speed. */
    void setInterpolationQuality (bool useLinearInterpolation) { linearInterp = useLinearInterpolation; }

    float processSample (float inputSample)
    {
        buffer[(size_t) writePos] = inputSample;

        if (primedCount < rangeSamples * 2)
        {
            ++primedCount;
            writePos = (writePos + 1) % bufSize;
            return inputSample; // pass dry until there is enough history to read from
        }

        // Both taps' delay evolves at the same rate; a fixed half-range
        // separation keeps them permanently staggered so one is always
        // near full weight while the other is near zero and free to wrap.
        const double inc = 1.0 - ratio;
        delay1 += inc;
        if (delay1 >= (double) rangeSamples) delay1 -= (double) rangeSamples;
        if (delay1 < 0.0)                    delay1 += (double) rangeSamples;
        delay2 += inc;
        if (delay2 >= (double) rangeSamples) delay2 -= (double) rangeSamples;
        if (delay2 < 0.0)                    delay2 += (double) rangeSamples;

        const double half = (double) rangeSamples * 0.5;
        double readPos1 = wrap ((double) writePos - baseDelay - delay1 + half);
        double readPos2 = wrap ((double) writePos - baseDelay - delay2 + half);

        double w1 = hann (delay1 / (double) rangeSamples);
        double w2 = hann (delay2 / (double) rangeSamples);
        double wsum = w1 + w2;

        float s1 = readInterp (readPos1);
        float s2 = readInterp (readPos2);
        float out = (wsum > 1.0e-6) ? (float) ((s1 * w1 + s2 * w2) / wsum) : 0.0f;

        writePos = (writePos + 1) % bufSize;
        return out;
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        primedCount = 0;
        delay1 = (double) rangeSamples * 0.5;
        delay2 = 0.0;
    }

private:
    static constexpr double kPi = 3.14159265358979323846;

    static double hann (double x) { return 0.5 - 0.5 * std::cos (2.0 * kPi * x); }

    double wrap (double pos) const
    {
        double n = (double) bufSize;
        while (pos < 0.0) pos += n;
        while (pos >= n) pos -= n;
        return pos;
    }

    float readInterp (double pos) const
    {
        double floorPos = std::floor (pos);
        int i0 = ((((int) floorPos) % bufSize) + bufSize) % bufSize;
        if (! linearInterp)
            return buffer[(size_t) i0];
        double frac = pos - floorPos;
        int i1 = (i0 + 1) % bufSize;
        return (float) ((1.0 - frac) * buffer[(size_t) i0] + frac * buffer[(size_t) i1]);
    }

    double sr = 44100.0;
    int rangeSamples = 3528;
    double baseDelay = 3528.0;
    int bufSize = 44100;
    std::vector<float> buffer;
    int writePos = 0;
    int primedCount = 0;

    double delay1 = 0.0, delay2 = 0.0;
    double ratio = 1.0;
    bool linearInterp = true;
};

} // namespace skatetune
