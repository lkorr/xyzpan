// TestSampleRateAdaptation.cpp
// Regression tests for INFRA-03: sample rate adaptation correctness.
// Verifies that Engine::prepare(newSampleRate, blockSize) causes all subsequent
// process() calls to use the new sample rate correctly — no stale coefficients,
// no NaN/Inf, no silence from coefficient explosion.
//
// Requirements: INFRA-03

#include <catch2/catch_test_macros.hpp>
#include "xyzpan/Engine.h"
#include "xyzpan/Types.h"
#include "xyzpan/Constants.h"
#include <cmath>
#include <vector>
#include <limits>

using namespace xyzpan;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static float rmsOf(const float* buf, int n) {
    float sum = 0.0f;
    for (int i = 0; i < n; ++i)
        sum += buf[i] * buf[i];
    return (n > 0) ? std::sqrt(sum / static_cast<float>(n)) : 0.0f;
}

static bool allFinite(const float* buf, int n) {
    for (int i = 0; i < n; ++i)
        if (!std::isfinite(buf[i])) return false;
    return true;
}

// Generate N samples of deterministic white noise via LCG.
static std::vector<float> makeNoise(int N, uint32_t seed = 42u) {
    std::vector<float> v(static_cast<size_t>(N));
    uint32_t rng = seed;
    for (int i = 0; i < N; ++i) {
        rng = rng * 1664525u + 1013904223u;
        v[static_cast<size_t>(i)] =
            static_cast<float>(static_cast<int32_t>(rng)) / 2147483648.0f;
    }
    return v;
}

// Process numBlocks blocks and return the output in outL/outR vectors.
// Returns combined RMS of all output blocks.
static float processNBlocks(XYZPanEngine& engine, const EngineParams& params,
                             int blockSize, int numBlocks,
                             std::vector<float>* outLVec = nullptr,
                             std::vector<float>* outRVec = nullptr) {
    const std::vector<float> input = makeNoise(blockSize);
    const float* inputPtrs[1] = { input.data() };

    std::vector<float> outL(static_cast<size_t>(blockSize), 0.0f);
    std::vector<float> outR(static_cast<size_t>(blockSize), 0.0f);

    engine.setParams(params);
    float totalRms = 0.0f;

    for (int b = 0; b < numBlocks; ++b) {
        engine.process(inputPtrs, 1, outL.data(), outR.data(), nullptr, nullptr, blockSize);
        totalRms += rmsOf(outL.data(), blockSize) + rmsOf(outR.data(), blockSize);
    }

    if (outLVec) *outLVec = outL;
    if (outRVec) *outRVec = outR;
    return totalRms / static_cast<float>(numBlocks * 2);
}

// ---------------------------------------------------------------------------
// Test 1: prepare() with new sample rate produces finite, non-silent output
// ---------------------------------------------------------------------------
TEST_CASE("Engine: prepare() with new sample rate produces valid output", "[samplerate][INFRA-03]") {
    constexpr int kBlockSize = 512;
    constexpr double kSR1 = 44100.0;
    constexpr double kSR2 = 96000.0;

    XYZPanEngine engine;
    engine.prepare(kSR1, kBlockSize);

    EngineParams params;
    params.x = 0.5f;
    params.y = 0.5f;
    params.z = 0.0f;
    params.dopplerEnabled = false;

    // Settle at 44100 Hz — 4 blocks
    std::vector<float> outL, outR;
    float rms44k = processNBlocks(engine, params, kBlockSize, 4, &outL, &outR);

    REQUIRE(rms44k > 0.0f);
    REQUIRE(allFinite(outL.data(), kBlockSize));
    REQUIRE(allFinite(outR.data(), kBlockSize));

    // Switch to 96000 Hz
    engine.prepare(kSR2, kBlockSize);

    // Process 4 blocks at 96000 Hz
    float rms96k = processNBlocks(engine, params, kBlockSize, 4, &outL, &outR);

    // Must still be finite and non-silent
    REQUIRE(allFinite(outL.data(), kBlockSize));
    REQUIRE(allFinite(outR.data(), kBlockSize));
    REQUIRE(rms96k > 0.0f);
}

// ---------------------------------------------------------------------------
// Test 2: ITD delay scales correctly with sample rate
// ---------------------------------------------------------------------------
// When position is X=1 (hard right), ITD delay in samples should be
// proportional to sample rate: delay_96k / delay_44k ≈ 96000/44100 ≈ 2.18.
// We measure this by comparing DSP state from each run.
TEST_CASE("Engine: ITD delay in samples scales with sample rate", "[samplerate][INFRA-03]") {
    constexpr int kBlockSize = 512;
    constexpr double kSR1 = 44100.0;
    constexpr double kSR2 = 96000.0;

    EngineParams params;
    params.x = 1.0f;   // hard right — maximum ITD
    params.y = 1.0f;
    params.z = 0.0f;
    params.dopplerEnabled = false;

    const std::vector<float> input = makeNoise(kBlockSize);
    const float* inputPtrs[1] = { input.data() };
    std::vector<float> outL(static_cast<size_t>(kBlockSize), 0.0f);
    std::vector<float> outR(static_cast<size_t>(kBlockSize), 0.0f);

    // Run at 44100 Hz — settle enough for smoothers to converge
    XYZPanEngine eng44;
    eng44.prepare(kSR1, kBlockSize);
    eng44.setParams(params);
    for (int b = 0; b < 16; ++b)
        eng44.process(inputPtrs, 1, outL.data(), outR.data(), nullptr, nullptr, kBlockSize);
    const float itd44 = eng44.getLastDSPState().itdSamples;

    // Run at 96000 Hz — settle same number of blocks
    XYZPanEngine eng96;
    eng96.prepare(kSR2, kBlockSize);
    eng96.setParams(params);
    for (int b = 0; b < 16; ++b)
        eng96.process(inputPtrs, 1, outL.data(), outR.data(), nullptr, nullptr, kBlockSize);
    const float itd96 = eng96.getLastDSPState().itdSamples;

    // ITD in samples should scale ~linearly with sr (within 10% tolerance for smoother convergence)
    REQUIRE(itd44 > 0.0f);
    REQUIRE(itd96 > 0.0f);
    const float expectedRatio = static_cast<float>(kSR2 / kSR1);  // 2.176...
    const float actualRatio   = itd96 / itd44;
    REQUIRE(std::abs(actualRatio - expectedRatio) < 0.1f * expectedRatio);
}

