#pragma once
#include "xyzpan/Types.h"
#include "xyzpan/Constants.h"
#include "xyzpan/DSPStateBridge.h"
#include "xyzpan/dsp/FractionalDelayLine.h"
#include "xyzpan/dsp/SVFLowPass.h"
#include "xyzpan/dsp/OnePoleSmooth.h"
#include "xyzpan/dsp/FeedbackCombFilter.h"
#include "xyzpan/dsp/SVFFilter.h"
#include "xyzpan/dsp/BiquadFilter.h"
#include "xyzpan/dsp/OnePoleLP.h"
#include "xyzpan/dsp/FDNReverb.h"
#include "xyzpan/dsp/LFO.h"
#include <vector>
#include <array>
#include <random>

namespace xyzpan {

// Per-source binaural DSP state. When stereo width > 0, L and R input channels
// are each processed through an independent BinauralPipeline with the same
// coefficients but separate filter state. Shared stages (chest/floor bounce,
// distance, reverb) run once on the summed binaural output.
struct BinauralPipeline {
    // ITD delay lines — one per ear
    dsp::FractionalDelayLine delayL, delayR;

    // Head shadow SVFs — one per ear
    dsp::SVFLowPass shadowL, shadowR;

    // Rear shadow SVFs — both ears
    dsp::SVFLowPass rearSvfL, rearSvfR;

    // Per-parameter smoothers
    dsp::OnePoleSmooth itdSmooth;
    dsp::OnePoleSmooth shadowCutoffSmooth;
    dsp::OnePoleSmooth ildGainSmooth;
    dsp::OnePoleSmooth rearCutoffSmooth;

    // Near-field ILD biquads
    dsp::BiquadFilter nearFieldLF_L, nearFieldLF_R;

    // Per-source comb bank (same coefficients as L, independent state)
    std::array<dsp::FeedbackCombFilter, kMaxCombFilters> combBank;
    dsp::OnePoleSmooth combWetSmooth;

    // Per-source mono EQ chain (same coefficients, independent state)
    dsp::BiquadFilter presenceShelf, earCanalPeak, pinnaP1;
    dsp::BiquadFilter pinnaNotch, pinnaNotch2, pinnaShelf;

    void prepare(float sr, int delayCap, float combMaxMs);
    void reset();
};

// Per-source distance DSP state. When stereo width > 0, L and R input channels
// each get independent distance processing (gain attenuation, delay+doppler,
// air absorption) based on their own node positions.
struct DistancePipeline {
    dsp::FractionalDelayLine distDelayL, distDelayR;
    dsp::OnePoleLP airLPF_L, airLPF_R;    // stage 1
    dsp::OnePoleLP airLPF2_L, airLPF2_R;  // stage 2
    dsp::OnePoleLP dopplerAA_L, dopplerAA_R;  // pre-delay anti-alias LP
    dsp::OnePoleSmooth distGainSmooth;
    dsp::OnePoleSmooth distDelaySmooth;
    void prepare(float sr);
    void reset();
};

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
// Phase 5 signal flow (per sample):
//   1. Stereo-to-mono sum (Phase 1)
//   2. Comb bank (series) with Y-driven dry/wet blend [DEPTH]
//   3. Mono EQ: presenceShelf (Y) → earCanalPeak (Y) → P1 → N1 → N2 → pinnaShelf (Z) [ELEV]
//   4. ITD/ILD binaural split (Phase 2) — with proximity-scaled ITD and head shadow
//   5. Chest bounce: parallel filtered+delayed copy added to both ears [ELEV-03]
//   6. Floor bounce: parallel delayed copy added to both ears [ELEV-04]
//   7. Distance processing: gain attenuation + delay+doppler + air absorption LPF [DIST-01 through DIST-06]
//   8. FDN Reverb (VERB-01, VERB-02) — final stereo stage, with distance-scaled pre-delay
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
    dsp::BiquadFilter pinnaNotch_;    // N1: elevation-shifted notch (6.5–10 kHz)
    dsp::BiquadFilter pinnaNotch2_;   // N2: secondary notch (N1 + 3 kHz)
    dsp::BiquadFilter pinnaP1_;       // P1: fixed +4 dB peak at 5 kHz
    dsp::BiquadFilter presenceShelf_; // front-boosting high shelf at 3 kHz (Y-mapped)
    dsp::BiquadFilter earCanalPeak_;  // ear canal resonance peak at 2.7 kHz (Y-mapped)
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
    dsp::OnePoleLP           floorLPF_;      // HF absorption on reflected floor signal
    dsp::OnePoleSmooth       floorGainSmooth_;  // smooth floor gain transitions

