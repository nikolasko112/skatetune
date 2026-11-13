#pragma once
#include <vector>
#include <deque>
#include <cmath>
#include <algorithm>

namespace skatetune
{

/**
    Simple real-time pitch tracker using time-domain autocorrelation with
    parabolic interpolation for sub-sample lag accuracy, plus a short
    median filter to reject octave-jump outliers.

    Not FFT-based on purpose: lag search range is small enough (vocal/
    instrument fundamentals, 70-1000 Hz) that direct autocorrelation is
    cheap enough for real time and keeps the dependency list at zero.
*/
class PitchTracker
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        frameSize = std::max (64, (int) std::round (0.030 * sr));
        hopSize   = std::max (16, (int) std::round (0.010 * sr));

        // Size the history buffer for the widest range setFrequencyRange()
        // is ever allowed to ask for (see kWorstCaseMinFreqHz) - once, here
        // - so that changing the frequency range later (e.g. an "Input
        // Type" choice, switched live at the top of every block) never has
        // to reallocate: it only ever adjusts minLag/maxLag, cheaply,
        // inside buffers already sized for the worst case.
        int worstCaseMaxLag = (int) std::round (sr / kWorstCaseMinFreqHz);
        historySize = frameSize + worstCaseMaxLag + 4;
        history.assign (historySize, 0.0f);
        writePos = 0;
        historyFilled = 0;
        samplesSinceHop = 0;
        lastFreq = 0.0f;
        voicedFlag = false;
        recentFreqs.clear();

        recalcLagRange();
    }

    /** Feed one input sample. Returns true on the samples where a new
        pitch estimate was computed (roughly every hopSize samples). */
    bool pushSample (float x)
    {
        history[(size_t) writePos] = x;
        writePos = (writePos + 1) % historySize;
        if (historyFilled < historySize)
            ++historyFilled;

        if (++samplesSinceHop >= hopSize && historyFilled >= frameSize)
        {
            samplesSinceHop = 0;
            analyze();
            return true;
        }
        return false;
    }

    float getFrequencyHz() const { return lastFreq; }
    bool  isVoiced()       const { return voicedFlag; }

    void setVoicingThreshold (double t) { voicingThreshold = t; }

    /** Narrowing this to the expected source (e.g. a vocal range) both
        rejects octave errors and speeds up the search. Safe to call every
        block with the same values - it's a no-op unless they actually
        changed - and safe to call with different values at any time after
        prepare(), since the history buffer is always sized up-front for
        the full worst-case range (see kWorstCaseMinFreqHz). */
    void setFrequencyRange (float minHz, float maxHz)
    {
        if (minHz == minFreqHz && maxHz == maxFreqHz)
            return;
        minFreqHz = minHz;
        maxFreqHz = maxHz;
        recalcLagRange();
    }

private:
    void recalcLagRange()
    {
        minLag = std::max (2, (int) std::round (sr / maxFreqHz));
        maxLag = (int) std::round (sr / minFreqHz);
        // Defensive clamp: never exceed what the history buffer actually
        // holds, even if a future caller passes something outside the
        // range kWorstCaseMinFreqHz was chosen to cover.
        maxLag = std::min (maxLag, historySize - frameSize - 4);
    }

    void analyze()
    {
        frameBuf.resize ((size_t) frameSize);
        int start = (writePos - frameSize + historySize) % historySize;
        for (int i = 0; i < frameSize; ++i)
            frameBuf[(size_t) i] = history[(size_t) ((start + i) % historySize)];

        double mean = 0.0;
        for (float v : frameBuf) mean += v;
        mean /= (double) frameBuf.size();
        for (float& v : frameBuf) v = (float) (v - mean);

        double r0 = 0.0;
        for (float v : frameBuf) r0 += (double) v * (double) v;

        if (r0 < 1.0e-7)
        {
            voicedFlag = false;
            return;
        }

        int usableMaxLag = std::min (maxLag, frameSize - 1);
        double bestR = -1.0e300;
        int bestLag = -1;

        for (int lag = minLag; lag <= usableMaxLag; ++lag)
        {
            double r = autocorrAt (lag);
            if (r > bestR) { bestR = r; bestLag = lag; }
        }

        if (bestLag < 0)
        {
            voicedFlag = false;
            return;
        }

        double normalized = bestR / r0;
        if (normalized < voicingThreshold)
        {
            voicedFlag = false;
            return;
        }

        double rMinus = autocorrAt (std::max (minLag, bestLag - 1));
        double rPlus  = autocorrAt (std::min (usableMaxLag, bestLag + 1));
        double denom = (rMinus - 2.0 * bestR + rPlus);
        double shift = 0.0;
        if (std::abs (denom) > 1.0e-9)
            shift = 0.5 * (rMinus - rPlus) / denom;
        shift = std::clamp (shift, -1.0, 1.0);

        double refinedLag = (double) bestLag + shift;
        if (refinedLag < 1.0) refinedLag = 1.0;
        float freq = (float) (sr / refinedLag);

        recentFreqs.push_back (freq);
        if (recentFreqs.size() > 3)
            recentFreqs.pop_front();

        lastFreq = medianOf (recentFreqs);
        voicedFlag = true;
    }

    double autocorrAt (int lag) const
    {
        lag = std::clamp (lag, 0, frameSize - 1);
        double r = 0.0;
        int count = frameSize - lag;
        for (int i = 0; i < count; ++i)
            r += (double) frameBuf[(size_t) i] * (double) frameBuf[(size_t) (i + lag)];
        return r;
    }

    static float medianOf (const std::deque<float>& d)
    {
        std::vector<float> v (d.begin(), d.end());
        std::sort (v.begin(), v.end());
        return v[v.size() / 2];
    }

    double sr = 44100.0;
    int frameSize = 1323;
    int hopSize = 441;
    int minLag = 44;
    int maxLag = 630;
    int historySize = 0;
    float minFreqHz = 70.0f;
    float maxFreqHz = 1000.0f;
    double voicingThreshold = 0.35;
    static constexpr float kWorstCaseMinFreqHz = 35.0f; // covers every Input Type choice (Bass = 50 Hz) with margin

    std::vector<float> history;
    int writePos = 0;
    int historyFilled = 0;
    int samplesSinceHop = 0;

    std::vector<float> frameBuf;
    std::deque<float> recentFreqs;

    float lastFreq = 0.0f;
    bool voicedFlag = false;
};

} // namespace skatetune
