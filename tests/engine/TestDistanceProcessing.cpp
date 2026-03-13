// TestDistanceProcessing.cpp
// Integration tests for Phase 4 distance processing DSP pipeline.
// Requirements covered: DIST-01, DIST-02, DIST-03, DIST-04, DIST-05, DIST-06
//
// All tests verify measurable perceptual properties of distance:
//   DIST-01: inverse-square gain attenuation
//   DIST-02: air absorption LPF (spectral darkening at far distances)
//   DIST-03: propagation delay offset (timing cue)
//   DIST-04: doppler pitch shift during movement
//   DIST-05: doppler toggle OFF removes delay
//   DIST-06: artifact-free Hermite interpolation (no NaN/Inf at extremes)

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "xyzpan/Engine.h"
#include "xyzpan/Types.h"
#include "xyzpan/Constants.h"
#include <cmath>
#include <vector>
#include <numeric>
#include <algorithm>
#include <limits>

using namespace xyzpan;

static constexpr float kTestSampleRate = 44100.0f;
static constexpr int   kTestBlockSize  = 512;

// ---------------------------------------------------------------------------
// Helper: compute RMS of a float buffer over [start, end).
// ---------------------------------------------------------------------------
static float rmsOf(const std::vector<float>& buf, int start, int end) {
    float sum = 0.0f;
    int n = end - start;
    if (n <= 0) return 0.0f;
    for (int i = start; i < end; ++i)
        sum += buf[static_cast<size_t>(i)] * buf[static_cast<size_t>(i)];
    return std::sqrt(sum / static_cast<float>(n));
}

// ---------------------------------------------------------------------------
// Helper: generate N samples of deterministic white noise via LCG.
// ---------------------------------------------------------------------------
static std::vector<float> makeNoise(int N, uint32_t seed = 12345u) {
    std::vector<float> v(static_cast<size_t>(N));
    uint32_t rng = seed;
    for (int i = 0; i < N; ++i) {
        rng = rng * 1664525u + 1013904223u;
        v[static_cast<size_t>(i)] =
            static_cast<float>(static_cast<int32_t>(rng)) / 2147483648.0f;
    }
    return v;
}

// ---------------------------------------------------------------------------
// Helper: generate N samples of a sine wave at freqHz.
// ---------------------------------------------------------------------------
static std::vector<float> makeSine(float freqHz, int N) {
    std::vector<float> v(static_cast<size_t>(N));
    for (int i = 0; i < N; ++i)
        v[static_cast<size_t>(i)] =
            std::sin(2.0f * 3.14159265f * freqHz * static_cast<float>(i) / kTestSampleRate);
    return v;
}

// ---------------------------------------------------------------------------
// Helper: prepare engine, settle smoothers with given params, then process N
//         samples of input. Returns {outL, outR}.
// The settle pass processes `settleN` samples of silence so that all smoothers
// (including distGainSmooth and distDelaySmooth) have converged before the
// measurement buffer is processed.
// ---------------------------------------------------------------------------
struct StereoOut {
    std::vector<float> L;
    std::vector<float> R;
};

static StereoOut settleAndProcess(const EngineParams& params,
                                  const std::vector<float>& input,
                                  int settleN = 8192) {
    XYZPanEngine engine;
    engine.prepare(static_cast<double>(kTestSampleRate), kTestBlockSize);
    engine.setParams(params);

    // Settle: process silence so all smoothers converge.
    {
        std::vector<float> silence(static_cast<size_t>(settleN), 0.0f);
        std::vector<float> silL(static_cast<size_t>(settleN));
        std::vector<float> silR(static_cast<size_t>(settleN));
        const float* ins[1] = { silence.data() };
        int offset = 0;
        while (offset < settleN) {
            int batch = std::min(kTestBlockSize, settleN - offset);
            engine.process(ins, 1, silL.data() + offset, silR.data() + offset, batch);
            ins[0] += batch;
            offset += batch;
        }
    }

    int N = static_cast<int>(input.size());
    StereoOut out;
    out.L.resize(static_cast<size_t>(N));
    out.R.resize(static_cast<size_t>(N));

    engine.setParams(params);
    const float* ins[1] = { input.data() };
    int offset = 0;
    while (offset < N) {
        int batch = std::min(kTestBlockSize, N - offset);
        engine.process(ins, 1, out.L.data() + offset, out.R.data() + offset, batch);
        ins[0] += batch;
        offset += batch;
    }
    return out;
}

