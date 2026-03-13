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
};

// Result of XYZ-to-spherical coordinate conversion.
// Y-forward convention: azimuth=0 means directly in front (Y=1).
struct SphericalCoord {
    float azimuth;    // radians: 0 = front, +PI/2 = right (X=1), clockwise positive
    float elevation;  // radians: +PI/2 = directly above, -PI/2 = directly below
    float distance;   // normalized Euclidean distance, clamped to [kMinDistance, sqrt(3)]
};

} // namespace xyzpan
