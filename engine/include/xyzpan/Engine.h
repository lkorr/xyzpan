#pragma once
#include "xyzpan/Types.h"
#include "xyzpan/Constants.h"
#include "xyzpan/dsp/FractionalDelayLine.h"
#include "xyzpan/dsp/SVFLowPass.h"
#include "xyzpan/dsp/OnePoleSmooth.h"
#include <vector>

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
// Phase 2 signal flow (per sample):
//   1. Stereo-to-mono sum (already from Phase 1)
//   2. Push mono into both delay lines (delayL_, delayR_)
//   3. Read delay lines with smoothed ITD (far ear delayed, near ear at 0)
//   4. Apply ILD gain attenuation to far ear
//   5. Apply head shadow SVF to far ear (near ear SVF stays wide open)
//   6. Apply rear shadow SVF equally to both ears
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
};

} // namespace xyzpan