// ---------------------------------------------------------------------------
// DIST-01: Inverse-square gain attenuation
// ---------------------------------------------------------------------------
TEST_CASE("DIST-01: Inverse-square gain attenuation", "[distance][DIST-01]") {
    // Formula: distGain = clamp(kDistGainRef / dist, 0, kDistGainMax)
    // At dist=1.0 (Y=1, default position): distGain = 1.0/1.0 = 1.0 (unity).
    // At dist=0.5 (Y=0.5): distGain = 1.0/0.5 = 2.0 (clamped to kDistGainMax).
    // So closer-to-reference ratio should be ~2.0 (closer is louder).
    //
    // Test: X=0, Z=0 for both (no ILD, no floor/chest bounce, combWet=0 at Y>0)
    // Compare "close" (Y=0.5) vs "reference" (Y=1.0).
    // dopplerEnabled=false so the delay does not shift the signal outside the window.

    constexpr int N = 4096;
    constexpr int skip = 512;  // skip smoother transient

    auto noise = makeNoise(N, 99991u);

    // Reference: x=0, y=1.0, z=0 (dist=1.0 → distGain = 1.0 = unity at default position)
    EngineParams ref;
    ref.x = 0.0f;
    ref.y = 1.0f;    // dist = 1.0, distGain = 1.0
    ref.z = 0.0f;
    ref.dopplerEnabled = false;

    // Close: x=0, y=0.5, z=0 (dist=0.5 → distGain = 1.0/0.5 = 2.0, clamped to kDistGainMax)
    EngineParams close;
    close.x = 0.0f;
    close.y = 0.5f;  // dist = 0.5, distGain = kDistGainMax = 2.0
    close.z = 0.0f;
    close.dopplerEnabled = false;

    auto refOut   = settleAndProcess(ref,   noise);
    auto closeOut = settleAndProcess(close, noise);

    float rmsRef   = rmsOf(refOut.L,   skip, N);
    float rmsClose = rmsOf(closeOut.L, skip, N);

    // Close source should be louder than reference (closer = louder = gain > 1.0).
    // Close/ref ratio should be ~kDistGainMax (2.0) ± smoother/binaural tolerance.
    REQUIRE(rmsRef   > 0.0f);
    REQUIRE(rmsClose > 0.0f);
    // Close should be louder than reference (gain ratio > 1.0)
    CHECK(rmsClose > rmsRef);
    // Ratio should be in the range [1.1, kDistGainMax+0.3] — the smoother ramp reduces
    // the close gain slightly since settle converges toward but not exactly kDistGainMax.
    float ratio = rmsClose / rmsRef;
    CHECK(ratio >= 1.1f);
    CHECK(ratio <= kDistGainMax + 0.3f);
}

