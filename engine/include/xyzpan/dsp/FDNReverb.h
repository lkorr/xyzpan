#pragma once
#include "xyzpan/dsp/FractionalDelayLine.h"
#include "xyzpan/Constants.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace xyzpan::dsp {

// FDN (Feedback Delay Network) algorithmic reverb.
// 4-delay Householder feedback matrix with one-pole damping per loop.
// Pre-delay line (FractionalDelayLine) before the FDN input, distance-scaled.
// All delay lines sized for 192kHz worst case in prepare() — no reallocation on
// sample rate changes (same approach as Phase 4 distance delay lines).
class FDNReverb {
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    // Set room size [0,1]: scales all delay lengths proportionally.
    // NOTE: delay length changes are applied at prepare() time only —
    // do NOT call setSize() per-sample (would cause pitch artifacts).
    // Size is fixed after prepare(); only feedback gain (decay) varies per block.
    void setSize(float sizeNorm);

    // Set decay [0,1]: maps to T60 (0 = instant decay, 1 = ~5s T60).
    // Computes feedbackGain_ = pow(10, -3 * maxDelayMs / (1000 * decayT60)).
    // Hard-clamped to [0.0, 0.999] to prevent feedback instability.
    void setDecay(float decayNorm);

    // Set damping [0,1]: one-pole lowpass pole position in each feedback loop.
    // 0 = no damping (bright), 1 = heavy damping (dark/muffled).
    void setDamping(float d);

    // Process one stereo sample. preDelaySamp is the fractional delay in
    // samples for the pre-delay line (0 = no pre-delay, computed from distance).
    // wetL/wetR are the reverb-only output; caller mixes with dry signal.
    void processSample(float inL, float inR,
                       float preDelaySamp,
                       float& wetL, float& wetR);

    // Set wet gain [0,1] for output mixing. Applied to wetL/wetR output.
    void setWetDry(float wet);

private:
    static constexpr int kN = 4;

    FractionalDelayLine preDelayLine_;
    std::array<FractionalDelayLine, kN> delays_;

    // Delay lengths in samples (set in prepare() from kFDNDelayMs * sampleRate/44100 * size)
    std::array<float, kN> delayLengths_ = {};

    // One-pole LP state per delay loop
    std::array<float, kN> dampState_ = {};

    float feedbackGain_ = 0.0f;   // per-loop feedback scalar (from decay)
    float damping_      = 0.0f;   // pole position for one-pole LP
    float wetGain_      = 0.0f;   // output wet mix
    double sampleRate_  = 44100.0;
};

} // namespace xyzpan::dsp
