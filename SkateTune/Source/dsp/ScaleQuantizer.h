#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace skatetune
{

enum class ScaleType
{
    Chromatic = 0,
    Major,
    Minor,
    MajorPentatonic,
    MinorPentatonic
};

inline std::vector<int> scaleIntervals (ScaleType t)
{
    switch (t)
    {
        case ScaleType::Major:           return { 0, 2, 4, 5, 7, 9, 11 };
        case ScaleType::Minor:           return { 0, 2, 3, 5, 7, 8, 10 };
        case ScaleType::MajorPentatonic: return { 0, 2, 4, 7, 9 };
        case ScaleType::MinorPentatonic: return { 0, 3, 5, 7, 10 };
        case ScaleType::Chromatic:
        default:                         return { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    }
}

/** Quantizes a continuous pitch (given as frequency in Hz) to the nearest
    note belonging to a key + scale, in MIDI-note space so octave handling
    falls out naturally. */
class ScaleQuantizer
{
public:
    void setKeyRoot (int rootSemitoneFromC) { root = ((rootSemitoneFromC % 12) + 12) % 12; }
    void setScale (ScaleType t) { scale = t; intervals = scaleIntervals (t); }

    static double freqToMidi (double freqHz)
    {
        return 69.0 + 12.0 * std::log2 (freqHz / 440.0);
    }

    static double midiToFreq (double midi)
    {
        return 440.0 * std::pow (2.0, (midi - 69.0) / 12.0);
    }

    double nearestScaleMidi (double inputMidi) const
    {
        int baseNote = (int) std::floor (inputMidi);
        double best = inputMidi;
        double bestDist = 1.0e9;

        for (int cand = baseNote - 14; cand <= baseNote + 14; ++cand)
        {
            int pc = ((cand % 12) + 12) % 12;
            int rel = ((pc - root) % 12 + 12) % 12;
            if (std::find (intervals.begin(), intervals.end(), rel) == intervals.end())
                continue;

            double dist = std::abs ((double) cand - inputMidi);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = (double) cand;
            }
        }
        return best;
    }

    /** freq in -> target freq out + signed correction in semitones (target - input). */
    void process (double inputFreqHz, double& targetFreqHzOut, double& correctionSemitonesOut) const
    {
        double inMidi = freqToMidi (inputFreqHz);
        double targetMidi = nearestScaleMidi (inMidi);
        targetFreqHzOut = midiToFreq (targetMidi);
        correctionSemitonesOut = targetMidi - inMidi;
    }

private:
    int root = 0;
    ScaleType scale = ScaleType::Major;
    std::vector<int> intervals = scaleIntervals (ScaleType::Major);
};

} // namespace skatetune