    // =========================================================================
    // Phase 4: Distance Processing (DIST-01 through DIST-06)
    // =========================================================================
    dsp::FractionalDelayLine distDelayL_;     // propagation delay + doppler, left
    dsp::FractionalDelayLine distDelayR_;     // propagation delay + doppler, right
    dsp::OnePoleLP           dopplerAA_L_;    // pre-delay anti-alias LP, left
    dsp::OnePoleLP           dopplerAA_R_;    // pre-delay anti-alias LP, right
    dsp::OnePoleLP           airLPF_L_;       // air absorption LPF stage 1, left
    dsp::OnePoleLP           airLPF_R_;       // air absorption LPF stage 1, right
    dsp::OnePoleLP           airLPF2_L_;      // air absorption LPF stage 2 (cascade → 12dB/oct), left
    dsp::OnePoleLP           airLPF2_R_;      // air absorption LPF stage 2, right
    dsp::BiquadFilter        nearFieldLF_L_;  // near-field ILD: ipsilateral LF boost, left
    dsp::BiquadFilter        nearFieldLF_R_;  // near-field ILD: ipsilateral LF boost, right
    dsp::OnePoleSmooth       distDelaySmooth_; // smooth delay target (produces doppler)
    dsp::OnePoleSmooth       distGainSmooth_;  // smooth gain rolloff (DIST-01)
    float lastDistSmoothMs_ = kDistSmoothMs;  // track dev panel changes to re-prepare smoother

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

    // =========================================================================
    // Phase 5: LFO (LFO-01 through LFO-05)
    // =========================================================================
    dsp::LFO lfoX_, lfoY_, lfoZ_;
    dsp::OnePoleSmooth lfoDepthXSmooth_, lfoDepthYSmooth_, lfoDepthZSmooth_;

    // Dev tool: test tone oscillator state — persistent across blocks
    float        sawPhase_ = 0.0f;  // [0, 1)
    dsp::LFO     pulseLFO_;
    std::mt19937 noiseRng_;

    // =========================================================================
    // Stereo source node splitting — R channel binaural pipeline
    // =========================================================================
    BinauralPipeline srcR_;

    // R-channel distance pipeline for stereo per-node distance processing
    DistancePipeline distR_;

    // Stereo orbit LFOs (3 planes) + depth smoothers
    dsp::LFO orbitLfoXY_, orbitLfoXZ_, orbitLfoYZ_;
    dsp::OnePoleSmooth orbitDepthXYSmooth_, orbitDepthXZSmooth_, orbitDepthYZSmooth_;

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
    float angularSmA_ = 0.0f;  // smoothing coefficient (shared, prepared once)

    // Helper: distance processing for a single source node (stereo path)
    struct DistanceResult { float left; float right; float distFrac; };
    DistanceResult processDistanceForNode(
        float dL, float dR,
        float nodeX, float nodeY, float nodeZ,
        float sr, bool dopplerOn,
        dsp::FractionalDelayLine& ddL, dsp::FractionalDelayLine& ddR,
        dsp::OnePoleLP& aaL, dsp::OnePoleLP& aaR,
        dsp::OnePoleLP& aL1, dsp::OnePoleLP& aR1,
        dsp::OnePoleLP& aL2, dsp::OnePoleLP& aR2,
        dsp::OnePoleSmooth& dgSmooth,
        dsp::OnePoleSmooth& ddSmooth
    );

    // Helper: run comb bank + mono EQ + binaural split for one source node
    struct BinauralResult { float left; float right; };
    BinauralResult processBinauralForSource(
        float inputSample,
        float nodeX, float nodeY, float nodeZ,
        float sr,
        // Pipeline state — references to either existing flat members (L) or srcR_ (R)
        dsp::FractionalDelayLine& dl, dsp::FractionalDelayLine& dr,
        dsp::SVFLowPass& shL, dsp::SVFLowPass& shR,
        dsp::SVFLowPass& rSvfL, dsp::SVFLowPass& rSvfR,
        dsp::OnePoleSmooth& itdSm, dsp::OnePoleSmooth& shCutSm,
        dsp::OnePoleSmooth& ildSm, dsp::OnePoleSmooth& rearCutSm,
        dsp::BiquadFilter& nfL, dsp::BiquadFilter& nfR,
        std::array<dsp::FeedbackCombFilter, kMaxCombFilters>& combs,
        dsp::OnePoleSmooth& combWetSm,
        dsp::BiquadFilter& presShelf, dsp::BiquadFilter& earCanal,
        dsp::BiquadFilter& pP1, dsp::BiquadFilter& pN1,
        dsp::BiquadFilter& pN2, dsp::BiquadFilter& pShelf
    );

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
    // Used by processBinauralForSource() to compute per-node ILD target without
    // re-calling std::pow on every sample.
    float ildGainBase_ = 1.0f;
    float blkDistRefScale_ = 0.047546796f;  // 10^(kDistGainFloorDb/40), recomputed per block
};

} // namespace xyzpan
