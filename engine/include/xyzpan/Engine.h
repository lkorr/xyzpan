#pragma once
#include "xyzpan/Types.h"
#include "xyzpan/Constants.h"
#include "xyzpan/DSPStateBridge.h"
#include "xyzpan/BinauralPipeline.h"
#include "xyzpan/DistancePipeline.h"
#include "xyzpan/ChestPipeline.h"
#include "xyzpan/FloorPipeline.h"
#include "xyzpan/ERPipeline.h"
#include "xyzpan/dsp/FractionalDelayLine.h"
#include "xyzpan/dsp/OnePoleSmooth.h"
#include "xyzpan/dsp/FDNReverb.h"
#include "xyzpan/dsp/LFO.h"
#include <vector>
#include <random>

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
// Signal flow (per sample):
//   1. Stereo-to-mono sum (Phase 1)
//   2. Doppler delay (mono, before all spatial processing) [DIST-03, DIST-04]
//   3. Comb bank (series) with Y-driven dry/wet blend [DEPTH]
//   4. Mono EQ: presenceShelf (Y) → earCanalPeak (Y) → P1 → N1 → N2 → pinnaShelf (Z) [ELEV]
//   5. ITD/ILD binaural split (Phase 2) — with proximity-scaled ITD and head shadow
//   6. Chest bounce: parallel filtered+delayed copy of doppler'd input [ELEV-03]
//   7. Floor bounce: parallel delayed copy added to both ears [ELEV-04]
//   8. Distance processing: gain attenuation + air absorption LPF [DIST-01, DIST-02, DIST-05, DIST-06]
//   9. Early reflections: image source method, taps doppler'd input [ER]
//  10. FDN Reverb (VERB-01, VERB-02) — final stereo stage, with distance-scaled pre-delay
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
                 float* outL, float* outR,
                 float* auxL, float* auxR,  // nullptr when aux bus inactive
                 int numSamples);

    // Reset all internal state (delay lines, filter states, smoothers).
    // Call on transport restart or after silence gaps.
    void reset();

    // Phase 6: Last modulated position (base XYZ + LFO offset) from most recent process() call.
    // Written on audio thread at end of process(); read via PositionBridge by audio thread.
    struct ModulatedPosition { float x = 0.0f, y = 1.0f, z = 0.0f; };
    ModulatedPosition getLastModulatedPosition() const noexcept { return lastModulated_; }

    // Stereo node positions from most recent process() call
    struct StereoNodePositions { float lx, ly, lz, rx, ry, rz; float width; };
    StereoNodePositions getLastStereoNodes() const noexcept { return lastStereoNodes_; }

    // Live DSP state snapshot for dev panel display (UI-07).
    DSPStateSnapshot getLastDSPState() const noexcept { return lastDSPState_; }

    // LFO output snapshot — final tick()*depth values for UI waveform displays.
    struct LFOOutputs {
        float x = 0.f, y = 0.f, z = 0.f;
        float orbitXY = 0.f, orbitXZ = 0.f, orbitYZ = 0.f;
    };
    LFOOutputs getLastLFOOutputs() const noexcept;

