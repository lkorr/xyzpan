#pragma once
// OnePoleLP.h
// First-order 6 dB/octave lowpass filter parameterised by cutoff frequency.
//
// Distinct from OnePoleSmooth (which uses a smoothing time constant in ms).
// This class uses a cutoff frequency in Hz, making it suitable for audio
// filtering rather than parameter smoothing.
//
// Coefficient formula:
//   a = exp(-2 * pi * cutoffHz / sampleRate)   [pole position]
//   b = 1 - a                                   [DC gain normalisation]
//
// Difference equation: z[n] = b * x[n] + a * z[n-1]
//
// Used for: chest bounce 6 dB/oct lowpass at 1 kHz (ELEV-03).
//
// Source: standard first-order IIR lowpass design
//   (equivalent to RC-circuit impulse response discretised via impulse invariant method)

#include <cmath>

namespace xyzpan::dsp {

class OnePoleLP {
public:
    // Zero filter state. Call before processing or after silence.
    void reset() {
        z_ = 0.0f;
    }

    // Compute filter coefficients.
    //   cutoffHz   — -3 dB cutoff frequency in Hz
    //   sampleRate — audio sample rate in Hz
    //
    // Call once per block or whenever the cutoff changes.
    void setCoefficients(float cutoffHz, float sampleRate) {
        a_ = std::exp(-2.0f * 3.14159265f * cutoffHz / sampleRate);
        b_ = 1.0f - a_;
    }

    // Process one input sample and return the filtered output.
    float process(float x) {
        z_ = b_ * x + a_ * z_;
        return z_;
    }

private:
    float a_ = 0.0f;  // pole (feedback coefficient)
    float b_ = 1.0f;  // feedforward coefficient (DC gain = 1)
    float z_ = 0.0f;  // filter state
};

} // namespace xyzpan::dsp
