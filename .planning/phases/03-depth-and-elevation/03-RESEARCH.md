# Phase 3: Depth and Elevation - Research

**Researched:** 2026-03-12
**Domain:** Audio DSP — comb filter arrays, peaking EQ notch, high shelf, highpass/lowpass cascade, parallel delay bounces (pure C++)
**Confidence:** HIGH

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| DEPTH-01 | ~10 comb filters in series model front/back depth perception | Feedback comb filter IIR: y[n] = x[n] + g*y[n-M]; series array with varied M and g per filter |
| DEPTH-02 | Comb filter delays range 0ms to 1.5ms (arbitrary per filter) | At 44.1kHz: 0 to ~66 samples; at 96kHz: 0 to ~144 samples — pre-allocate per-filter ring buffers |
| DEPTH-03 | Comb filter dry/wet scales from 0% (Y=0) to 30% max (Y=-1) | Blend = wet * dryWet + dry * (1-dryWet); dryWet = 0.3f * max(0, -Y) |
| DEPTH-04 | Comb filter feedback hard-clamped to prevent instability | Clamp each g to [-0.95, 0.95] regardless of dev panel input; document why |
| DEPTH-05 | All comb filter parameters (count, delays, wet amounts) tuneable via dev panel | EngineParams grows: combDelays[10], combGains[10], combWetAmounts[10], combDryWetMax |
| ELEV-01 | Pinna notch: -15dB notch at 8kHz at elevation 0, to +5dB at elevation 1 with +3dB high shelf at 4kHz+ | Peaking EQ biquad (Audio EQ Cookbook) for variable-gain notch/peak; High shelf biquad from same source |
| ELEV-02 | Pinna filter stays at elevation-0 values between -1 and 0; high shelf removes 3dB from 0 to -1 | Two separate modulation paths: pinna notch active only Z>=0, shelf active for Z in [-1,1] |
| ELEV-03 | Chest bounce: parallel 4x HPF at 700Hz + 1x LP at 1kHz, delay 0ms at -1 to 2ms at 1, -8dB at -1 to -inf at 1 | SVFFilter HP mode (extend existing class) x4 cascade + SVF LP; FractionalDelayLine reuse; gain = pow(10, -8/20) * (1 - elevation_normalized) |
| ELEV-04 | Floor bounce: parallel delayed copy -5dB at -1 to -inf at 1, delay 0ms at -1 to 20ms at 1 | FractionalDelayLine (reuse, larger capacity for 20ms); linear gain fade-out with elevation |
| ELEV-05 | All elevation filter parameters tuneable via dev panel | EngineParams grows: pinnaNotchFreq, pinnaNotchGain, pinnaShelfFreq, pinnaShelfGain, chestDelayMs, chestGainDb, floorDelayMs, floorGainDb |
</phase_requirements>

---

## Summary

Phase 3 adds two independent DSP systems: a comb filter array for front/back depth perception (driven by Y position), and a multi-stage elevation model (driven by Z position). Neither system requires new external libraries — both extend the existing pure C++ engine using the same prepare/setParams/process/reset lifecycle established in Phases 1 and 2.

**Comb filter depth** works by running the signal through a series of feedback comb filters with short delays (0–1.5ms). In series, they create a complex spectral coloration from stacked comb responses. At Y=1 (front), the comb bank is bypassed (0% wet). At Y=-1 (back), the wet amount reaches its maximum (30% default). The blend is a straight linear dry/wet mix after the comb bank output. Feedback gain is hard-clamped to ±0.95 regardless of dev panel settings to prevent runaway resonance.

**Elevation** is modeled by three parallel mechanisms: a pinna notch/peak filter (primary elevation cue, active for Z≥0), chest bounce (parallel filtered+delayed copy, active for Z in all range but peaks at Z=-1), and floor bounce (simpler parallel delay, most pronounced at Z=-1). The pinna notch is a peaking EQ biquad whose gain sweeps from -15dB (Z=0, ear level) to +5dB (Z=1, above). A high shelf (4kHz+) adds +3dB boost at Z=1. For below-horizon (Z<0), the pinna filter freezes at its Z=0 values while the chest and floor bounce filters come alive.

**Primary recommendation:** Extend the SVFLowPass class to a general `SVFFilter` supporting LP, HP, BP, notch, and the mix-coefficient generalisation. Implement feedback comb filters as a new `FeedbackCombFilter` class. Implement peaking EQ as a new `BiquadFilter` class using Audio EQ Cookbook coefficients. Reuse `FractionalDelayLine` for chest and floor bounce delays.

---

## Standard Stack

### Core (all hand-rolled per project mandate — no new external libraries)

| Component | Implementation | Purpose | Why |
|-----------|---------------|---------|-----|
| `FeedbackCombFilter` | New class: ring buffer + feedback gain | Comb filter depth array | Direct feedback IIR, simplest possible |
| `SVFFilter` (extended) | Extend `SVFLowPass` with m0/m1/m2 mix | HP output for chest bounce HPF | SVF mandated for modulated filters; HP mode is trivial extension |
| `BiquadFilter` | New class: biquad Direct Form II | Peaking EQ (pinna notch), high shelf | TPT SVF doesn't support peaking EQ; biquad is the standard for EQ |
| `FractionalDelayLine` | Existing — reuse unchanged | Chest bounce delay (0–2ms) and floor bounce delay (0–20ms) | Already implemented; just allocate larger capacity |
| `OnePoleSmooth` | Existing — reuse unchanged | Smooth all Phase 3 parameter transitions | Already implemented |

### No New External Dependencies

All DSP is hand-rolled. No new CMake targets needed. The existing build provides:
- C++20 (MSVC) — `<cmath>` functions (`std::cos`, `std::sin`, `std::pow`)
- Catch2 3.7.1 — already wired in tests/CMakeLists.txt
- JUCE 8.0.12 — plugin layer only, not engine

---

## Architecture Patterns

### Recommended Project Structure (additions to Phase 2)

```
engine/
├── include/xyzpan/
│   ├── dsp/
│   │   ├── FractionalDelayLine.h    # Existing — reuse for bounces
│   │   ├── SVFLowPass.h             # Existing — keep for Phase 2 use
│   │   ├── SVFFilter.h              # NEW — generalised SVF (LP/HP/BP/notch via m0,m1,m2)
│   │   ├── FeedbackCombFilter.h     # NEW — y[n] = x[n] + g * y[n-M]
│   │   └── BiquadFilter.h           # NEW — peaking EQ, high shelf (Audio EQ Cookbook)
│   ├── Engine.h                     # Updated — gains CombBank, PinnaFilter, BounceDelays
│   ├── Types.h                      # Updated — EngineParams grows Phase 3 fields
│   └── Constants.h                  # Updated — Phase 3 default constants
├── src/
│   └── Engine.cpp                   # Updated — Phase 3 signal chain inserted
tests/
└── engine/
    ├── TestCoordinates.cpp           # Existing — untouched
    ├── TestBinauralPanning.cpp       # Existing — untouched
    └── TestDepthAndElevation.cpp     # NEW — covers DEPTH-01 through ELEV-05
plugin/
└── src/
    └── ParamIDs.h                    # Updated — Phase 3 APVTS parameter IDs
```

### Pattern 1: Feedback Comb Filter

**What:** A delay line with its output fed back to the input via a gain coefficient. Creates a series of exponentially decaying echoes at multiples of the delay frequency.

