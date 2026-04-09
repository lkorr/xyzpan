#pragma once
#include <algorithm>
#include <cmath>

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

// Virtual ear offsets for distance-difference spatial cue computation.
// Each axis uses a pair of virtual ear nodes offset along that axis;
// the cue factor is derived from (distFar − distNear) / (2 * offset).
constexpr float kAzimuthEarOffset = 0.087f;  // L/R virtual ears at (±h, 0, 0)
constexpr float kRearEarOffset    = 0.087f;  // F/B virtual ears at (0, ±h, 0)

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
constexpr float kILDCrossfadeWidth     = 4.0f;

// Hardpan mode: opposite-ear attenuation when binaural (ITD) is disabled.
// At full azimuth, the opposite ear is cut by this amount (in dB).
constexpr float kHardpanMaxDb          = -4.0f;

// Rear shadow SVF — both ears, only active when Y < 0 (source behind listener)
// Provides a subtle front/back cue before Phase 3 comb filters are added.
// Full-open at 22kHz means no filtering when source is in front; SVF clamps near Nyquist internally.
constexpr float kRearShadowFullOpenHz  = 22000.0f;  // no rear shadow (SVF clamps internally near Nyquist)
constexpr float kRearShadowMinHz       = 20000.0f;  // effectively disabled (at range cap)

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

// Maximum dry/wet blend for the comb bank output (15% wet at Y=-1).
constexpr float kCombMaxWet = 0.15f;

// Maximum comb filter delay time in milliseconds (caps the delay range).
constexpr float kCombMaxDelay_ms = 1.50f;

// ============================================================================
// Phase 3: Elevation (pinna, chest bounce, floor bounce)
// ============================================================================

// Virtual ear vertical offset for distance-difference elevation computation.
// Top/bottom nodes at (0,0,±h) in listener-relative space; elevation factor
// derived from (distBottom − distTop) / (2h). Matches the distance-difference
// pattern used for L/R (azimuth) and F/B (rear) cues.
constexpr float kElevEarOffset = 0.087f;   // normalized units (~head radius)

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

// Pinna P1 peak: +2.8 dB at 5 kHz, fixed (not elevation-dependent)
constexpr float kPinnaP1FreqHz    = 5000.0f;
constexpr float kPinnaP1GainDb    = 2.8f;
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

// Chest bounce filter cutoffs — shape the reflected signal
constexpr float kChestHPFHz       = 700.0f;    // chest bounce highpass cutoff (4x HP cascade)
constexpr float kChestLPHz        = 1000.0f;   // chest bounce lowpass cutoff (1x 6dB/oct LP)

// Floor bounce delay: 0 ms at Z=1 (above), 20 ms at Z=-1 (below) — max delay
constexpr float kFloorDelayMaxMs  = 20.0f;

// Floor bounce attenuation at Z=-1 (maximum bounce contribution)
constexpr float kFloorGainDb      = -5.0f;

// Floor bounce HF absorption: LPF cutoff on reflected signal (floors absorb HF)
constexpr float kFloorAbsHz       = 5000.0f;

// DEPRECATED — cylinder blend replaced by distance-difference azimuth model.
// Kept for preset backward compatibility (parameter exists but DSP ignores it).
constexpr float kVertMonoCylinderRadius    = 0.2f;
constexpr float kVertMonoCylinderRadiusMin = 0.0f;
constexpr float kVertMonoCylinderRadiusMax = 1.0f;

// ============================================================================
// Phase 4: Distance Processing (DIST-01 through DIST-06)
// ============================================================================

// Doppler anti-alias LP cutoff: band-limits signal before distance delay line
// to prevent Catmull-Rom interpolation aliasing during delay modulation.
// 18kHz is above audible range; combined with Hermite ~20dB stopband rejection,
// total fold-back attenuation is 24+ dB.
constexpr float kDopplerAAMaxHz = 18000.0f;

// Maximum propagation delay for distance effect (DIST-03).
// At the unit-cube corner (sqrt(3) away), the source has a 500ms delay offset.
constexpr float kDistDelayMaxMs = 500.0f;