// ---------------------------------------------------------------------------
// DIST-02: Air absorption LPF (spectral darkening at far distances)
// ---------------------------------------------------------------------------
TEST_CASE("DIST-02: Air absorption LPF darkens distant sources", "[distance][DIST-02]") {
    // Process a 12kHz sine at near distance vs far distance.
    // At far distance (distFrac ~0.8), airCutoff ≈ 22000 + (8000-22000)*0.8 = 10800 Hz.
    // A 12kHz sine at 10800 Hz LPF cutoff should be noticeably attenuated.
    // At near distance (distFrac ~0), airCutoff ≈ 22000 Hz — 12kHz passes almost unchanged.
    //
    // The far output should have lower RMS at 12kHz than the near output,
    // beyond just the gain rolloff from DIST-01.
    //
    // Verification: process 12kHz sine at near and far. Compute ratio (far/near).
    // The gain-only expected ratio at far (x=0.7,y=0.7,z=0.7, dist≈1.21) is
    //   distGain_far = kMinDistance/1.21 ≈ 0.083.
    // The actual ratio should be LOWER than the gain-only ratio because the LPF
    // additionally attenuates 12kHz.
    // Simply check: far 12kHz RMS is less than near 12kHz RMS (the gain+LPF combined).

    constexpr int N = 4096;
    constexpr int skip = 512;

    auto sine12k = makeSine(12000.0f, N);

    // Near: x=0, y=kMinDistance (gain=1.0, airCutoff≈22000 Hz — almost no filtering)
    EngineParams near;
    near.x = 0.0f;
    near.y = kMinDistance;
    near.z = 0.0f;
    near.dopplerEnabled = false;

    // Far: x=0.7, y=0.7, z=0.7 (dist ≈ 1.21, airCutoff ≈ 10800 Hz)
    EngineParams farDist;
    farDist.x = 0.7f;
    farDist.y = 0.7f;
    farDist.z = 0.7f;
    farDist.dopplerEnabled = false;

    auto nearOut = settleAndProcess(near,    sine12k);
    auto farOut  = settleAndProcess(farDist, sine12k);

    float rmsNear = rmsOf(nearOut.L, skip, N);
    float rmsFar  = rmsOf(farOut.L,  skip, N);

    // Far should be quieter at 12kHz (combined gain + LPF attenuation)
    CHECK(rmsNear > 0.0f);
    CHECK(rmsFar  < rmsNear);

    // The ratio far/near should be well below 1.0 (at least 50% attenuation)
    float ratio = rmsFar / rmsNear;
    CHECK(ratio < 0.5f);
}

// ---------------------------------------------------------------------------
// DIST-03: Distance delay timing
// ---------------------------------------------------------------------------
TEST_CASE("DIST-03: Propagation delay shifts output in time", "[distance][DIST-03]") {
    // Verify that a source at far distance has a measurable timing offset.
    //
    // Strategy: Process a settled engine (dopplerEnabled=true) with an impulse.
    // After settling, the distDelaySmooth has converged to the target delay.
    // The impulse pushed into the delay line will exit at ~(settled delay) samples later.
    //
    // Use x=0, y=kMinDistance first: minimal delay (distFrac≈0, delaySamp=2.0).
    // Use x=0.5, y=0.5, z=0.5 for far: dist=0.866, distFrac≈0.47, delay≈141ms≈6223 samp.
    //
    // Rather than capturing 6000+ samples, just verify the near-distance output
    // has its impulse peak near sample ~2, while far-distance output has its peak
    // much later (beyond the near-distance peak position).
    //
    // For practical capture: compare near (peak near sample 2) vs far.
    // For far: we need a large enough buffer. Use 8192 samples.
    // At far dist: delay ≈ 6223 samples. With dopplerEnabled=true and settle=44100,
    // the smoother has converged. The impulse at sample 0 exits at sample ~6223.
    // Check that the output peak in the far case is at position > 100.

    constexpr int N = 8192;
    constexpr int impulsePos = 0;

    // --- Near distance ---
    std::vector<float> inputNear(N, 0.0f);
    inputNear[impulsePos] = 1.0f;

    EngineParams nearP;
    nearP.x = 0.0f;
    nearP.y = kMinDistance;  // dist = 0.1, distFrac ≈ 0, delay ≈ 2.0 samples
    nearP.z = 0.0f;
    nearP.dopplerEnabled = true;

    auto nearOut = settleAndProcess(nearP, inputNear, 4096);

    int peakNear = 0;
    float maxNear = 0.0f;
    for (int i = 0; i < N; ++i) {
        if (std::abs(nearOut.L[static_cast<size_t>(i)]) > maxNear) {
            maxNear = std::abs(nearOut.L[static_cast<size_t>(i)]);
            peakNear = i;
        }
    }

    // --- Far distance (doppler ON, delay should be significant) ---
    std::vector<float> inputFar(N, 0.0f);
    inputFar[impulsePos] = 1.0f;

    EngineParams farP;
    farP.x = 0.5f;
    farP.y = 0.5f;
    farP.z = 0.5f;  // dist ≈ 0.866, distFrac ≈ 0.47, delay ≈ 141ms ≈ 6223 samples
    farP.z = 0.5f;
    farP.dopplerEnabled = true;

    auto farOut = settleAndProcess(farP, inputFar, 44100);

    int peakFar = 0;
    float maxFar = 0.0f;
    for (int i = 0; i < N; ++i) {
        if (std::abs(farOut.L[static_cast<size_t>(i)]) > maxFar) {
            maxFar = std::abs(farOut.L[static_cast<size_t>(i)]);
            peakFar = i;
        }
    }

    // Near-distance peak should be early (delay ≈ 2 samples)
    CHECK(peakNear < 20);

    // Far-distance peak should be at a much larger position (delay ≈ 6223 samples)
    CHECK(peakFar > 100);

    // Far peak should come significantly later than near peak
    CHECK(peakFar > peakNear);
}

