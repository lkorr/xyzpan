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
//
// NOTE: kHeadShadowFullOpenHz is set to 16000 Hz rather than 20000 Hz to avoid
// SVF instability at 44100 Hz sample rate. With cutoff = 20000 Hz, the SVF's g
// coefficient = tan(pi * 20000/44100) ≈ 6.3, which approaches the Nyquist limit
// of 0.45 * sampleRate. The TPT SVF requires g to stay well below the Nyquist
// singularity for stable operation under per-sample coefficient changes. 16000 Hz
// gives g ≈ 2.25 — safe, stable, and still inaudible to the human ear (most adults
// cannot hear above 16-18 kHz).
constexpr float kHeadShadowFullOpenHz  = 16000.0f;  // LPF wide open (inaudible, safe SVF range)
constexpr float kHeadShadowMinHz       = 1200.0f;   // LPF at full azimuth (~12dB cut by 4kHz)

// ILD (Interaural Level Difference) — distance-dependent far-ear attenuation
// At close range + full azimuth: maximum attenuation (hard panning feel).
// At max distance: negligible attenuation (both ears roughly equal).
constexpr float kDefaultILDMaxDb       = 8.0f;      // max ILD attenuation in dB (far ear, near ear = unity)

// Rear shadow SVF — both ears, only active when Y < 0 (source behind listener)
// Provides a subtle front/back cue before Phase 3 comb filters are added.
// Full-open value matches kHeadShadowFullOpenHz to avoid Nyquist instability.
constexpr float kRearShadowFullOpenHz  = 16000.0f;  // no rear shadow (safe SVF range)
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

// ============================================================================
// Phase 3: Depth (comb filter bank)
// ============================================================================

// Maximum number of comb filters in the series bank.
constexpr int kMaxCombFilters = 10;

// Per-filter delay times in milliseconds at 44.1 kHz (range: 0–1.5 ms).
// These primes/near-primes avoid alignment that would produce a repetitive
// comb pattern — the irregular spacing gives a richer spectral coloration.
constexpr float kCombDefaultDelays_ms[10] = {
    0.21f, 0.37f, 0.54f, 0.68f, 0.83f,
    0.97f, 1.08f, 1.23f, 1.38f, 1.50f
};

// Per-filter feedback gain — each slightly different for varied decay character.
// All values are within the stable range [-0.95, 0.95] (FeedbackCombFilter will
// hard-clamp these even if they drift due to floating-point rounding).
constexpr float kCombDefaultFeedback[10] = {
    0.15f, 0.14f, 0.16f, 0.13f, 0.15f,
    0.14f, 0.16f, 0.13f, 0.15f, 0.14f
};

// Maximum dry/wet blend for the comb bank output (30% wet at Y=-1).
constexpr float kCombMaxWet = 0.30f;

// Maximum comb filter delay time in milliseconds (caps the delay range).
constexpr float kCombMaxDelay_ms = 1.50f;

// ============================================================================
// Phase 3: Elevation (pinna, chest bounce, floor bounce)
// ============================================================================

// Pinna notch / peak filter (PeakingEQ biquad at 8 kHz)
constexpr float kPinnaNotchFreqHz = 8000.0f;   // center frequency
constexpr float kPinnaNotchQ      = 2.0f;       // bandwidth (~0.5 octave)

// Pinna high shelf (HighShelf biquad at 4 kHz)
constexpr float kPinnaShelfFreqHz = 4000.0f;   // shelf knee frequency

// Chest bounce delay: 0 ms at Z=1 (above), 2 ms at Z=-1 (below) — max delay
constexpr float kChestDelayMaxMs  = 2.0f;

// Chest bounce attenuation at Z=-1 (maximum bounce contribution)
constexpr float kChestGainDb      = -8.0f;

// Floor bounce delay: 0 ms at Z=1 (above), 20 ms at Z=-1 (below) — max delay
constexpr float kFloorDelayMaxMs  = 20.0f;

// Floor bounce attenuation at Z=-1 (maximum bounce contribution)
constexpr float kFloorGainDb      = -5.0f;

// ============================================================================
// Phase 4: Distance Processing (DIST-01 through DIST-06)
// ============================================================================

// Maximum propagation delay for distance effect (DIST-03).
// At the unit-cube corner (sqrt(3) away), the source has a 300ms delay offset.
constexpr float kDistDelayMaxMs = 300.0f;

// Delay smoother time constant — controls doppler feel during movement.
// Longer = smoother pitch glide, shorter = tighter tracking.
constexpr float kDistSmoothMs = 30.0f;

// Air absorption LPF cutoff range (DIST-02).
// At minimum distance: LPF fully open (no absorption).
// At maximum distance: LPF at minimum cutoff (strong HF rolloff).
constexpr float kAirAbsMaxHz = 22000.0f;  // LPF cutoff at min distance (no absorption)
constexpr float kAirAbsMinHz = 8000.0f;   // LPF cutoff at max distance (full absorption)

// ============================================================================
// Phase 5: Reverb (VERB-01 through VERB-04)
// ============================================================================
constexpr float kVerbDefaultSize      = 0.5f;
constexpr float kVerbDefaultDecay     = 0.5f;
constexpr float kVerbDefaultDamping   = 0.5f;
constexpr float kVerbDefaultWet       = 0.0f;   // reverb off by default
constexpr float kVerbPreDelayMaxMs    = 50.0f;
constexpr float kVerbMaxDecayT60_s    = 5.0f;   // maximum T60 at decay=1.0
// FDN delay lengths in ms at 44100 Hz — mutually prime, scaled in prepare()
constexpr float kFDNDelayMs[4]        = { 30.98f, 42.40f, 51.95f, 63.45f };

} // namespace xyzpan
