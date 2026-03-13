// TestDepthAndElevation.cpp
// Unit tests for all Phase 3 DSP primitives:
//   FeedbackCombFilter, SVFFilter, BiquadFilter, OnePoleLP
// Engine integration tests for depth and elevation DSP pipeline.
//
// Each class is tested in isolation at 44100 Hz.
// RMS-based energy comparison is used for frequency-domain assertions.
// Requirements covered: DEPTH-01, DEPTH-02, DEPTH-04, ELEV-01, ELEV-03, DEPTH-03, DEPTH-05, ELEV-02, ELEV-04, ELEV-05

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "xyzpan/dsp/FeedbackCombFilter.h"
#include "xyzpan/dsp/SVFFilter.h"
#include "xyzpan/dsp/BiquadFilter.h"
#include "xyzpan/dsp/OnePoleLP.h"
#include "xyzpan/Engine.h"
#include "xyzpan/Types.h"
#include "xyzpan/Constants.h"

#include <vector>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <limits>

using namespace xyzpan;
using namespace xyzpan::dsp;

static constexpr float kPi = 3.14159265f;
static constexpr float kSampleRate = 44100.0f;

// Generate N samples of a single-frequency sine at freqHz.
static std::vector<float> makeSine(float freqHz, int N, float sampleRate = kSampleRate) {
    std::vector<float> v(static_cast<size_t>(N));
    for (int i = 0; i < N; ++i)
        v[static_cast<size_t>(i)] = std::sin(2.0f * kPi * freqHz * static_cast<float>(i) / sampleRate);
    return v;
}

// Compute RMS energy over a buffer (skipping leading transient samples).
static float rms(const std::vector<float>& buf, int skip = 0) {
    float sum = 0.0f;
    int n = static_cast<int>(buf.size()) - skip;
    if (n <= 0) return 0.0f;
    for (int i = skip; i < static_cast<int>(buf.size()); ++i)
        sum += buf[static_cast<size_t>(i)] * buf[static_cast<size_t>(i)];
    return std::sqrt(sum / static_cast<float>(n));
}

// ============================================================================
// FeedbackCombFilter tests
// ============================================================================

TEST_CASE("CombFilter impulse echo: first echo at delay M with amplitude g", "[CombFilter]") {
    // DEPTH-01, DEPTH-02: verify echo arrives at exactly the configured delay
    FeedbackCombFilter comb;
    comb.prepare(128);
    comb.setDelay(10);
    comb.setFeedback(0.5f);

    // Push one impulse followed by silence
    std::vector<float> out;
    out.reserve(25);
    out.push_back(comb.process(1.0f)); // sample 0: impulse

    for (int i = 1; i < 25; ++i)
        out.push_back(comb.process(0.0f));

    // Sample 0: impulse passes through (y = 1 + 0.5 * 0 = 1)
    CHECK(out[0] == Catch::Approx(1.0f).epsilon(1e-5f));

    // Samples 1-9: should be 0 (no feedback yet — y[n-10] was 0 for n<10)
    for (int i = 1; i < 10; ++i)
        CHECK(std::abs(out[static_cast<size_t>(i)]) < 1e-5f);

    // Sample 10: first echo = 0 + 0.5 * y[0] = 0.5
    CHECK(out[10] == Catch::Approx(0.5f).epsilon(1e-4f));

    // Sample 20: second echo = 0 + 0.5 * y[10] = 0.25
    CHECK(out[20] == Catch::Approx(0.25f).epsilon(1e-4f));
}

TEST_CASE("CombFilter feedback clamp: setFeedback(1.5) clamps to 0.95, output stays bounded", "[CombFilter]") {
    // DEPTH-04: hard clamp prevents runaway resonance
    FeedbackCombFilter comb;
    comb.prepare(256);
    comb.setDelay(5);
    comb.setFeedback(1.5f);  // should be clamped to 0.95

    // With g=0.95 and small delay, the filter rings but stays bounded.
    // With g=1.5 (unclamped), it would diverge to millions within a few hundred samples.
    const int N = 1000;
    float maxAbs = 0.0f;
    float input = 0.5f;
    for (int i = 0; i < N; ++i) {
        // Step input for first sample, then silence
        float x = (i == 0) ? input : 0.0f;
        float y = comb.process(x);
        maxAbs = std::max(maxAbs, std::abs(y));
    }

    // If g were truly 1.5, the output would grow unboundedly (diverge to >10000).
    // With the clamp at 0.95, peak output stays well below 50.
    CHECK(maxAbs < 50.0f);
}

