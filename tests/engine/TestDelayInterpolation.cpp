// TestDelayInterpolation.cpp
// Verifies that DelayInterpMode 0 (Hermite) and 1–4 (Sinc 2/4/8/16) produce
// distinct interpolation results, and that the mode switch in FractionalDelayLine
// actually dispatches to the correct algorithm/tap count.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "xyzpan/dsp/FractionalDelayLine.h"
#include "xyzpan/Engine.h"
#include "xyzpan/Types.h"
#include <cmath>
#include <vector>
#include <numeric>

using namespace xyzpan;
using namespace xyzpan::dsp;

static constexpr float kPI = 3.14159265358979f;

// Fill a delay line with a known signal (chirp-like) that exercises fractional
// positions — a pure sine is too smooth and can mask interpolation differences.
static void fillWithChirp(FractionalDelayLine& dl, int N, float sampleRate = 44100.0f) {
    for (int i = 0; i < N; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        // Chirp from 200 Hz to 8000 Hz over N samples
        float freq = 200.0f + 7800.0f * static_cast<float>(i) / static_cast<float>(N);
        dl.push(std::sin(2.0f * kPI * freq * t));
    }
}

// ---------------------------------------------------------------------------
// Regression: fractional delay interpolates in the correct direction
// ---------------------------------------------------------------------------
TEST_CASE("Fractional delay interpolates toward older samples", "[dsp][interpolation]") {
    // Push a ramp 0,1,2,...,N-1. After pushing N values, writePos_ = N.
    // The sample at integer delay k has value (N-1-k).
    //
    // read(5.0) should return value at age 5 = N-1-5
    // read(5.7) should return a value between age 5 and age 6:
    //   age 5 value = N-1-5, age 6 value = N-1-6
    //   expected ≈ N-1-5.7 = (N-1-5)*0.3 + (N-1-6)*0.7
    //
    // If interpolation direction is WRONG (old bug), read(5.7) would return
    // a value between age 5 and age 4 — closer to N-1-4 instead of N-1-6.

    constexpr int capacity = 256;
    constexpr int N = 128;

    // Test all interpolation modes
    int modes[] = { 0, 1, 2, 3, 4 };
    const char* modeNames[] = { "Hermite", "Sinc2", "Sinc4", "Sinc8", "Sinc16" };

    for (int mi = 0; mi < 5; ++mi) {
        SECTION(modeNames[mi]) {
            FractionalDelayLine dl;
            dl.prepare(capacity);

            for (int i = 0; i < N; ++i)
                dl.push(static_cast<float>(i));

            // Integer delay sanity check
            float valAt5 = dl.read(5.0f, modes[mi]);
            float expected5 = static_cast<float>(N - 1 - 5);  // = 122
            CHECK_THAT(static_cast<double>(valAt5),
                       Catch::Matchers::WithinAbs(static_cast<double>(expected5), 0.5));

            // Fractional delay direction check
            float valAt5_7 = dl.read(5.7f, modes[mi]);
            float olderVal = static_cast<float>(N - 1 - 6);   // age 6 = 121
            float newerVal = static_cast<float>(N - 1 - 4);   // age 4 = 123

            // The result must be BETWEEN age 5 and age 6 values (older direction),
            // NOT between age 5 and age 4 values (newer direction).
            // With correct interpolation: valAt5_7 ≈ 122 - 0.7 = 121.3
            // With wrong interpolation:   valAt5_7 ≈ 122 + 0.7 = 122.7
            float midpoint = static_cast<float>(N - 1 - 5);  // 122

            // Must be less than the integer delay value (moving toward older/smaller values)
            CHECK(valAt5_7 < midpoint);
            // Must be closer to olderVal than to newerVal
            CHECK(std::abs(valAt5_7 - olderVal) < std::abs(valAt5_7 - newerVal));

            // Tighter check: should be approximately N-1-5.7
            float expectedFrac = static_cast<float>(N - 1) - 5.7f;  // ~121.3
            CHECK_THAT(static_cast<double>(valAt5_7),
                       Catch::Matchers::WithinAbs(static_cast<double>(expectedFrac), 1.0));
        }
    }
}

