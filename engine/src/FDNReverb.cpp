#include "xyzpan/dsp/FDNReverb.h"
#include "xyzpan/Constants.h"
#include <cmath>
#include <algorithm>

namespace xyzpan::dsp {

// ============================================================================
// prepare()
// ============================================================================

void FDNReverb::prepare(double sampleRate, int /*maxBlockSize*/) {
    sampleRate_ = sampleRate;

    // Pre-delay line: sized for kVerbPreDelayMaxMs at 192kHz worst case.
    // 50ms * 192000/1000 + 4 = 9604 samples, next power-of-2 allocation.
    int preDelayCap = static_cast<int>(kVerbPreDelayMaxMs * 192000.0 / 1000.0) + 8;
    preDelayLine_.prepare(preDelayCap);

    // FDN delay lines: sized for kFDNDelayMs[3] (largest = 63.45ms) at 192kHz.
    // 63.45ms * 192000/1000 + 8 = 12182. Use 13000 as safe upper bound.
    int fdnDelayCap = 13000;
    for (int i = 0; i < kN; ++i)
        delays_[i].prepare(fdnDelayCap);

    // Compute initial delay lengths from current sample rate.
    // Default size = 1.0 (will be set by setSize() after prepare()).
    float sizeNorm = 1.0f;
    for (int i = 0; i < kN; ++i)
        delayLengths_[i] = kFDNDelayMs[i] * static_cast<float>(sampleRate_) / 1000.0f * sizeNorm;

    // Zero all state.
    dampState_.fill(0.0f);
    feedbackGain_ = 0.0f;
    damping_      = 0.0f;
    wetGain_      = 0.0f;
}

// ============================================================================
// reset()
// ============================================================================

void FDNReverb::reset() {
    preDelayLine_.reset();
    for (int i = 0; i < kN; ++i)
        delays_[i].reset();
    dampState_.fill(0.0f);
}

// ============================================================================
// setSize()
// ============================================================================

void FDNReverb::setSize(float sizeNorm) {
    // Clamp to [0.1, 1.0] to prevent zero-length delays.
    sizeNorm = std::clamp(sizeNorm, 0.1f, 1.0f);
    for (int i = 0; i < kN; ++i)
        delayLengths_[i] = kFDNDelayMs[i] * static_cast<float>(sampleRate_) / 1000.0f * sizeNorm;
}

// ============================================================================
// setDecay()
// ============================================================================

void FDNReverb::setDecay(float decayNorm) {
    // Map decayNorm [0,1] to T60 in seconds.
    // Linear: 0 = instant (near-zero T60), 1 = kVerbMaxDecayT60_s (5s).
    float t60 = decayNorm * kVerbMaxDecayT60_s;

    // feedbackGain = pow(10, -3 * maxDelayMs / (1000 * T60))
    // This is the standard T60 formula for feedback delay networks.
    // maxDelayMs is the longest FDN delay loop (kFDNDelayMs[3] = 63.45ms).
    constexpr float maxDelayMs = kFDNDelayMs[3];  // 63.45f

    // Guard against t60 near zero (instant decay).
    float safeT60 = std::max(t60, 0.001f);
    feedbackGain_ = std::pow(10.0f, -3.0f * maxDelayMs / (1000.0f * safeT60));

    // Hard clamp to prevent instability.
    feedbackGain_ = std::clamp(feedbackGain_, 0.0f, 0.999f);
}

// ============================================================================
// setDamping()
// ============================================================================

void FDNReverb::setDamping(float d) {
    // Clamp to [0.0, 0.95] — values above 0.95 would be excessively muffled
    // and could cause subtle instability in the one-pole feedback path.
    damping_ = std::clamp(d, 0.0f, 0.95f);
}

// ============================================================================
// setWetDry()
// ============================================================================

void FDNReverb::setWetDry(float wet) {
    wetGain_ = std::clamp(wet, 0.0f, 1.0f);
}

// ============================================================================
// processSample()
// ============================================================================

void FDNReverb::processSample(float inL, float inR,
                               float preDelaySamp,
                               float& wetL, float& wetR) {
    // 1. Push mono sum into pre-delay line.
    const float monoIn = (inL + inR) * 0.5f;
    preDelayLine_.push(monoIn);

    // 2. Read pre-delayed signal (minimum 2 samples per Phase 2 decision).
    const float preDOut = preDelayLine_.read(std::max(2.0f, preDelaySamp));

    // 3. Read current FDN delay outputs.
    float x[kN];
    for (int i = 0; i < kN; ++i)
        x[i] = delays_[i].read(std::max(2.0f, delayLengths_[i]));

    // 4. Householder feedback matrix (negated Hadamard-style 4x4).
    // sum * 0.5 - x[i] ensures energy-preserving mixing without building
    // correlated copies. The factor 0.5 normalizes the 4-input sum.
    float sum = x[0] + x[1] + x[2] + x[3];
    float fb[kN];
    for (int i = 0; i < kN; ++i)
        fb[i] = sum * 0.5f - x[i];

    // 5. One-pole LP damping per loop.
    // dampState_[i] = (1 - damping) * fb[i] + damping * dampState_[i]
    for (int i = 0; i < kN; ++i)
        dampState_[i] = (1.0f - damping_) * fb[i] + damping_ * dampState_[i];

    // 6. Push back: pre-delayed input + feedback * gain.
    for (int i = 0; i < kN; ++i)
        delays_[i].push(preDOut + dampState_[i] * feedbackGain_);

    // 7. Stereo output: split FDN outputs L/R across two pairs.
    // Left = average of delays 0 and 1, Right = average of delays 2 and 3.
    wetL = (x[0] + x[1]) * 0.5f * wetGain_;
    wetR = (x[2] + x[3]) * 0.5f * wetGain_;
}

} // namespace xyzpan::dsp
