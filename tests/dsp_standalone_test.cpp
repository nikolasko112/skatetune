// Pure C++ sanity tests for the SkateTune DSP core, zero JUCE dependency.
// Build: g++ -std=c++17 -O2 -I.. dsp_standalone_test.cpp -o dsp_standalone_test
#include <cstdio>
#include <cmath>
#include <vector>
#include "../Source/dsp/PitchTracker.h"
#include "../Source/dsp/ScaleQuantizer.h"
#include "../Source/dsp/RetuneSmoother.h"
#include "../Source/dsp/GranularPitchShifter.h"
#include "../Source/dsp/TrickEventDetector.h"
#include "../Source/dsp/HumanizeController.h"
#include "../Source/dsp/FormantTilt.h"

using namespace skatetune;

static int failures = 0;

static void check (bool cond, const char* name, double actual = 0, double expected = 0)
{
    if (cond)
    {
        std::printf ("  [PASS] %s\n", name);
    }
    else
    {
        std::printf ("  [FAIL] %s (actual=%.4f expected~%.4f)\n", name, actual, expected);
        ++failures;
    }
}

static std::vector<float> makeSine (double freqHz, double sampleRate, double seconds, float amp = 0.5f)
{
    size_t n = (size_t) (seconds * sampleRate);
    std::vector<float> v (n);
    for (size_t i = 0; i < n; ++i)
        v[i] = amp * (float) std::sin (2.0 * 3.14159265358979323846 * freqHz * (double) i / sampleRate);
    return v;
}

// Independent pitch measurement (average period between rising zero
// crossings, over a stable window) - deliberately NOT the plugin's own
// PitchTracker, so a shifter accuracy bug can't be masked by also grading it
// with the same tool that would be fooled the same way. Precise to a
// fraction of a cent for a clean sine, which matters here: a shifter that
// silently applies ~0 correction instead of the intended one can still land
// within a loose percentage tolerance, but not within a tight cents one.
static double measureFreqZeroCross (const std::vector<float>& x, size_t from, size_t to, double sr)
{
    std::vector<double> crossings;
    for (size_t i = from + 1; i < to; ++i)
        if (x[i - 1] <= 0.0f && x[i] > 0.0f)
        {
            double frac = (double) (-x[i - 1]) / ((double) x[i] - (double) x[i - 1]);
            crossings.push_back ((double) (i - 1) + frac);
        }
    if (crossings.size() < 3) return 0.0;
    double totalPeriod = crossings.back() - crossings.front();
    return sr / (totalPeriod / ((double) crossings.size() - 1));
}

static double centsBetween (double a, double b) { return 1200.0 * std::log2 (a / b); }

static void testPitchTracker()
{
    std::printf ("-- PitchTracker --\n");
    const double sr = 44100.0;
    PitchTracker tracker;
    tracker.prepare (sr);

    auto sine = makeSine (445.0, sr, 0.5);
    float lastFreq = 0.0f;
    bool anyVoiced = false;
    for (float s : sine)
    {
        if (tracker.pushSample (s) && tracker.isVoiced())
        {
            lastFreq = tracker.getFrequencyHz();
            anyVoiced = true;
        }
    }
    check (anyVoiced, "detects voiced signal on a clean sine");
    check (std::abs (lastFreq - 445.0f) < 445.0 * 0.03, "tracks 445 Hz sine within 3%", lastFreq, 445.0);

    // low-level noise / silence should not be called voiced
    PitchTracker tracker2;
    tracker2.prepare (sr);
    bool falsePositive = false;
    for (int i = 0; i < (int) (0.3 * sr); ++i)
        if (tracker2.pushSample (0.0f) && tracker2.isVoiced())
            falsePositive = true;
    check (! falsePositive, "silence is not reported as voiced");
}

