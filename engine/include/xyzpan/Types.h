#pragma once
#include "xyzpan/Constants.h"

namespace xyzpan {

// Parameters passed to the engine each processBlock.
// Grows per phase as DSP features are added.
struct EngineParams {
    // Spatial position (Phase 1)
    float x = 0.0f;  // [-1, 1]: X=1 = right, X=-1 = left
    float y = 1.0f;  // [-1, 1]: Y=1 = front, Y=-1 = behind
    float z = 0.0f;  // [-1, 1]: Z=1 = above, Z=-1 = below

    // Dev panel parameters (Phase 2 — binaural panning)
    // All have defaults from Constants.h matching physical/perceptual baselines.
    // Exposed via APVTS in the plugin layer for real-time tuning.
    float maxITD_ms         = kDefaultMaxITD_ms;      // ITD maximum in ms
    float headShadowMinHz   = kHeadShadowMinHz;        // head shadow LPF minimum cutoff
    float ildMaxDb          = kDefaultILDMaxDb;        // maximum ILD attenuation in dB
    float rearShadowMinHz   = kRearShadowMinHz;        // rear shadow LPF minimum cutoff
    float smoothMs_ITD      = kDefaultSmoothMs_ITD;    // ITD smoother time constant
    float smoothMs_Filter   = kDefaultSmoothMs_Filter; // SVF cutoff smoother time constant
    float smoothMs_Gain     = kDefaultSmoothMs_Gain;   // ILD gain smoother time constant

    // =========================================================================
    // Phase 3: Depth (DEPTH-01, DEPTH-02, DEPTH-04, DEPTH-05)
    // =========================================================================
    // Per-filter delay times (ms) and feedback gains for the series comb bank.
    // Defaults mirror kCombDefaultDelays_ms / kCombDefaultFeedback in Constants.h.
    // Arrays use in-place initializers because constexpr arrays cannot be used
    // as default member initializers in C++17/20 without extra boilerplate.
    float combDelays_ms[kMaxCombFilters] = {
        0.21f, 0.37f, 0.54f, 0.68f, 0.83f,
        0.97f, 1.08f, 1.23f, 1.38f, 1.50f
    };
    float combFeedback[kMaxCombFilters] = {
        0.15f, 0.14f, 0.16f, 0.13f, 0.15f,
        0.14f, 0.16f, 0.13f, 0.15f, 0.14f
    };
    float combWetMax = kCombMaxWet;  // 0.30f — max wet blend at Y=-1

    // =========================================================================
    // Phase 3: Elevation (ELEV-01, ELEV-02, ELEV-03, ELEV-04, ELEV-05)
    // =========================================================================
    float pinnaNotchFreqHz = kPinnaNotchFreqHz;  // 8000 Hz — pinna notch center
    float pinnaNotchQ      = kPinnaNotchQ;        // 2.0   — ~0.5 octave bandwidth
    float pinnaShelfFreqHz = kPinnaShelfFreqHz;   // 4000 Hz — high shelf knee
    float chestDelayMaxMs  = kChestDelayMaxMs;    // 2.0 ms — chest bounce max delay
    float chestGainDb      = kChestGainDb;        // -8 dB  — chest bounce attenuation
    float floorDelayMaxMs  = kFloorDelayMaxMs;    // 20.0 ms — floor bounce max delay
    float floorGainDb      = kFloorGainDb;        // -5 dB  — floor bounce attenuation
};

// Result of XYZ-to-spherical coordinate conversion.
// Y-forward convention: azimuth=0 means directly in front (Y=1).
struct SphericalCoord {
    float azimuth;    // radians: 0 = front, +PI/2 = right (X=1), clockwise positive
    float elevation;  // radians: +PI/2 = directly above, -PI/2 = directly below
    float distance;   // normalized Euclidean distance, clamped to [kMinDistance, sqrt(3)]
};

} // namespace xyzpan