**Stability requirement:** `|g| < 1`. Hard-clamp to `[-0.95, 0.95]` in the class itself (not caller's responsibility). At g=0.95, decay is slow but stable; at g=0.99, it takes hundreds of milliseconds to decay; at g>=1.0, it diverges.

**Why feedback over feedforward for this use:** The requirement specifies comb filter depth with "feedback" semantics (resonant coloration), not a pure FIR feedforward comb. IIR feedback combs create notch/peak patterns with resonance that is perceptually richer for front/back timbral differentiation.

**Why series, not parallel:** The requirement says "in series." Series combs create a more complex cumulative spectral coloration than parallel sums.

```cpp
// Source: Julius O. Smith, Physical Audio Signal Processing
// ccrma.stanford.edu/~jos/pasp/Feedback_Comb_Filters.html
// engine/include/xyzpan/dsp/FeedbackCombFilter.h

#pragma once
#include <vector>
#include <algorithm>
#include <cmath>

namespace xyzpan::dsp {

class FeedbackCombFilter {
public:
    // capacitySamples: maximum delay in samples (round up for safety)
    void prepare(int capacitySamples) {
        // Power-of-2 for fast modulo via bitmask
        int n = 1;
        while (n < capacitySamples + 2) n <<= 1;
        mask_     = n - 1;
        buf_.assign(static_cast<size_t>(n), 0.0f);
        writePos_ = 0;
        delayInSamples_ = 0;
        feedback_       = 0.0f;
    }

    void reset() {
        std::fill(buf_.begin(), buf_.end(), 0.0f);
        writePos_ = 0;
    }

    // Set delay in samples (integer — comb filter does not require fractional)
    void setDelay(int delaySamples) {
        // Clamp to buffer capacity
        delayInSamples_ = std::max(1, std::min(delaySamples, static_cast<int>(buf_.size()) - 2));
    }

    // Set feedback gain — HARD CLAMPED to [-0.95, 0.95] for stability
    void setFeedback(float g) {
        feedback_ = std::clamp(g, -0.95f, 0.95f);
    }

    // Process one sample: y[n] = x[n] + g * y[n - M]
    float process(float x) {
        int readPos = (writePos_ - delayInSamples_) & mask_;
        float y = x + feedback_ * buf_[static_cast<size_t>(readPos)];
        buf_[static_cast<size_t>(writePos_ & mask_)] = y;
        ++writePos_;
        return y;
    }

private:
    std::vector<float> buf_;
    int   mask_             = 0;
    int   writePos_         = 0;
    int   delayInSamples_   = 0;
    float feedback_         = 0.0f;
};

} // namespace xyzpan::dsp
```

**Note on delay time to samples:** `delaySamples = static_cast<int>(delayMs * 0.001f * sampleRate)`. At 44.1kHz and 1.5ms: `delaySamples = 66`. At 96kHz and 1.5ms: `delaySamples = 144`. Allocate per-filter ring buffers for `ceil(1.5ms * maxSampleRate) + 2` = ~146 samples, round to next power of 2 = 256.

### Pattern 2: Generalised SVFFilter (extends SVFLowPass)

**What:** The Andy Simper TPT SVF produces LP, HP, BP, and notch outputs from the same state update, selected by mix coefficients:
- LP:    `m0=0, m1=0,  m2=1`    — output = v2
- HP:    `m0=1, m1=-k, m2=-1`   — output = v0 - k*v1 - v2 (or equivalently v0 - k*v1 - LP)
- BP:    `m0=0, m1=k,  m2=0`    — output = k*v1
- Notch: `m0=1, m1=-k, m2=0`   — output = v0 - k*v1

The existing `SVFLowPass` only returns v2. A new `SVFFilter` class adds the mix-coefficient output. The `SVFLowPass` stays unchanged for Phase 2 use; the `SVFFilter` is a clean parallel class.

```cpp
// Source: Cytomic SvfLinearTrapOptimised.pdf (Andy Simper, 2011)
//         gist.github.com/hollance/2891d89c57adc71d9560bcf0e1e55c4b
// engine/include/xyzpan/dsp/SVFFilter.h

#pragma once
#include <cmath>
#include <algorithm>

namespace xyzpan::dsp {

enum class SVFType { LP, HP, BP, Notch };

class SVFFilter {
public:
    void reset() { ic1eq_ = ic2eq_ = 0.0f; }

    void setType(SVFType type) { type_ = type; }

    // Q = 0.7071 = Butterworth (no resonance bump)
    // Safe to call per-sample for smooth modulation.
    void setCoefficients(float cutoffHz, float sampleRate, float Q = 0.7071f) {
        float safeHz = std::min(cutoffHz, 0.45f * sampleRate);
        float g = std::tan(3.14159265f * safeHz / sampleRate);
        k_  = 1.0f / Q;
        a1_ = 1.0f / (1.0f + g * (g + k_));
        a2_ = g * a1_;
        a3_ = g * a2_;
    }

    // Process one sample, return selected output
    float process(float v0) {
        float v3 = v0 - ic2eq_;
        float v1 = a1_ * ic1eq_ + a2_ * v3;
        float v2 = ic2eq_ + a2_ * ic1eq_ + a3_ * v3;
        ic1eq_ = 2.0f * v1 - ic1eq_;
        ic2eq_ = 2.0f * v2 - ic2eq_;

        switch (type_) {
            case SVFType::LP:    return v2;
            case SVFType::HP:    return v0 - k_ * v1 - v2;
            case SVFType::BP:    return k_ * v1;
            case SVFType::Notch: return v0 - k_ * v1;
            default:             return v2;
        }
    }

private:
    SVFType type_  = SVFType::LP;
    float k_       = 1.4142f;  // 1/Q = 1/0.7071
    float a1_      = 0.0f, a2_ = 0.0f, a3_ = 0.0f;
    float ic1eq_   = 0.0f, ic2eq_ = 0.0f;
};

} // namespace xyzpan::dsp
```

**For chest bounce 4x HPF:** Instantiate 4 `SVFFilter` objects set to HP mode. Run them in series (output of each feeds input of next). Use Q=0.7071 (Butterworth — no resonance bump on chest color). The 4th-order slope gives ~24dB/octave below 700Hz, cleaning out low-frequency muddiness from the bounce path.

**CRITICAL:** The "1x 6dB/oct lowpass at 1kHz" in ELEV-03 is a single-pole LP. A TPT SVF with Q=0.7071 is a second-order -12dB/oct filter. For a true 6dB/oct single-pole, use a one-pole LP: `y[n] = x[n]*b + y[n-1]*a` with `a = exp(-2pi*fc/sr)`. This is distinct from an SVF lowpass. Add a `OnePoleLP` class or reuse `OnePoleSmooth` (its `process()` is identical in structure).

### Pattern 3: BiquadFilter (Peaking EQ + High Shelf)

**What:** A Direct Form II biquad implementing the Audio EQ Cookbook peakingEQ and highShelf types. Used for pinna notch (peaking EQ, gain from -15dB to +5dB at 8kHz) and high shelf (boost/cut above 4kHz at extreme elevation).

**Why biquad, not SVF:** The SVF does not directly support peaking EQ with variable gain in a clean parameterization. The Audio EQ Cookbook peakingEQ biquad is the industry standard for this — it handles the gain parameter directly, with smooth gain modulation possible via re-computing coefficients per block (slow enough to not require per-sample update).

**Stability note:** For the pinna filter, cutoff is 8kHz and gain changes slowly (driven by Z position automation). Per-block coefficient updates are sufficient and safe. Direct Form II is slightly less numerically stable than transposed DF2 for double-precision, but for 32-bit float at these frequencies it is fine.

```cpp
// Source: webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html
//         Robert Bristow-Johnson, Audio EQ Cookbook
// engine/include/xyzpan/dsp/BiquadFilter.h

#pragma once
#include <cmath>
#include <algorithm>

namespace xyzpan::dsp {

enum class BiquadType { PeakingEQ, HighShelf, LowShelf };

class BiquadFilter {
public:
    void reset() { z1_ = z2_ = 0.0f; }

    // Set coefficients for peaking EQ or shelf filter.
    //   freqHz  — center/shelf frequency
    //   Q       — bandwidth control (0.707 = ~1 octave for peaking, shelving slope for shelf)
    //   gainDb  — boost/cut in dB (peaking EQ and shelves); 0 = bypass
    // Safe to call per block (not per sample — cos/sin/sqrt cost).
    void setCoefficients(BiquadType type, float freqHz, float sampleRate,
                         float Q, float gainDb) {
        // Audio EQ Cookbook intermediate variables
        float A  = std::pow(10.0f, gainDb / 40.0f);          // 10^(dBgain/40)
        float w0 = 2.0f * 3.14159265f * freqHz / sampleRate;
        float cs = std::cos(w0);
        float sn = std::sin(w0);
        float alpha = sn / (2.0f * Q);

        float b0, b1, b2, a0, a1, a2;

        switch (type) {
            case BiquadType::PeakingEQ:
                b0 = 1.0f + alpha * A;
                b1 = -2.0f * cs;
                b2 = 1.0f - alpha * A;
                a0 = 1.0f + alpha / A;
                a1 = -2.0f * cs;
                a2 = 1.0f - alpha / A;
                break;
            case BiquadType::HighShelf: {
                float sqrtA = std::sqrt(A);
                float twoSqrtA_alpha = 2.0f * sqrtA * alpha;
                b0 =      A * ((A+1.0f) + (A-1.0f)*cs + twoSqrtA_alpha);
                b1 = -2.0f*A * ((A-1.0f) + (A+1.0f)*cs);
                b2 =      A * ((A+1.0f) + (A-1.0f)*cs - twoSqrtA_alpha);
                a0 =           (A+1.0f) - (A-1.0f)*cs + twoSqrtA_alpha;
                a1 =  2.0f *  ((A-1.0f) - (A+1.0f)*cs);
                a2 =           (A+1.0f) - (A-1.0f)*cs - twoSqrtA_alpha;
                break;
            }
            case BiquadType::LowShelf: {
                float sqrtA = std::sqrt(A);
                float twoSqrtA_alpha = 2.0f * sqrtA * alpha;
                b0 =      A * ((A+1.0f) - (A-1.0f)*cs + twoSqrtA_alpha);
                b1 =  2.0f*A * ((A-1.0f) - (A+1.0f)*cs);
                b2 =      A * ((A+1.0f) - (A-1.0f)*cs - twoSqrtA_alpha);
                a0 =           (A+1.0f) + (A-1.0f)*cs + twoSqrtA_alpha;
                a1 = -2.0f *  ((A-1.0f) + (A+1.0f)*cs);
                a2 =           (A+1.0f) + (A-1.0f)*cs - twoSqrtA_alpha;
                break;
            }
        }

        // Normalize by a0
        b0_ = b0 / a0;  b1_ = b1 / a0;  b2_ = b2 / a0;
        a1_ = a1 / a0;  a2_ = a2 / a0;
    }

    // Direct Form II (Canonical Form)
    float process(float x) {
        float y = b0_ * x + z1_;
        z1_ = b1_ * x - a1_ * y + z2_;
        z2_ = b2_ * x - a2_ * y;
        return y;
    }

private:
    float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f;
    float a1_ = 0.0f, a2_ = 0.0f;
    float z1_ = 0.0f, z2_ = 0.0f;
};

} // namespace xyzpan::dsp
```

**A = 10^(dB/40) interpretation:** For gainDb = -15dB: A = 10^(-15/40) = 10^(-0.375) ≈ 0.422. For gainDb = +5dB: A = 10^(5/40) = 10^(0.125) ≈ 1.334. The peaking EQ biquad handles both boost and cut with the same formula.

**CRITICAL:** At gainDb = 0, the peaking EQ becomes unity gain (all-pass). Verify by setting gainDb=0 and confirming output equals input. The biquad should reduce to b0=b2=1, b1=a1, a2=1 — i.e., all-pass.

### Pattern 4: Comb Bank Signal Flow

**What:** The ~10 comb filters run in series on the MONO signal before the binaural (ITD/ILD) stage or after it. The dry signal bypasses the bank. Wet/dry mix is controlled by Y position.

**Signal flow position:** Comb filters model early pinna reflections / room reflections that differ front vs back. They operate on the mono signal, before binaural processing. The binaural stereo split happens after. This is the correct signal flow:

```
mono input
  -> comb bank (series, Y-driven wet)   [DEPTH]
  -> pinna/shelf EQ (Z-driven)           [ELEV-01, ELEV-02]
  -> ITD/ILD binaural split              [Phase 2]
  -> chest bounce (parallel into L+R)   [ELEV-03]
  -> floor bounce (parallel into L+R)   [ELEV-04]
  -> stereo output
```

Wait — the chest/floor bounces need to be in the STEREO domain (applied to both L and R ears separately) to avoid coloring the binaural image. Per-ear bounce is more physically accurate: both L and R ears hear the bounce. The bounces are added post-binaural to both output channels.

**Dry/wet formula for comb bank:**

```cpp
// Per-sample inside the comb bank (Engine::process):
//   wetAmount = kCombMaxWet * max(0, -y)   // 0 at front, up to 0.3 at back
//   combOut = signal through all series combs
//   output = dry * (1 - wetAmount) + combOut * wetAmount
//
// At Y=0 (side): wetAmount=0, pure dry (no comb coloration)
// At Y=-1 (back): wetAmount=kCombMaxWet=0.3 (default), 30% comb
// At Y=1 (front): wetAmount=0, pure dry (no comb coloration)
```

**Note on wet formula:** The requirement says "0% at Y=0 to 30% at Y=-1." The `max(0, -Y)` gives 0 for Y=0 and Y=1 (both front and side = no comb), increasing linearly to 1 at Y=-1. This is multiplied by `kCombMaxWet = 0.3f`. The blend uses `wet * combOut + (1-wet) * dryIn` to preserve the dry signal energy.

### Pattern 5: Pinna Filter Modulation

**Elevation Z to filter gain mapping (ELEV-01, ELEV-02):**

```cpp
// Pinna notch: only active Z in [-1, 1], frozen for Z < 0 at Z=0 values
// ELEV-02: "Pinna filter stays at elevation-0 values between -1 and 0"
// So: only modulate the pinna notch for Z >= 0
//
// At Z=0:   gainDb = -15.0f (deep notch at 8kHz)
// At Z=1:   gainDb = +5.0f  (peak/boost at 8kHz)
// At Z<0:   gainDb = -15.0f (frozen at Z=0 value)

float z_clamped   = std::max(0.0f, params.z);         // [0, 1]
float pinnaGainDb = std::lerp(-15.0f, 5.0f, z_clamped); // -15 to +5 dB
// LERP: at z=0 → -15dB, at z=1 → +5dB

// High shelf: +3dB at elevation 1.0, 0dB at elevation 0
// ELEV-02: "high shelf removes 3dB scaling from 0 to -1"
// So shelf gain = +3dB at Z=1, 0dB at Z=0, removing shelf gradually from 0 to -1
//
// For Z >= 0: shelfGainDb = +3.0f * z_clamped  (0 to +3dB)
// For Z < 0:  shelfGainDb = +3.0f * (1 + params.z) → 0 at Z=-1, +3dB at Z=0
//   = +3.0f * (1 + params.z)
//   Unified: shelfGainDb = +3.0f * (params.z + 1.0f) / 2.0f ... no, re-read:
//   "high shelf removes 3dB scaling from 0 to -1" means:
//   At Z=0: shelf is at +3dB (max, since pinna EQ is boosting above)
//   At Z=-1: shelf removed (0dB, flat)
//   SIMPLER INTERPRETATION:
//   shelfGainDb = std::lerp(0.0f, 3.0f, (params.z + 1.0f) * 0.5f)
//   This gives: Z=-1 → 0dB, Z=0 → +1.5dB, Z=1 → +3dB
//
//   But ELEV-01 says shelf only applies at elevation 1.0 "+3dB high shelf at 4kHz+ at elevation 1.0"
//   And ELEV-02 says "high shelf removes 3dB scaling from 0 to -1"
//   Cleanest reading: shelf goes from +3dB (at Z=0) to 0dB (at Z=-1), fixed at +3dB for Z>=0
//   Let shelfDb = +3.0f * std::clamp((params.z + 1.0f), 0.0f, 1.0f)
//   Z=-1 → 0dB, Z=0 → +3dB, Z=1 → stays +3dB (clamped)
//   But ELEV-01 says "+3dB at elevation 1.0" which is the maximum...
//   FINAL interpretation (matches both requirements):
//   shelfGainDb = +3.0f * std::max(0.0f, params.z)   (only active Z >= 0, scales with Z)
//   PLUS: for Z in [-1,0], it decays: shelfGainDb = 0 (no boost below horizon)
//   But ELEV-02 says the shelf is "removed" from 0 to -1, implying it was at 3dB at Z=0.
//   RESOLVED: shelfGainDb = +3.0f at Z=0 → +3dB at Z=1; for Z<0, shelf fades to 0.
//   shelfGainDb = std::lerp(0.0f, 3.0f, std::clamp(params.z + 1.0f, 0.0f, 2.0f) * 0.5f)
//   But the simplest and most planner-friendly: expose shelf gain as a dev panel param,
//   with a default curve of +3*(Z+1)/2 clamped to [0, 3]. Tune by ear.
```

**NOTE FOR PLANNER:** The exact shelf modulation curve is an intentional area for empirical tuning — the dev panel exposure (ELEV-05) means the exact formula can be adjusted at runtime. Start with `shelfGainDb = 3.0f * std::clamp(params.z, 0.0f, 1.0f)` (shelf active only above horizon, maxes at +3dB at Z=1) and add a separate "below-horizon shelf removal" that ramps shelf down for Z<0. The exact formula is documented in the requirements but benefits from listening tests.

### Pattern 6: Chest Bounce Signal Flow

```
chest bounce path:
  mono signal                                // unfiltered mono input
    -> SVFFilter HP x4 (700Hz, Q=0.7071)    // cascade 4th-order HPF
    -> OnePoleLP (1kHz) OR SVFFilter LP      // 6dB/oct LP (single pole preferred)
    -> FractionalDelayLine.read(delayMs)     // 0 to 2ms delay driven by Z
    -> multiply by chestGain                 // -8dB at Z=-1, fade to -inf at Z=+1

  stereo output L += chestPath
  stereo output R += chestPath    // same path added to both ears
```

**Chest gain calculation:**
```cpp
// chestGain: -8dB at Z=-1, -inf (0 linear) at Z=1
// elevation_normalized = (-params.z + 1.0f) * 0.5f  → 0 at Z=1, 1 at Z=-1
float elevNorm = std::clamp((-params.z + 1.0f) * 0.5f, 0.0f, 1.0f);
float chestLinear = std::pow(10.0f, -8.0f / 20.0f) * elevNorm;
// At Z=-1: elevNorm=1.0, chestLinear=0.398 (-8dB)
// At Z=0:  elevNorm=0.5, chestLinear=0.199 (-14dB)
// At Z=+1: elevNorm=0.0, chestLinear=0.0 (-inf)
```

**Chest delay calculation:**
```cpp
// delayMs: 0ms at Z=-1, 2ms at Z=+1
// Wait — ELEV-03: "delay 0ms at -1 to 2ms at 1"
// Z=-1 = below, Z=+1 = above. Source above has chest bounce delay of 2ms.
// This makes sense: sound from above must travel down to chest and back up.
float chestDelayMs = std::clamp((params.z + 1.0f) * 0.5f, 0.0f, 1.0f)
                    * kChestDelayMaxMs;  // kChestDelayMaxMs = 2.0f
// At Z=-1: 0ms delay. At Z=+1: 2ms delay.
```

**Single-pole LP for 6dB/oct at 1kHz:**
```cpp
// A true single-pole (first-order) LP:
// a = exp(-2pi * fc / sr)
// b = 1 - a
// y[n] = b * x[n] + a * y[n-1]
// This is structurally identical to OnePoleSmooth.
// Can reuse OnePoleSmooth class with prepare(fc_Hz_as_time_constant, sr).
// BUT: OnePoleSmooth uses a 63% time constant, not a -3dB frequency.
// For a 1kHz first-order LP, -3dB at fc:
//   a = exp(-2pi * fc / sr)  [same formula! OnePoleSmooth uses 2pi not just pi]
// CONFIRMED: OnePoleSmooth.prepare() formula IS correct for a 1kHz -3dB LP.
// Instantiate as: OnePoleSmooth chest1kLP; chest1kLP.prepare(1000.0f / (2pi * sr) * 1000.0f, sr)
// Wait: OnePoleSmooth.prepare(smoothingMs, sr) computes a = exp(-6.28318530f / (smoothingMs * 0.001f * sampleRate))
// For -3dB at 1kHz: smoothingMs = 1000.0f / (2pi * 1000Hz) * 1000ms = 1000/(6283) ≈ 0.159ms
// So: chest1kLP.prepare(0.159f, sr)
// This gives a = exp(-6.28318530f / (0.159f * 0.001f * 44100f)) = exp(-6.28318530 / 7.01) = exp(-0.897) ≈ 0.408
// Check: -3dB at 1kHz => fc = sr * (1-a) / (2pi) ... ACTUALLY verify this differently:
//   First-order LP: H(z) = (1-a) / (1 - a*z^-1)
//   At w = 2pi*fc/sr: |H| = -3dB means (1-a)^2 / (1 - 2a*cos(w) + a^2) = 0.5
// The simpler approach: just use SVFFilter LP at 1kHz — it's second order (-12dB/oct),
// but with Q=0.7071 it rolls off at -12dB/oct. However, the requirement says "6dB/oct."
// TO MATCH SPEC: use a true single-pole. Can implement as OnePoleSmooth feeding audio.
// Correct formula for OnePoleSmooth repurposed as single-pole LP:
//   prepare() uses a = exp(-2pi*f0/sr), which IS the correct -3dB at f0 single-pole formula.
//   BUT prepare() takes smoothingMs as argument, not Hz.
//   Conversion: smoothingMs = 1000 / (2pi * freqHz) = 1000 / (6.28318 * freqHz)
//   At 1kHz: smoothingMs = 1000 / 6283.18 = 0.1592ms
// FINAL: chest1kLP.prepare(1000.0f / (6.28318f * 1000.0f), sr) = prepare(0.1592f, sr)
// This correctly gives a first-order LP with -3dB at 1kHz.
```

### Pattern 7: Floor Bounce Signal Flow

```
floor bounce path:
  mono signal                                   // raw mono (no filtering)
    -> FractionalDelayLine.read(floorDelayMs)   // 0ms at Z=-1 to 20ms at Z=+1
    -> multiply by floorGain                    // -5dB at Z=-1, fade to -inf at Z=+1

  stereo output L += floorPath
  stereo output R += floorPath
```

**Floor bounce ring buffer capacity:** 20ms at 96kHz = 1920 samples. Round up to next power of 2 = 2048 samples. This is the minimum floor bounce delay line capacity.

**Floor gain and delay calculations (same pattern as chest):**
```cpp
// floorGain: -5dB at Z=-1, -inf at Z=+1
float floorLinear = std::pow(10.0f, -5.0f / 20.0f) * std::clamp((-params.z + 1.0f) * 0.5f, 0.0f, 1.0f);
// At Z=-1: 0.562 (-5dB); At Z=+1: 0.0 (-inf)

// floorDelayMs: 0ms at Z=-1, 20ms at Z=+1
float floorDelayMs = std::clamp((params.z + 1.0f) * 0.5f, 0.0f, 1.0f) * kFloorDelayMaxMs;
// kFloorDelayMaxMs = 20.0f
```

**No filter on floor bounce:** The requirement (ELEV-04) specifies no filtering on floor bounce — just a delayed+attenuated copy. The floor acts as a hard reflective surface with a simple time delay.

### Pattern 8: EngineParams Extensions

```cpp
// Types.h additions for Phase 3:

struct EngineParams {
    // ... existing Phase 1/2 fields ...

    // === Phase 3: Depth (DEPTH) ===
    // Per-filter comb parameters (up to kMaxCombFilters = 10)
    static constexpr int kMaxCombFilters = 10;
    float combDelays_ms[kMaxCombFilters]   = { /* see Constants.h */ };
    float combFeedback[kMaxCombFilters]    = { /* see Constants.h */ };
    float combWetMax = 0.30f;   // maximum wet amount (at Y=-1); 0..1 range

    // === Phase 3: Elevation (ELEV) ===
    float pinnaNotchFreqHz  = 8000.0f;   // pinna notch center frequency
    float pinnaNotchQ       = 2.0f;      // pinna notch bandwidth (higher Q = narrower)
    float pinnaShelfFreqHz  = 4000.0f;   // high shelf transition frequency
    float chestDelayMaxMs   = 2.0f;      // chest bounce max delay
    float chestGainDb       = -8.0f;     // chest bounce gain at Z=-1
    float floorDelayMaxMs   = 20.0f;     // floor bounce max delay
    float floorGainDb       = -5.0f;     // floor bounce gain at Z=-1
};
```

### Anti-Patterns to Avoid

- **Setting comb feedback >= 1.0:** Never. Always hard-clamp in the class. Even at 0.999, the filter decays extremely slowly and will clip on sustained material. 0.95 is the practical maximum for musical use without runaway.
- **Using fractional delay for comb filters:** The DEPTH-01 comb bank uses integer delay (no Hermite needed). Comb filter pitch is determined by integer sample delay. Fractional delay would detune the comb frequencies non-musically. Only chest/floor bounces use `FractionalDelayLine`.
- **Applying comb filters AFTER binaural split:** Comb filters operate on the mono signal before the stereo split. Applying them in the stereo domain would introduce stereo width artifacts.
- **Per-sample biquad coefficient updates for pinna filter:** Unlike the SVF, direct-form biquads are NOT designed for per-sample coefficient changes — they can become unstable with rapid coefficient changes. Update pinna biquad coefficients once per block (driven by the smoothed Z value). The change in pinna notch frequency as the user adjusts elevation is slow compared to the block rate.
- **Not zeroing comb filter state on reset():** Comb filter buffers hold feedback state. On transport restart, un-zeroed state produces a burst of coloration. Zero all ring buffers in reset().
- **Forgetting to pre-allocate chest/floor delay lines:** These use `FractionalDelayLine`, which does a `std::vector<float>` allocation in `prepare()`. Must be called in `Engine::prepare()` along with all other DSP. Zero allocation during `process()`.
- **OnePoleSmooth for chest 1kHz filter confused with parameter smoother:** The chest 1kHz LP reuses OnePoleSmooth's mechanism but with a Hz-derived time constant (0.159ms). Document clearly this is an audio filter, not a parameter smoother.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Peaking EQ biquad coefficients | Custom formula from scratch | Audio EQ Cookbook (Robert Bristow-Johnson) | Industry standard, handles gain=0 correctly, verified by Web Audio API spec |
| High shelf biquad | Custom allpass + gain combination | Audio EQ Cookbook highShelf | Exact formula derivation; all shelving/gain/slope cases handled |
| 4th-order Butterworth HPF | Two-stage biquad with manual Q values | 2x SVFFilter HP with Q=0.7071 each | SVF is already in project; 2x second-order HP gives 4th-order slope (24dB/oct); phase response is not Butterworth exactly but close enough for a creative effect |
| Comb filter stability | Custom overflow detection | Hard clamp g to [-0.95, 0.95] in setFeedback() | Simple and bulletproof; no dynamic range monitoring needed |
| Floor bounce all-pass diffusion | Complex Schroeder diffuser | Simple delay + gain | ELEV-04 spec says no filtering on floor bounce — simplicity is correct here |

**Key insight:** All Phase 3 DSP uses at most 5-10 lines of arithmetic per filter type. The complexity is in signal routing (series vs parallel, mono vs stereo domain), not in individual filter computation.

---

## Common Pitfalls

### Pitfall 1: Comb Filter DC Buildup with Positive Feedback

**What goes wrong:** With g > 0 (positive feedback), DC offset in the signal accumulates through the feedback loop, gradually saturating and clipping the output even at moderate levels.

**Why it happens:** DC is a frequency component at 0 Hz, and the comb filter passes DC with gain `1/(1-g)`. At g=0.95, DC gain = 20 (26dB). Any tiny DC offset in the input is amplified massively.

**How to avoid:** Either (a) use only negative feedback values (g < 0) to notch DC instead of boost it, or (b) insert a DC-blocking first-order highpass before the comb bank (a = 0.9999 one-pole HP: `y[n] = x[n] - x[n-1] + 0.9999*y[n-1]`). Recommended: check if Phase 2 output has DC; if not, DC-blocking may not be needed in practice. Test with sustained tones and monitor DC level.

**Warning signs:** Gradual output level increase during sustained input with high comb feedback values.

### Pitfall 2: Biquad Instability from Rapid Coefficient Changes

**What goes wrong:** Updating the peaking EQ biquad coefficients at audio rate (per sample) can destabilize the filter, producing large output transients or silence.

**Why it happens:** Direct Form II biquad state variables (`z1`, `z2`) encode the filter's history in a way that is not invariant to coefficient changes. When `a1`, `a2` change abruptly, the stored state no longer corresponds to a valid filter state for the new coefficients.

**How to avoid:** Update biquad coefficients once per block, not per sample. Smooth the Z position parameter (elevation) with `OnePoleSmooth` before computing new coefficients. The one-pole smoother provides a gradual coefficient change that the biquad state can track without instability.

**Warning signs:** Clicking or loud burst when Z parameter changes rapidly; sustained output after input goes silent.

### Pitfall 3: FractionalDelayLine Capacity for 20ms Floor Bounce

**What goes wrong:** Allocating the floor bounce delay line with a capacity based on 44.1kHz sample rate, then running at 96kHz — the actual delay in samples exceeds the buffer, causing a ring buffer read from garbage memory.

**Why it happens:** `FractionalDelayLine::read()` has the precondition `delayInSamples < buffer_size - 4`. At 96kHz, 20ms = 1920 samples. A buffer sized for 44.1kHz (882 samples) is overrun.

**How to avoid:** Size all delay line capacities from `sampleRate` in `Engine::prepare()`, not from hard-coded sample counts. Pass `static_cast<int>(delayMaxMs * 0.001f * sampleRate) + 8` as the capacity argument. The existing `FractionalDelayLine::prepare()` rounds up to next power-of-2, so a 1920-sample capacity becomes a 2048-sample buffer automatically.

**Warning signs:** Crackling at high sample rates; silence or distortion at 96kHz that doesn't appear at 44.1kHz.

### Pitfall 4: Series Comb Filters Accumulate Gain

**What goes wrong:** 10 comb filters in series, each with g=0.5, can sum to a net gain much higher than unity on resonant frequencies.

**Why it happens:** Each feedback comb has peak gain `1/(1-|g|)`. 10 stages: `(1/(1-0.5))^10 = 1024` in the worst resonant case. With wet=30%, the overall output is `0.7*dry + 0.3*1024*dry = 308*dry` — catastrophic clipping.

**How to avoid:** Either keep individual feedback values low enough that the series product stays bounded, OR apply hard clipping/limiting to the comb bank output before the dry/wet blend. Recommended: at the default 30% wet mix, keep g values small enough that the maximum resonant gain times wetAmount < 1. With g=0.5 per stage, 10 stages, wet=0.3: peak_gain = 0.3*(1024) + 0.7 ≈ 308. So default g must be much lower. A practical default: `g = 0.3f` per filter → peak gain per stage = 1.43, 10 stages = 33, wet=0.3 → 10+0.7 = 10.7. Still too high for resonant tones. Recommended safe default: `g = 0.15f` → stage peak = 1.18, 10 stages = 5.2, wet=0.3 → 1.56 + 0.7 = 2.26. Acceptable for most material. **Test with sine waves at comb resonant frequencies.**

**Warning signs:** Loud output at specific frequencies during sustained tones; clipping meters lighting up.

### Pitfall 5: Chest Bounce SVF HP Cascade Phase Issues

**What goes wrong:** Four SVF HP filters in series with the same center frequency and Q=0.7071 do not produce a Butterworth 4th-order response — they produce a steeply resonant 4th-order response.

**Why it happens:** Two cascaded Butterworth 2nd-order HP filters with Q=0.7071 each give a combined 4th-order response with a peak near the cutoff. A true 4th-order Butterworth requires different Q values per stage (Q1≈0.5412, Q2≈1.3066).

**How to avoid:** The requirement says "4x highpass at 700Hz" without specifying Butterworth. For a chest bounce coloration filter, the exact pole distribution doesn't matter musically — what matters is that low frequencies below 700Hz are cut. Use 4x SVF HP with Q=0.7071 (Butterworth per stage). The combined response will have a slight bump near 700Hz, which is actually pleasant for chest coloration. Expose the Q values in the dev panel for tuning.

**Warning signs:** None for functionality, but resonance near 700Hz might be audible on bass-heavy material — tune the Q down if problematic.

### Pitfall 6: Denormals in Silent Comb Feedback

**What goes wrong:** After audio input goes silent, comb filter feedback slowly decays toward 0, producing subnormal float values and 100x CPU spikes.

**Why it happens:** MSVC FTZ (Flush-to-Zero) in the JUCE processBlock context should handle this, but the comb filters' exponential decay at high feedback values takes many seconds, continuously producing near-zero values.

**How to avoid:** JUCE sets `_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON)` in the audio thread. Verify this is active. If CPU spikes after silence persist, add a small DC offset technique: `buf_[pos] += 1e-25f` before reading back. This keeps state non-zero and prevents subnormal issues.

---

## Code Examples

### Comb Filter Bank (10 filters in series)

```cpp
// Source: Julius O. Smith, Physical Audio Signal Processing
// ccrma.stanford.edu/~jos/pasp/Feedback_Comb_Filters.html
// In Engine.h: std::array<dsp::FeedbackCombFilter, kMaxCombFilters> combBank_;

// In Engine::prepare():
for (int i = 0; i < kMaxCombFilters; ++i) {
    int cap = static_cast<int>(kCombMaxDelay_ms * 0.001f * sampleRate) + 4;
    combBank_[i].prepare(cap);
    // Set default delay and feedback from Constants.h
    combBank_[i].setDelay(static_cast<int>(kCombDefaultDelays_ms[i] * 0.001f * sampleRate));
    combBank_[i].setFeedback(kCombDefaultFeedback[i]);
}

// In Engine::process() per-sample:
float wetAmount  = kCombMaxWet * std::max(0.0f, -y);  // 0 at front/side, 0.3 at back
float combSignal = monoIn;
for (int i = 0; i < kMaxCombFilters; ++i)
    combSignal = combBank_[i].process(combSignal);
float depthOut = monoIn * (1.0f - wetAmount) + combSignal * wetAmount;
```

### Default Comb Delay Times (Constants.h)

```cpp
// 10 comb filter delays in ms, spread from ~0.2ms to 1.5ms
// Irregular spacing to avoid harmonic stacking (which would create a pitched comb)
// Based on typical HRTF measurement spacing for pinna reflections
constexpr float kCombDefaultDelays_ms[10] = {
    0.21f, 0.37f, 0.54f, 0.68f, 0.83f,
    0.97f, 1.08f, 1.23f, 1.38f, 1.50f
};
// Feedback values: conservative defaults (0.15f) to prevent gain stacking
constexpr float kCombDefaultFeedback[10] = {
    0.15f, 0.14f, 0.16f, 0.13f, 0.15f,
    0.14f, 0.16f, 0.13f, 0.15f, 0.14f
};
constexpr float kCombMaxWet      = 0.30f;  // DEPTH-03: max wet at Y=-1
constexpr float kCombMaxDelay_ms = 1.50f;  // DEPTH-02: max individual comb delay
```

### Pinna Notch / High Shelf Setup

```cpp
// Source: webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html
// In Engine.h: dsp::BiquadFilter pinnaNotchL_, pinnaNotchR_;
//              dsp::BiquadFilter pinnaShelfL_, pinnaShelfR_;

// In Engine::process() — per block (not per sample):
float z_pos_norm = std::max(0.0f, params.z);   // [0, 1] for above-horizon
float pinnaGainDb  = std::lerp(-15.0f, 5.0f, z_pos_norm);
float shelfGainDb  = 3.0f * z_pos_norm;         // 0 to +3dB as z goes 0 to 1

// Below-horizon shelf decay (ELEV-02):
// For Z < 0, shelf gain fades from +3dB (at Z=0) to 0 (at Z=-1)
// Use: shelfGainDb = 3.0f * std::clamp(params.z + 1.0f, 0.0f, 1.0f)
// This gives: Z=-1 → 0dB, Z=0 → 3dB, Z=1 → 3dB (clamped)
float shelfGainDbFull = 3.0f * std::clamp(params.z + 1.0f, 0.0f, 1.0f);

// Update biquad coefficients once per block:
pinnaNotchL_.setCoefficients(BiquadType::PeakingEQ,
    params.pinnaNotchFreqHz, sr, params.pinnaNotchQ, pinnaGainDb);
pinnaNotchR_.setCoefficients(BiquadType::PeakingEQ,
    params.pinnaNotchFreqHz, sr, params.pinnaNotchQ, pinnaGainDb);
pinnaShelfL_.setCoefficients(BiquadType::HighShelf,
    params.pinnaShelfFreqHz, sr, 0.7071f, shelfGainDbFull);
pinnaShelfR_.setCoefficients(BiquadType::HighShelf,
    params.pinnaShelfFreqHz, sr, 0.7071f, shelfGainDbFull);

// Apply per-sample (inside sample loop):
float monoEQ  = pinnaNotch_.process(depthOut);  // after comb bank
       monoEQ = pinnaShelf_.process(monoEQ);
// Then proceed to ITD/ILD binaural split with monoEQ as input
```

### Floor Bounce Delay Line Setup

```cpp
// In Engine::prepare():
int floorCap = static_cast<int>(kFloorDelayMaxMs * 0.001f * sr) + 8;
floorDelayL_.prepare(floorCap);
floorDelayR_.prepare(floorCap);

// In Engine::process() per-sample (post binaural split):
float floorDelayMs   = std::clamp((params.z + 1.0f) * 0.5f, 0.0f, 1.0f) * params.floorDelayMaxMs;
float floorDelaySamp = floorDelayMs * 0.001f * sr;
float floorLinear    = std::pow(10.0f, params.floorGainDb / 20.0f)
                       * std::clamp((-params.z + 1.0f) * 0.5f, 0.0f, 1.0f);

floorDelayL_.push(dL_pre_floor);   // push the post-binaural L channel
floorDelayR_.push(dR_pre_floor);
if (floorDelaySamp >= 2.0f) {
    outL[i] += floorDelayL_.read(floorDelaySamp) * floorLinear;
    outR[i] += floorDelayR_.read(floorDelaySamp) * floorLinear;
}
```

**Note:** Floor bounce pushes the post-binaural stereo signal into separate L/R delay lines, reads at the computed delay, and adds the attenuated copy to the output. The minimum delay check (`>= 2.0f`) reuses the same Hermite safety rule from Phase 2.

---

## State of the Art

| Old Approach | Current Approach | Notes |
|--------------|------------------|-------|
| HRTF measurement-based front/back | Tuneable comb filter array | Project-mandated; more customizable |
| SOFA file loading for elevation | Parametric pinna notch + bounces | Avoids HRTF coloration artifacts; matches project philosophy |
| Fixed biquad coefficients for EQ | Modulated peaking EQ (per-block update) | Slow modulation (elevation changes) is safe with per-block update |
| Convolution for room reflections | Simple delay + gain for bounces | Per-spec: chest and floor are modeled as single reflections, not full room IR |

**Deprecated/outdated for this project:**
- HRTF convolution for elevation: Out of scope; no SOFA loading
- Static (non-modulated) pinna filter: The parametric approach requires modulation by elevation Z

---

## Open Questions

1. **Comb filter default delay spacings and feedback values**
   - What we know: Delays must be between 0–1.5ms, approximately 10 values; feedback < 1.0
   - What's unclear: Whether irregular vs. regular spacing sounds better for front/back perception; whether positive or negative g values are more natural
   - Recommendation: Start with the irregular spacings proposed above (0.21 to 1.5ms). Use positive feedback (g>0) for peaks at harmonic frequencies of the delay. Expose all 10 delays and 10 feedback values in dev panel (DEPTH-05) for empirical tuning.

2. **Comb filter gain stacking safety margin**
   - What we know: Series combs can multiply gains dangerously at resonant frequencies
   - What's unclear: What default feedback values produce audible front/back without exceeding 6dB total gain across the comb bank
   - Recommendation: Default g=0.15 per filter; DEPTH-04 hard-clamp at 0.95. Test with sine wave sweeps and verify no clipping at any frequency before shipping defaults.

3. **Pinna filter applied mono vs. per-ear**
   - What we know: The pinna notch models the listener's physical ear canal interaction
   - What's unclear: Whether to apply pinna notch to both ears equally (mono domain before split) or per ear with slight variation
   - Recommendation: Apply in the mono domain before the binaural split (simpler, physically reasonable since both ears have pinna notches). If needed, a small frequency offset between L and R can be added as a dev panel parameter later.

4. **Chest bounce: per-ear or summed to mono before adding**
   - What we know: The chest is below the head, so both ears hear the chest bounce similarly
   - What's unclear: Whether adding an identical chest path to both L and R channels is correct, or if the chest path should also go through a binaural split
   - Recommendation: Add an identical chest path to both L and R (same amplitude, same delay). The chest is a coloration cue, not a spatial cue.

5. **OnePoleSmooth as single-pole LP for chest 1kHz filter**
   - What we know: The formula `a = exp(-2pi*fc/sr)` with b = 1-a is a correct single-pole LP
   - What's unclear: Whether `OnePoleSmooth.prepare()` uses `exp(-2pi / (smoothingMs * 0.001 * sr))` which maps correctly — need to verify the exact formula matches
   - Verification: `OnePoleSmooth.prepare(1000.0f / (2*pi * 1000.0f), sr)` → `prepare(0.1592f, sr)` → `a = exp(-6.28318 / (0.1592 * 0.001 * 44100)) = exp(-6.28318 / 7.0) = exp(-0.8976) = 0.407`. At 1kHz, the one-pole LP magnitude = (1-a) / sqrt((1-a*cos(w))^2 + (a*sin(w))^2) where w=2pi*1000/44100 ≈ 0.1424 rad. Numerically: (0.593) / sqrt((1-0.407*0.99)^2 + (0.407*0.142)^2) = 0.593 / sqrt(0.596^2 + 0.058^2) = 0.593 / sqrt(0.355 + 0.0034) = 0.593/0.598 = 0.99 ≈ 0dB. That's NOT -3dB at 1kHz — the formula is incorrect at 1kHz in this configuration. The one-pole LP -3dB is when magnitude = 1/sqrt(2). **RESOLVED:** Use an `SVFFilter` in LP mode with very high Q override is wrong. Use `SVFLowPass::setCoefficients(1000.0f, sr)` with default Q=0.7071 — this gives a 2nd-order LP at 1kHz (-12dB/oct slope). The requirement says "1x 6dB/oct lowpass at 1kHz" but the SVF gives -12dB/oct. A true 6dB/oct requires a single-pole. For practical implementation, use a `dsp::OnePoleLP` class (tiny new class). See below.

---

## Additional DSP Class Needed: OnePoleLP

The ELEV-03 chest bounce requires a "1x 6dB/oct lowpass at 1kHz." The SVFLowPass is 12dB/oct. A true single-pole LP is trivial:

```cpp
// engine/include/xyzpan/dsp/OnePoleLP.h
// A first-order (6dB/oct) IIR low-pass filter.
// Cutoff is the -3dB frequency.

#pragma once
#include <cmath>
#include <algorithm>

namespace xyzpan::dsp {

class OnePoleLP {
public:
    void reset() { z_ = 0.0f; }

    // Set -3dB cutoff frequency in Hz.
    // Formula: a = exp(-2pi * cutoffHz / sampleRate)
    void setCoefficients(float cutoffHz, float sampleRate) {
        a_ = std::exp(-6.28318530f * cutoffHz / sampleRate);
        b_ = 1.0f - a_;
    }

    float process(float x) {
        z_ = b_ * x + a_ * z_;
        return z_;
    }

private:
    float a_ = 0.0f, b_ = 1.0f, z_ = 0.0f;
};

} // namespace xyzpan::dsp
```

This is distinct from `OnePoleSmooth` (which smooths parameters using a time-constant-based formula, not a cutoff-frequency-based formula). `OnePoleLP` uses the exact -3dB frequency formula suitable for audio processing.

---

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | Catch2 3.7.1 |
| Config file | Root CMakeLists.txt (enable_testing() + catch_discover_tests()) |
| Quick run command | `ctest --test-dir build -R "DepthElevation" --output-on-failure` |
| Full suite command | `ctest --test-dir build --output-on-failure` |

### Phase Requirements to Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| DEPTH-01 | FeedbackCombFilter: push impulse, verify echo at delay distance | Unit | `ctest --test-dir build -R "CombFilter" --output-on-failure` | Wave 0 |
| DEPTH-01 | 10 comb filters in series: output has different spectral content than input | Unit | same | Wave 0 |
| DEPTH-02 | setDelay(N): read at delay N returns correct sample | Unit | `ctest --test-dir build -R "CombDelay" --output-on-failure` | Wave 0 |
| DEPTH-03 | Y=1 (front): wet=0, output == input (no comb coloration) | Unit | `ctest --test-dir build -R "CombWet" --output-on-failure` | Wave 0 |
| DEPTH-03 | Y=-1 (back): wet=0.3, output differs from input | Unit | same | Wave 0 |
| DEPTH-04 | setFeedback(1.5): clamped to 0.95, output does not grow unbounded | Unit | `ctest --test-dir build -R "CombStability" --output-on-failure` | Wave 0 |
| DEPTH-04 | Sustained sine at comb resonant frequency: output stays below 2x input peak | Unit | same | Wave 0 |
| ELEV-01 | BiquadFilter peaking at -15dB, 8kHz: 8kHz sine attenuated ~15dB vs passband | Unit | `ctest --test-dir build -R "PinnaNotch" --output-on-failure` | Wave 0 |
| ELEV-01 | BiquadFilter peaking at +5dB: 8kHz sine boosted ~5dB vs passband | Unit | same | Wave 0 |
| ELEV-01 | HighShelf +3dB at 4kHz: signal above 4kHz is boosted, below is unchanged | Unit | `ctest --test-dir build -R "HighShelf" --output-on-failure` | Wave 0 |
| ELEV-02 | Z < 0: pinna gain stays at -15dB (frozen) | Unit | `ctest --test-dir build -R "PinnaFreeze" --output-on-failure` | Wave 0 |
| ELEV-03 | Chest bounce: Z=-1 signal delayed by 0ms, Z=1 signal delayed by 2ms | Unit | `ctest --test-dir build -R "ChestBounce" --output-on-failure` | Wave 0 |
| ELEV-03 | Chest bounce: SVF HP x4 — sine at 100Hz is attenuated, sine at 5kHz is not | Unit | same | Wave 0 |
| ELEV-04 | Floor bounce: Z=-1 → 0ms delay, signal added at -5dB | Unit | `ctest --test-dir build -R "FloorBounce" --output-on-failure` | Wave 0 |
| ELEV-04 | Floor bounce: Z=+1 → 20ms delay, signal added at near-0 gain | Unit | same | Wave 0 |
| ELEV-05 | Engine integration: setting all Phase 3 EngineParams produces no NaN output | Unit | `ctest --test-dir build -R "Phase3Integration" --output-on-failure` | Wave 0 |

### Sampling Rate

- **Per task commit:** `ctest --test-dir build --output-on-failure`
- **Per wave merge:** `ctest --test-dir build --output-on-failure`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps

- [ ] `tests/engine/TestDepthAndElevation.cpp` — covers DEPTH-01 through ELEV-05
- [ ] `tests/CMakeLists.txt` — add `engine/TestDepthAndElevation.cpp` to `XYZPanTests` target (add one line)

*(No new framework install needed — Catch2 already wired)*

---

## Sources

### Primary (HIGH confidence)

- [Julius O. Smith — Physical Audio Signal Processing: Feedback Comb Filters](https://ccrma.stanford.edu/~jos/pasp/Feedback_Comb_Filters.html) — difference equation, stability criterion (|g| < 1), transfer function
- [Audio EQ Cookbook (Robert Bristow-Johnson, W3C)](https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html) — peakingEQ, highShelf, notch filter coefficient formulas (b0/b1/b2/a0/a1/a2)
- [Cytomic SVF — Andy Simper 2011 (hollance gist)](https://gist.github.com/hollance/2891d89c57adc71d9560bcf0e1e55c4b) — mix coefficients (m0, m1, m2) for all SVF outputs (LP, HP, BP, notch)
- Existing project code — SVFLowPass.h, FractionalDelayLine.h, OnePoleSmooth.h, Engine.cpp (Phase 2 patterns)
- [EarLevel Engineering — Cascading biquad filters](https://www.earlevel.com/main/2016/09/29/cascading-filters/) — confirmed: cascaded biquads for 4th-order response, Q values for Butterworth

### Secondary (MEDIUM confidence)

- [DAFx-15 NTNU — Pinna notch frequency estimation](https://www.ntnu.edu/documents/1001201110/1266017954/DAFx-15_submission_61.pdf) — N1 pinna notch typically in 6–12kHz range; elevation dependent; supports 8kHz as a reasonable default for frontal elevation
- [ScienceDirect — Parametric elevation control approach](https://www.sciencedirect.com/science/article/abs/pii/S0003682X18305991) — confirms parametric elevation modeling via filter coefficients, referenced as prior art
- [Avendano, Algazi, Duda (1999) — Head-and-Torso model](https://www.researchgate.net/publication/228632121_Binaural_Rendering_for_Enhanced_3D_Audio_Perception) — chest/torso reflection introduces 1–3ms delay with low-pass characteristic; supports ELEV-03 parameter values
- [KVR Audio — Comb filter stability and feedback](https://www.kvraudio.com/forum/viewtopic.php?t=380206) — confirms feedback must be < 1; series comb bank patterns for spatial audio

### Tertiary (LOW confidence — for awareness)

- Web searches on binaural front/back comb filter implementations (no single authoritative reference found for the specific XYZPan approach — it is a creative/novel design, not from a published paper)
- Default delay time spacings (0.21–1.5ms spread): Estimated from typical pinna measurement data; these are starting points for dev panel tuning, not physically-derived values

---

## Metadata

**Confidence breakdown:**
- Standard Stack: HIGH — all DSP classes are hand-rolled; formulas from primary sources (JOS, Audio EQ Cookbook, Andy Simper SVF paper)
- Architecture: HIGH — follows established Phase 2 patterns (prepare/setParams/process/reset); signal flow derivable directly from requirements
- Comb filter delays/feedback defaults: MEDIUM — reasonable starting values from pinna acoustics; empirical tuning required (dev panel covers this)
- Elevation filter formulas: MEDIUM-HIGH — Audio EQ Cookbook coefficients are HIGH; the elevation-to-gain curve interpretation has minor ambiguity (documented above)
- Pitfalls: HIGH — comb stability is textbook; biquad modulation instability is well-documented; delay line sizing is mechanical

**Research date:** 2026-03-12
**Valid until:** 2026-09-12 (stable domain — biquad and comb filter formulas do not change)