// ---------------------------------------------------------------------------
// Test 3: No stale coefficient explosion at 192 kHz
// ---------------------------------------------------------------------------
// Prepare at 44100, process, then switch to 192000 Hz and process 8 blocks.
// Output must be finite and contain signal (not NaN, not silence/explosion).
TEST_CASE("Engine: no stale coefficients at 192 kHz after mid-session rate change", "[samplerate][INFRA-03]") {
    constexpr int kBlockSize = 256;
    constexpr double kSR1 = 44100.0;
    constexpr double kSR2 = 192000.0;

    XYZPanEngine engine;
    engine.prepare(kSR1, kBlockSize);

    EngineParams params;
    params.x = 0.3f;
    params.y = 0.7f;
    params.z = 0.2f;
    params.dopplerEnabled = false;

    const std::vector<float> input = makeNoise(kBlockSize);
    const float* inputPtrs[1] = { input.data() };
    std::vector<float> outL(static_cast<size_t>(kBlockSize), 0.0f);
    std::vector<float> outR(static_cast<size_t>(kBlockSize), 0.0f);

    // Process 4 blocks at 44100
    engine.setParams(params);
    for (int b = 0; b < 4; ++b)
        engine.process(inputPtrs, 1, outL.data(), outR.data(), nullptr, nullptr, kBlockSize);

    // Switch to 192000 Hz
    engine.prepare(kSR2, kBlockSize);
    engine.setParams(params);

    // Process 8 blocks — all must be finite, output must have signal
    bool anyNaN = false;
    float totalRms = 0.0f;
    for (int b = 0; b < 8; ++b) {
        engine.process(inputPtrs, 1, outL.data(), outR.data(), nullptr, nullptr, kBlockSize);
        if (!allFinite(outL.data(), kBlockSize) || !allFinite(outR.data(), kBlockSize))
            anyNaN = true;
        totalRms += rmsOf(outL.data(), kBlockSize) + rmsOf(outR.data(), kBlockSize);
    }

    REQUIRE_FALSE(anyNaN);
    // Output should have some signal (not completely silent, not exploded past clamp range)
    const float avgRms = totalRms / (8.0f * 2.0f);
    REQUIRE(avgRms > 0.0f);
    REQUIRE(avgRms < 2.5f);  // clamp range is [-2, 2], so RMS should be well below 2.0
}

// ---------------------------------------------------------------------------
// Test 4: Doppler at 96kHz produces clean output
// ---------------------------------------------------------------------------
TEST_CASE("Engine: doppler at 96kHz produces clean output", "[samplerate][doppler]") {
    constexpr int kBlockSize = 512;
    constexpr double kSR = 96000.0;

    XYZPanEngine engine;
    engine.prepare(kSR, kBlockSize);

    // Ramp position from far to near with doppler enabled
    EngineParams params;
    params.dopplerEnabled = true;

    const std::vector<float> input = makeNoise(kBlockSize, 88888u);
    const float* inputPtrs[1] = { input.data() };
    std::vector<float> outL(static_cast<size_t>(kBlockSize));
    std::vector<float> outR(static_cast<size_t>(kBlockSize));

    bool anyNonFinite = false;
    bool anySilent = true;
    float maxAbs = 0.0f;

    constexpr int numBlocks = 32;
    for (int b = 0; b < numBlocks; ++b) {
        float t = static_cast<float>(b) / static_cast<float>(numBlocks);
        // Ramp from far (1,1,1) to near (0,0.1,0)
        params.x = 1.0f * (1.0f - t);
        params.y = 1.0f * (1.0f - t) + 0.1f * t;
        params.z = 1.0f * (1.0f - t);
        engine.setParams(params);

        engine.process(inputPtrs, 1, outL.data(), outR.data(), nullptr, nullptr, kBlockSize);

        for (int i = 0; i < kBlockSize; ++i) {
            if (!std::isfinite(outL[i]) || !std::isfinite(outR[i]))
                anyNonFinite = true;
            float absMax = std::max(std::abs(outL[i]), std::abs(outR[i]));
            if (absMax > 1e-10f) anySilent = false;
            maxAbs = std::max(maxAbs, absMax);
        }
    }

    REQUIRE_FALSE(anyNonFinite);
    REQUIRE_FALSE(anySilent);
    REQUIRE(maxAbs <= 2.0f);  // within clamp range
}