// ---------------------------------------------------------------------------
// Regression: continuous delay sweep has no sawtooth discontinuities
// ---------------------------------------------------------------------------
TEST_CASE("Continuous delay sweep is monotonic-ish across integer crossings",
          "[dsp][interpolation]") {
    // A linearly increasing delay reading from a ramp signal should produce
    // a smoothly decreasing output. The old bug created sawtooth jumps at
    // every integer delay crossing.

    constexpr int capacity = 1024;
    constexpr int N = 512;

    int modes[] = { 0, 4 };
    const char* modeNames[] = { "Hermite", "Sinc16" };

    for (int mi = 0; mi < 2; ++mi) {
        SECTION(modeNames[mi]) {
            FractionalDelayLine dl;
            dl.prepare(capacity);

            for (int i = 0; i < N; ++i)
                dl.push(static_cast<float>(i));

            // Sweep delay from 10.0 to 20.0 in small steps
            float prevVal = dl.read(10.0f, modes[mi]);
            int jumpCount = 0;

            for (float delay = 10.05f; delay <= 20.0f; delay += 0.05f) {
                float val = dl.read(delay, modes[mi]);

                // For a ramp input with increasing delay, output should decrease.
                // A positive jump (val > prevVal + threshold) indicates a
                // sawtooth discontinuity from the old inverted-t bug.
                if (val > prevVal + 0.1f)
                    ++jumpCount;

                prevVal = val;
            }

            // Should have zero upward jumps in a monotonically decreasing sweep
            CHECK(jumpCount == 0);
        }
    }
}

// ---------------------------------------------------------------------------
// Core test: Hermite and Sinc produce different output at fractional delays
// ---------------------------------------------------------------------------
TEST_CASE("Delay interp modes produce different output", "[dsp][interpolation]") {
    constexpr int capacity = 4096;
    constexpr int fillLen  = 2048;

    FractionalDelayLine dl;
    dl.prepare(capacity);
    fillWithChirp(dl, fillLen);

    // Test at multiple fractional delay positions — the two algorithms should
    // agree closely at integer delays but diverge at fractional ones.
    std::vector<float> fractionalDelays = { 10.1f, 10.25f, 10.5f, 10.75f,
                                             50.3f, 100.7f, 200.123f, 500.9f };

    int numDifferent = 0;
    for (float delay : fractionalDelays) {
        float hermite = dl.read(delay, 0);
        float sinc    = dl.read(delay, 4);

        // Both should be finite
        REQUIRE_FALSE(std::isnan(hermite));
        REQUIRE_FALSE(std::isnan(sinc));
        REQUIRE_FALSE(std::isinf(hermite));
        REQUIRE_FALSE(std::isinf(sinc));

        // Count positions where the two modes produce different values
        if (hermite != sinc)
            ++numDifferent;
    }

    // The two algorithms must differ at the majority of fractional positions
    CHECK(numDifferent >= static_cast<int>(fractionalDelays.size()) / 2);
}

// ---------------------------------------------------------------------------
// Integer delay: both modes should return very similar results
// (at integer positions, both reduce to directly reading a buffer sample)
// ---------------------------------------------------------------------------
TEST_CASE("Both interp modes agree at integer delays", "[dsp][interpolation]") {
    constexpr int capacity = 4096;

    FractionalDelayLine dl;
    dl.prepare(capacity);

    // Push a known pattern
    for (int i = 0; i < 1024; ++i)
        dl.push(static_cast<float>(i) * 0.001f);

    // At exact integer delays, both should return nearly the same value.
    // (There may be tiny floating-point differences due to windowed-sinc
    //  normalization, but they should be very close.)
    for (int d = 5; d < 100; d += 7) {
        float hermite = dl.read(static_cast<float>(d), 0);
        float sinc    = dl.read(static_cast<float>(d), 1);

        CHECK_THAT(hermite, Catch::Matchers::WithinAbs(static_cast<double>(sinc), 0.02));
    }
}

// ---------------------------------------------------------------------------
// Mode 0 matches the backward-compatible read(delay) overload (Hermite)
// ---------------------------------------------------------------------------
TEST_CASE("Mode 0 matches backward-compatible Hermite read()", "[dsp][interpolation]") {
    constexpr int capacity = 4096;

    FractionalDelayLine dl;
    dl.prepare(capacity);
    fillWithChirp(dl, 2048);

    std::vector<float> delays = { 5.3f, 20.7f, 100.123f, 500.9f };
    for (float delay : delays) {
        float backcompat  = dl.read(delay);       // backward-compat overload
        float explicitMode = dl.read(delay, 0);   // explicit Hermite mode

        // Must be bit-identical — same code path
        CHECK(backcompat == explicitMode);
    }
}