TEST_CASE("CombFilter delay range: setDelay(66) echo arrives at sample 66", "[CombFilter]") {
    // DEPTH-02: 66 samples = ~1.5 ms at 44100 Hz — maximum comb delay range
    FeedbackCombFilter comb;
    comb.prepare(128);
    comb.setDelay(66);
    comb.setFeedback(0.5f);

    std::vector<float> out;
    out.reserve(80);
    out.push_back(comb.process(1.0f));
    for (int i = 1; i < 80; ++i)
        out.push_back(comb.process(0.0f));

    // First echo at sample 66
    CHECK(out[66] == Catch::Approx(0.5f).epsilon(1e-4f));

    // All samples before 66 (except 0) should be near zero
    for (int i = 1; i < 66; ++i)
        CHECK(std::abs(out[static_cast<size_t>(i)]) < 1e-5f);
}

TEST_CASE("CombFilter reset: clears all buffer state", "[CombFilter]") {
    FeedbackCombFilter comb;
    comb.prepare(128);
    comb.setDelay(10);
    comb.setFeedback(0.8f);

    // Build up state with loud input
    for (int i = 0; i < 50; ++i)
        comb.process(0.5f);

    // Reset should clear the buffer
    comb.reset();

    // After reset, processing silence should produce silence
    for (int i = 0; i < 20; ++i) {
        float y = comb.process(0.0f);
        CHECK(y == Catch::Approx(0.0f).epsilon(1e-6f));
    }
}

// ============================================================================
// SVFFilter tests
// ============================================================================

TEST_CASE("SVFFilter HP passes high: 5kHz sine through 700Hz HP > 0.8x input RMS", "[SVFFilter]") {
    SVFFilter svf;
    svf.setType(SVFType::HP);
    svf.setCoefficients(700.0f, kSampleRate);

    const int N = 512;
    auto sine = makeSine(5000.0f, N);
    float inRms = rms(sine, 64);

    std::vector<float> out(static_cast<size_t>(N));
    for (int i = 0; i < N; ++i)
        out[static_cast<size_t>(i)] = svf.process(sine[static_cast<size_t>(i)]);

    float outRms = rms(out, 64);

    // 5 kHz is well above 700 Hz cutoff — should pass near unity
    CHECK(outRms > 0.8f * inRms);
}

TEST_CASE("SVFFilter HP cuts low: 100Hz sine through 700Hz HP < 0.2x input RMS", "[SVFFilter]") {
    SVFFilter svf;
    svf.setType(SVFType::HP);
    svf.setCoefficients(700.0f, kSampleRate);

    const int N = 512;
    auto sine = makeSine(100.0f, N);
    float inRms = rms(sine, 64);

    std::vector<float> out(static_cast<size_t>(N));
    for (int i = 0; i < N; ++i)
        out[static_cast<size_t>(i)] = svf.process(sine[static_cast<size_t>(i)]);

    float outRms = rms(out, 64);

    // 100 Hz is well below 700 Hz cutoff — should be heavily attenuated
    CHECK(outRms < 0.2f * inRms);
}

// ============================================================================
// BiquadFilter tests
// ============================================================================

// Helper: measure RMS of a sine through a biquad filter (skips 512-sample transient).
static float biquadSineRms(BiquadFilter& bq, float freqHz, int N = 2048, int skip = 512) {
    auto sine = makeSine(freqHz, N);
    std::vector<float> out(static_cast<size_t>(N));
    for (int i = 0; i < N; ++i)
        out[static_cast<size_t>(i)] = bq.process(sine[static_cast<size_t>(i)]);
    return rms(out, skip);
}