private:
    EngineParams currentParams;
    std::vector<float> monoBuffer;  // pre-allocated in prepare() to maxBlockSize
    double sampleRate   = 44100.0;
    int    maxBlockSize = 512;

    // L-channel binaural pipeline (comb bank + pinna EQ + ITD/ILD/shadow)
    BinauralPipeline src_;

    // Tracking last smoothing time constants to detect changes from dev panel.
    float lastSmoothMs_ITD_    = kDefaultSmoothMs_ITD;
    float lastSmoothMs_Filter_ = kDefaultSmoothMs_Filter;
    float lastSmoothMs_Gain_   = kDefaultSmoothMs_Gain;

    // =========================================================================
    // Phase 3: Elevation — chest bounce (post-binaural, parallel path)
    // =========================================================================
    ChestPipeline chest_;  // L-channel chest pipeline

    // =========================================================================
    // Phase 3: Elevation — floor bounce (post-binaural, parallel path)
    // =========================================================================
    FloorPipeline floor_;  // L-channel floor pipeline

    // =========================================================================
    // Phase 4: Distance Processing (DIST-01 through DIST-06)
    // =========================================================================
    DistancePipeline dist_;                    // L-channel distance pipeline
    float lastDistSmoothMs_ = kDistSmoothMs;   // track dev panel changes to re-prepare smoother

    // =========================================================================
    // Phase 5: Reverb (VERB-01 through VERB-04)
    // =========================================================================
    dsp::FDNReverb   reverb_;
    dsp::OnePoleSmooth verbWetSmooth_;   // smooth wet/dry transitions

    // =========================================================================
    // Aux reverb send (post-air-absorption, pre-FDN reverb)
    // =========================================================================
    dsp::FractionalDelayLine auxPreDelayL_;
    dsp::FractionalDelayLine auxPreDelayR_;
    dsp::OnePoleSmooth       auxGainSmooth_;
    dsp::OnePoleSmooth       auxDelaySmooth_;   // smooth aux pre-delay transitions

    // =========================================================================
    // Phase 5: LFO (LFO-01 through LFO-05)
    // =========================================================================
    dsp::LFO lfoX_, lfoY_, lfoZ_;
    dsp::OnePoleSmooth lfoDepthXSmooth_, lfoDepthYSmooth_, lfoDepthZSmooth_;

    // Dev tool: test tone oscillator state — persistent across blocks
    float        sawPhase_ = 0.0f;  // [0, 1)
    float        clickSamplesLeft_ = 0.0f;
    bool         prevPulseGate_ = false;
    dsp::LFO     pulseLFO_;
    std::mt19937 noiseRng_;

    // =========================================================================
    // Stereo source node splitting — R channel binaural pipeline
    // =========================================================================
    BinauralPipeline srcR_;

    // R-channel distance pipeline for stereo per-node distance processing
    DistancePipeline distR_;

    // R-channel chest, floor, and ER pipelines for stereo per-node processing
    ChestPipeline chestR_;
    FloorPipeline floorR_;
    ERPipeline erR_;

    // Stereo orbit LFOs (3 planes) + depth smoothers
    dsp::LFO orbitLfoXY_, orbitLfoXZ_, orbitLfoYZ_;
    dsp::OnePoleSmooth orbitDepthXYSmooth_, orbitDepthXZSmooth_, orbitDepthYZSmooth_;

    // =========================================================================
    // Early Reflections (Image Source Method)
    // =========================================================================
    ERPipeline er_;                              // L-channel ER pipeline
    dsp::OnePoleSmooth erLevelSmooth_;
    dsp::OnePoleSmooth erReverbSendSmooth_;

    // Test tone gain smoother — prevents click when test tone is enabled
    // on fresh prepare/reset (first block goes from silence to full gain).
    dsp::OnePoleSmooth testToneGainSmooth_;

    // Last LFO output values (tick()*depth) — captured per block for UI display
    float lastLfoOutX_ = 0.f, lastLfoOutY_ = 0.f, lastLfoOutZ_ = 0.f;
    float lastLfoOutOrbitXY_ = 0.f, lastLfoOutOrbitXZ_ = 0.f, lastLfoOutOrbitYZ_ = 0.f;

    // Width transition smoother (avoids pops when width changes)
    dsp::OnePoleSmooth stereoWidthSmooth_;

    // Circular (angular) smoothers for phase and offset — prevents clicks at wrap-around.
    // These smooth in the angular domain (radians) using sin/cos decomposition to handle
    // the 0↔1 boundary correctly: the smoother always takes the shortest path.
    float phaseSmCos_ = 1.0f, phaseSmSin_ = 0.0f;   // unit-circle state for phase
    float offsetSmCos_ = 1.0f, offsetSmSin_ = 0.0f;  // unit-circle state for offset
    float angularSmA_ = 0.0f;     // per-sample IIR coefficient (used for phase/offset)
    float listenerBlkSmA_ = 0.0f; // per-block IIR coefficient (used after movement stops)
    float listenerMovSmA_ = 0.0f; // per-block IIR coefficient (used during movement)

    // Circular (angular) smoothers for listener yaw/pitch — prevents clicks at 360°↔0° wrap.
    float yawSmCos_ = 1.0f, yawSmSin_ = 0.0f;
    float pitchSmCos_ = 1.0f, pitchSmSin_ = 0.0f;
    float rollSmCos_ = 1.0f, rollSmSin_ = 0.0f;

    // Previous block's smoothed cos/sin for per-sample trig interpolation.
    // Eliminates block-boundary discontinuities during fast head rotation.
    float prevCosY_ = 1.f, prevSinY_ = 0.f;
    float prevCosP_ = 1.f, prevSinP_ = 0.f;
    float prevCosR_ = 1.f, prevSinR_ = 0.f;

    // Cached listener orientation for change-detection gating (skip smoother when static)
    float prevListenerYaw_   = 0.0f;
    float prevListenerPitch_ = 0.0f;
    float prevListenerRoll_  = 0.0f;
    float cachedCosY_ = 1.0f, cachedSinY_ = 0.0f;
    float cachedCosP_ = 1.0f, cachedSinP_ = 0.0f;
    float cachedCosR_ = 1.0f, cachedSinR_ = 0.0f;
    bool  listenerSettled_ = true;  // true when cos/sin have snapped to exact target

    // Last L/R node positions for position bridge
    StereoNodePositions lastStereoNodes_{};

    // Phase 6: Last-sample modulated position from most recent process() call (UI-07).
    // Audio thread writes after process(); PositionBridge propagates to GL thread.
    ModulatedPosition lastModulated_;

    // Live DSP state for dev panel display bridge (UI-07).
    DSPStateSnapshot lastDSPState_;

    // =========================================================================
    // Per-block pre-computed cache (optimization: avoid per-sample transcendentals)
    // =========================================================================
    // Cached linear ILD gain base: pow(10, -ildMaxDb/20). Updated once per block.
    // Used by BinauralPipeline::processSample() to compute per-node ILD target
    // without re-calling std::pow on every sample.
    float ildGainBase_ = 1.0f;
    float blkDistGainMaxDb_ = 6.0206f;  // 20*log10(kDistGainMax), recomputed per block

    // Binaural toggle — click-free blend between binaural (1) and hardpan (0)
    dsp::OnePoleSmooth binauralBlendSmooth_;
    float hardpanGainBase_ = 1.0f;  // pow(10, kHardpanMaxDb/20), recomputed per block
};

} // namespace xyzpan
