#pragma once

namespace xyzpan {

// Minimum distance from the origin to prevent division-by-zero and undefined
// azimuth/elevation when the source is placed at or near the listener position.
constexpr float kMinDistance = 0.1f;

// Base parameter range for XYZ inputs. UI knobs and mouse drag clamp to
// [-1, 1], but LFO modulation can push effective coordinates to [-2, 2].
// This constant defines the base range only — it is NOT a hard DSP clamp.
constexpr float kMaxInputXYZ = 1.0f;

// ============================================================================
// Binaural panning constants (Phase 2)
// ============================================================================

// ITD (Interaural Time Difference) — Geometric ear-distance model
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

// ILD (Interaural Level Difference) — ipsilateral (near) ear boost
// At close range + full azimuth: maximum boost on near ear (hard panning feel).
// At max distance: negligible boost (both ears roughly equal).
constexpr float kDefaultILDMaxDb       = 8.0f;      // max ILD boost in dB (near ear, far ear = unity)

// ILD crossfade width: smooth linear crossfade over this ITD range (in samples)
// to avoid gain discontinuity when ITD crosses zero (median plane crossing).
// 1.0 sample ≈ 0.023ms at 44.1kHz — matches exactly at |itdSamples| >= 1.
constexpr float kILDCrossfadeWidth     = 1.0f;

// Near-field ILD scaling: boost ildMaxDb at close range for stronger panning
constexpr float kNearFieldILDThreshold = 0.3f;       // distance fraction below which scaling kicks in
constexpr float kNearFieldILDMaxDb     = 15.0f;      // max ILD at very close range

// Rear shadow SVF — both ears, only active when Y < 0 (source behind listener)
// Provides a subtle front/back cue before Phase 3 comb filters are added.
// Full-open at 22kHz means no filtering when source is in front; SVF clamps near Nyquist internally.
constexpr float kRearShadowFullOpenHz  = 22000.0f;  // no rear shadow (SVF clamps internally near Nyquist)
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

// Sphere of Influence default radius — matches kSqrt3 so default behavior is unchanged.
constexpr float kSphereRadiusDefault = kSqrt3;

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

// Pinna N1 notch: elevation-dependent frequency shift
// Below horizon (Z=-1): 6.5 kHz; ear level (Z=0): 8 kHz; above (Z=1): 10 kHz
constexpr float kPinnaN1MinHz     = 6500.0f;   // N1 frequency at Z=-1 (below)
constexpr float kPinnaN1MidHz     = 8000.0f;   // N1 frequency at Z=0 (ear level) — legacy alias
constexpr float kPinnaN1MaxHz     = 10000.0f;  // N1 frequency at Z=1 (above)
constexpr float kPinnaNotchFreqHz = kPinnaN1MidHz; // default / fallback
constexpr float kPinnaNotchQ      = 2.0f;       // bandwidth (~0.5 octave)

// Pinna N2 secondary notch: ~N1 + 3 kHz, elevation-dependent
constexpr float kPinnaN2OffsetHz  = 3000.0f;   // N2 frequency = N1 + this offset
constexpr float kPinnaN2GainDb    = -8.0f;      // notch depth
constexpr float kPinnaN2Q         = 2.0f;       // bandwidth

// Pinna P1 peak: +4 dB at 5 kHz, fixed (not elevation-dependent)
constexpr float kPinnaP1FreqHz    = 5000.0f;
constexpr float kPinnaP1GainDb    = 4.0f;
constexpr float kPinnaP1Q         = 1.5f;

// Front-boosting presence shelf: high shelf at 3 kHz, Y-mapped gain
constexpr float kPresenceShelfFreqHz  = 3000.0f;
constexpr float kPresenceShelfMaxDb   = 4.0f;   // +4 dB at Y=1 (front), -4 dB at Y=-1 (back)

// Ear canal resonance peak: ~2.7 kHz, Y-mapped gain (quarter-wave resonance, highly universal)
constexpr float kEarCanalFreqHz = 2700.0f;  // ear canal quarter-wave resonance center
constexpr float kEarCanalQ      = 2.0f;     // moderate width (~2–4 kHz coverage)
constexpr float kEarCanalMaxDb  = 4.0f;     // max rear attenuation at Y=-1 (0 dB front, -maxDb rear)

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

// Floor bounce HF absorption: LPF cutoff on reflected signal (floors absorb HF)
constexpr float kFloorAbsHz       = 5000.0f;

// Vertical mono cylinder: collapses lateral panning near the Z-axis.
// Radius is in normalized [0,1] units matching the XYZ coordinate space.
constexpr float kVertMonoCylinderRadius    = 0.2f;
constexpr float kVertMonoCylinderRadiusMin = 0.0f;
constexpr float kVertMonoCylinderRadiusMax = 1.0f;

// ============================================================================
// Phase 4: Distance Processing (DIST-01 through DIST-06)
// ============================================================================

// Maximum per-sample delay change for doppler (rate limiter).
// 0.5 = max ~1.5x playback speed, prevents clicks on fast pass-by.
constexpr float kMaxDopplerSlewPerSample = 0.5f;

// Doppler anti-alias LP cutoff: band-limits signal before distance delay line
// to prevent Catmull-Rom interpolation aliasing during delay modulation.
// 18kHz is above audible range; combined with Hermite ~20dB stopband rejection,
// total fold-back attenuation is 24+ dB.
constexpr float kDopplerAAMaxHz = 18000.0f;

// Maximum propagation delay for distance effect (DIST-03).
// At the unit-cube corner (sqrt(3) away), the source has a 300ms delay offset.
constexpr float kDistDelayMaxMs = 300.0f;

// Distance gain: true inverse-square law (DIST-01).
// distRef = sphereRadius * 10^(kDistGainFloorDb/40)  → gain = -72dB at sphere boundary.
// distGainTarget = clamp((distRef / dist)², 0, kDistGainMax)
// kDistGainMax = 2.0 allows up to +6dB boost for very close sources.
constexpr float kDistGainFloorDb = -72.0f;  // gain in dB at sphere boundary
constexpr float kDistGainMax = 2.0f;        // max +6dB boost for very close sources

// Delay smoother time constant — controls doppler feel during movement.
// Longer = smoother pitch glide, shorter = tighter tracking.
// 150ms gives ~150ms ramp time for delay changes — audible doppler without glitching.
constexpr float kDistSmoothMs = 150.0f;

// Air absorption LPF cutoff range (DIST-02).
// At minimum distance: LPF fully open (no absorption).
// At maximum distance: LPF at minimum cutoff (strong HF rolloff).
constexpr float kAirAbsMaxHz = 22000.0f;  // LPF cutoff at min distance (no absorption)
constexpr float kAirAbsMinHz = 8000.0f;   // LPF cutoff at max distance (full absorption)

// Air absorption stage 2 — cascaded with stage 1 for 12dB/oct effective rolloff at distance.
// Stage 2 sweeps from 22kHz (close, flat) to 12kHz (far, extra HF loss).
// The cascade of stage1 (22→8k) + stage2 (22→12k) gives ~12dB/oct at far range.
constexpr float kAirAbs2MaxHz = 22000.0f;
constexpr float kAirAbs2MinHz = 12000.0f;

// Near-field ILD: LF boost on ipsilateral ear at close range
constexpr float kNearFieldLFHz    = 200.0f;  // low-shelf frequency
constexpr float kNearFieldLFMaxDb = 6.0f;    // max boost at proximity=1, full azimuth

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

// Aux reverb send: default max distance-gain boost (dB)
constexpr float kAuxSendGainMaxDb     = 6.0f;

// ============================================================================
// Phase 5: LFO (LFO-01 through LFO-05)
// ============================================================================
constexpr float kLFODefaultRate   = 0.5f;   // Hz, free-running default
constexpr float kLFOMinRate       = 0.0f;   // Hz minimum (0 = stopped)
constexpr float kLFOMaxRate       = 10.0f;  // Hz maximum
constexpr float kLFOMaxDepth      = 1.0f;   // max depth = full ±1 axis swing
constexpr float kLFOSpeedMulMin     = 0.0f;
constexpr float kLFOSpeedMulMax     = 3.0f;
constexpr float kLFOSpeedMulDefault = 1.0f;

// Beat division discrete values for tempo-synced LFOs
// Index 6 ("1") = quarter note = default
constexpr int   kBeatDivCount        = 11;
constexpr int   kBeatDivDefaultIndex = 6;
constexpr float kBeatDivValues[kBeatDivCount] = {
    0.0625f, 0.125f, 0.25f, 0.3333f, 0.5f,
    0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f
};
constexpr const char* kBeatDivLabels[kBeatDivCount] = {
    "1/16", "1/8", "1/4", "1/3", "1/2",
    "3/4", "1", "1.5", "2", "3", "4"
};

// ============================================================================
// ============================================================================
// Spatial DSP Improvements (P2–P6)
// ============================================================================

// P3: 1 kHz Rear Cue — bell filter boosted when source is behind listener
constexpr float kRearCue1kHz_FreqHz = 1000.0f;
constexpr float kRearCue1kHz_Q      = 1.5f;
constexpr float kRearCue1kHz_MaxDb  = 2.0f;

// P2: Front-Back Notch — PeakingEQ attenuated when source is behind listener
constexpr float kFrontBackNotchFreqHz = 6500.0f;
constexpr float kFrontBackNotchQ      = 2.0f;
constexpr float kFrontBackNotchMaxDb  = -6.0f;

// P5: Expanded Pinna EQ — 4 additional bands
// Concha notch: 4kHz, Q=3.0, Z-mapped: 0dB (above) → −8dB (below)
constexpr float kConchaNotchFreqHz = 4000.0f;
constexpr float kConchaNotchQ      = 3.0f;
constexpr float kConchaNotchMaxDb  = -8.0f;

// Upper pinna peak: 12kHz, Q=2.0, Z-mapped: −4dB (below) → +3dB (above)
constexpr float kUpperPinnaFreqHz  = 12000.0f;
constexpr float kUpperPinnaQ       = 2.0f;
constexpr float kUpperPinnaMinDb   = -4.0f;   // at Z=-1 (below)
constexpr float kUpperPinnaMaxDb   = 3.0f;    // at Z=+1 (above)

// Shoulder reflection: 1.5kHz, Q=1.0, Z-mapped: 0dB (above) → +2dB (below horizon)
constexpr float kShoulderPeakFreqHz = 1500.0f;
constexpr float kShoulderPeakQ      = 1.0f;
constexpr float kShoulderPeakMaxDb  = 2.0f;

// Tragus notch: 8.5kHz, Q=3.5, Y+Z mapped: −5dB max depth
constexpr float kTragusNotchFreqHz  = 8500.0f;
constexpr float kTragusNotchQ       = 3.5f;
constexpr float kTragusNotchMaxDb   = -5.0f;

// ============================================================================
// Stereo Source Node Splitting
// ============================================================================
constexpr float kStereoMaxSpreadRadius = 1.0f;   // max half-separation in world units
constexpr float kStereoOrbitDefaultRate = 0.5f;   // Hz

// ============================================================================
// Dev tool: Test tone oscillator
// ============================================================================
constexpr float kTestTonePitchMinHz   = 20.0f;
constexpr float kTestTonePitchMaxHz   = 2000.0f;
constexpr float kTestTonePitchDefault = 100.0f;
constexpr float kTestTonePulseMinHz   = 0.1f;
constexpr float kTestTonePulseMaxHz   = 10.0f;
constexpr float kTestTonePulseDefault = 2.0f;

} // namespace xyzpan