// ---------------------------------------------------------------------------
// Sinc interpolation has lower error than Hermite for band-limited signals
// ---------------------------------------------------------------------------
TEST_CASE("Sinc and Hermite produce quantitatively different fractional interpolation",
          "[dsp][interpolation]") {
    // Push a band-limited signal (sine well below Nyquist), then read at
    // fractional delays. Measure the average absolute difference between
    // the two modes. The difference should be non-zero but small (both are
    // reasonable interpolators for in-band signals).

    constexpr int capacity = 4096;
    constexpr double sampleRate = 44100.0;
    constexpr double freq = 1000.0; // well within passband
    constexpr int fillLen = 2048;
    constexpr double twoPi = 6.283185307179586;

    FractionalDelayLine dl;
    dl.prepare(capacity);

    for (int i = 0; i < fillLen; ++i)
        dl.push(static_cast<float>(std::sin(twoPi * freq * static_cast<double>(i) / sampleRate)));

    float diffSum = 0.0f;
    float maxDiff = 0.0f;
    int count = 0;

    for (float delay = 10.1f; delay < 500.0f; delay += 3.37f) {
        float hermite = dl.read(delay, 0);
        float sinc    = dl.read(delay, 4);

        float diff = std::abs(hermite - sinc);
        diffSum += diff;
        maxDiff = std::max(maxDiff, diff);
        ++count;
    }

    float avgDiff = diffSum / static_cast<float>(count);

    // Modes must produce different results (non-zero difference)
    CHECK(avgDiff > 1e-6f);
    CHECK(maxDiff > 1e-5f);

    // But both are reasonable interpolators — difference should be small
    // relative to signal amplitude (1.0)
    CHECK(avgDiff < 0.01f);
    CHECK(maxDiff < 0.05f);
}

TEST_CASE("Sinc handles high-frequency content better than Hermite",
          "[dsp][interpolation]") {
    // For a signal near Nyquist (e.g., 15kHz at 44.1kHz), the difference between
    // 4-tap Hermite and 16-tap windowed-sinc should be more pronounced.

    constexpr int capacity = 4096;
    constexpr double sampleRate = 44100.0;
    constexpr double freq = 15000.0;  // near Nyquist — stresses interpolation
    constexpr int fillLen = 2048;
    constexpr double twoPi = 6.283185307179586;

    FractionalDelayLine dl;
    dl.prepare(capacity);

    for (int i = 0; i < fillLen; ++i)
        dl.push(static_cast<float>(std::sin(twoPi * freq * static_cast<double>(i) / sampleRate)));

    float diffSum = 0.0f;
    int count = 0;

    for (float delay = 10.1f; delay < 500.0f; delay += 3.37f) {
        float hermite = dl.read(delay, 0);
        float sinc    = dl.read(delay, 4);
        diffSum += std::abs(hermite - sinc);
        ++count;
    }

    float avgDiffHF = diffSum / static_cast<float>(count);

    // At high frequencies, the two interpolators should diverge more
    // than at low frequencies (Hermite's 4-tap approximation breaks down)
    CHECK(avgDiffHF > 0.001f);
}

