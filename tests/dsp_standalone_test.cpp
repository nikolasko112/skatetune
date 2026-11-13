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
    testScaleQuantizer();
    testGranularPitchShifter();
    testRetuneSmoother();
    testTrickEventDetector();
    testFullChainIntegration();

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