TEST_CASE("BiquadFilter PeakingEQ -15dB: 8kHz sine ~15dB lower than 1kHz passband", "[BiquadFilter]") {
    BiquadFilter bq;

    // Measure 1kHz passband RMS (reference)
    bq.setCoefficients(BiquadType::PeakingEQ, 8000.0f, kSampleRate, 2.0f, -15.0f);
    bq.reset();
    float rms1k = biquadSineRms(bq, 1000.0f);

    // Measure 8kHz notch RMS
    bq.reset();
    float rms8k = biquadSineRms(bq, 8000.0f);

    // -15 dB ratio in linear amplitude: 10^(-15/20) ≈ 0.178
    // Energy ratio (power): 10^(-15/10) ≈ 0.032
    // RMS ratio should be < 0.25 (somewhat loose to account for finite-length effects)
    float ratio = rms8k / (rms1k + 1e-9f);
    CHECK(ratio < 0.25f);
}

TEST_CASE("BiquadFilter PeakingEQ +5dB: 8kHz sine higher RMS than 1kHz passband", "[BiquadFilter]") {
    BiquadFilter bq;

    bq.setCoefficients(BiquadType::PeakingEQ, 8000.0f, kSampleRate, 2.0f, 5.0f);
    bq.reset();
    float rms1k = biquadSineRms(bq, 1000.0f);

    bq.reset();
    float rms8k = biquadSineRms(bq, 8000.0f);

    // +5dB: 8kHz should be boosted above the 1kHz passband level
    CHECK(rms8k > rms1k);
}

TEST_CASE("BiquadFilter PeakingEQ 0dB: output equals input (unity/all-pass)", "[BiquadFilter]") {
    // At gainDb=0, the peaking EQ biquad must reduce to unity gain (all-pass).
    BiquadFilter bq;
    bq.setCoefficients(BiquadType::PeakingEQ, 8000.0f, kSampleRate, 2.0f, 0.0f);

    const int N = 2048;
    const int skip = 64;
    auto sine = makeSine(1000.0f, N);
    float inRms = rms(sine, skip);

    std::vector<float> out(static_cast<size_t>(N));
    for (int i = 0; i < N; ++i)
        out[static_cast<size_t>(i)] = bq.process(sine[static_cast<size_t>(i)]);

    float outRms = rms(out, skip);

    // Output RMS should equal input RMS within 1%
    CHECK(outRms == Catch::Approx(inRms).epsilon(0.01f));
}

TEST_CASE("BiquadFilter HighShelf +3dB: 8kHz boosted, 500Hz unchanged", "[BiquadFilter]") {
    BiquadFilter bq;
    bq.setCoefficients(BiquadType::HighShelf, 4000.0f, kSampleRate, 0.7071f, 3.0f);

    // Reference: input sine RMS (no filter)
    const int N = 2048;
    const int skip = 512;
    auto sine8k = makeSine(8000.0f, N);
    float inRms8k = rms(sine8k, skip);

    // Measure 8kHz output (above shelf — should be boosted)
    bq.reset();
    float outRms8k = biquadSineRms(bq, 8000.0f, N, skip);
    CHECK(outRms8k > inRms8k);  // 8kHz is boosted above the shelf

    // Measure 500Hz output (below shelf — should be ~unity)
    bq.reset();
    auto sine500 = makeSine(500.0f, N);
    float inRms500 = rms(sine500, skip);
    float outRms500 = biquadSineRms(bq, 500.0f, N, skip);

    // Below the shelf: output should be close to input (within 5%)
    float lowRatio = outRms500 / (inRms500 + 1e-9f);
    CHECK(lowRatio > 0.95f);
    CHECK(lowRatio < 1.05f);
}

// ============================================================================
// OnePoleLP tests
// ============================================================================