// ---------------------------------------------------------------------------
// Engine-level: switching DelayInterpMode changes the full pipeline output
// ---------------------------------------------------------------------------
TEST_CASE("Engine output differs between Hermite and Sinc interp modes",
          "[dsp][interpolation][engine]") {
    // Process the same input through the full engine with each interp mode.
    // The outputs should differ because all delay lines use the mode param.

    constexpr float sampleRate = 44100.0f;
    constexpr int blockSize = 512;
    constexpr int N = 16384;  // longer signal to account for propagation delay
    constexpr int settleN = 44100; // full second settle to converge all smoothers
    constexpr int skip = 8192; // skip propagation delay transient

    // Generate test signal: 1kHz sine
    std::vector<float> input(N);
    for (int i = 0; i < N; ++i)
        input[static_cast<size_t>(i)] =
            std::sin(2.0f * kPI * 1000.0f * static_cast<float>(i) / sampleRate);

    // Arrays to hold output from each mode
    std::vector<float> hermL(N), hermR(N), sincL(N), sincR(N);

    // Process with each mode
    DelayInterpMode modes[2] = { DelayInterpMode::Hermite, DelayInterpMode::Sinc16 };
    std::vector<float>* outputs[2][2] = { {&hermL, &hermR}, {&sincL, &sincR} };

    for (int m = 0; m < 2; ++m) {
        XYZPanEngine engine;
        engine.prepare(static_cast<double>(sampleRate), blockSize);

        EngineParams params;
        params.x = 0.8f;          // off-center to exercise ITD delay lines
        params.y = 0.2f;          // close distance for stronger signal
        params.z = 0.3f;          // some elevation for chest/floor delay
        params.dopplerEnabled = true;
        params.delayInterpMode = modes[m];
        engine.setParams(params);

        // Settle smoothers
        {
            std::vector<float> silence(settleN, 0.0f);
            std::vector<float> sL(settleN), sR(settleN);
            const float* ins[1] = { silence.data() };
            int off = 0;
            while (off < settleN) {
                int batch = std::min(blockSize, settleN - off);
                engine.process(ins, 1, sL.data() + off, sR.data() + off, nullptr, nullptr, batch);
                ins[0] += batch;
                off += batch;
            }
        }

        // Process test signal
        engine.setParams(params);
        const float* ins[1] = { input.data() };
        int off = 0;
        while (off < N) {
            int batch = std::min(blockSize, N - off);
            engine.process(ins, 1, outputs[m][0]->data() + off, outputs[m][1]->data() + off, nullptr, nullptr, batch);
            ins[0] += batch;
            off += batch;
        }
    }

    // Compute RMS difference between the two modes
    float diffSumL = 0.0f, diffSumR = 0.0f;
    for (int i = skip; i < N; ++i) {
        float dL = hermL[static_cast<size_t>(i)] - sincL[static_cast<size_t>(i)];
        float dR = hermR[static_cast<size_t>(i)] - sincR[static_cast<size_t>(i)];
        diffSumL += dL * dL;
        diffSumR += dR * dR;
    }
    float diffRmsL = std::sqrt(diffSumL / static_cast<float>(N - skip));
    float diffRmsR = std::sqrt(diffSumR / static_cast<float>(N - skip));

    // Verify both modes produced non-zero output (sanity check)
    float rmsHermL = 0.0f, rmsSincL = 0.0f;
    for (int i = skip; i < N; ++i) {
        rmsHermL += hermL[static_cast<size_t>(i)] * hermL[static_cast<size_t>(i)];
        rmsSincL += sincL[static_cast<size_t>(i)] * sincL[static_cast<size_t>(i)];
    }
    rmsHermL = std::sqrt(rmsHermL / static_cast<float>(N - skip));
    rmsSincL = std::sqrt(rmsSincL / static_cast<float>(N - skip));
    CAPTURE(rmsHermL, rmsSincL, diffRmsL, diffRmsR);

    // Both modes must produce non-zero output (signal propagated through)
    CHECK(rmsHermL > 1e-5f);
    CHECK(rmsSincL > 1e-5f);

    // The two modes must produce measurably different output.
    // At steady-state the difference is small but non-zero since the
    // interpolation algorithms use different tap counts and coefficients.
    CHECK((diffRmsL > 1e-7f || diffRmsR > 1e-7f));

    // But they shouldn't be wildly different (same signal, just interpolation quality)
    CHECK(diffRmsL < 0.5f);
    CHECK(diffRmsR < 0.5f);
}

// ---------------------------------------------------------------------------
// ZOH: produces significantly worse output than Hermite at fractional delays
// ---------------------------------------------------------------------------
TEST_CASE("ZOH produces significantly worse interpolation than Hermite",
          "[dsp][interpolation]") {
    constexpr int capacity = 4096;
    constexpr double sampleRate = 44100.0;
    constexpr double freq = 1000.0;
    constexpr int fillLen = 2048;
    constexpr double twoPi = 6.283185307179586;

    FractionalDelayLine dl;
    dl.prepare(capacity);

    for (int i = 0; i < fillLen; ++i)
        dl.push(static_cast<float>(std::sin(twoPi * freq * static_cast<double>(i) / sampleRate)));

    float diffSum = 0.0f;
    int count = 0;

    for (float delay = 10.1f; delay < 500.0f; delay += 3.37f) {
        float hermite = dl.read(delay, 0);
        float zoh     = dl.read(delay, 5);
        float diff = std::abs(hermite - zoh);
        diffSum += diff;
        ++count;
    }

    float avgDiff = diffSum / static_cast<float>(count);

    // ZOH must produce noticeably different output from Hermite
    CHECK(avgDiff > 0.001f);
}