// ---------------------------------------------------------------------------
// DIST-04: Doppler pitch shift during movement
// ---------------------------------------------------------------------------
TEST_CASE("DIST-04: Doppler pitch shift during distance change", "[distance][DIST-04]") {
    // Process a 1kHz sine while ramping distance from near to far.
    // Compare output to a static-distance reference — they should differ
    // because the changing delay causes a pitch shift (doppler effect).
    //
    // Simple proxy: process 4096 samples with y ramping from kMinDistance to 1.0.
    // Process same 4096 samples at static y=0.5. RMS difference should be > 0.

    constexpr int N = 4096;
    auto sine1k = makeSine(1000.0f, N);

    // Static reference: y=0.5 (constant distance)
    EngineParams staticP;
    staticP.x = 0.0f;
    staticP.y = 0.5f;
    staticP.z = 0.0f;
    staticP.dopplerEnabled = true;

    auto staticOut = settleAndProcess(staticP, sine1k, 4096);

    // Dynamic: ramp y from kMinDistance to 1.0 across 4096 samples
    // We simulate by changing params per block during processing.
    XYZPanEngine engine;
    engine.prepare(static_cast<double>(kTestSampleRate), kTestBlockSize);

    // Settle with start position
    EngineParams startP;
    startP.x = 0.0f;
    startP.y = kMinDistance;
    startP.z = 0.0f;
    startP.dopplerEnabled = true;
    engine.setParams(startP);
    {
        std::vector<float> silence(4096, 0.0f);
        std::vector<float> silL(4096), silR(4096);
        const float* ins[1] = { silence.data() };
        int off = 0;
        while (off < 4096) {
            int b = std::min(kTestBlockSize, 4096 - off);
            engine.process(ins, 1, silL.data() + off, silR.data() + off, b);
            ins[0] += b;
            off += b;
        }
    }

    std::vector<float> rampL(N), rampR(N);
    int offset = 0;
    while (offset < N) {
        int batch = std::min(kTestBlockSize, N - offset);
        // Ramp y from kMinDistance to 1.0
        float t = static_cast<float>(offset) / static_cast<float>(N);
        EngineParams p = startP;
        p.y = kMinDistance + (1.0f - kMinDistance) * t;
        engine.setParams(p);
        const float* ins[1] = { sine1k.data() + offset };
        engine.process(ins, 1, rampL.data() + offset, rampR.data() + offset, batch);
        offset += batch;
    }

    // Compare ramp output to static output — they should differ due to doppler
    float diffSum = 0.0f;
    constexpr int skip = 256;
    for (int i = skip; i < N; ++i) {
        float d = rampL[static_cast<size_t>(i)] - staticOut.L[static_cast<size_t>(i)];
        diffSum += d * d;
    }
    float diffRms = std::sqrt(diffSum / static_cast<float>(N - skip));

    // The outputs must differ — doppler shift changes the waveform
    CHECK(diffRms > 1e-6f);
}

