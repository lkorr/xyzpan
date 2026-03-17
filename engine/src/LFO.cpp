#include "xyzpan/dsp/LFO.h"
#include "xyzpan/dsp/SineLUT.h"
#include <cmath>

namespace xyzpan::dsp {

void LFO::prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    // Recompute smoothing coefficients if smoothing is active
    if (smoothMs_ > 0.0f)
        setSmoothMs(smoothMs_);
}

void LFO::reset(float phaseOffsetNorm) {
    accumulator_ = phaseOffsetNorm - std::floor(phaseOffsetNorm); // wrap to [0, 1)

    // Reset S&H state
    shHeldValue_ = 0.0f;
    shPrevPhase_ = accumulator_;
    shState_     = 123456789u;

    // Reset smoother state
    smoothZ_ = 0.0f;
}

void LFO::setRateHz(float hz) {
    if (hz < 0.0f) hz = 0.0f;
    increment_ = hz / static_cast<float>(sampleRate_);
}

void LFO::setPhaseOffset(float offset) {
    float newOffset = offset - std::floor(offset); // wrap to [0, 1)
    float delta = newOffset - phaseOffset_;
    if (delta != 0.0f) {
        accumulator_ += delta;
        accumulator_ -= std::floor(accumulator_); // wrap to [0, 1)
    }
    phaseOffset_ = newOffset;
}

void LFO::setSmoothMs(float ms) {
    smoothMs_ = ms;
    if (ms <= 0.0f || sampleRate_ <= 0.0) {
        smoothCoeffA_ = 0.0f;
        smoothCoeffB_ = 1.0f;
        return;
    }
    // One-pole lowpass: coeff = exp(-1 / (tau * sampleRate))
    // tau = ms / 1000
    const double tau = static_cast<double>(ms) * 0.001;
    smoothCoeffA_ = static_cast<float>(std::exp(-1.0 / (tau * sampleRate_)));
    smoothCoeffB_ = 1.0f - smoothCoeffA_;
}

void LFO::requestReset() {
    resetPending_ = true;
}

float LFO::peek() const {
    const float phase = resetPending_ ? phaseOffset_ : accumulator_;
    switch (waveform) {
        case LFOWaveform::Sine:
            return SineLUT::lookup(phase);
        case LFOWaveform::Triangle:
            return 1.0f - 4.0f * std::abs(phase - 0.5f);
        case LFOWaveform::Saw:
            return 2.0f * phase - 1.0f;
        case LFOWaveform::RampDown:
            return 1.0f - 2.0f * phase;
        case LFOWaveform::Square:
            return phase < 0.5f ? 1.0f : -1.0f;
        case LFOWaveform::SampleHold:
            return shHeldValue_;
        default:
            return 0.0f;
    }
}

float LFO::xorshift32() {
    shState_ ^= shState_ << 13;
    shState_ ^= shState_ >> 17;
    shState_ ^= shState_ << 5;
    // Map uint32 to [-1, 1]
    return static_cast<float>(static_cast<int32_t>(shState_)) / 2147483648.0f;
}

float LFO::tick() {
    if (resetPending_) {
        accumulator_ = phaseOffset_;
        resetPending_ = false;
        // Reset S&H on phase reset
        shPrevPhase_ = accumulator_;
        shHeldValue_ = xorshift32();
    }

    float out;
    switch (waveform) {
        case LFOWaveform::Sine:
            out = SineLUT::lookup(accumulator_);
            break;
        case LFOWaveform::Triangle:
            out = 1.0f - 4.0f * std::abs(accumulator_ - 0.5f);
            break;
        case LFOWaveform::Saw:
            out = 2.0f * accumulator_ - 1.0f;
            break;
        case LFOWaveform::RampDown:
            out = 1.0f - 2.0f * accumulator_;
            break;
        case LFOWaveform::Square:
            out = accumulator_ < 0.5f ? 1.0f : -1.0f;
            break;
        case LFOWaveform::SampleHold:
            out = shHeldValue_;
            break;
        default:
            out = 0.0f;
    }

    // Advance accumulator
    float prevAcc = accumulator_;
    accumulator_ += increment_;
    if (accumulator_ >= 1.0f) accumulator_ -= 1.0f;

    // S&H: latch new random value on phase wrap
    if (waveform == LFOWaveform::SampleHold) {
        if (accumulator_ < prevAcc) {
            // Phase wrapped — latch new value
            shHeldValue_ = xorshift32();
        }
    }

    // Apply output smoother
    if (smoothMs_ > 0.0f) {
        out = out * smoothCoeffB_ + smoothZ_ * smoothCoeffA_;
        smoothZ_ = out;
    }

    return out;
}

} // namespace xyzpan::dsp
