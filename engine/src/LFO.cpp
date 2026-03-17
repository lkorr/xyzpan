#include "xyzpan/dsp/LFO.h"
#include <cmath>

namespace xyzpan::dsp {

void LFO::prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    // Do not reset accumulator — allows live sample rate changes without phase jump.
}

void LFO::reset(float phaseOffsetNorm) {
    accumulator_ = phaseOffsetNorm - std::floor(phaseOffsetNorm); // wrap to [0, 1)
}

void LFO::setRateHz(float hz) {
    if (hz < 0.0f) hz = 0.0f;
    increment_ = hz / static_cast<float>(sampleRate_);
}

void LFO::setPhaseOffset(float offset) {
    phaseOffset_ = offset - std::floor(offset); // wrap to [0, 1)
}

void LFO::requestReset() {
    resetPending_ = true;
}

float LFO::peek() const {
    const float phase = resetPending_ ? phaseOffset_ : accumulator_;
    switch (waveform) {
        case LFOWaveform::Sine:
            return std::sin(phase * 6.28318530f);
        case LFOWaveform::Triangle:
            return 1.0f - 4.0f * std::abs(phase - 0.5f);
        case LFOWaveform::Saw:
            return 2.0f * phase - 1.0f;
        case LFOWaveform::Square:
            return phase < 0.5f ? 1.0f : -1.0f;
        case LFOWaveform::RampDown:
            return 1.0f - 2.0f * phase;
        default:
            return 0.0f;
    }
}

float LFO::tick() {
    if (resetPending_) {
        accumulator_ = phaseOffset_;
        resetPending_ = false;
    }
    float out;
    switch (waveform) {
        case LFOWaveform::Sine:
            out = std::sin(accumulator_ * 6.28318530f);
            break;
        case LFOWaveform::Triangle:
            out = 1.0f - 4.0f * std::abs(accumulator_ - 0.5f);
            break;
        case LFOWaveform::Saw:
            out = 2.0f * accumulator_ - 1.0f;
            break;
        case LFOWaveform::Square:
            out = accumulator_ < 0.5f ? 1.0f : -1.0f;
            break;
        case LFOWaveform::RampDown:
            out = 1.0f - 2.0f * accumulator_;
            break;
        default:
            out = 0.0f;
    }
    accumulator_ += increment_;
    if (accumulator_ >= 1.0f) accumulator_ -= 1.0f;
    return out;
}

} // namespace xyzpan::dsp