static void testScaleQuantizer()
{
    std::printf ("-- ScaleQuantizer --\n");
    ScaleQuantizer q;
    q.setKeyRoot (0); // C
    q.setScale (ScaleType::Major); // C D E F G A B

    // slightly sharp C4 (midi 60) should snap to C4 exactly
    double targetFreq = 0.0, corr = 0.0;
    double sharpC4 = ScaleQuantizer::midiToFreq (60.3);
    q.process (sharpC4, targetFreq, corr);
    check (std::abs (targetFreq - ScaleQuantizer::midiToFreq (60.0)) < 0.5,
           "slightly sharp C snaps to C", targetFreq, ScaleQuantizer::midiToFreq (60.0));
    check (std::abs (corr - (-0.3)) < 0.05, "correction ~ -0.3 semitones for sharp C", corr, -0.3);

    // D4 (midi 62) is already in C major -> near zero correction
    double dFreq = ScaleQuantizer::midiToFreq (62.0);
    q.process (dFreq, targetFreq, corr);
    check (std::abs (corr) < 0.01, "in-scale note gets ~0 correction", corr, 0.0);

    // C#4 (midi 61) is NOT in C major -> should move ~1 semitone to C or D
    double csFreq = ScaleQuantizer::midiToFreq (61.0);
    q.process (csFreq, targetFreq, corr);
    check (std::abs (std::abs (corr) - 1.0) < 0.01, "out-of-scale note moves ~1 semitone", std::abs (corr), 1.0);
}

static void testGranularPitchShifter()
{
    std::printf ("-- GranularPitchShifter --\n");
    const double sr = 44100.0;

    // Pass-through check: ratio 1.0 should keep ~440 Hz recognizable
    {
        GranularPitchShifter shifter;
        shifter.prepare (sr);
        shifter.setRatio (1.0);
        auto sine = makeSine (440.0, sr, 1.0);
        std::vector<float> out (sine.size());
        for (size_t i = 0; i < sine.size(); ++i)
            out[i] = shifter.processSample (sine[i]);

        PitchTracker tracker;
        tracker.prepare (sr);
        float lastFreq = 0.0f;
        for (size_t i = sine.size() / 2; i < sine.size(); ++i) // skip the priming/startup region
            if (tracker.pushSample (out[i]) && tracker.isVoiced())
                lastFreq = tracker.getFrequencyHz();

        check (std::abs (lastFreq - 440.0f) < 440.0 * 0.05, "ratio=1.0 keeps ~440 Hz", lastFreq, 440.0);
    }

    // Octave-up check: ratio 2.0 on a 440 Hz sine should read back ~880 Hz
    {
        GranularPitchShifter shifter;
        shifter.prepare (sr);
        shifter.setRatio (2.0);
        auto sine = makeSine (440.0, sr, 1.0);
        std::vector<float> out (sine.size());
        for (size_t i = 0; i < sine.size(); ++i)
            out[i] = shifter.processSample (sine[i]);

        PitchTracker tracker;
        tracker.prepare (sr);
        tracker.setFrequencyRange (70.0f, 2000.0f);
        float lastFreq = 0.0f;
        for (size_t i = sine.size() / 2; i < sine.size(); ++i)
            if (tracker.pushSample (out[i]) && tracker.isVoiced())
                lastFreq = tracker.getFrequencyHz();

        check (std::abs (lastFreq - 880.0f) < 880.0 * 0.10, "ratio=2.0 shifts ~440 Hz to ~880 Hz", lastFreq, 880.0);
    }

    // Small-ratio accuracy, measured precisely (not via the plugin's own
    // tracker): this is the regime every *realistic* autotune correction
    // actually lives in (well under an octave, usually under a few
    // semitones) - a shifter that only gets ratio=1.0 and ratio=2.0 right
    // is not good enough, even if it passes the two checks above. Each of
    // these corresponds to a plausible in-use correction amount.
    {
        struct Case { double semitones; double tol; };
        const Case cases[] = {
            { 0.3,  20.0 }, { 1.0, 20.0 }, { -1.0, 20.0 }, { 2.0, 25.0 }, { -2.0, 25.0 }
        };
        for (auto& c : cases)
        {
            const double inFreq = 277.18; // C#4; a generic mid-range tone
            const double ratio = std::pow (2.0, c.semitones / 12.0);

            GranularPitchShifter shifter;
            shifter.prepare (sr);
            shifter.setRatio (ratio);
            auto sine = makeSine (inFreq, sr, 1.5);
            std::vector<float> out (sine.size());
            for (size_t i = 0; i < sine.size(); ++i)
                out[i] = shifter.processSample (sine[i]);

            double measured = measureFreqZeroCross (out, out.size() * 2 / 3, out.size(), sr);
            double expected = inFreq * ratio;
            double errCents = measured > 0.0 ? centsBetween (measured, expected) : 1.0e9;

            char name[128];
            std::snprintf (name, sizeof (name), "ratio for %+.1f semitone correction lands within %.0f cents",
                           c.semitones, c.tol);
            check (std::abs (errCents) < c.tol, name, errCents, 0.0);
        }
    }
}