// ---------------------------------------------------------------------------
// DIST-05: Doppler toggle OFF removes delay
// ---------------------------------------------------------------------------
TEST_CASE("DIST-05: dopplerEnabled=false removes propagation delay", "[distance][DIST-05]") {
    // With dopplerEnabled=false at far distance, the signal reads at delay=2.0 (minimum).
    // The impulse peak should appear near sample ~2 in the output.
    // Compare to DIST-03 where the peak was at ~6223 samples.

    constexpr int N = 256;

    std::vector<float> input(N, 0.0f);
    input[0] = 1.0f;  // impulse at sample 0

    // Far distance, doppler OFF
    EngineParams farOff;
    farOff.x = 0.5f;
    farOff.y = 0.5f;
    farOff.z = 0.5f;  // dist ≈ 0.866 → delay would be ~6223 samples if doppler was ON
    farOff.dopplerEnabled = false;

    auto out = settleAndProcess(farOff, input, 4096);

    // Find output peak
    int peak = 0;
    float maxVal = 0.0f;
    for (int i = 0; i < N; ++i) {
        if (std::abs(out.L[static_cast<size_t>(i)]) > maxVal) {
            maxVal = std::abs(out.L[static_cast<size_t>(i)]);
            peak = i;
        }
    }

    // Peak should appear near sample 2 (minimum delay guard), NOT at 6223
    // Allow some tolerance for filter ringing (peak within first 20 samples)
    CHECK(peak < 20);

    // There must be a non-trivial peak (signal gets through with gain attenuation)
    CHECK(maxVal > 0.0f);
}

// ---------------------------------------------------------------------------
// DIST-06: Stability with extreme parameters (no NaN/Inf)
// ---------------------------------------------------------------------------
TEST_CASE("DIST-06: Stability at extreme parameters", "[distance][DIST-06]") {
    // Process 10000 samples with rapid position changes and extreme distances.
    // No NaN or Inf should appear in output.

    XYZPanEngine engine;
    engine.prepare(static_cast<double>(kTestSampleRate), kTestBlockSize);

    constexpr int N = 10000;
    auto noise = makeNoise(N, 55555u);

    std::vector<float> outL(N), outR(N);
    bool hasNaN = false;
    bool hasInf = false;

    int offset = 0;
    int blockIndex = 0;
    while (offset < N) {
        int batch = std::min(kTestBlockSize, N - offset);

        // Alternate between extreme positions every block
        EngineParams p;
        if (blockIndex % 4 == 0) {
            p.x =  1.0f; p.y =  1.0f; p.z =  1.0f;  // max distance (sqrt(3))
        } else if (blockIndex % 4 == 1) {
            p.x =  0.0f; p.y = kMinDistance; p.z = 0.0f;  // min distance
        } else if (blockIndex % 4 == 2) {
            p.x = -1.0f; p.y = -1.0f; p.z = -1.0f;  // max distance, other corner
        } else {
            p.x =  0.5f; p.y = -0.5f; p.z =  0.5f;  // mid distance
        }
        p.dopplerEnabled = true;
        engine.setParams(p);

        const float* ins[1] = { noise.data() + offset };
        engine.process(ins, 1, outL.data() + offset, outR.data() + offset, batch);
        offset += batch;
        ++blockIndex;
    }

    for (int i = 0; i < N; ++i) {
        if (std::isnan(outL[i]) || std::isnan(outR[i])) hasNaN = true;
        if (std::isinf(outL[i]) || std::isinf(outR[i])) hasInf = true;
    }

    CHECK_FALSE(hasNaN);
    CHECK_FALSE(hasInf);
}