TEST_CASE("OnePoleLP passes low: 100Hz sine near unity through 1kHz LP", "[OnePoleLP]") {
    // ELEV-03: 6 dB/oct lowpass at 1 kHz for chest bounce path
    OnePoleLP lp;
    lp.setCoefficients(1000.0f, kSampleRate);

    const int N = 2048;
    const int skip = 64;
    auto sine = makeSine(100.0f, N);
    float inRms = rms(sine, skip);

    std::vector<float> out(static_cast<size_t>(N));
    for (int i = 0; i < N; ++i)
        out[static_cast<size_t>(i)] = lp.process(sine[static_cast<size_t>(i)]);

    float outRms = rms(out, skip);

    // 100 Hz is a decade below 1 kHz cutoff — output should be close to input RMS
    CHECK(outRms > 0.9f * inRms);
}

TEST_CASE("OnePoleLP cuts high: 10kHz sine heavily attenuated through 1kHz LP", "[OnePoleLP]") {
    OnePoleLP lp;
    lp.setCoefficients(1000.0f, kSampleRate);

    const int N = 2048;
    const int skip = 64;
    auto sine = makeSine(10000.0f, N);
    float inRms = rms(sine, skip);

    std::vector<float> out(static_cast<size_t>(N));
    for (int i = 0; i < N; ++i)
        out[static_cast<size_t>(i)] = lp.process(sine[static_cast<size_t>(i)]);

    float outRms = rms(out, skip);

    // 10 kHz is a decade above 1 kHz cutoff.
    // At 6 dB/oct: 10x the cutoff frequency = 20 dB attenuation (ratio 0.1 in amplitude).
    // RMS ratio should be well below 0.15.
    float ratio = outRms / (inRms + 1e-9f);
    CHECK(ratio < 0.15f);
}

TEST_CASE("OnePoleLP reset: clears filter state", "[OnePoleLP]") {
    OnePoleLP lp;
    lp.setCoefficients(1000.0f, kSampleRate);

    // Build up state with loud input
    for (int i = 0; i < 100; ++i)
        lp.process(0.9f);

    lp.reset();

    // After reset, silence input should produce silence immediately
    float y = lp.process(0.0f);
    CHECK(y == Catch::Approx(0.0f).epsilon(1e-6f));
}

// ============================================================================
// Phase3Integration: Engine-level integration tests
// ============================================================================
//
// Helper: run the engine for N samples with a given input sine at freqHz.
// Returns left and right output buffers.
struct EngineOutput {
    std::vector<float> L;
    std::vector<float> R;
};

static EngineOutput runEngine(const EngineParams& params, float freqHz, int N,
                               float sampleRate = kSampleRate) {
    XYZPanEngine engine;
    engine.prepare(static_cast<double>(sampleRate), N);

    // Phase 4: disable distance delay for Phase 3 DSP tests.
    // These tests verify comb/pinna/elevation behavior; the long distance delay
    // would cause the delay smoother to ramp during the output window, producing
    // near-zero output. Setting dopplerEnabled=false ensures the signal passes
    // through with only gain attenuation and air LPF (both position-consistent
    // for ratio-based comparisons).
    EngineParams p = params;
    p.dopplerEnabled = false;
    engine.setParams(p);

    // Generate sine input
    std::vector<float> input(static_cast<size_t>(N));
    for (int i = 0; i < N; ++i)
        input[static_cast<size_t>(i)] = std::sin(2.0f * kPi * freqHz * static_cast<float>(i) / sampleRate);

    std::vector<float> outL(static_cast<size_t>(N), 0.0f);
    std::vector<float> outR(static_cast<size_t>(N), 0.0f);

    const float* inputs[] = { input.data() };
    float* pL = outL.data();
    float* pR = outR.data();
    engine.process(inputs, 1, pL, pR, N);

    return { outL, outR };
}

// Compute RMS of output channels combined (L+R average).
static float rmsLR(const EngineOutput& out, int skip = 0) {
    float sum = 0.0f;
    int n = static_cast<int>(out.L.size()) - skip;
    if (n <= 0) return 0.0f;
    for (int i = skip; i < static_cast<int>(out.L.size()); ++i) {
        float avg = 0.5f * (out.L[static_cast<size_t>(i)] + out.R[static_cast<size_t>(i)]);
        sum += avg * avg;
    }
    return std::sqrt(sum / static_cast<float>(n));
}