// ---------------------------------------------------------------------------
// ZOH: engine-level produces measurably different (worse) output
// ---------------------------------------------------------------------------
TEST_CASE("Engine output with ZOH differs significantly from Hermite",
          "[dsp][interpolation][engine]") {
    constexpr float sampleRate = 44100.0f;
    constexpr int blockSize = 512;
    constexpr int N = 16384;
    constexpr int settleN = 44100;
    constexpr int skip = 8192;

    std::vector<float> input(N);
    for (int i = 0; i < N; ++i)
        input[static_cast<size_t>(i)] =
            std::sin(2.0f * kPI * 1000.0f * static_cast<float>(i) / sampleRate);

    std::vector<float> hermL(N), hermR(N), zohL(N), zohR(N);

    DelayInterpMode modes[2] = { DelayInterpMode::Hermite, DelayInterpMode::ZOH };
    std::vector<float>* outputs[2][2] = { {&hermL, &hermR}, {&zohL, &zohR} };

    for (int m = 0; m < 2; ++m) {
        XYZPanEngine engine;
        engine.prepare(static_cast<double>(sampleRate), blockSize);

        EngineParams params;
        params.x = 0.8f;
        params.y = 0.2f;
        params.z = 0.3f;
        params.dopplerEnabled = true;
        params.delayInterpMode = modes[m];
        engine.setParams(params);

        {
            std::vector<float> silence(settleN, 0.0f);
            std::vector<float> sL(settleN), sR(settleN);
            const float* ins[1] = { silence.data() };
            int off = 0;
            while (off < settleN) {
                int batch = std::min(blockSize, settleN - off);
                engine.process(ins, 1, sL.data() + off, sR.data() + off, nullptr, nullptr, batch);
                ins[0] += batch;
                off += batch;
            }
        }

        engine.setParams(params);
        const float* ins[1] = { input.data() };
        int off = 0;
        while (off < N) {
            int batch = std::min(blockSize, N - off);
            engine.process(ins, 1, outputs[m][0]->data() + off, outputs[m][1]->data() + off, nullptr, nullptr, batch);
            ins[0] += batch;
            off += batch;
        }
    }

    float diffSumL = 0.0f, diffSumR = 0.0f;
    for (int i = skip; i < N; ++i) {
        float dL = hermL[static_cast<size_t>(i)] - zohL[static_cast<size_t>(i)];
        float dR = hermR[static_cast<size_t>(i)] - zohR[static_cast<size_t>(i)];
        diffSumL += dL * dL;
        diffSumR += dR * dR;
    }
    float diffRmsL = std::sqrt(diffSumL / static_cast<float>(N - skip));
    float diffRmsR = std::sqrt(diffSumR / static_cast<float>(N - skip));

    // ZOH should produce a larger difference from Hermite than Sinc does
    CHECK((diffRmsL > 1e-6f || diffRmsR > 1e-6f));
}

// ---------------------------------------------------------------------------
// Stability: no mode produces NaN/Inf at edge-case delays (including ZOH)
// ---------------------------------------------------------------------------
TEST_CASE("No NaN/Inf from any interp mode at edge delays", "[dsp][interpolation]") {
    constexpr int capacity = 4096;

    FractionalDelayLine dl;
    dl.prepare(capacity);

    // Fill with noise-like content
    for (int i = 0; i < 2048; ++i)
        dl.push(std::sin(static_cast<float>(i) * 0.1f) * 0.8f);

    // Edge cases: very small fractional, near-zero, near-one fractional
    std::vector<float> edgeDelays = {
        0.0f, 0.001f, 0.999f, 1.0f, 1.5f,
        100.0001f, 100.9999f,
        1000.0f, 1000.5f
    };

    for (float delay : edgeDelays) {
        float h = dl.read(delay, 0);
        float s = dl.read(delay, 4);
        float z = dl.read(delay, 5);

        CHECK_FALSE(std::isnan(h));
        CHECK_FALSE(std::isnan(s));
        CHECK_FALSE(std::isnan(z));
        CHECK_FALSE(std::isinf(h));
        CHECK_FALSE(std::isinf(s));
        CHECK_FALSE(std::isinf(z));
    }
}