static void testRetuneSmoother()
{
    std::printf ("-- RetuneSmoother --\n");
    RetuneSmoother sm;
    sm.prepare (44100.0);

    sm.setRetuneSpeed (100.0); // near-instant
    sm.reset (0.0);
    double v = 0.0;
    for (int i = 0; i < 50; ++i) v = sm.process (1.0);
    check (v > 0.95, "fast retune speed snaps quickly", v, 1.0);

    sm.setRetuneSpeed (0.0); // slow glide
    sm.reset (0.0);
    v = 0.0;
    for (int i = 0; i < 50; ++i) v = sm.process (1.0);
    check (v < 0.5, "slow retune speed glides, not snapped after 50 samples", v, 0.0);
}

static void testPitchTrackerLiveRangeChange()
{
    std::printf ("-- PitchTracker (live setFrequencyRange) --\n");
    const double sr = 44100.0;

    // This is the exact bug class the fix targets: setFrequencyRange() used
    // to only store the Hz values, never recomputing the actual lag search
    // window after prepare() had already run - so a live range change (as
    // "Input Type" does, once per block) was a complete no-op in practice.
    //
    // Proven here with a real, empirically-confirmed octave error rather
    // than a hand-derived case: a 1500 Hz tone, searched only within the
    // default ~70-1000 Hz range, autocorrelates just as strongly an octave
    // down and is mismeasured as ~750 Hz - a genuine failure mode plain
    // autocorrelation pitch tracking has, and exactly what a correct
    // "Input Type" is supposed to fix by narrowing the search to where the
    // source actually lives. If setFrequencyRange() were still a no-op
    // after prepare() (the bug), live-widening the range could never change
    // that reading - so a tracker that measures the SAME tone correctly at
    // ~1500 Hz once widened is direct proof the live call took effect.
    auto highTone = makeSine (1500.0, sr, 0.5);

    PitchTracker aliasedTracker;
    aliasedTracker.prepare (sr); // default range ~70-1000 Hz
    float aliasedMeasured = 0.0f;
    for (float s : highTone)
        if (aliasedTracker.pushSample (s) && aliasedTracker.isVoiced())
            aliasedMeasured = aliasedTracker.getFrequencyHz();
    check (std::abs (aliasedMeasured - 750.0f) < 750.0f * 0.05f,
           "default range mismeasures 1500 Hz as an octave-down alias (~750 Hz)", aliasedMeasured, 750.0);

    PitchTracker fixedTracker;
    fixedTracker.prepare (sr);
    fixedTracker.setFrequencyRange (900.0f, 2000.0f); // live-widen upward, away from the original 70-1000 default
    float measured = 0.0f;
    bool everVoiced = false;
    for (float s : highTone)
        if (fixedTracker.pushSample (s) && fixedTracker.isVoiced())
        {
            measured = fixedTracker.getFrequencyHz();
            everVoiced = true;
        }
    check (everVoiced, "the same 1500 Hz tone is voiced once live-widened into range");
    check (std::abs (measured - 1500.0f) < 1500.0f * 0.05f,
           "...and is now measured correctly at ~1500 Hz, not the ~750 Hz alias", measured, 1500.0);

    // Calling it again with the SAME values must stay cheap/harmless (no
    // reallocation, no state reset) - this is the "called every block"
    // case, which is the common case in actual use.
    fixedTracker.setFrequencyRange (900.0f, 2000.0f);
    bool stillVoiced = false;
    for (float s : highTone)
        if (fixedTracker.pushSample (s) && fixedTracker.isVoiced())
            stillVoiced = true;
    check (stillVoiced, "repeating the same range is harmless and tracking still works");
}