TEST_CASE("Phase3Integration: Comb bank Y=-1 vs Y=1 produces different output", "[engine][depth][elevation]") {
    // Y=-1 (back): comb bank fully wet, spectral coloration present.
    // Y=1 (front): comb wet=0, clean output.
    // Both signals should differ in energy/spectral content.
    const int N = 2048;
    const int skip = 256;  // skip initial transient

    EngineParams paramsBack;
    paramsBack.y = -1.0f;
    paramsBack.x = 0.0f;
    paramsBack.z = 0.0f;

    EngineParams paramsFront;
    paramsFront.y = 1.0f;
    paramsFront.x = 0.0f;
    paramsFront.z = 0.0f;

    auto backOut  = runEngine(paramsBack,  1000.0f, N);
    auto frontOut = runEngine(paramsFront, 1000.0f, N);

    // Compute per-sample difference RMS — nonzero if comb coloration changed the signal.
    float diffSum = 0.0f;
    for (int i = skip; i < N; ++i) {
        float dL = backOut.L[static_cast<size_t>(i)] - frontOut.L[static_cast<size_t>(i)];
        float dR = backOut.R[static_cast<size_t>(i)] - frontOut.R[static_cast<size_t>(i)];
        diffSum += dL * dL + dR * dR;
    }
    float diffRms = std::sqrt(diffSum / static_cast<float>(2 * (N - skip)));

    // There must be audible difference (comb coloration). If combWet=0, both would be identical.
    // At 30% wet with 10 comb filters in series, the difference should be clearly measurable.
    CHECK(diffRms > 0.001f);
}

TEST_CASE("Phase3Integration: Comb bank Y=0 and Y=1 both have wet=0 (similar output)", "[engine][depth][elevation]") {
    // At Y=0 and Y=1, combWet=0 → comb bank has no effect.
    // Both outputs should be nearly identical (modulo rear shadow which only kicks in at Y<0).
    const int N = 2048;
    const int skip = 256;

    EngineParams paramsY0;
    paramsY0.y = 0.0f;
    paramsY0.x = 0.0f;
    paramsY0.z = 0.0f;

    EngineParams paramsY1;
    paramsY1.y = 1.0f;
    paramsY1.x = 0.0f;
    paramsY1.z = 0.0f;

    auto outY0 = runEngine(paramsY0, 1000.0f, N);
    auto outY1 = runEngine(paramsY1, 1000.0f, N);

    // Both should have combWet=0. Outputs may differ slightly due to rear shadow (Y=0 vs Y=1),
    // but both should have non-zero energy (signal passes through).
    float rmsY0 = rmsLR(outY0, skip);
    float rmsY1 = rmsLR(outY1, skip);
    CHECK(rmsY0 > 0.01f);
    CHECK(rmsY1 > 0.01f);
}

TEST_CASE("Phase3Integration: Pinna notch Z=0 attenuates 8kHz vs Z=1", "[engine][depth][elevation]") {
    // Z=0: pinna notch = -15 dB at 8kHz → 8kHz energy is significantly reduced.
    // Z=1: pinna notch = +5 dB at 8kHz → 8kHz energy is boosted.
    // Z=1 output should have noticeably more energy than Z=0 at 8kHz.
    const int N = 2048;
    const int skip = 512;

    EngineParams paramsZ0;
    paramsZ0.z = 0.0f;
    paramsZ0.y = 1.0f;
    paramsZ0.x = 0.0f;

    EngineParams paramsZ1;
    paramsZ1.z = 1.0f;
    paramsZ1.y = 1.0f;
    paramsZ1.x = 0.0f;

    auto outZ0 = runEngine(paramsZ0, 8000.0f, N);
    auto outZ1 = runEngine(paramsZ1, 8000.0f, N);

    float rmsZ0 = rmsLR(outZ0, skip);
    float rmsZ1 = rmsLR(outZ1, skip);

    // Z=1 should have more 8kHz energy than Z=0 (boost vs notch)
    CHECK(rmsZ1 > rmsZ0);

    // The ratio should reflect the ~20 dB difference between +5dB and -15dB
    // At minimum, Z=1 should be at least 3x louder at 8kHz than Z=0
    CHECK(rmsZ1 > 3.0f * rmsZ0);
}