// Distance gain (DIST-01).
// Uses a cubic Hermite spline in the dB domain, parameterised by a steepness
// control (0 = matches old inverse-square character, 1 = flat near listener
// with a cliff at the sphere boundary).
constexpr float kDistGainFloorDb = -72.0f;  // gain in dB at sphere boundary
constexpr float kDistGainMax = 2.0f;        // max +6dB boost for very close sources
constexpr float kDistCurveSteepDefault = 0.0f;  // 0 = legacy inverse-square match, 1 = max compression

// Hermite slope coefficients as multiples of the dB range.
// At steepness=0 these reproduce the old (distRef/dist)² curve shape:
//   S0 = -3.63 * range  (steep near listener, drops fast)
//   S1 = -0.21 * range  (gentle near boundary, already quiet)
// At steepness=1 (max compression):
//   S0 = 0              (flat near listener)
//   S1 = -3.0 * range   (cliff at boundary, monotonicity limit)
constexpr float kDistCurveS0Factor_Legacy = -3.63f;
constexpr float kDistCurveS1Factor_Legacy = -0.21f;
constexpr float kDistCurveS0Factor_Max    =  0.0f;
constexpr float kDistCurveS1Factor_Max    = -3.0f;

// Compressed distance gain helper.
// nodeDistFrac: 0 = listener, 1 = sphere boundary.
// steepness:    0 = legacy inverse-square match, 1 = flat center / cliff at edge.
// distGainMaxDb: 20*log10(distGainMax), pre-computed per block.
// floorDb:      gain in dB at sphere boundary (negative).
// distGainMax:  linear max gain clamp.
inline float compressedDistGain(float nodeDistFrac, float distGainMaxDb,
                                float floorDb, float steepness, float distGainMax) {
    const float range = distGainMaxDb - floorDb;          // total dB span (positive)
    const float S0 = range * (kDistCurveS0Factor_Legacy
        + (kDistCurveS0Factor_Max - kDistCurveS0Factor_Legacy) * steepness);
    const float S1 = range * (kDistCurveS1Factor_Legacy
        + (kDistCurveS1Factor_Max - kDistCurveS1Factor_Legacy) * steepness);
    const float A3 = 2.0f * range + S0 + S1;
    const float A2 = -3.0f * range - 2.0f * S0 - S1;
    const float A1 = S0;
    const float A0 = distGainMaxDb;
    const float f  = nodeDistFrac;
    const float gainDb = ((A3 * f + A2) * f + A1) * f + A0;
    return std::clamp(std::pow(10.0f, gainDb * 0.05f), 0.0f, distGainMax);
}

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
// Phase 5: Reverb — Dattorro Plate (VERB-01 through VERB-04)
// ============================================================================
constexpr float kVerbDefaultSize      = 0.5f;
constexpr float kVerbDefaultDecay     = 0.5f;
constexpr float kVerbDefaultDamping   = 0.5f;
constexpr float kVerbDefaultWet       = 0.0f;   // reverb off by default
constexpr float kVerbPreDelayMaxMs    = 50.0f;
constexpr float kVerbMaxDecayT60_s    = 5.0f;   // maximum T60 at decay=1.0
constexpr float kVerbDefaultModDepth  = 0.5f;
constexpr float kVerbDefaultDiffusion = 0.7f;

// Dattorro plate reverb reference sample rate and delay lengths (in samples).
// All delay lengths are specified at 29761 Hz and scaled to actual sample rate.
constexpr double kDattorroRefRate = 29761.0;

// Input diffusion allpass delays (series chain, 4 stages)
constexpr int kDatInputAP[4] = { 142, 107, 379, 277 };
// Input diffusion coefficients (stages 1-2 use coeff1, stages 3-4 use coeff2)
constexpr float kDatInputDiffCoeff1 = 0.75f;
constexpr float kDatInputDiffCoeff2 = 0.625f;

