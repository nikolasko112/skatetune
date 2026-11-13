#pragma once
#include <vector>
#include <cmath>

namespace skatetune
{

/** Lightweight real-time pitch shifter: two Hann-windowed grains, offset
    by half a grain period and crossfaded, each reading from a circular
    history buffer at a variable rate (the pitch ratio) and restarting
    from "now minus one grain" whenever its window completes. This is a
    simplified, dependency-free relative of classic granular/PSOLA-style
    time-domain pitch shifters - not as clean as a commercial formant-
    aware shifter, but it produces a convincingly "pitch corrected, and
    audibly robotic when pushed hard" result, which is exactly what this
    plugin is going for. */
class GranularPitchShifter
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        grainSizeSamples = std::max (32, (int) std::round (0.040 * sr));
        bufSize = std::max (grainSizeSamples * 8, (int) std::round (1.0 * sr));
        buffer.assign ((size_t) bufSize, 0.0f);
        writePos = 0;
        primedCount = 0;
        phase1 = 0.0;
        phase2 = (double) grainSizeSamples * 0.5;
        readPos1 = 0.0;
        readPos2 = (double) grainSizeSamples * 0.5;
        ratio = 1.0;
    }

    void setRatio (double r) { ratio = r; }

    float processSample (float inputSample)
    {
        buffer[(size_t) writePos] = inputSample;

        if (primedCount < grainSizeSamples * 2)
        {
            ++primedCount;
            writePos = (writePos + 1) % bufSize;
            return inputSample; // pass dry until there is enough history to read from
        }

        if (phase1 >= (double) grainSizeSamples)
        {
            phase1 = 0.0;
            readPos1 = wrap ((double) writePos - (double) grainSizeSamples);
        }
        if (phase2 >= (double) grainSizeSamples)
        {
            phase2 = 0.0;
            readPos2 = wrap ((double) writePos - (double) grainSizeSamples);
        }

        float s1 = readInterp (readPos1);
        float s2 = readInterp (readPos2);

        double w1 = hann (phase1 / (double) grainSizeSamples);
        double w2 = hann (phase2 / (double) grainSizeSamples);
        double wsum = w1 + w2;

        float out = (wsum > 1.0e-6) ? (float) ((s1 * w1 + s2 * w2) / wsum) : 0.0f;

        readPos1 = wrap (readPos1 + ratio);
        readPos2 = wrap (readPos2 + ratio);
        phase1 += 1.0;
        phase2 += 1.0;

        writePos = (writePos + 1) % bufSize;
        return out;
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        primedCount = 0;
        phase1 = 0.0;
        phase2 = (double) grainSizeSamples * 0.5;
        readPos1 = 0.0;
        readPos2 = (double) grainSizeSamples * 0.5;
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
        int i0 = (int) std::floor (pos);
        double frac = pos - (double) i0;
        i0 = ((i0 % bufSize) + bufSize) % bufSize;
        int i1 = (i0 + 1) % bufSize;
        return (float) ((1.0 - frac) * buffer[(size_t) i0] + frac * buffer[(size_t) i1]);
    }

    double sr = 44100.0;
    int grainSizeSamples = 1764;
    int bufSize = 44100;
    std::vector<float> buffer;
    int writePos = 0;
    int primedCount = 0;

    double phase1 = 0.0, phase2 = 0.0;
    double readPos1 = 0.0, readPos2 = 0.0;
    double ratio = 1.0;
};

} // namespace skatetune
