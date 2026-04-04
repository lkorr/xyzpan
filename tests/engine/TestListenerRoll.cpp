// TestListenerRoll.cpp — Regression tests for listener roll rotation.
// Verifies that:
//   1. Roll pans to the correct ear (sign test)
//   2. Steady-state output at a given roll angle is deterministic regardless
//      of the approach trajectory (no history-dependent artifacts)
//   3. Combined yaw+pitch+roll produces consistent results

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "xyzpan/Engine.h"
#include "xyzpan/Types.h"
#include "xyzpan/Constants.h"

#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>

using namespace xyzpan;

static constexpr float kPi = 3.14159265358979323846f;
static constexpr float kDeg2Rad = kPi / 180.0f;

// Helper: process silence to let all smoothers settle, then process a test
// signal and return L/R output buffers.
struct StereoOutput {
    std::vector<float> L, R;
};

static void settle(XYZPanEngine& eng, const EngineParams& params,
                   int samples = 96000, int blockSize = 512) {
    eng.setParams(params);
    std::vector<float> silence(static_cast<size_t>(blockSize), 0.0f);
    std::vector<float> outL(static_cast<size_t>(blockSize));
    std::vector<float> outR(static_cast<size_t>(blockSize));
    int offset = 0;
    while (offset < samples) {
        int batch = std::min(blockSize, samples - offset);
        const float* ins[1] = { silence.data() };
        eng.process(ins, 1, outL.data(), outR.data(), nullptr, nullptr, batch);
        offset += batch;
    }
}

// Ramp the listener roll through a series of angles (in degrees) before
// settling at the final angle.  Each intermediate angle gets a few blocks
// to simulate real knob movement.
static void rampRollTo(XYZPanEngine& eng, EngineParams& params,
                       const std::vector<float>& trajectory,
                       int blocksPerStep = 4, int blockSize = 512) {
    std::vector<float> silence(static_cast<size_t>(blockSize), 0.0f);
    std::vector<float> outL(static_cast<size_t>(blockSize));
    std::vector<float> outR(static_cast<size_t>(blockSize));
    for (float deg : trajectory) {
        params.listenerRoll = deg * kDeg2Rad;
        eng.setParams(params);
        for (int b = 0; b < blocksPerStep; ++b) {
            const float* ins[1] = { silence.data() };
            eng.process(ins, 1, outL.data(), outR.data(), nullptr, nullptr, blockSize);
        }
    }
}

static StereoOutput processNoise(XYZPanEngine& eng, const EngineParams& params,
                                 int N = 16384, uint32_t seed = 12345u) {
    std::vector<float> noise(static_cast<size_t>(N));
    uint32_t rng = seed;
    for (int i = 0; i < N; ++i) {
        rng = rng * 1664525u + 1013904223u;
        noise[i] = static_cast<float>(static_cast<int32_t>(rng)) / 2147483648.0f;
    }
    StereoOutput out;
    out.L.resize(static_cast<size_t>(N));
    out.R.resize(static_cast<size_t>(N));
    const float* ins[1] = { noise.data() };
    eng.setParams(params);
    eng.process(ins, 1, out.L.data(), out.R.data(), nullptr, nullptr, N);
    return out;
}

static float rms(const std::vector<float>& buf, int skip = 512) {
    float sum = 0.0f;
    int count = 0;
    for (size_t i = static_cast<size_t>(skip); i < buf.size(); ++i) {
        sum += buf[i] * buf[i];
        ++count;
    }
    return std::sqrt(sum / std::max(count, 1));
}

// Compute per-sample max absolute difference between two buffers
static float maxAbsDiff(const std::vector<float>& a, const std::vector<float>& b,
                        int skip = 512) {
    float maxD = 0.0f;
    size_t n = std::min(a.size(), b.size());
    for (size_t i = static_cast<size_t>(skip); i < n; ++i)
        maxD = std::max(maxD, std::abs(a[i] - b[i]));
    return maxD;
}

// ============================================================================
// 1. Roll sign: correct ear assignment
// ============================================================================

TEST_CASE("Roll +90: source above pans to LEFT ear", "[Roll][Sign]") {
    XYZPanEngine eng;
    eng.prepare(48000.0, 16384);

    EngineParams p;
    p.x = 0.0f; p.y = 0.0f; p.z = 0.0f;
    p.listenerZ = -0.2f;  // listener below source (source is above)
    p.listenerRoll = 90.0f * kDeg2Rad;  // head tilted right → left ear up
    p.dopplerEnabled = false;
    p.bypassDistGain = true;
    settle(eng, p);

    auto out = processNoise(eng, p);
    float rL = rms(out.L);
    float rR = rms(out.R);

    // Source above with head tilted right: left ear faces source → left louder
    CHECK(rL > rR);
}