// ---------------------------------------------------------------------------
// DIST-06 (additional): Stability at near-zero distance (clamped to kMinDistance)
// ---------------------------------------------------------------------------
TEST_CASE("DIST-06: Stability at near-zero distance", "[distance][DIST-06]") {
    constexpr int N = 4096;
    auto noise = makeNoise(N, 77777u);

    // x=0, y=0, z=0 → computeDistance clamps to kMinDistance
    EngineParams p;
    p.x = 0.0f;
    p.y = 0.0f;
    p.z = 0.0f;
    p.dopplerEnabled = true;

    auto out = settleAndProcess(p, noise, 2048);

    for (int i = 0; i < N; ++i) {
        CHECK_FALSE(std::isnan(out.L[static_cast<size_t>(i)]));
        CHECK_FALSE(std::isnan(out.R[static_cast<size_t>(i)]));
        CHECK_FALSE(std::isinf(out.L[static_cast<size_t>(i)]));
        CHECK_FALSE(std::isinf(out.R[static_cast<size_t>(i)]));
    }
}

// ---------------------------------------------------------------------------
// Close sources pan harder than distant sources (ITD+ILD+head shadow proximity scaling)
// ---------------------------------------------------------------------------
TEST_CASE("Distance-dependent hardpan: close sources pan harder than distant", "[distance][hardpan]") {
    // At x=1 (hard right), close distance vs far distance:
    //   Close: proximity ≈ 1.0 → ITD, head shadow, ILD all at maximum
    //   Far:   proximity ≈ 0   → ITD, head shadow, ILD all near zero
    // Close should produce larger L/R RMS difference than far.
    //
    // Both tests use dopplerEnabled=false to avoid delay issues.

    constexpr int N = 4096;
    constexpr int skip = 512;
    auto noise = makeNoise(N, 44444u);

    // Close: x=1, y=kMinDistance (distance = sqrt(1 + 0.01 + 0) ≈ 1.005... wait,
    // actually x=1, y=kMinDistance=0.1, z=0 → dist = sqrt(1 + 0.01) ≈ 1.005.
    // proximity = 1 - (1.005 - 0.1)/(1.732 - 0.1) = 1 - 0.905/1.632 ≈ 0.446.
    //
    // Use x=1, y=0, z=0 → dist = kMinDistance (clamped to 0.1) for truly close.
    EngineParams closeP;
    closeP.x = 1.0f;
    closeP.y = 0.0f;  // dist = kMinDistance (clamped) → proximity ≈ 1.0
    closeP.z = 0.0f;
    closeP.dopplerEnabled = false;

    // Far: x=1, y=1, z=1 → dist = sqrt(3) ≈ 1.732, proximity ≈ 0.0
    EngineParams farP;
    farP.x = 1.0f;
    farP.y = 1.0f;
    farP.z = 1.0f;  // dist = kSqrt3, proximity = 0.0
    farP.dopplerEnabled = false;

    auto closeOut = settleAndProcess(closeP, noise, 8192);
    auto farOut   = settleAndProcess(farP,   noise, 8192);

    // Measure L/R RMS difference for each
    float closeLRDiff = 0.0f, farLRDiff = 0.0f;
    for (int i = skip; i < N; ++i) {
        closeLRDiff += std::abs(closeOut.L[static_cast<size_t>(i)] -
                                closeOut.R[static_cast<size_t>(i)]);
        farLRDiff   += std::abs(farOut.L[static_cast<size_t>(i)] -
                                farOut.R[static_cast<size_t>(i)]);
    }

    // Close source should pan harder (larger L/R difference)
    CHECK(closeLRDiff > farLRDiff);

    // At max distance, proximity=0 → ITD=0, head shadow=open, ILD=0 → L≈R
    // Both L and R should be very similar (nearly zero L/R diff)
    // Allow some residual difference from the air absorption per-block LPF.
    float farLRRatio = farLRDiff / (farLRDiff + closeLRDiff + 1e-10f);
    CHECK(farLRRatio < 0.3f);  // far contributes less than 30% of total L/R diff
}