static void testHumanizeController()
{
    std::printf ("-- HumanizeController --\n");
    const double sr = 44100.0;
    const double userSpeed = 70.0;

    // amount = 0: must always return exactly the user's speed, regardless
    // of how the tracked pitch moves - "Humanize off" must mean off.
    {
        HumanizeController hc;
        hc.prepare (sr);
        hc.setAmount (0.0);
        hc.onNewPitchHop (60.0);
        double eff1 = 0.0;
        for (int i = 0; i < 1000; ++i) eff1 = hc.getEffectiveRetuneSpeed (userSpeed);
        hc.onNewPitchHop (72.0); // a big jump
        double eff2 = hc.getEffectiveRetuneSpeed (userSpeed);
        check (std::abs (eff1 - userSpeed) < 0.001, "amount=0 holds exactly user speed while steady", eff1, userSpeed);
        check (std::abs (eff2 - userSpeed) < 0.001, "amount=0 holds exactly user speed right after a jump too", eff2, userSpeed);
    }

    // amount = 100: a note held steady for a while should relax toward 0;
    // the instant a real jump is seen, it must snap back near full speed;
    // then, held steady again, it should relax back down.
    {
        HumanizeController hc;
        hc.prepare (sr);
        hc.setAmount (100.0);
        hc.onNewPitchHop (60.0);

        double settled = 0.0;
        for (int i = 0; i < (int) (0.3 * sr); ++i) settled = hc.getEffectiveRetuneSpeed (userSpeed);
        check (settled < userSpeed * 0.05, "amount=100, held steady, relaxes close to 0", settled, 0.0);

        hc.onNewPitchHop (64.0); // 4-semitone jump: a real note change
        double rightAfterJump = hc.getEffectiveRetuneSpeed (userSpeed);
        check (rightAfterJump > userSpeed * 0.8, "amount=100, right after a jump, snaps back near full speed", rightAfterJump, userSpeed);

        double resettled = 0.0;
        for (int i = 0; i < (int) (1.5 * sr); ++i) resettled = hc.getEffectiveRetuneSpeed (userSpeed);
        check (resettled < userSpeed * 0.1, "amount=100, held steady again, relaxes back down", resettled, 0.0);
    }
}

static void testFormantTilt()
{
    std::printf ("-- FormantTilt --\n");
    const double sr = 44100.0;

    auto rmsGain = [sr] (double amount, double freqHz) -> double
    {
        FormantTilt tilt;
        tilt.prepare (sr);
        tilt.setAmount (amount);
        auto sine = makeSine (freqHz, sr, 0.3, 0.5f);
        double inRms = 0.0, outRms = 0.0;
        size_t from = sine.size() / 3; // skip filter settling
        for (size_t i = from; i < sine.size(); ++i)
        {
            float out = tilt.processSample (sine[i]);
            inRms += (double) sine[i] * (double) sine[i];
            outRms += (double) out * (double) out;
        }
        return std::sqrt (outRms) / std::sqrt (inRms);
    };

    check (std::abs (rmsGain (0.0, 200.0) - 1.0) < 0.05, "amount=0 leaves a low tone unchanged (gain ~1)");
    check (std::abs (rmsGain (0.0, 8000.0) - 1.0) < 0.05, "amount=0 leaves a high tone unchanged (gain ~1)");

    double lowGainBright  = rmsGain (100.0, 200.0);
    double highGainBright = rmsGain (100.0, 8000.0);
    check (lowGainBright < 1.15, "+100 (brightest) barely touches a low tone well under the tilt centre", lowGainBright, 1.0);
    check (highGainBright > 1.3, "+100 (brightest) clearly boosts a high tone above the tilt centre", highGainBright, 1.5);

    double lowGainDark  = rmsGain (-100.0, 200.0);
    double highGainDark = rmsGain (-100.0, 8000.0);
    check (lowGainDark < 1.15, "-100 (darkest) barely touches a low tone well under the tilt centre", lowGainDark, 1.0);
    check (highGainDark < 0.7, "-100 (darkest) clearly cuts a high tone above the tilt centre", highGainDark, 0.5);
}

static void testClassicInterpolationMode()
{
    std::printf ("-- GranularPitchShifter (Classic/nearest-neighbour mode) --\n");
    const double sr = 44100.0;

    // Classic mode is allowed to be grittier than Modern, but it must still
    // be a recognisable pitch shift in roughly the right place, not noise.
    GranularPitchShifter shifter;
    shifter.prepare (sr);
    shifter.setInterpolationQuality (false); // Classic: nearest-neighbour
    const double ratio = std::pow (2.0, 2.0 / 12.0); // +2 semitones
    const double inFreq = 277.18;
    auto sine = makeSine (inFreq, sr, 1.5);
    shifter.setRatio (ratio);
    std::vector<float> out (sine.size());
    for (size_t i = 0; i < sine.size(); ++i)
        out[i] = shifter.processSample (sine[i]);

    double measured = measureFreqZeroCross (out, out.size() * 2 / 3, out.size(), sr);
    double expected = inFreq * ratio;
    double errCents = measured > 0.0 ? centsBetween (measured, expected) : 1.0e9;
    check (std::abs (errCents) < 60.0, "Classic mode still lands a +2 semitone shift within 60 cents", errCents, 0.0);
}