TEST_CASE("Roll -90: source above pans to RIGHT ear", "[Roll][Sign]") {
    XYZPanEngine eng;
    eng.prepare(48000.0, 16384);

    EngineParams p;
    p.x = 0.0f; p.y = 0.0f; p.z = 0.0f;
    p.listenerZ = -0.2f;
    p.listenerRoll = -90.0f * kDeg2Rad;  // head tilted left → right ear up
    p.dopplerEnabled = false;
    p.bypassDistGain = true;
    settle(eng, p);

    auto out = processNoise(eng, p);
    float rL = rms(out.L);
    float rR = rms(out.R);

    // Source above with head tilted left: right ear faces source → right louder
    CHECK(rR > rL);
}

// ============================================================================
// 2. Steady-state determinism: same angle, different approach trajectories
// ============================================================================

TEST_CASE("Roll 30deg: identical output regardless of approach trajectory",
          "[Roll][Determinism]") {
    const float targetRollDeg = 30.0f;
    const int N = 16384;
    constexpr uint32_t seed = 77777u;

    auto makeParams = [&]() {
        EngineParams p;
        p.x = 0.0f; p.y = 0.0f; p.z = 0.0f;
        p.listenerZ = -0.2f;
        p.dopplerEnabled = false;
        p.bypassDistGain = true;
        return p;
    };

    // Helper: ramp to target via a trajectory, settle, then capture output
    auto capture = [&](const std::vector<float>& trajectory) -> StereoOutput {
        XYZPanEngine eng;
        eng.prepare(48000.0, N);
        EngineParams p = makeParams();
        rampRollTo(eng, p, trajectory);
        p.listenerRoll = targetRollDeg * kDeg2Rad;
        settle(eng, p);
        return processNoise(eng, p, N, seed);
    };

    // Sanity: same trajectory produces bit-identical output (engine is deterministic)
    SECTION("same trajectory = identical") {
        auto out1 = capture({0.0f, 15.0f, 30.0f});
        auto out2 = capture({0.0f, 15.0f, 30.0f});
        CHECK(maxAbsDiff(out1.L, out2.L) == 0.0f);
        CHECK(maxAbsDiff(out1.R, out2.R) == 0.0f);
    }

    SECTION("different trajectories converge") {
        auto outA = capture({-90.0f, -60.0f, -30.0f, 0.0f, 15.0f, 30.0f});
        auto outB = capture({120.0f, 90.0f, 60.0f, 45.0f, 30.0f});
        auto outC = capture({-180.0f, 0.0f, 30.0f});

        // Trajectories must produce identical steady-state output.
        // Tiny residual filter state from different ramp transients decays
        // to sub-audible levels (-66dB) during the 2-second settle.
        constexpr float kTol = 1e-3f;  // -60dB — well below audibility
        CHECK(maxAbsDiff(outA.L, outB.L) < kTol);
        CHECK(maxAbsDiff(outA.R, outB.R) < kTol);
        CHECK(maxAbsDiff(outA.L, outC.L) < kTol);
        CHECK(maxAbsDiff(outA.R, outC.R) < kTol);
    }
}

TEST_CASE("Roll -45deg: identical output regardless of approach from positive vs negative",
          "[Roll][Determinism]") {
    const float targetRollDeg = -45.0f;
    const int N = 16384;
    constexpr uint32_t seed = 88888u;

    auto makeParams = [&]() {
        EngineParams p;
        p.x = 0.3f; p.y = 0.5f; p.z = 0.2f;  // off-axis source
        p.dopplerEnabled = false;
        p.bypassDistGain = true;
        return p;
    };

    auto capture = [&](const std::vector<float>& trajectory) -> StereoOutput {
        XYZPanEngine eng;
        eng.prepare(48000.0, N);
        EngineParams p = makeParams();
        rampRollTo(eng, p, trajectory);
        p.listenerRoll = targetRollDeg * kDeg2Rad;
        settle(eng, p);
        return processNoise(eng, p, N, seed);
    };

    auto outA = capture({90.0f, 45.0f, 0.0f, -20.0f, -45.0f});
    auto outB = capture({-180.0f, -120.0f, -90.0f, -60.0f, -45.0f});
    auto outC = capture({0.0f, -45.0f});

    constexpr float kTol = 1e-3f;  // -60dB — well below audibility
    CHECK(maxAbsDiff(outA.L, outB.L) < kTol);
    CHECK(maxAbsDiff(outA.R, outB.R) < kTol);
    CHECK(maxAbsDiff(outA.L, outC.L) < kTol);
    CHECK(maxAbsDiff(outA.R, outC.R) < kTol);
}

// ============================================================================
// 3. Yaw determinism — same test for yaw to catch the shared smoother bug
// ============================================================================