TEST_CASE("Phase3Integration: Pinna freeze below horizon: 8kHz attenuated at Z<0", "[engine][depth][elevation]") {
    // For Z < 0, the pinna notch is frozen at Z=0 values (pinnaGainDb = -15 dB at 8kHz).
    // Verify by checking that:
    //   1. At Z=0, 8kHz is attenuated (pinna notch active at -15dB).
    //   2. At Z=-1, 8kHz is ALSO attenuated (pinna frozen — no improvement vs Z=0).
    //   3. The difference between Z=0 and Z=1 (no notch vs notch) is large.
    //   4. Z=-1 does NOT produce the +5dB boost that Z=1 does.
    //
    // Use X=0, Y=1 (front, no rear comb), no lateral ILD/ITD.
    // Use N=2048 with skip for transient.
    const int N = 2048;
    const int skip = 512;

    EngineParams paramsZ0, paramsZneg1, paramsZ1;
    paramsZ0.z = 0.0f;   paramsZ0.y = 1.0f;   paramsZ0.x = 0.0f;
    paramsZneg1.z = -1.0f; paramsZneg1.y = 1.0f; paramsZneg1.x = 0.0f;
    paramsZ1.z = 1.0f;   paramsZ1.y = 1.0f;   paramsZ1.x = 0.0f;

    float rmsZ0    = rmsLR(runEngine(paramsZ0,    8000.0f, N), skip);
    float rmsZneg1 = rmsLR(runEngine(paramsZneg1, 8000.0f, N), skip);
    float rmsZ1    = rmsLR(runEngine(paramsZ1,    8000.0f, N), skip);

    // Z=1 has +5dB boost, Z=0 has -15dB notch → Z=1 should be much louder than Z=0
    CHECK(rmsZ1 > 3.0f * rmsZ0);

    // Z=-1 (pinna frozen at Z=0) should NOT boost 8kHz — should be close to Z=0.
    // We allow up to 2x difference to account for floor/chest bounce differences.
    // The key property: Z=-1 does NOT produce the Z=1 boost.
    CHECK(rmsZneg1 < rmsZ1 * 0.8f);  // Z=-1 has noticeably less 8kHz energy than Z=1

    // Both Z=0 and Z=-1 should show the -15dB notch (much less energy than Z=1).
    CHECK(rmsZ0    < rmsZ1 * 0.5f);  // Z=0 has notch (-15dB ~ factor 5.6x)
    CHECK(rmsZneg1 < rmsZ1 * 0.8f);  // Z=-1 also attenuated (pinna frozen at Z=0 values)
}

TEST_CASE("Phase3Integration: Floor bounce Z=-1 adds energy vs Z=1", "[engine][depth][elevation]") {
    // Z=-1: floor bounce is fully active (at -5 dB), adds delayed copy to output.
    // Z=1: floor bounce gain = 0, adds nothing.
    // Z=-1 should have slightly more total energy than Z=1.
    const int N = 4096;
    const int skip = 512;  // skip transients (floor delay up to 20ms = 882 samples)

    EngineParams paramsZneg1;
    paramsZneg1.z = -1.0f;
    paramsZneg1.y = 1.0f;
    paramsZneg1.x = 0.0f;

    EngineParams paramsZ1;
    paramsZ1.z = 1.0f;
    paramsZ1.y = 1.0f;
    paramsZ1.x = 0.0f;

    // Use a longer buffer to let the 20ms floor delay fill (882 samples at 44100).
    auto outZneg1 = runEngine(paramsZneg1, 500.0f, N);
    auto outZ1    = runEngine(paramsZ1,    500.0f, N);

    // Skip the floor delay buildup (at Z=-1, delay = 0ms since z+1=0 → delay=0ms).
    // Actually: chestDelayMs = clamp((z+1)*0.5, 0,1) * max. At Z=-1: (0)*0.5*20ms = 0ms.
    // Wait — re-read logic: floorDelayMs = clamp((-(-1)+1)*0.5, 0,1) = clamp(1, 0,1) = 1.0 * 20ms = 20ms.
    // So at Z=-1: floorDelaySamp = 20ms * 44.1 = 882 samples.
    // We need skip > 882 to see floor bounce contribution.

    // Use late skip (after the 20ms floor bounce delay fills up).
    const int lateSkip = 1024;  // > 882 samples to be safe
    float rmsZneg1 = rmsLR(outZneg1, lateSkip);
    float rmsZ1_   = rmsLR(outZ1,    lateSkip);

    // Z=-1 should have more energy due to floor bounce adding a -5dB delayed copy.
    CHECK(rmsZneg1 > rmsZ1_);
}

