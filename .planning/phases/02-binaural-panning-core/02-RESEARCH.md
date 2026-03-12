# Phase 2: Binaural Panning Core - Research

**Researched:** 2026-03-12
**Domain:** Audio DSP — ITD delay lines, head shadow filtering, ILD, parameter smoothing (pure C++)
**Confidence:** HIGH

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Head Shadow Filter**
- Single low-pass filter (SVF) per ear, cutoff modulated by azimuth X component
- At X=0 (center): LPF fully open on both ears — inaudible
- At X=-1 (hard left): right ear LPF active (darkened), left ear LPF fully open
- At X=+1 (hard right): left ear LPF active (darkened), right ear LPF fully open
- Filter only — no level reduction from head shadow itself
- Exaggerated/dramatic character: ~10-15 dB cut starting at 1-2 kHz at full azimuth

**Distance-Dependent Level Reduction (ILD)**
- Far ear gets attenuated based on proximity — near ear stays at unity
- Close source + full azimuth = significant level difference (hard panning feel)
- Far source = negligible level difference (both ears roughly equal)
- Distance gain attenuation (Phase 4) will handle overall level naturally when combined with this
- Max attenuation value exposed in dev panel for tuning

**ITD (Interaural Time Difference)**
- Sinusoidal scaling with floor function — NOT a crossover model
- At X=0: both ears have 0 delay
- X moving from 0 to -1: right ear delay increases sinusoidally from 0 to 0.72ms, left ear stays at 0
- X moving from 0 to +1: left ear delay increases sinusoidally from 0 to 0.72ms, right ear stays at 0
- Only the far ear gets delayed; near ear is always at 0
- Delay line uses cubic (Hermite) interpolation (decided pre-project)
- 0.72ms default max — dev panel range at Claude's discretion

**Rear Shadow Hint**
- Slight high-frequency rolloff applied to BOTH ears equally when source is behind
- Scales linearly with Y: no shadow at Y=0, full shadow at Y=-1
- No effect in front hemisphere (Y=0 to Y=1)
- Independent from azimuth-based head shadow (does not stack on far ear)
- Provides a hint of front/back distinction before Phase 3 comb filters arrive

**Parameter Smoothing**
- As fast as possible without inducing clicks — artifact avoidance is the priority
- Per-parameter smoothing if needed (different rates for ITD delay vs filter cutoff vs level)
- Target ~5ms baseline, Claude's discretion on exact per-parameter values
- Smoothing time(s) exposed in dev panel for tuning

**Spatial Character**
- Realistic baseline using physically accurate head model values
- Dev panel allows pushing beyond realistic values for creative use

