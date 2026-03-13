#pragma once

namespace xyzpan {

// Minimum distance from the origin to prevent division-by-zero and undefined
// azimuth/elevation when the source is placed at or near the listener position.
constexpr float kMinDistance = 0.1f;

// Hard clamp limit for all XYZ inputs. Values outside this range are clamped
// before processing. LFO overshoot beyond ±1 is the caller's responsibility.
constexpr float kMaxInputXYZ = 1.0f;

// ============================================================================
// Binaural panning constants (Phase 2)
// ============================================================================

// ITD (Interaural Time Difference) — Woodworth spherical head model
// Maximum ITD at 90-degree azimuth (empirical: ~0.66–0.72ms, head size dependent).
constexpr float kDefaultMaxITD_ms      = 0.72f;   // default max ITD in milliseconds
constexpr float kMaxITDUpperBound_ms   = 5.0f;    // dev panel upper limit (allows creative exaggeration)

// Head shadow SVF cutoff range
// At X=0 (center): LPF fully open — inaudible.
// At X=±1 (hard pan): far-ear LPF at minimum cutoff — audibly darkened.
constexpr float kHeadShadowFullOpenHz  = 20000.0f;  // LPF wide open (inaudible)
constexpr float kHeadShadowMinHz       = 1200.0f;   // LPF at full azimuth (~12dB cut by 4kHz)

// ILD (Interaural Level Difference) — distance-dependent far-ear attenuation
// At close range + full azimuth: maximum attenuation (hard panning feel).
// At max distance: negligible attenuation (both ears roughly equal).
constexpr float kDefaultILDMaxDb       = 8.0f;      // max ILD attenuation in dB (far ear, near ear = unity)

// Rear shadow SVF — both ears, only active when Y < 0 (source behind listener)
// Provides a subtle front/back cue before Phase 3 comb filters are added.
constexpr float kRearShadowFullOpenHz  = 20000.0f;  // no rear shadow
constexpr float kRearShadowMinHz       = 4000.0f;   // subtle HF rolloff at Y=-1

// Parameter smoothing time constants (RC time constant, ~63% rise time)
// ITD delay uses the slowest smoothing to avoid Doppler pitch glitches.
// Filter cutoff and gain can be faster — less perceptible pitch artifact.
constexpr float kDefaultSmoothMs_ITD    = 8.0f;   // ITD delay smoother (slower = less pitch glitch)
constexpr float kDefaultSmoothMs_Filter = 5.0f;   // SVF cutoff smoother
constexpr float kDefaultSmoothMs_Gain   = 5.0f;   // ILD gain smoother

// Geometry constants
// sqrt(3) is the maximum Euclidean distance when XYZ are all at ±1 (corner of the unit cube).
constexpr float kSqrt3 = 1.7320508f;

} // namespace xyzpan