static void testTrickEventDetector()
{
    std::printf ("-- TrickEventDetector --\n");
    const double sr = 1000.0; // easy to reason about: 1 sample = 1 ms
    TrickEventDetector det;
    det.prepare (sr);
    det.setThresholds (0.15, 0.60);
    det.setCooldownSeconds (0.35); // 350 samples at this fake sample rate

    int fireCount = 0;
    // Pattern: ramp above threshold, hold, drop low, repeat 3 times quickly (cooldown should suppress extras)
    for (int rep = 0; rep < 3; ++rep)
    {
        for (int i = 0; i < 20; ++i) if (det.update (0.8)) ++fireCount; // above high threshold
        for (int i = 0; i < 20; ++i) if (det.update (0.05)) ++fireCount; // drop below low threshold
    }
    check (fireCount >= 1 && fireCount <= 3, "cooldown prevents runaway firing", fireCount, 2);

    // Never crossing the high threshold should never fire
    TrickEventDetector det2;
    det2.prepare (sr);
    int fireCount2 = 0;
    for (int i = 0; i < 2000; ++i) if (det2.update (0.3)) ++fireCount2;
    check (fireCount2 == 0, "never fires if it never crosses the high threshold", fireCount2, 0);
}

static void testFullChainIntegration()
{
    std::printf ("-- Full chain integration (mimics processBlock) --\n");
    const double sr = 44100.0;

    PitchTracker tracker;      tracker.prepare (sr);
    ScaleQuantizer quant;      quant.setKeyRoot (0); quant.setScale (ScaleType::Major); // C major
    RetuneSmoother smoother;   smoother.prepare (sr); smoother.setRetuneSpeed (85.0);   // snappy
    GranularPitchShifter shifter; shifter.prepare (sr);
    TrickEventDetector detector;  detector.prepare (sr);
    detector.setThresholds (0.15, 0.60);
    detector.setCooldownSeconds (0.35);

    // A detuned "melody": a run of notes each ~0.35 semitones sharp of a C-major
    // scale tone, held 0.4s each - big enough corrections to be audible and to
    // cross the trick-trigger threshold on every note.
    std::vector<double> noteMidis = { 60.9, 64.9, 67.9, 72.9, 67.9, 64.9 };
    int triggerCount = 0;
    float lastRatioSeen = 1.0f;
    bool everCorrected = false;

    // IMPORTANT (this is the exact shape processBlock must use): the pitch
    // tracker only produces a new *raw* target every ~hop (10ms), but that
    // target must be held and fed into the smoother/shifter/detector on
    // EVERY sample - the smoother's one-pole coefficient is derived assuming
    // it is called at the full sample rate. Calling it only at hop-rate makes
    // "Retune Speed" ~40x slower than its knob position says.
    double rawCorrSemis = 0.0;

    for (double midi : noteMidis)
    {
        double freq = ScaleQuantizer::midiToFreq (midi);
        auto block = makeSine (freq, sr, 0.4, 0.4f);
        for (float x : block)
        {
            if (tracker.pushSample (x) && tracker.isVoiced())
            {
                double targetFreq = 0.0;
                quant.process ((double) tracker.getFrequencyHz(), targetFreq, rawCorrSemis);
            }

            double smoothed = smoother.process (rawCorrSemis);
            double ratio = std::pow (2.0, smoothed / 12.0);
            shifter.setRatio (ratio);
            lastRatioSeen = (float) ratio;
            if (std::abs (smoothed) > 0.05) everCorrected = true;
            if (detector.update (std::abs (smoothed)))
                ++triggerCount;

            float wet = shifter.processSample (x);
            (void) wet; // this is the actual plugin output sample in the real processBlock
        }
    }

    check (everCorrected, "chain actually computes a non-zero correction at some point");
    check (triggerCount >= (int) noteMidis.size() - 2,
           "a trick fires on roughly every strongly-detuned note", triggerCount, (double) noteMidis.size());
    check (std::abs (lastRatioSeen - 1.0f) < 0.2f, "pitch ratio stays in a sane range (<~3 semitones)", lastRatioSeen, 1.0);
}

int main()
{
    testPitchTracker();
    testPitchTrackerLiveRangeChange();
    testScaleQuantizer();
    testGranularPitchShifter();
    testClassicInterpolationMode();
    testRetuneSmoother();
    testHumanizeController();
    testFormantTilt();
    testTrickEventDetector();
    testFullChainIntegration();

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