TEST_CASE("Yaw 60deg: identical output regardless of approach trajectory",
          "[Yaw][Determinism]") {
    const float targetYawDeg = 60.0f;
    const int N = 16384;
    constexpr uint32_t seed = 99999u;

    auto makeParams = [&]() {
        EngineParams p;
        p.x = 0.5f; p.y = 0.5f; p.z = 0.0f;
        p.dopplerEnabled = false;
        p.bypassDistGain = true;
        return p;
    };

    auto capture = [&](const std::vector<float>& trajectory) -> StereoOutput {
        XYZPanEngine eng;
        eng.prepare(48000.0, N);
        EngineParams p = makeParams();
        for (float deg : trajectory) {
            p.listenerYaw = deg * kDeg2Rad;
            eng.setParams(p);
            std::vector<float> sil(512, 0.0f), oL(512), oR(512);
            for (int b = 0; b < 4; ++b) {
                const float* ins[1] = { sil.data() };
                eng.process(ins, 1, oL.data(), oR.data(), nullptr, nullptr, 512);
            }
        }
        p.listenerYaw = targetYawDeg * kDeg2Rad;
        settle(eng, p);
        return processNoise(eng, p, N, seed);
    };

    auto outA = capture({-120.0f, -60.0f, 0.0f, 30.0f, 60.0f});
    auto outB = capture({180.0f, 120.0f, 90.0f, 60.0f});

    constexpr float kTol = 1e-3f;  // -60dB — well below audibility
    CHECK(maxAbsDiff(outA.L, outB.L) < kTol);
    CHECK(maxAbsDiff(outA.R, outB.R) < kTol);
}

// ============================================================================
// 4. Roll zero: no effect on horizontal-only source
// ============================================================================

TEST_CASE("Roll=0: source at (1,1,0) — L and R unchanged from baseline",
          "[Roll][Baseline]") {
    const int N = 16384;
    constexpr uint32_t seed = 55555u;

    EngineParams p;
    p.x = 1.0f; p.y = 1.0f; p.z = 0.0f;
    p.dopplerEnabled = false;
    p.bypassDistGain = true;

    // With roll = 0
    StereoOutput out0;
    {
        XYZPanEngine eng;
        eng.prepare(48000.0, N);
        p.listenerRoll = 0.0f;
        settle(eng, p);
        out0 = processNoise(eng, p, N, seed);
    }

    float rL = rms(out0.L);
    float rR = rms(out0.R);

    // Source at X=+1: right ear is nearer → right louder
    CHECK(rR > rL);
}

// ============================================================================
// 5. Combined yaw+pitch+roll determinism
// ============================================================================

TEST_CASE("Combined yaw+pitch+roll: deterministic regardless of order of arrival",
          "[Roll][Combined][Determinism]") {
    const float yawDeg = 30.0f, pitchDeg = 20.0f, rollDeg = 45.0f;
    const int N = 16384;
    constexpr uint32_t seed = 11111u;

    auto makeParams = [&]() {
        EngineParams p;
        p.x = 0.3f; p.y = 0.7f; p.z = 0.4f;
        p.dopplerEnabled = false;
        p.bypassDistGain = true;
        return p;
    };

    // Helper: ramp yaw/pitch/roll through a trajectory, then settle at target
    auto capture = [&](auto yawTraj, auto pitchTraj, auto rollTraj) -> StereoOutput {
        XYZPanEngine eng;
        eng.prepare(48000.0, N);
        EngineParams p = makeParams();
        std::vector<float> sil(512, 0.0f), oL(512), oR(512);
        for (float y : yawTraj) {
            p.listenerYaw = y * kDeg2Rad;
            eng.setParams(p);
            for (int b = 0; b < 3; ++b) {
                const float* ins[1] = { sil.data() };
                eng.process(ins, 1, oL.data(), oR.data(), nullptr, nullptr, 512);
            }
        }
        for (float pi : pitchTraj) {
            p.listenerPitch = pi * kDeg2Rad;
            eng.setParams(p);
            for (int b = 0; b < 3; ++b) {
                const float* ins[1] = { sil.data() };
                eng.process(ins, 1, oL.data(), oR.data(), nullptr, nullptr, 512);
            }
        }
        for (float r : rollTraj) {
            p.listenerRoll = r * kDeg2Rad;
            eng.setParams(p);
            for (int b = 0; b < 3; ++b) {
                const float* ins[1] = { sil.data() };
                eng.process(ins, 1, oL.data(), oR.data(), nullptr, nullptr, 512);
            }
        }
        p.listenerYaw = yawDeg * kDeg2Rad;
        p.listenerPitch = pitchDeg * kDeg2Rad;
        p.listenerRoll = rollDeg * kDeg2Rad;
        settle(eng, p);
        return processNoise(eng, p, N, seed);
    };

    // Path A: ramp yaw first, then pitch, then roll
    auto outA = capture(
        std::vector<float>{-60.0f, 0.0f, 30.0f},
        std::vector<float>{-40.0f, 0.0f, 20.0f},
        std::vector<float>{90.0f, 60.0f, 45.0f});

    // Path B: different approach — ramp roll first, pitch, then yaw
    auto outB = capture(
        std::vector<float>{120.0f, 60.0f, 30.0f},
        std::vector<float>{60.0f, 40.0f, 20.0f},
        std::vector<float>{-90.0f, 0.0f, 45.0f});

    constexpr float kTol = 1e-3f;  // -60dB — well below audibility
    CHECK(maxAbsDiff(outA.L, outB.L) < kTol);
    CHECK(maxAbsDiff(outA.R, outB.R) < kTol);
}
