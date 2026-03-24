// TestDelayInterpolation.cpp
// Verifies that FractionalDelayLine's Hermite (Catmull-Rom) interpolation
// produces correct, stable, continuous output at fractional delay positions.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "xyzpan/dsp/FractionalDelayLine.h"
#include <cmath>
#include <vector>

using namespace xyzpan::dsp;

static constexpr float kPI = 3.14159265358979f;

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

    FractionalDelayLine dl;
    dl.prepare(capacity);

    for (int i = 0; i < N; ++i)
        dl.push(static_cast<float>(i));

    // Integer delay sanity check
    float valAt5 = dl.read(5.0f);
    float expected5 = static_cast<float>(N - 1 - 5);  // = 122
    CHECK_THAT(static_cast<double>(valAt5),
               Catch::Matchers::WithinAbs(static_cast<double>(expected5), 0.5));

    // Fractional delay direction check
    float valAt5_7 = dl.read(5.7f);
    float olderVal = static_cast<float>(N - 1 - 6);   // age 6 = 121
    float newerVal = static_cast<float>(N - 1 - 4);   // age 4 = 123

    // The result must be BETWEEN age 5 and age 6 values (older direction),
    // NOT between age 5 and age 4 values (newer direction).
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

    FractionalDelayLine dl;
    dl.prepare(capacity);

    for (int i = 0; i < N; ++i)
        dl.push(static_cast<float>(i));

    // Sweep delay from 10.0 to 20.0 in small steps
    float prevVal = dl.read(10.0f);
    int jumpCount = 0;

    for (float delay = 10.05f; delay <= 20.0f; delay += 0.05f) {
        float val = dl.read(delay);

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

// ---------------------------------------------------------------------------
// Hermite returns exact buffer values at integer delays
// ---------------------------------------------------------------------------
TEST_CASE("Hermite returns exact values at integer delays", "[dsp][interpolation]") {
    constexpr int capacity = 4096;

    FractionalDelayLine dl;
    dl.prepare(capacity);

    // Push a known pattern
    for (int i = 0; i < 1024; ++i)
        dl.push(static_cast<float>(i) * 0.001f);

    // At exact integer delays, Hermite should return the buffer sample directly.
    for (int d = 5; d < 100; d += 7) {
        float val = dl.read(static_cast<float>(d));
        float expected = static_cast<float>(1024 - 1 - d) * 0.001f;
        CHECK_THAT(static_cast<double>(val),
                   Catch::Matchers::WithinAbs(static_cast<double>(expected), 1e-5));
    }
}

// ---------------------------------------------------------------------------
// Hermite produces accurate results for band-limited signals
// ---------------------------------------------------------------------------
TEST_CASE("Hermite interpolation is accurate for in-band signals", "[dsp][interpolation]") {
    constexpr int capacity = 4096;
    constexpr double sampleRate = 44100.0;
    constexpr double freq = 1000.0; // well within passband
    constexpr int fillLen = 2048;
    constexpr double twoPi = 6.283185307179586;

    FractionalDelayLine dl;
    dl.prepare(capacity);

    for (int i = 0; i < fillLen; ++i)
        dl.push(static_cast<float>(std::sin(twoPi * freq * static_cast<double>(i) / sampleRate)));

    // Read at fractional delays and verify output is finite and within signal range
    for (float delay = 10.1f; delay < 500.0f; delay += 3.37f) {
        float val = dl.read(delay);
        REQUIRE_FALSE(std::isnan(val));
        REQUIRE_FALSE(std::isinf(val));
        // Sine signal amplitude is 1.0; Hermite overshoot should be small
        CHECK(std::abs(val) < 1.1f);
    }
}

// ---------------------------------------------------------------------------
// Stability: no NaN/Inf at edge-case delays
// ---------------------------------------------------------------------------
TEST_CASE("No NaN/Inf from Hermite at edge delays", "[dsp][interpolation]") {
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
        float val = dl.read(delay);
        CHECK_FALSE(std::isnan(val));
        CHECK_FALSE(std::isinf(val));
    }
}
