#pragma once
#include "xyzpan/Types.h"
#include "xyzpan/Constants.h"
#include "xyzpan/dsp/FractionalDelayLine.h"
#include "xyzpan/dsp/SVFLowPass.h"
#include "xyzpan/dsp/OnePoleSmooth.h"
#include "xyzpan/dsp/FeedbackCombFilter.h"
#include "xyzpan/dsp/SVFFilter.h"
#include "xyzpan/dsp/BiquadFilter.h"
#include "xyzpan/dsp/OnePoleLP.h"
#include <vector>
#include <array>

namespace xyzpan {

// XYZPanEngine — pure C++ audio processing engine with no JUCE dependency.
//
// Lifecycle:
//   1. prepare(sampleRate, maxBlockSize) — called before processing; pre-allocates buffers
//   2. setParams(params)                 — called once per processBlock with current parameter snapshot
//   3. process(...)                      — called each processBlock; fills outL and outR
//   4. reset()                           — called on transport restart; clears all state
//
// Audio contract:
//   - Accepts 1 or 2 input channels; stereo is summed to mono internally.
//   - Always produces 2-channel (stereo) output.
//   - No allocation inside process(); all buffers pre-allocated in prepare().
//
// Phase 4 signal flow (per sample):
//   1. Stereo-to-mono sum (Phase 1)
//   2. Comb bank (series) with Y-driven dry/wet blend [DEPTH]
//   3. Pinna notch EQ + high shelf (Z-driven) [ELEV-01, ELEV-02]
//   4. ITD/ILD binaural split (Phase 2) — with proximity-scaled ITD and head shadow
//   5. Chest bounce: parallel filtered+delayed copy added to both ears [ELEV-03]
//   6. Floor bounce: parallel delayed copy added to both ears [ELEV-04]
//   7. Distance processing: gain attenuation + delay+doppler + air absorption LPF [DIST-01 through DIST-06]
class XYZPanEngine {
public:
    XYZPanEngine() = default;
    ~XYZPanEngine() = default;

    // Non-copyable, non-movable (owns audio buffers).
    XYZPanEngine(const XYZPanEngine&) = delete;
    XYZPanEngine& operator=(const XYZPanEngine&) = delete;

    // Called before processing begins. Allocates monoBuffer to maxBlockSize.
    void prepare(double sampleRate, int maxBlockSize);

    // Set current parameters (snapshot from APVTS atomics, called once per block).
    void setParams(const EngineParams& params);

    // Process audio.
    //   inputs            — array of input channel pointers (1 or 2 channels)
    //   numInputChannels  — number of valid input channel pointers
    //   outL, outR        — output channel pointers (pre-allocated by caller)
    //   numSamples        — number of samples to process (must not exceed maxBlockSize)
    void process(const float* const* inputs, int numInputChannels,
                 float* outL, float* outR, int numSamples);

    // Reset all internal state (delay lines, filter states, smoothers).
    // Call on transport restart or after silence gaps.
    void reset();

private:
    EngineParams currentParams;
    std::vector<float> monoBuffer;  // pre-allocated in prepare() to maxBlockSize
    double sampleRate   = 44100.0;
    int    maxBlockSize = 512;

    // ITD delay lines — one per ear.
    // Allocated in prepare() for kMaxITDUpperBound_ms of capacity.
    dsp::FractionalDelayLine delayL_;
    dsp::FractionalDelayLine delayR_;

    // Head shadow SVFs — one per ear.
    // Near ear stays wide open (kHeadShadowFullOpenHz).
    // Far ear gets cutoff modulated by azimuth magnitude.
    dsp::SVFLowPass shadowL_;
    dsp::SVFLowPass shadowR_;

    // Rear shadow SVFs — applied equally to both ears when Y < 0 (source behind).
    dsp::SVFLowPass rearSvfL_;
    dsp::SVFLowPass rearSvfR_;

    // Per-parameter exponential smoothers for click-free automation.
    dsp::OnePoleSmooth itdSmooth_;           // ITD delay in samples
    dsp::OnePoleSmooth shadowCutoffSmooth_;  // head shadow SVF cutoff
    dsp::OnePoleSmooth ildGainSmooth_;       // ILD gain (linear, far ear)
    dsp::OnePoleSmooth rearCutoffSmooth_;    // rear shadow SVF cutoff

    // Tracking last smoothing time constants to detect changes from dev panel.
    // When a smoothMs param changes, the corresponding smoother is re-prepared
    // with the new time constant (prepare() updates coefficients without resetting
    // the smoother state — no click from parameter change).
    float lastSmoothMs_ITD_    = kDefaultSmoothMs_ITD;
    float lastSmoothMs_Filter_ = kDefaultSmoothMs_Filter;
    float lastSmoothMs_Gain_   = kDefaultSmoothMs_Gain;

    // =========================================================================
    // Phase 3: Depth — comb filter bank (series, Y-driven wet/dry)
    // =========================================================================
    std::array<dsp::FeedbackCombFilter, kMaxCombFilters> combBank_;
    dsp::OnePoleSmooth combWetSmooth_;   // smooth wet amount transitions

    // =========================================================================
    // Phase 3: Elevation — pinna notch and high shelf (mono domain, before binaural split)
    // =========================================================================
    dsp::BiquadFilter pinnaNotch_;
    dsp::BiquadFilter pinnaShelf_;

    // =========================================================================
    // Phase 3: Elevation — chest bounce (post-binaural, parallel path)
    // =========================================================================
    std::array<dsp::SVFFilter, 4> chestHPF_;  // 4x HP cascade at 700Hz
    dsp::OnePoleLP                chestLP_;    // 1x 6dB/oct LP at 1kHz
    dsp::FractionalDelayLine      chestDelay_; // 0–2ms delay
    dsp::OnePoleSmooth            chestGainSmooth_;  // smooth chest gain transitions

    // =========================================================================
    // Phase 3: Elevation — floor bounce (post-binaural, parallel path)
    // =========================================================================
    dsp::FractionalDelayLine floorDelayL_;   // per-ear floor bounce delay
    dsp::FractionalDelayLine floorDelayR_;
    dsp::OnePoleSmooth       floorGainSmooth_;  // smooth floor gain transitions

    // =========================================================================
    // Phase 4: Distance Processing (DIST-01 through DIST-06)
    // =========================================================================
    dsp::FractionalDelayLine distDelayL_;     // propagation delay + doppler, left
    dsp::FractionalDelayLine distDelayR_;     // propagation delay + doppler, right
    dsp::OnePoleLP           airLPF_L_;       // air absorption LPF, left
    dsp::OnePoleLP           airLPF_R_;       // air absorption LPF, right
    dsp::OnePoleSmooth       distDelaySmooth_; // smooth delay target (produces doppler)
    dsp::OnePoleSmooth       distGainSmooth_;  // smooth gain rolloff (DIST-01)
    float lastDistSmoothMs_ = kDistSmoothMs;  // track dev panel changes to re-prepare smoother
};

} // namespace xyzpan