### Claude's Discretion
- Head shadow filter topology (single LPF vs LPF + high shelf — pick what sounds best for the exaggerated character)
- ILD direction: far ear quieter (near ear unity) — confirmed, but exact curve shape is Claude's choice
- Max ILD attenuation at closest distance + full azimuth (reasonable value, exposed in dev panel)
- Max ITD dev panel range (default 0.72ms, upper bound Claude's choice)
- Per-parameter smoothing time values (optimize for artifact-free, expose in dev panel)
- Rear shadow filter cutoff and depth (subtle hint, not dramatic)

### Deferred Ideas (OUT OF SCOPE)
None — discussion stayed within phase scope
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PAN-01 | ITD applies up to 0.7ms delay to opposite ear based on azimuth X component | Woodworth spherical head model; sinusoidal delay mapping; fractional delay with cubic Hermite interpolation |
| PAN-02 | Head shadow filter applied to opposite ear based on azimuth X component | SVF TPT low-pass; Andy Simper coefficient formulas; per-sample cutoff modulation |
| PAN-03 | Mono input split to stereo L/R at the panning stage | Pipeline architecture: mono source -> ITD delay split -> per-ear filter chain -> stereo out |
| PAN-04 | Stereo input accepted and summed to mono before processing | Already implemented in Phase 1 Engine.cpp; no new work needed |
| PAN-05 | Panning is smooth and click-free during parameter automation | One-pole exponential smoothers per parameter; separate rates for delay, filter, and gain |
</phase_requirements>

---

## Summary

Phase 2 implements three DSP subsystems — an ITD fractional delay line, a head shadow SVF low-pass filter, and a distance-based ILD gain stage — that together convert a mono signal into a convincing stereo binaural image based on azimuth X position and distance. All three operate in the `Engine::process()` loop with pre-allocated state and zero run-time allocation.

The ITD model follows the Woodworth spherical head formula, which gives a sinusoidal delay curve that peaks near 0.65–0.72ms at 90-degree azimuth. The user decision to use only the far-ear delay (near ear always 0) is a simplification of this model that maps cleanly to the floor-function-style behavior requested: `delay_far = maxITD * sin(abs(azimuth))`, `delay_near = 0`. Fractional delay positions require cubic Hermite interpolation across 4 surrounding samples in a circular ring buffer. The head shadow filter is a TPT State Variable Filter (Andy Simper's formulation) run in low-pass mode, with cutoff driven by the azimuth X magnitude through a smoothed parameter. A rear shadow hint adds a second SVF (also LPF) applied equally to both ears controlled by the rear Y component.

Parameter smoothing uses independent one-pole exponential smoothers (IIR, form: `z = in*b + z*a`, `a = exp(-2π / (timeMs * 0.001 * sampleRate))`) per-parameter. The ITD delay target needs the slowest smoothing (Doppler pitch effect is the concern), while filter cutoff and gain can be faster. All smoothers are pre-computed at `prepare()` with sample-rate-compensated coefficients.

**Primary recommendation:** Implement `FractionalDelayLine`, `SVFLowPass`, and `OnePoleSmooth` as standalone header-only pure-C++ classes in `engine/include/xyzpan/dsp/`, then wire them together inside `Engine::process()` with the azimuth-to-parameter mappings.

---

## Standard Stack

### Core (no new external libraries — hand-rolled DSP per project decision)

| Component | Implementation | Purpose | Why |
|-----------|---------------|---------|-----|
| Fractional delay line | Custom circular buffer + cubic Hermite | ITD per-ear delay with sub-sample precision | Project mandates cubic Hermite; hand-rolled DSP |
| SVF low-pass filter | Custom TPT SVF (Andy Simper formulation) | Head shadow and rear shadow filtering | Project mandates SVF for modulated filters |
| One-pole smoother | Custom IIR (exponential) | Click-free parameter transitions | Cheapest correct approach; no external dep |
| ILD gain stage | Simple multiply (smoothed gain) | Distance-dependent level difference | Trivial once smoother exists |

### No New Dependencies

All DSP is hand-rolled. The existing build already provides:
- C++20 (MSVC) — all `<cmath>` functions available (`std::sin`, `std::exp`, `std::tan`)
- Catch2 3.7.1 — already wired in `tests/CMakeLists.txt`
- JUCE 8.0.12 — used only in plugin layer, NOT in engine

---

## Architecture Patterns

### Recommended Project Structure

```
engine/
├── include/xyzpan/
│   ├── dsp/
│   │   ├── FractionalDelayLine.h   # Ring buffer + cubic Hermite interpolation
│   │   ├── SVFLowPass.h            # TPT SVF in LP mode (Andy Simper)
│   │   └── OnePoleSmooth.h         # Exponential parameter smoother
│   ├── Engine.h                    # Existing — gains delay lines + SVFs as members
│   ├── Types.h                     # Existing — EngineParams grows new fields
│   ├── Constants.h                 # Existing — new ITD/ILD/filter constants added
│   └── Coordinates.h               # Existing — already computes azimuth
├── src/
│   └── Engine.cpp                  # Existing — process() wired to new DSP
tests/
└── engine/
    ├── TestCoordinates.cpp          # Existing (22 tests, all GREEN)
    └── TestBinauralPanning.cpp      # NEW — Wave 0 gap: covers PAN-01 through PAN-05
plugin/
└── src/
    ├── ParamIDs.h                   # NEW constants: ITD_MAX_MS, ILD_MAX_DB, etc.
    └── ParamLayout.cpp              # NEW APVTS params for dev panel exposure
```

### Pattern 1: Fractional Delay Line (Ring Buffer + Cubic Hermite)

**What:** A fixed-size circular buffer with a floating-point read pointer. Write at integer positions; read at fractional positions using 4-sample cubic Hermite interpolation.

**When to use:** Wherever sub-sample precision is needed during position modulation (ITD delay, later Doppler).

**Implementation:**

```cpp
// Source: demofox.org cubic Hermite + standard ring buffer pattern
// engine/include/xyzpan/dsp/FractionalDelayLine.h

#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace xyzpan::dsp {

class FractionalDelayLine {
public:
    void prepare(int capacitySamples) {
        // Power-of-2 size for bitmask wraparound optimization
        // Pad to next power of 2 >= capacitySamples + 4 (Hermite lookahead)
        int n = 1;
        while (n < capacitySamples + 4) n <<= 1;
        mask_ = n - 1;
        buf_.assign(n, 0.0f);
        writePos_ = 0;
    }

    void reset() {
        std::fill(buf_.begin(), buf_.end(), 0.0f);
        writePos_ = 0;
    }

    // Push one sample into the delay line
    void push(float sample) {
        buf_[writePos_ & mask_] = sample;
        ++writePos_;
    }

    // Read at fractional delay (0.0 = most recent sample, positive = older)
    // delayInSamples must be in [0, capacity - 2]
    float read(float delayInSamples) const {
        // Integer and fractional parts
        int d = static_cast<int>(delayInSamples);
        float t = delayInSamples - static_cast<float>(d);

        // 4 surrounding samples (Catmull-Rom / cubic Hermite)
        // writePos_ - 1 is the most recently written sample
        int base = writePos_ - 1 - d;
        float A = buf_[(base - 1) & mask_];  // oldest of 4
        float B = buf_[(base    ) & mask_];
        float C = buf_[(base + 1) & mask_];  // should NOT exist in real delay line sense
        float D = buf_[(base + 2) & mask_];  // newest

        // NOTE: for a delay line reading "into the past":
        // base + 0 = sample at integer delay d
        // base - 1 = sample at d+1 (older)
        // base + 1 = sample at d-1 (newer)
        // Hermite coefficients (Catmull-Rom)
        float a = -0.5f*A + 1.5f*B - 1.5f*C + 0.5f*D;
        float b =       A - 2.5f*B + 2.0f*C - 0.5f*D;
        float c = -0.5f*A          + 0.5f*C;
        float dd = B;
        return ((a*t + b)*t + c)*t + dd;  // Horner's method
    }

private:
    std::vector<float> buf_;
    int mask_     = 0;
    int writePos_ = 0;
};

} // namespace xyzpan::dsp
```

**CRITICAL NOTE on sample ordering:** For a delay-line reading into the past, the 4 surrounding samples are `[d+1, d, d-1, d-2]` in time (oldest to newest). Verify carefully during implementation — getting A/B/C/D backwards introduces subtle pre-ringing.

### Pattern 2: TPT SVF Low-Pass (Andy Simper Formulation)

**What:** Topology-Preserving Transform state variable filter in low-pass mode. Two integrator states (`ic1eq`, `ic2eq`). Stable under per-sample cutoff modulation without coefficient interpolation tricks.

**When to use:** Anywhere a modulated low-pass filter is needed (head shadow, rear shadow, Phase 3+ filters).

**Implementation:**

```cpp
// Source: cytomic.com/files/dsp/SvfLinearTrapOptimised.pdf
//         gist.github.com/hollance/2891d89c57adc71d9560bcf0e1e55c4b
// engine/include/xyzpan/dsp/SVFLowPass.h

#pragma once
#include <cmath>

namespace xyzpan::dsp {

class SVFLowPass {
public:
    void reset() { ic1eq_ = ic2eq_ = 0.0f; }

    // Set cutoff in Hz. Call at prepare() or whenever cutoff changes.
    // Q = 0.707 for Butterworth (no resonance bump) — appropriate for head shadow
    void setCoefficients(float cutoffHz, float sampleRate, float Q = 0.7071f) {
        float g = std::tan(3.14159265f * cutoffHz / sampleRate);
        float k = 1.0f / Q;
        a1_ = 1.0f / (1.0f + g * (g + k));
        a2_ = g * a1_;
        a3_ = g * a2_;
    }

    // Per-sample process — safe for audio-rate coefficient modulation
    float process(float v0) {
        float v3 = v0 - ic2eq_;
        float v1 = a1_ * ic1eq_ + a2_ * v3;
        float v2 = ic2eq_ + a2_ * ic1eq_ + a3_ * v3;
        ic1eq_ = 2.0f * v1 - ic1eq_;
        ic2eq_ = 2.0f * v2 - ic2eq_;
        return v2;  // LP output
    }

private:
    float a1_ = 0.0f, a2_ = 0.0f, a3_ = 0.0f;
    float ic1eq_ = 0.0f, ic2eq_ = 0.0f;
};

} // namespace xyzpan::dsp
```

### Pattern 3: One-Pole Exponential Smoother

**What:** First-order IIR low-pass filter run on parameter values (not audio). Converts instantaneous target jumps into smooth exponential ramps.

**When to use:** Every time a control value (ITD delay, SVF cutoff, gain) changes each block.

**Implementation:**

```cpp
// Source: musicdsp.org/en/latest/Filters/257-1-pole-lpf-for-smooth-parameter-changes.html
// engine/include/xyzpan/dsp/OnePoleSmooth.h

#pragma once
#include <cmath>

namespace xyzpan::dsp {

class OnePoleSmooth {
public:
    // smoothingMs: time constant in milliseconds (63% reach time)
    void prepare(float smoothingMs, float sampleRate) {
        a_ = std::exp(-6.28318530f / (smoothingMs * 0.001f * sampleRate));
        b_ = 1.0f - a_;
        z_ = 0.0f;
    }

    void reset(float value = 0.0f) { z_ = value; }

    // Call once per sample or once per block depending on use
    float process(float target) {
        z_ = target * b_ + z_ * a_;
        return z_;
    }

    float current() const { return z_; }

private:
    float a_ = 0.0f, b_ = 1.0f, z_ = 0.0f;
};

} // namespace xyzpan::dsp
```

### Pattern 4: Azimuth-to-Parameter Mapping

**What:** Formulas converting the normalized X coordinate to ITD delay samples, SVF cutoff Hz, and ILD gain.

**Implementation:**

```cpp
// In Engine::process() per-block preamble:

// azimuth from Coordinates.h (already computed):
//   azimuth = atan2(x, y),  range [-PI, +PI]
//   abs(azimuth) in [0, PI/2] for pure left/right at Y=0

// ITD — only far ear delayed
//   Woodworth sinusoidal: delay = maxITD_ms * sin(abs(x) * PI/2) * sampleRate / 1000
//   (Using X directly as sin-proxy since X = sin(azimuth) when Y=0; see note below)
float itdSamples = maxITD_ms * std::sin(std::abs(params.x) * (PI / 2.0f))
                  * static_cast<float>(sampleRate) / 1000.0f;
// x > 0 → left ear is far → delay left
// x < 0 → right ear is far → delay right

// Head shadow LPF cutoff — far ear only
//   At X=0: cutoff = kFullOpen_Hz (e.g., 20000 Hz — inaudible)
//   At X=1: cutoff = kShadow_Hz   (e.g., 1200 Hz — ~12 dB cut by 4 kHz)
float shadowCutoff = fullOpenHz + (shadowHz - fullOpenHz) * std::abs(params.x);
// Apply to far-ear SVF only; near-ear SVF stays at fullOpenHz

// ILD gain — far ear only
//   At distance = sqrt(3) (max, corners): negligible
//   At distance = kMinDistance (nearest): maximum attenuation
//   Curve: linear interpolation (or inverse-distance) in linear gain domain
float normDist = computeDistance(params.x, params.y, params.z);
float ildGain = 1.0f - ildMaxAttenLinear * std::abs(params.x)
                      * (1.0f - normDist / sqrtf(3.0f));
// ildMaxAttenLinear = dBToLinear(-ildMaxDb)

// Rear shadow — both ears equally, only when Y < 0
float rearAmount = std::max(0.0f, -params.y);  // 0..1, 0 = front, 1 = full rear
float rearCutoff = rearFullOpenHz + (rearShadowHz - rearFullOpenHz) * rearAmount;
```

**Note on X vs. azimuth for ITD:** The Woodworth formula is `ITD = (r/c)(θ + sinθ)` where θ is azimuth. When Y=0, X = sinθ exactly. When Y ≠ 0, using X directly underestimates the true lateral angle. Given the user decision to use sinusoidal scaling driven by X, this is intentional — it's a creative model, not a physically exact model. Document this choice in code comments.

### Pattern 5: Engine Integration — Signal Flow

```
prepare():
  - Allocate 2x FractionalDelayLine (L, R) sized for maxITD + margin
  - Instantiate 2x SVFLowPass (L, R head shadow)
  - Instantiate 1x SVFLowPass rear shadow (shared both ears)
  - Instantiate OnePoleSmooth for: itd_delay, shadow_cutoff, ild_gain, rear_cutoff
  - Call each smoother.prepare() and svf.setCoefficients() with initial values
  - Call each delayLine.prepare()

setParams():
  - Store params snapshot (unchanged from Phase 1 behavior)

process() per sample:
  1. monoIn = monoBuffer[i]                          // stereo->mono already done
  2. Compute smooth parameters:
       itdSmooth.process(itdTarget)
       shadowSmooth.process(shadowCutoffTarget)
       ildSmooth.process(ildGainTarget)
       rearSmooth.process(rearCutoffTarget)
  3. Update SVF coefficients if cutoff changed (or every N samples)
  4. delayL.push(monoIn)
     delayR.push(monoIn)
  5. Read from delay lines:
       float delayedL = delayL.read(xyzpan > 0 ? itdDelaySamples : 0.0f)
       float delayedR = delayR.read(xyzpan < 0 ? itdDelaySamples : 0.0f)
  6. Apply ILD gain to far ear
  7. Apply head shadow SVF (far ear only, near ear bypassed)
  8. Apply rear shadow SVF to both ears equally
  9. outL[i] = delayedL
     outR[i] = delayedR

reset():
  - delayL.reset(), delayR.reset()
  - shadowL.reset(), shadowR.reset(), rearSvf.reset()
  - Reset all smoothers
```

### Anti-Patterns to Avoid

- **Recalculating SVF coefficients every sample with `tan()` inside the sample loop without profiling:** `std::tan()` is expensive. Either smooth the cutoff value and recalculate coefficients only when the smoothed value changes detectably (epsilon check), or update every N=64 samples with linear coefficient interpolation between blocks. Audio-rate full recalculation should be profiled before shipping.
- **Modulo for ring buffer wraparound:** Use `& mask_` (bitmask) with power-of-2 buffer size — modulo is measurably slower on MSVC in tight audio loops.
- **Not zeroing delay buffers in `reset()`:** Uninitialized state causes clicks on transport restart. Zero all ring buffer contents in `reset()`.
- **Applying head shadow to BOTH ears:** Only the far (contralateral) ear gets darkened. The near ear SVF must remain wide open (cutoff = 20kHz or higher).
- **Smoothing the wrong domain:** Smooth cutoff frequency in Hz (linear Hz domain is acceptable at these ranges), not the SVF coefficient `g`. Smoothing `g` is unusual and not what Andy Simper's paper addresses — smooth the input frequency to `setCoefficients()`.
- **Using Phase 1 Y default:** `y = 1.0f` (front) by default, so `rearAmount = max(0, -1.0) = 0` — rear shadow is correctly inactive at startup. No special-casing needed.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| HRTF convolution | Custom FFT overlap-add per ear | (Not applicable — out of scope) | Coloration, CPU cost, SOFA format complexity |
| Ambisonic encode/decode | SH basis functions | (Out of scope — binaural stereo only) | Different product entirely |
| Sinc interpolation | Long FIR kernel for delay | Cubic Hermite (already decided) | Diminishing returns; cubic is sufficient for 0.72ms |
| Thread-safe ring buffer | Lock-based or complex atomic FIFO | Existing pattern: audio thread owns delay lines exclusively | Engine lives entirely on audio thread; no cross-thread state needed here |

**Key insight:** This phase adds no new external libraries. All DSP is ~100 lines of pure arithmetic. The complexity is in correctness, not in tool selection.

---

## Common Pitfalls

### Pitfall 1: Delay Line Index Direction (Hermite Sample Ordering)
**What goes wrong:** Reading samples in the wrong temporal order (newest/oldest swapped) produces pre-ringing or incorrect interpolation.
**Why it happens:** Ring buffer indices wrap; "past" samples are at `writePos_ - delay - 1`, not `writePos_ + delay`. The 4 samples fed to Hermite must be ordered oldest→newest OR the formula must be adapted.
**How to avoid:** Write a unit test: push a known impulse (1.0 at time 0, 0.0 elsewhere), then verify `read(0.0)` returns 1.0 and `read(0.5)` returns a value between 0 and 1 with no overshoot beyond 1.0.
**Warning signs:** Output level > input level from the delay line (Hermite overshoot from wrong ordering).

### Pitfall 2: SVF Instability Near Nyquist
**What goes wrong:** At 96kHz+ sample rates, very high cutoff values can push `g = tan(π * f / sr)` toward 1.0 or beyond, destabilizing the SVF.
**Why it happens:** The TPT SVF's `g` coefficient must stay below ~1.0 for stability. At 96kHz with cutoff = 20kHz: `g = tan(π * 20000/96000) ≈ tan(0.654) ≈ 0.77` — fine. But at 48kHz with f = 22kHz: `g = tan(π * 22000/48000) ≈ tan(1.44) ≈ 6.7` — unstable.
**How to avoid:** Clamp cutoff to `min(cutoffHz, 0.45f * sampleRate)` before computing `g`. For the head shadow filter, cutoff ≤ 20kHz is always safe at 44.1kHz and 48kHz. Add the clamp defensively.
**Warning signs:** Loud output burst or silence at high sample rates; NaN in filter output.

### Pitfall 3: Clicking from Delay Line Jump (ITD Discontinuity)
**What goes wrong:** X parameter jumps from -1 to +1 in automation; the delay target changes from max to 0 on the other ear, creating a large instantaneous read-pointer jump — audible as a click or pitch glitch.
**Why it happens:** Cubic Hermite cannot hide a discontinuous read pointer. Even with integer-sample accuracy, a sudden jump of ~32 samples (0.72ms @ 44.1kHz) creates an audible transient.
**How to avoid:** The `OnePoleSmooth` on `itdDelaySamples` is the fix. Set its time constant long enough that the read pointer moves at most 1–2 samples per block (≤ 128 samples @ 44.1kHz). At 5ms smoothing, maximum rate = 0.72ms / 5ms = 14.4% of max per 5ms window — manageable. Test by automating X from -1 to +1 in one block and listening for artifacts.
**Warning signs:** Audible "pop" on fast X parameter changes; pitch blip on slow changes (Doppler from pointer movement — expected, but should not be excessive).

### Pitfall 4: SVF Coefficient Update Frequency (tan() Cost)
**What goes wrong:** `std::tan()` in `setCoefficients()` called 44100 times/second per SVF × 3 SVFs = 132k tan() calls/second. On debug builds this is measurable; on release it depends on compiler SIMD.
**Why it happens:** Coefficient recomputation at audio rate is the simplest correct approach but not the cheapest.
**How to avoid:** Update SVF coefficients once per block (not per sample) by smoothing the cutoff frequency at block rate with the one-pole smoother, then calling `setCoefficients()` once per block with the smoothed value. This is sufficient — cutoff changes ~30 times per second in typical automation scenarios.
**Warning signs:** High CPU on debug builds; profile with VTune or MSVC profiler on release if needed.

### Pitfall 5: Denormal Floats in Filter State
**What goes wrong:** SVF or one-pole smoother state converges toward zero exponentially, producing subnormal float values that cause 100x slowdowns on x86 without FTZ (Flush-To-Zero).
**Why it happens:** MSVC does not enable FTZ by default. When `ic1eq_` and `ic2eq_` are near zero (silence), each multiplication produces smaller and smaller subnormals.
**How to avoid:** JUCE sets `_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON)` in `AudioProcessor::processBlock()` preamble. Verify this is active in `PluginProcessor.cpp`. Alternatively, add DC offset technique: add a tiny DC value (`1e-25f`) to filter state after silence to prevent exact zero.
**Warning signs:** High CPU during silence; perf drops when audio signal stops.

### Pitfall 6: ILD Gain Curve Shape vs. Distance
**What goes wrong:** Using `distance` directly as a linear multiplier gives wrong behavior — at maximum distance (sqrt(3) ≈ 1.73), ILD should be negligible; at kMinDistance (0.1), it should be strong.
**Why it happens:** `computeDistance()` returns Euclidean distance, not a 0–1 proximity scale.
**How to avoid:** Normalize: `proximity = 1.0f - (dist - kMinDistance) / (sqrt(3) - kMinDistance)`. Then `ildGain = 1.0f - ildMaxAttenLinear * abs(x) * proximity`. This gives 0 attenuation at max distance, full attenuation at min distance.

---

## Code Examples

### ITD Delay Calculation (Physical Basis)

```cpp
// Source: Woodworth (1938) spherical head model
// Wikipedia: Interaural time difference article
// Max ITD at 90 degrees: (r/c)(π/2 + 1) ≈ (0.0875/344)(2.571) ≈ 0.654ms
// Empirical measurements: 0.66–0.72ms depending on head size

// The user decision: sinusoidal scaling from X, only far ear delayed
// Near ear always at 0 delay.

float computeITDSamples(float x, float maxITD_ms, float sampleRate) {
    // sin-curve maps X in [0,1] to delay in [0, maxITD_ms]
    // abs(x) because left/right are symmetric; sign determines which ear
    float itd_ms = maxITD_ms * std::sin(std::abs(x) * (3.14159265f / 2.0f));
    return itd_ms * 0.001f * static_cast<float>(sampleRate);
}

// Delay routing:
// x > 0 (source right): right ear is near, left ear is far -> delay LEFT
// x < 0 (source left):  left ear is near, right ear is far -> delay RIGHT
// x = 0: both at 0 delay

float delayL_samples = (x > 0.0f) ? computeITDSamples(x, maxITD_ms, sr) : 0.0f;
float delayR_samples = (x < 0.0f) ? computeITDSamples(x, maxITD_ms, sr) : 0.0f;
```

### Cubic Hermite Interpolation (4-point, C++)

```cpp
// Source: demofox.org/2015/08/08/cubic-hermite-interpolation/
// Parameters: A=sample[i-1], B=sample[i], C=sample[i+1], D=sample[i+2]
// t: fractional position 0..1 between B and C
// In a delay line: B is the sample at integer delay, C is one sample newer

inline float cubicHermite(float A, float B, float C, float D, float t) {
    float a = -0.5f*A + 1.5f*B - 1.5f*C + 0.5f*D;
    float b =        A - 2.5f*B + 2.0f*C - 0.5f*D;
    float c = -0.5f*A           + 0.5f*C;
    // Horner's method for numerical stability
    return ((a*t + b)*t + c)*t + B;
}
```

### SVF Coefficient Formula (Andy Simper / Cytomic)

```cpp
// Source: cytomic.com/files/dsp/SvfLinearTrapOptimised.pdf
//         gist.github.com/hollance/2891d89c57adc71d9560bcf0e1e55c4b

// g  = tan(π * cutoffHz / sampleRate)   -- frequency warping
// k  = 1/Q                               -- damping (0.707 = no resonance bump)
// a1 = 1 / (1 + g*(g + k))
// a2 = g * a1
// a3 = g * a2
//
// Per-sample update:
//   v3 = v0 - ic2eq
//   v1 = a1*ic1eq + a2*v3
//   v2 = ic2eq + a2*ic1eq + a3*v3
//   ic1eq = 2*v1 - ic1eq
//   ic2eq = 2*v2 - ic2eq
//   LP output = v2
```

### One-Pole Smoother (sample-rate compensated)

```cpp
// Source: musicdsp.org/en/latest/Filters/257-1-pole-lpf-for-smooth-parameter-changes.html

// Coefficient formula:
//   a = exp(-2π / (smoothingMs * 0.001 * sampleRate))
//   b = 1.0 - a
//
// Per-sample:
//   z = target * b + z * a
//
// Interpretation:
//   smoothingMs ≈ 63% rise time (RC time constant)
//   5ms @ 44100Hz: a = exp(-2π / (0.005 * 44100)) ≈ exp(-0.01426) ≈ 0.9858
//   1ms @ 44100Hz: a ≈ 0.9288
//   10ms @ 44100Hz: a ≈ 0.9929
```

---

## State of the Art

| Old Approach | Current Approach | Notes |
|--------------|------------------|-------|
| Linear interpolation for fractional delay | Cubic Hermite (Catmull-Rom) | Higher spectral quality; less HF rolloff; project-mandated |
| Direct-form biquad for modulated filter | TPT SVF (Andy Simper) | Stable under audio-rate modulation; project-mandated |
| Fixed gain coefficient applied per block | One-pole exponentially smoothed per-sample | Sample-rate invariant; artifact-free |
| Single global smoothing time for all params | Per-parameter smoothing times | Different perceptual sensitivity per parameter |

**Deprecated/outdated for this project:**
- HRTF convolution: Out of scope; no SOFA loading, no convolution engine
- Euler-integrated SVF (classic analog-modeled): Use trapezoidal (TPT) instead — avoids gain error at high frequencies

---

## Open Questions

1. **SVF coefficient update frequency: per-sample vs. per-block**
   - What we know: Per-block is cheaper; per-sample is theoretically more accurate for modulated filters
   - What's unclear: Whether the difference is audible for the relatively slow automation rates of a spatial panner (not a synth FM filter)
   - Recommendation: Start with per-block (once per process() call, using the smoothed cutoff value). Profile and move to per-N-samples interpolation if zipper artifacts appear. The one-pole smoother already does most of the smoothing work.

2. **ILD curve shape: linear vs. inverse-square proximity**
   - What we know: User said close + full azimuth = strong ILD, far = negligible; exact curve is Claude's discretion
   - What's unclear: Whether a linear proximity scale sounds right or needs a curve
   - Recommendation: Start with `ildGain = 1.0 - maxAtten * |x| * proximity_linear`. Expose max attenuation in dev panel and tune by ear. A square-law curve (proximity^2) may sound more natural but is harder to explain — verify empirically.

3. **Ring buffer capacity for ITD delay lines**
   - What we know: Max ITD = 0.72ms; at 44100Hz = ~32 samples; need 4 extra for Hermite lookahead
   - What's unclear: What capacity to allocate for the dev panel "max ITD" upper bound
   - Recommendation: Allocate for 5ms (= ~221 samples at 44100Hz, ~480 at 96kHz) — this gives headroom for the dev panel upper bound without wasting much memory. Round up to next power-of-2 = 256 @ 44100Hz, 512 @ 96kHz.

---

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | Catch2 3.7.1 |
| Config file | Root CMakeLists.txt (enable_testing() + catch_discover_tests()) |
| Quick run command | `ctest --test-dir build -R "binaural" --output-on-failure` |
| Full suite command | `ctest --test-dir build --output-on-failure` |

### Phase Requirements to Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| PAN-01 | ITD delay: hard left (X=-1) → right ear delayed ~32 samples, left at 0 | Unit | `ctest --test-dir build -R "ITD" --output-on-failure` | Wave 0 |
| PAN-01 | ITD delay: hard right (X=1) → left ear delayed, right at 0 | Unit | same | Wave 0 |
| PAN-01 | ITD delay: X=0 → both ears at delay=0 | Unit | same | Wave 0 |
| PAN-01 | FractionalDelayLine: read(0.5) returns interpolated value, no overshoot | Unit | `ctest --test-dir build -R "DelayLine" --output-on-failure` | Wave 0 |
| PAN-02 | Head shadow SVF: X=1 → left ear LPF active, right LPF open | Unit | `ctest --test-dir build -R "HeadShadow" --output-on-failure` | Wave 0 |
| PAN-02 | Head shadow SVF: X=0 → both ears LPF fully open (cutoff ~ 20kHz) | Unit | same | Wave 0 |
| PAN-03 | Mono input → L and R output differ at extreme azimuth (ITD + ILD applied) | Unit | `ctest --test-dir build -R "PannerOutput" --output-on-failure` | Wave 0 |
| PAN-04 | Stereo input summed to mono before processing (already in Phase 1) | Unit | `ctest --test-dir build -R "StereoSum" --output-on-failure` | Existing / verify |
| PAN-05 | Automation test: X sweeps -1→+1 in one block, no NaN, no |sample| > 1.5 | Unit | `ctest --test-dir build -R "Automation" --output-on-failure` | Wave 0 |
| PAN-05 | OnePoleSmooth: settles to target within ~5 time constants | Unit | `ctest --test-dir build -R "Smoother" --output-on-failure` | Wave 0 |

### Sampling Rate

- **Per task commit:** `ctest --test-dir build --output-on-failure`
- **Per wave merge:** `ctest --test-dir build --output-on-failure` (same — only one test binary)
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps

- [ ] `tests/engine/TestBinauralPanning.cpp` — covers PAN-01 through PAN-05 (ITD, head shadow, ILD, stereo sum, automation/smoothing)
- [ ] `tests/CMakeLists.txt` — add `engine/TestBinauralPanning.cpp` to `XYZPanTests` target

*(No new framework install needed — Catch2 already wired)*

---

## Sources

### Primary (HIGH confidence)
- [Cytomic SvfLinearTrapOptimised.pdf](https://www.cytomic.com/files/dsp/SvfLinearTrapOptimised.pdf) — SVF coefficient formulas (g, k, a1, a2, a3) and per-sample state update equations
- [hollance GitHub gist — Cytomic SVF C++](https://gist.github.com/hollance/2891d89c57adc71d9560bcf0e1e55c4b) — verified C++ implementation of Andy Simper's SVF
- [musicdsp.org — 1-pole LPF for parameter smoothing](https://www.musicdsp.org/en/latest/Filters/257-1-pole-lpf-for-smooth-parameter-changes.html) — one-pole smoother formula with sample-rate compensation
- [demofox.org — Cubic Hermite Interpolation](https://blog.demofox.org/2015/08/08/cubic-hermite-interpolation/) — exact 4-point Hermite formula with C++ code
- Wikipedia — [Interaural time difference](https://en.wikipedia.org/wiki/Interaural_time_difference) — Woodworth formula, max ITD ~0.7ms, duplex theory

### Secondary (MEDIUM confidence)
- [Aaronson & Hartmann (2014) via PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC3985894/) — testing/correcting Woodworth model; confirms sinusoidal shape
- [KVR Audio — Parameter smoothing for delay line](https://www.kvraudio.com/forum/viewtopic.php?t=412600) — delay time smoothing techniques, one-pole vs. ramp
- [KVR Audio — SVF audio-rate modulation cost discussion](https://www.kvraudio.com/forum/viewtopic.php?t=489694) — confirms per-block update is standard practice
- [JUCE forum — SmoothedValue for delay time](https://forum.juce.com/t/how-to-smooth-changes-in-delay-time-using-linear-smoothedvalue/36525) — confirms linear smoothing for delay time

### Tertiary (LOW confidence — for awareness only)
- [ScienceDirect — Interaural Level Difference overview](https://www.sciencedirect.com/topics/engineering/interaural-level-difference) — ILD frequency dependence (primarily > 1500 Hz); not directly used since our ILD is gain-based not spectral
- [KVR Audio — Efficient circular buffer for delay line](https://www.kvraudio.com/forum/viewtopic.php?t=408611) — bitmask wraparound optimization confirmed as fastest

---

## Metadata

**Confidence breakdown:**
- Standard Stack: HIGH — no new libraries; all formulas sourced from primary references (Andy Simper paper, demofox.org, musicdsp.org)
- Architecture: HIGH — follows established Phase 1 patterns (pure C++ engine, prepare/setParams/process/reset lifecycle); signal flow is straightforward
- ITD model: HIGH — Woodworth formula is textbook; user decision to use sinusoidal X mapping is a simplification that is clearly correct for the creative context
- Pitfalls: MEDIUM-HIGH — Hermite ordering and SVF stability are well-documented gotchas from primary sources; denormal issue is JUCE-known; ILD curve shape is empirical

**Research date:** 2026-03-12
**Valid until:** 2026-09-12 (stable domain — audio DSP formulas do not change)
