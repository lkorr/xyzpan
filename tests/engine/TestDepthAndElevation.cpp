// TestDepthAndElevation.cpp
// Unit tests for all Phase 3 DSP primitives:
//   FeedbackCombFilter, SVFFilter, BiquadFilter, OnePoleLP
//
// Each class is tested in isolation at 44100 Hz.
// RMS-based energy comparison is used for frequency-domain assertions.
// Requirements covered: DEPTH-01, DEPTH-02, DEPTH-04, ELEV-01, ELEV-03

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "xyzpan/dsp/FeedbackCombFilter.h"
#include "xyzpan/dsp/SVFFilter.h"
#include "xyzpan/dsp/BiquadFilter.h"
#include "xyzpan/dsp/OnePoleLP.h"

#include <vector>
#include <cmath>
#include <numeric>
#include <algorithm>

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