// Tank modulated allpass delays
constexpr int kDatTankAP[2] = { 672, 908 };
// Tank allpass (decay diffusion) coefficient
constexpr float kDatDecayDiffCoeff = 0.7f;

// Tank delay line lengths
constexpr int kDatTankDelay[2] = { 4453, 4217 };

// Output tap positions (in samples at reference rate)
// L: +tap(tankDelay0, 266) +tap(tankDelay0, 2974) -tap(tankAP1, 1913)
// R: +tap(tankDelay1, 266) +tap(tankDelay1, 2974) -tap(tankAP0, 1913)
constexpr int kDatTapA = 266;    // early tap from tank delays
constexpr int kDatTapB = 2974;   // late tap from tank delays
constexpr int kDatTapC = 1913;   // cross-channel tap from tank allpasses

// Modulation: 2 LFOs for tank allpasses
constexpr float kDatModRate1    = 1.0f;    // Hz
constexpr float kDatModRate2    = 0.87f;   // Hz (slightly detuned)
constexpr float kDatModExcursion = 16.0f;  // max excursion in samples at ref rate

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
constexpr float kLFOSpeedMulMax     = 5.0f;
constexpr float kLFOSpeedMulDefault = 1.0f;
constexpr float kLFOSpeedMulSkew    = 0.431f; // exponential: slider midpoint = 1.0

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

// P5: Expanded Pinna EQ — 4 additional bands
// Concha notch: 4kHz, Q=3.0, elevation-mapped: 0dB (above) → −8dB (below)
constexpr float kConchaNotchFreqHz = 4000.0f;
constexpr float kConchaNotchQ      = 3.0f;
constexpr float kConchaNotchMaxDb  = -8.0f;

// Upper pinna peak: 12kHz, Q=2.0, elevation-mapped: −4dB (below) → +3dB (above)
constexpr float kUpperPinnaFreqHz  = 12000.0f;
constexpr float kUpperPinnaQ       = 2.0f;
constexpr float kUpperPinnaMinDb   = -4.0f;   // at Z=-1 (below)
constexpr float kUpperPinnaMaxDb   = 3.0f;    // at Z=+1 (above)

// Shoulder reflection: 1.5kHz, Q=1.0, elevation-mapped: 0dB (above) → +2dB (below horizon)
constexpr float kShoulderPeakFreqHz = 1500.0f;
constexpr float kShoulderPeakQ      = 1.0f;
constexpr float kShoulderPeakMaxDb  = 2.0f;

// Tragus notch: 8.5kHz, Q=3.5, Y+Z mapped: −5dB max depth
constexpr float kTragusNotchFreqHz  = 8500.0f;
constexpr float kTragusNotchQ       = 3.5f;
constexpr float kTragusNotchMaxDb   = -5.0f;

// ============================================================================
// Early Reflections (Image Source Method)
// ============================================================================
constexpr int   kNumER                = 6;       // 6 walls: ±X, ±Y, ±Z
constexpr float kERRoomSizeMin        = 1.0f;    // meters, half-dimension
constexpr float kERRoomSizeMax        = 30.0f;
constexpr float kERRoomSizeDefault    = 5.0f;
constexpr float kERDampingDefault     = 0.5f;    // maps to 500–16000 Hz wall LPF cutoff
constexpr float kERDampingLPMinHz     = 500.0f;  // fully damped wall cutoff
constexpr float kERDampingLPMaxHz     = 16000.0f;// undamped wall cutoff
constexpr float kERLevelDefault       = 0.5f;
constexpr float kERReverbSendDefault  = 0.7f;
constexpr float kERMaxDelayMs         = 350.0f;  // margin over worst case (~213ms at 192kHz)
constexpr float kSpeedOfSound         = 343.0f;  // m/s

// ============================================================================
// Stereo Source Node Splitting
// ============================================================================
constexpr float kStereoMaxSpreadRadius = 1.0f;   // max half-separation in world units
constexpr float kStereoOrbitDefaultRate = 0.5f;   // Hz
constexpr float kStereoNodeGain        = 0.70710678f; // -3dB per node (1/sqrt(2))

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
