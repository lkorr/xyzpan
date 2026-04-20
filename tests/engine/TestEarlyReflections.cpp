// TestEarlyReflections.cpp
// Validates the image-source method delay math accounts for listener position.
// Bug previously: walls mirrored at fixed ±1 in listener-relative coords, giving
// catastrophically wrong delays when listener moved away from world origin.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "xyzpan/ERPipeline.h"
#include "xyzpan/Types.h"
#include "xyzpan/Constants.h"

#include <cmath>

using namespace xyzpan;

namespace {

// Drive an ERPipeline long enough for delaySmooth to settle on each target.
// Returns the smoothed delay (samples) for the requested wall index.
// Walls indexing matches ERPipeline: 0:+X, 1:-X, 2:+Y, 3:-Y, 4:+Z, 5:-Z.
float settleDelayForWall(ERPipeline& er,
                         float nodeX, float nodeY, float nodeZ,
                         float listenerX, float listenerY, float listenerZ,
                         float roomHalf, float sr,
                         int wallIndex,
                         int numSamples) {
    EngineParams params{};
    params.distGainMax = kDistGainMax;
    params.maxITD_ms = kDefaultMaxITD_ms;
    params.headShadowMinHz = kHeadShadowMinHz;
    params.headShadowFullOpenHz = kHeadShadowFullOpenHz;

    const float distGainTarget = 1.0f;
    const float ildGainBase = 1.0f;

    for (int i = 0; i < numSamples; ++i) {
        er.processSample(0.0f,
                         nodeX, nodeY, nodeZ,
                         listenerX, listenerY, listenerZ,
                         distGainTarget, sr, roomHalf,
                         ildGainBase, /*rotated=*/false,
                         1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
                         params);
    }
    return er.reflections[wallIndex].delaySmooth.current();
}

} // namespace

TEST_CASE("ER: listener at left wall produces short left-wall reflection delay", "[ER][ImageSource]") {
    constexpr float sr = 48000.0f;
    constexpr float roomHalf = 5.0f;

    // Listener 1 cm inside the left wall (-roomHalf + 0.01)
    const float listenerX = -roomHalf + 0.01f;
    const float listenerY = 0.0f;
    const float listenerZ = 0.0f;

    // Source 10 cm inside the left wall (just to the right of the listener)
    const float sourceWorldX = -roomHalf + 0.10f;
    const float nodeX = sourceWorldX - listenerX;  // 0.09 m
    const float nodeY = 0.0f;
    const float nodeZ = 0.0f;

    ERPipeline er;
    er.prepare(sr);

    // -X wall is index 1 (wallSign[1] = -1 on axis 0).
    // wRel = -roomHalf - listenerX = -5.0 - (-4.99) = -0.01
    // imgX = 2*(-0.01) - 0.09 = -0.11 → pathMeters = 0.11 m
    // Expected delay = 0.11 / 343 * 48000 ≈ 15.4 samples
    const float expectedPathMeters = 0.11f;
    const float expectedDelay = expectedPathMeters / kSpeedOfSound * sr;

    // Run long enough for exponential smoother (default ~kDefaultSmoothMs_ITD) to settle
    const float smoothedDelay = settleDelayForWall(
        er, nodeX, nodeY, nodeZ, listenerX, listenerY, listenerZ,
        roomHalf, sr, /*wallIndex=*/1, /*numSamples=*/8192);

    CHECK(smoothedDelay == Catch::Approx(expectedDelay).margin(1.0f));

    // Regression check: old buggy math produced pathNorm ≈ 2.09, pathMeters ≈ 10.45 m,
    // delay ≈ 1463 samples. The fix must be nowhere near that.
    CHECK(smoothedDelay < 100.0f);
}

TEST_CASE("ER: listener at left wall produces long right-wall reflection delay", "[ER][ImageSource]") {
    constexpr float sr = 48000.0f;
    constexpr float roomHalf = 5.0f;

    const float listenerX = -roomHalf + 0.01f;
    const float sourceWorldX = -roomHalf + 0.10f;
    const float nodeX = sourceWorldX - listenerX;  // 0.09 m

    ERPipeline er;
    er.prepare(sr);

    // +X wall is index 0. wRel = +roomHalf - listenerX = 5.0 - (-4.99) = 9.99
    // imgX = 2*9.99 - 0.09 = 19.89 → pathMeters ≈ 19.89 m
    // Expected delay = 19.89 / 343 * 48000 ≈ 2783 samples (~58 ms)
    const float expectedPathMeters = 19.89f;
    const float expectedDelay = expectedPathMeters / kSpeedOfSound * sr;
    const float delayCap = kERMaxDelayMs * 0.001f * sr;

    const float smoothedDelay = settleDelayForWall(
        er, nodeX, 0.0f, 0.0f, listenerX, 0.0f, 0.0f,
        roomHalf, sr, /*wallIndex=*/0, /*numSamples=*/16384);

    // Should be close to expected (within 2 samples) and below the hard cap
    CHECK(smoothedDelay == Catch::Approx(expectedDelay).margin(2.0f));
    CHECK(smoothedDelay < delayCap);
    CHECK(smoothedDelay > 100.0f);  // definitively in echo territory, unlike near-wall case
}

TEST_CASE("ER: listener at world origin matches pre-fix geometry (origin invariant)", "[ER][ImageSource]") {
    // When listener is at the origin, the new math should reduce to the same result
    // as the old normalized-then-scaled formula: wRel = sign * roomHalf, same as old
    // 2*sign*roomHalf mirror behavior would have yielded physically.
    constexpr float sr = 48000.0f;
    constexpr float roomHalf = 3.0f;

    const float nodeX = 1.0f;
    const float nodeY = 0.0f;
    const float nodeZ = 0.0f;

    ERPipeline er;
    er.prepare(sr);

    // -X wall (index 1): wRel = -3.0 - 0 = -3.0; imgX = -6 - 1 = -7 → pathMeters = 7 m
    const float expectedPathMeters = 7.0f;
    const float expectedDelay = expectedPathMeters / kSpeedOfSound * sr;

    const float smoothedDelay = settleDelayForWall(
        er, nodeX, nodeY, nodeZ, 0.0f, 0.0f, 0.0f,
        roomHalf, sr, /*wallIndex=*/1, /*numSamples=*/8192);

    CHECK(smoothedDelay == Catch::Approx(expectedDelay).margin(1.5f));
}
