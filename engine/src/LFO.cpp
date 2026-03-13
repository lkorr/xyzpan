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

float LFO::tick() {
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
        default:
            out = 0.0f;
    }
    accumulator_ += increment_;
    if (accumulator_ >= 1.0f) accumulator_ -= 1.0f;
    return out;
}

} // namespace xyzpan::dsp