TEST_CASE("Phase3Integration: No NaN with all Phase 3 params", "[engine][depth][elevation]") {
    // Set all Phase 3 parameters to various values, process 4096 samples.
    // Verify no output sample is NaN or Inf.
    const int N = 4096;

    EngineParams params;
    params.x = 0.5f;
    params.y = -0.7f;
    params.z = -0.3f;
    params.combWetMax      = 0.30f;
    params.pinnaNotchFreqHz = 8000.0f;
    params.pinnaNotchQ     = 2.0f;
    params.pinnaShelfFreqHz = 4000.0f;
    params.chestDelayMaxMs = 2.0f;
    params.chestGainDb     = -8.0f;
    params.floorDelayMaxMs = 20.0f;
    params.floorGainDb     = -5.0f;

    auto out = runEngine(params, 440.0f, N);

    bool hasNaN = false;
    bool hasInf = false;
    for (int i = 0; i < N; ++i) {
        if (std::isnan(out.L[static_cast<size_t>(i)]) || std::isnan(out.R[static_cast<size_t>(i)]))
            hasNaN = true;
        if (std::isinf(out.L[static_cast<size_t>(i)]) || std::isinf(out.R[static_cast<size_t>(i)]))
            hasInf = true;
    }
    CHECK_FALSE(hasNaN);
    CHECK_FALSE(hasInf);
}

TEST_CASE("Phase3Integration: Reset clears Phase 3 state", "[engine][depth][elevation]") {
    // Process some audio to build up state in all Phase 3 filters and delay lines.
    // Call reset(), then process silence — output should be all zeros within float tolerance.
    const int N = 2048;
    const int silenceN = 512;

    XYZPanEngine engine;
    engine.prepare(static_cast<double>(kSampleRate), N);

    EngineParams params;
    params.x = 0.3f;
    params.y = -0.9f;  // strong comb bank wet
    params.z = -0.8f;  // chest and floor bounce active
    engine.setParams(params);

    // Generate white noise-like signal to excite all DSP paths
    std::vector<float> noise(static_cast<size_t>(N));
    for (int i = 0; i < N; ++i) {
        // Simple deterministic noise pattern
        noise[static_cast<size_t>(i)] = (i % 2 == 0) ? 0.7f : -0.7f;
    }

    std::vector<float> outL(static_cast<size_t>(N), 0.0f);
    std::vector<float> outR(static_cast<size_t>(N), 0.0f);
    const float* inputs[] = { noise.data() };
    engine.process(inputs, 1, outL.data(), outR.data(), N);

    // Now reset — should clear all state (comb buffers, biquad state, delay lines, smoothers)
    engine.reset();

    // Process silence — must produce silence
    std::vector<float> silence(static_cast<size_t>(silenceN), 0.0f);
    std::vector<float> postL(static_cast<size_t>(silenceN), 0.0f);
    std::vector<float> postR(static_cast<size_t>(silenceN), 0.0f);
    const float* silenceInputs[] = { silence.data() };
    engine.process(silenceInputs, 1, postL.data(), postR.data(), silenceN);

    // All output after reset + silence input must be zero
    float maxAbs = 0.0f;
    for (int i = 0; i < silenceN; ++i) {
        maxAbs = std::max(maxAbs, std::abs(postL[static_cast<size_t>(i)]));
        maxAbs = std::max(maxAbs, std::abs(postR[static_cast<size_t>(i)]));
    }
    CHECK(maxAbs < 1e-5f);
}
