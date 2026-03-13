# Phase 4: Distance Processing - Research

**Researched:** 2026-03-12
**Domain:** Distance attenuation, air absorption LPF, propagation delay, Doppler shift (C++ DSP, hand-rolled)
**Confidence:** HIGH

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| DIST-01 | Gain attenuation follows inverse-square law (every distance doubling removes 6dB) | Inverse-square math section; distance normalization mapping |
| DIST-02 | Low-pass filter rolls off from 22kHz (closest) to 8kHz (furthest) at 6dB/octave | OnePoleLP already in codebase — use it; cutoff mapping section |
| DIST-03 | Distance delay ranges 0ms (closest) to 300ms (furthest) — not latency-compensated, creative effect | FractionalDelayLine already in codebase; 300ms sizing section |
| DIST-04 | Doppler shift occurs naturally from delay modulation over time when distance changes | Delay modulation IS Doppler; OnePoleSmooth on delay target section |
| DIST-05 | Doppler shift is toggleable (off = no distance delay applied) | Toggle pattern section; branch outside sample loop |
| DIST-06 | Delay line uses cubic (Hermite) interpolation to avoid artifacts during modulation | FractionalDelayLine already implements Catmull-Rom — no new code needed |
| DIST-07 | All distance parameters tuneable via dev panel | EngineParams + ParamIDs + ParamLayout extension pattern documented |
</phase_requirements>

---

## Summary

Phase 4 adds the DistanceProcessor stage to the XYZPan signal chain. The engine already computes Euclidean distance (`computeDistance()`) and passes it to the per-sample loop; Phase 4 uses that value to drive three effects: gain rolloff (inverse-square law), high-frequency air absorption (one-pole LPF), and propagation delay with optional Doppler shift.

All required DSP primitives are already in the codebase from Phases 2-3. `FractionalDelayLine` (cubic Hermite / Catmull-Rom) handles both the delay and Doppler — Doppler is a natural consequence of smoothly varying the delay time, not a separate DSP stage. `OnePoleLP` handles air absorption. `OnePoleSmooth` handles parameter smoothing. No new DSP classes are required; the work is implementing the DistanceProcessor logic inside `Engine.cpp` plus the usual EngineParams / ParamIDs / ParamLayout / PluginProcessor wiring.

The most critical technical decision for Phase 4 is how to smooth the distance delay target. Distance change rate must be limited to produce natural-sounding Doppler rather than discontinuous jumps. `OnePoleSmooth` (which is already used for ITD smoothing with a configurable time constant) provides the exact mechanism needed: set a target delay (in samples) each block, let the smoother ramp toward it per-sample. The read pointer moves at a varying rate relative to the write pointer, creating the pitch shift naturally.

**Primary recommendation:** Implement DistanceProcessor as a new set of members in `XYZPanEngine` (same pattern as chest/floor bounce in Phase 3) — no separate file needed unless the planner prefers one. Wire it into the Engine.cpp per-sample loop after the existing floor bounce stage. Follow the established EngineParams / ParamIDs / ParamLayout / PluginProcessor pattern exactly as Phase 3 did.

---

## Standard Stack

### Core (already in codebase — no new dependencies)

| Component | Location | Purpose | Status |
|-----------|----------|---------|--------|
| `FractionalDelayLine` | `engine/include/xyzpan/dsp/FractionalDelayLine.h` | Ring buffer with cubic Hermite interpolation — handles both delay and Doppler | EXISTS |
| `OnePoleLP` | `engine/include/xyzpan/dsp/OnePoleLP.h` | 6dB/oct one-pole lowpass — air absorption LPF | EXISTS |
| `OnePoleSmooth` | `engine/include/xyzpan/dsp/OnePoleSmooth.h` | Exponential parameter smoother — smooths delay target and gain | EXISTS |
| `computeDistance()` | `engine/include/xyzpan/Coordinates.h` | Already called in `Engine::process()` per-block — distance value available | EXISTS |

### Supporting (conceptual pattern only)

| Pattern | Description | Where in Codebase |
|---------|-------------|-------------------|
| Per-block target, per-sample smoother | Compute target once per block, `OnePoleSmooth::process()` per sample | Matches ITD, ILD, combWet, floor/chest gain patterns throughout Engine.cpp |
| Branch outside sample loop | `if (dopplerEnabled)` checked once per block, two code paths | Matches existing `if (x > 0.0f)` ITD side selection |
| `std::max(2.0f, delaySamples)` guard | Ensures Hermite interpolation always has valid C/D points | Identical guard already used for chest and floor bounce delay lines |

### Installation

No new dependencies. All code is self-contained in the existing engine library.

---

## Architecture Patterns

### Recommended Signal Chain Insertion Point

Distance processing goes AFTER the floor bounce (current last step) in the per-sample loop:

```
[Comb bank] -> [Pinna EQ] -> [ITD/ILD binaural split] ->
[Chest bounce] -> [Floor bounce] -> [Distance Proc] -> outL/R
```

This ordering means distance attenuation and LPF act on the fully-localized stereo signal — perceptually correct because distance affects the whole auditory image, not just one component of it.

### Pattern 1: Distance-to-Delay Mapping

The normalized distance from `computeDistance()` is in `[kMinDistance, sqrt(3)]` where `sqrt(3) ≈ 1.732`. Map this to the delay range `[0ms, 300ms]`:

```cpp
// Source: established pattern from Engine.cpp per-block target computation
// kMinDistance = 0.1f, kSqrt3 = 1.7320508f (Constants.h)
// distNorm in [kMinDistance, kSqrt3]

constexpr float kDistDelayMaxMs = 300.0f;

float distNorm = computeDistance(x, y, z);  // already computed in process()

// Map to [0, 1] then to [0, kDistDelayMaxMs]
float distFrac = (distNorm - kMinDistance) / (kSqrt3 - kMinDistance);  // [0, 1]
float delayTargetMs = distFrac * currentParams.distDelayMaxMs;
float delayTargetSamples = delayTargetMs * 0.001f * static_cast<float>(sampleRate);
```

### Pattern 2: Doppler via Delay Smoothing (the key insight)

Doppler is NOT a separate DSP stage. It is the natural consequence of smoothly modulating the delay line's read position. When the delay target decreases (source approaching), the smoother reads ahead slightly relative to the write pointer, producing a pitch increase. When the delay target increases (source receding), it reads behind, producing a pitch decrease.

```cpp
// Per-block: set target
// distDelaySmooth_ is an OnePoleSmooth (same class as itdSmooth_)
// Per-sample: advance the smoother and read at the smoothed position

// In the per-sample loop:
float delaySamp = distDelaySmooth_.process(delayTargetSamples);
delaySamp = std::max(2.0f, delaySamp);  // Hermite guard — same as ITD/floor/chest

distDelayL_.push(dL);
distDelayR_.push(dR);
dL = distDelayL_.read(delaySamp);
dR = distDelayR_.read(delaySamp);
```

**The smoothing time constant controls the Doppler intensity.** A longer time constant (slower ramp) produces gentle pitch shift. A shorter time constant (faster ramp) produces stronger pitch shift. Expose this as a dev panel parameter. A reasonable default is 20-50ms — faster than ITD smoothing (8ms) because the 300ms range needs enough time to avoid discontinuous jumps.

### Pattern 3: Doppler Toggle (DIST-05)

When Doppler is toggled OFF, the spec says "no distance delay applied" (not just no pitch shift). This is consistent with the requirement: toggling off removes the timing offset entirely.

```cpp
// Evaluated once per block, outside the sample loop
bool dopplerOn = currentParams.dopplerEnabled;  // bool param

// Inside per-sample loop:
if (dopplerOn) {
    float delaySamp = std::max(2.0f, distDelaySmooth_.process(delayTargetSamples));
    distDelayL_.push(dL);
    distDelayR_.push(dR);
    dL = distDelayL_.read(delaySamp);
    dR = distDelayR_.read(delaySamp);
} else {
    // Still push into delay line to keep write pointer advancing
    // (if delay line is not pushed, it falls out of sync on re-enable)
    distDelayL_.push(dL);
    distDelayR_.push(dR);
    // But read at minimum (kMinDelay) so no timing offset
    dL = distDelayL_.read(2.0f);
    dR = distDelayR_.read(2.0f);
}
```

**Alternative toggle implementation:** When doppler is off, bypass the delay lines entirely and pass dL/dR straight through. Simpler but requires a crossfade on toggle to avoid a click from the timing discontinuity. The push-but-read-at-minimum approach avoids the click because the smoother will naturally ramp from whatever position it's at to 2.0f.

**IMPORTANT:** Reset `distDelaySmooth_` to `2.0f` (not `0.0f`) when toggle turns ON again, to avoid a sudden jump from the current delay to the new target. Or use `distDelaySmooth_.reset(2.0f)` on toggle event. Actually, `OnePoleSmooth` does NOT reset z_ in `prepare()` — it will naturally ramp from wherever it currently is, which is the correct behavior. No special handling needed.

### Pattern 4: Inverse-Square Gain Attenuation (DIST-01)

The inverse-square law: gain = 1 / distance. But "distance" here is the normalized distance in [kMinDistance, kSqrt3]. We want gain = 1 at closest (kMinDistance) and gain rolls off with doubling. The requirement says "every distance doubling removes 6dB" — that is exactly inverse-square law (6dB = factor of 2 in amplitude = factor of 4 in power, but for amplitude: every 2x distance = 1/2x amplitude = -6dB).

```cpp
// Per-block: compute gain target from distance
// distNorm = computeDistance(x, y, z) in [kMinDistance, kSqrt3]
// At kMinDistance: gain = kMinDistance / distNorm = 1.0 (full gain)
// At kSqrt3: gain = kMinDistance / kSqrt3 ≈ 0.058 (-24.7dB)

float gainTarget = kMinDistance / distNorm;  // inverse law, normalized so closest = 1.0
gainTarget = std::clamp(gainTarget, 0.0f, 1.0f);

// Per-sample: smooth the gain and apply
float distGain = distGainSmooth_.process(gainTarget);
dL *= distGain;
dR *= distGain;
```

**Why `kMinDistance / distNorm` and not `1.0f / distNorm`:** This keeps gain at exactly 1.0 when the source is at the minimum distance (closest point), preventing a gain boost when the source is very near. The physical model is: reference gain is defined at the closest listening distance.

### Pattern 5: Air Absorption LPF (DIST-02)

One-pole lowpass per ear, cutoff sweeps from 22kHz (closest) to 8kHz (furthest). `OnePoleLP` is already in the codebase from Phase 3 (used for chest bounce).

```cpp
// Per-block: compute cutoff target
// distFrac in [0, 1]: 0 = closest, 1 = furthest
constexpr float kAirAbsMaxHz  = 22000.0f;  // no absorption at minimum distance
constexpr float kAirAbsMinHz  = 8000.0f;   // full absorption at maximum distance

float airCutoffTarget = kAirAbsMaxHz + (kAirAbsMinHz - kAirAbsMaxHz) * distFrac;
// = 22000 - 14000 * distFrac  (sweeps down from 22kHz to 8kHz)

// Apply to both ears (OnePoleLP, setCoefficients() called per-block only):
airLPF_L_.setCoefficients(airCutoffTarget, static_cast<float>(sampleRate));
airLPF_R_.setCoefficients(airCutoffTarget, static_cast<float>(sampleRate));

// Per-sample: filter applies on the full-bandwidth L/R signal
dL = airLPF_L_.process(dL);
dR = airLPF_R_.process(dR);
```

**Why per-block coefficient update:** `OnePoleLP::setCoefficients()` involves `std::exp()` which is expensive per-sample. The precedent is already established in Phase 3: BiquadFilter coefficients are only updated per-block for the same reason. Per-block is sufficient — cutoff changes at audio rate are inaudible for a distance-driven parameter.

**Why OnePoleLP and not SVFLowPass:** The spec says "6dB/octave rolloff characteristic." One-pole LP is exactly 6dB/oct. SVF is 12dB/oct. The one-pole is the correct filter for this requirement.

**Order of operations:** Apply LPF BEFORE the delay, or AFTER? Physically, air absorption happens during propagation, so it is part of the delay path. In practice, the order does not matter audibly because both effects are driven by the same distance value. Apply LPF after delay (after Doppler) for simplicity, since the delay lines emit the signal and then we filter the output.

### Pattern 6: Delay Line Sizing for 300ms at All Sample Rates

The delay cap must accommodate 300ms at 192kHz (maximum supported sample rate per INFRA-03):

```cpp
// In prepare():
constexpr float kDistDelayMaxMs = 300.0f;
constexpr float kMaxSupportedSampleRate = 192000.0f;

// Worst-case: 300ms at 192kHz = 57,600 samples
// FractionalDelayLine rounds up to next power-of-2 + 4 guard
// So allocate for maxSupportedSampleRate, not current sampleRate
int distDelayCap = static_cast<int>(kDistDelayMaxMs * 0.001f * kMaxSupportedSampleRate) + 8;
// = 57,608 → rounds up to 65,536 (next power-of-2)
// At 44.1kHz: only ~13,230 samples are ever used
// Memory: 65,536 * 4 bytes = 262KB per delay line, 524KB for two (stereo)
// Acceptable — pre-allocated in prepare(), zero runtime allocation

distDelayL_.prepare(distDelayCap);
distDelayR_.prepare(distDelayCap);
```

**CRITICAL:** Always size for the maximum supported sample rate, not `currentSampleRate`, so that `prepare()` does not need to reallocate if the sample rate increases. This is the same principle established in ARCHITECTURE.md section 4.4.

### Pattern 7: EngineParams Extension (DIST-07)

Follow the Phase 3 pattern exactly. Add to `EngineParams` in `Types.h`:

```cpp
// Phase 4: Distance Processing (DIST-01 through DIST-07)
float distDelayMaxMs   = kDistDelayMaxMs;    // 300.0f — max propagation delay
float distGainRef      = kMinDistance;       // reference distance for gain normalization
bool  dopplerEnabled   = true;               // DIST-05: toggleable doppler
float distSmoothMs     = kDistSmoothMs;      // smoother time constant for delay (dev panel)
float airAbsMaxHz      = kAirAbsMaxHz;       // 22000.0f — LPF cutoff at closest
float airAbsMinHz      = kAirAbsMinHz;       // 8000.0f  — LPF cutoff at furthest
```

Add corresponding `constexpr` values to `Constants.h`. Add ParamIDs to `ParamIDs.h`. Add parameters to `ParamLayout.cpp`. Add `std::atomic<float>*` members and wiring in `PluginProcessor.h/.cpp`. This is the exact same 4-file pattern done in Phases 2 and 3.

**Note on bool parameter in APVTS:** JUCE does not have `AudioParameterBool` in APVTS the same way as float. Use `juce::AudioParameterChoice` with "Off"/"On" choices, or `juce::AudioParameterBool` directly. The existing codebase uses `AudioParameterFloat` everywhere — use a float in [0.0, 1.0] with a step of 1.0 (values 0 or 1) and cast to bool in `processBlock()`. This is simpler and consistent.

```cpp
// In ParamLayout.cpp:
layout.add(std::make_unique<juce::AudioParameterBool>(
    juce::ParameterID{ ParamID::DOPPLER_ENABLED, 1 },
    "Doppler", true  // enabled by default
));
```

Using `AudioParameterBool` is cleaner — it renders as a toggle in the generic editor. Add `std::atomic<float>*` for it as usual (JUCE stores bool params as float 0/1 internally).

### Anti-Patterns to Avoid

- **DO NOT** add a separate `DistanceProcessor` class file. The Phase 3 pattern (chest/floor bounce as Engine.cpp members) works well and the planner should continue it. The codebase is small enough that separation into a new file adds overhead without benefit.
- **DO NOT** apply gain before the delay. The spec implies we hear a quieter, delayed version — the delay should be applied first, then gain. (Physically: signal travels, then arrives attenuated. Either order is mathematically equivalent for static distance, but for Doppler the delay modulation affects what signal is read, so gain-after-delay is cleaner.)
- **DO NOT** use a separate read pointer approach for Doppler. The `FractionalDelayLine` already handles fractional reads via Hermite interpolation. Just vary the `delayInSamples` argument to `read()` continuously via the smoother — that is all Doppler requires.
- **DO NOT** apply the air absorption LPF inside the delay line (in the feedback path). It goes on the output side. Air absorption is not a feedback phenomenon.
- **DO NOT** set `distDelaySmooth_` with the same short time constant as other smoothers (5-8ms). 300ms of range requires a longer ramp to avoid sounding like pitch shifting. 20-50ms is a good starting range.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Fractional delay with interpolation | Custom ring buffer | `FractionalDelayLine` (already exists) | Already implements Catmull-Rom cubic Hermite, power-of-2 bitmask, all edge cases handled |
| One-pole lowpass for air absorption | Direct coefficient computation | `OnePoleLP` (already exists) | Already handles `setCoefficients(cutoffHz, sampleRate)` API |
| Parameter smoothing | Custom ramp | `OnePoleSmooth` (already exists) | Already used for ITD, gains, comb wet — consistent codebase |
| Parameter registration | Manual APVTS construction | Extend existing `ParamLayout.cpp` | Pattern established in Phases 2 and 3 |

---

## Common Pitfalls

### Pitfall 1: Delay Line Read-Pointer Overrun During Doppler
**What goes wrong:** If the distance decreases very rapidly, the smoothed delay decreases toward 2.0f (minimum). If distance then increases again, the delay must increase. However, if the decrease happened faster than 1 sample per sample (impossible with a smoother), the read pointer could advance ahead of the write pointer. With `OnePoleSmooth`, the ramp rate is bounded by the time constant, so this cannot happen as long as the time constant is > 0. **The minimum delay guard (`std::max(2.0f, delaySamp)`) prevents the read position from entering the "future" region of the ring buffer.**

**Prevention:** Use `std::max(2.0f, delaySamp)` on the smoothed delay before passing to `read()`. This is already the established pattern for ITD, chest, and floor delays.

### Pitfall 2: 524KB Delay Buffer Allocation at Startup
**What goes wrong:** Sizing the delay line for 300ms at 192kHz allocates 65,536 samples * 2 ears * 4 bytes = ~512KB. This happens in `prepare()`, which is called on the audio thread in some DAWs. Allocation in `prepare()` is acceptable per JUCE convention (it is allowed there, just not in `process()`).

**Prevention:** This is fine — `prepare()` is the designated allocation point. The 512KB is a one-time cost per instance, not per block.

### Pitfall 3: getTailLengthSeconds() Not Updated
**What goes wrong:** With a 300ms max distance delay, the plugin's tail is now 300ms. If `getTailLengthSeconds()` returns 0, DAWs will cut off the tail during offline bounce. The distance delay itself may be carrying audio that the DAW truncates.

**Prevention:** Update `getTailLengthSeconds()` in `PluginProcessor.h` to return the maximum delay time:

```cpp
double getTailLengthSeconds() const override {
    // 300ms distance delay + existing phase contributions
    return 0.300 + 0.020;  // 300ms dist delay + 20ms floor bounce max
}
```

Or read from the APVTS parameter for distDelayMaxMs. Conservative: just return 1.0 (1 second) to ensure no tail truncation.

### Pitfall 4: Doppler Toggle Click
**What goes wrong:** When Doppler is toggled ON, the delay jumps from 2.0f (minimum — used while OFF) to whatever the current distance delay target is. If the source is far away, this is a sudden jump of potentially hundreds of milliseconds of delay, which causes a loud click or a whoosh of interpolated samples.

**Prevention:** When Doppler is toggled ON, initialize `distDelaySmooth_` to `2.0f` first, then let it ramp toward the target naturally. This ensures a smooth transition from "no delay" to "correct delay."

However — if the delay line was being continuously pushed with `dL/dR` during the OFF state (which the push-always pattern does), then the content at position `delaySamp` in the past already contains valid audio. The click comes not from the delay line content but from the abrupt change in read position. Using the smoother to ramp naturally from 2.0f solves this.

**Recommended approach:** On toggle ON, if `distDelaySmooth_`'s current value (its internal state `z_`) is at 2.0f (from the OFF state), it will naturally ramp up. This requires that the OFF state actually held the smoother at 2.0f — which it does if we process the smoother every sample even when OFF (but clamp the output). Alternatively, call `distDelaySmooth_.reset(2.0f)` when toggling ON.

### Pitfall 5: Distance at kMinDistance Producing Gain > 1.0
**What goes wrong:** If `distNorm` can be exactly `kMinDistance`, then `kMinDistance / distNorm = 1.0`. If `distNorm` is somehow less than `kMinDistance` (it cannot be — `computeDistance()` clamps to `kMinDistance`), gain would exceed 1.0. The `std::clamp(gainTarget, 0.0f, 1.0f)` guard prevents this.

**Prevention:** Always clamp: `gainTarget = std::clamp(kMinDistance / distNorm, 0.0f, 1.0f)`.

### Pitfall 6: Air Absorption LPF at High Sample Rates
**What goes wrong:** At 192kHz, the cutoff of 22000 Hz is only ~11% of Nyquist (88kHz). `OnePoleLP` computes `alpha = exp(-2 * pi * cutoffHz / sampleRate)`. At 192kHz with 22kHz cutoff: `exp(-0.72) ≈ 0.49` — still well within safe range. No instability concern.

**Prevention:** None needed — one-pole LP is unconditionally stable for all positive cutoff values. But document the expected behavior: at 192kHz with the "max" cutoff of 22kHz, the filter will roll off less aggressively than at 44.1kHz, which is physically correct (higher sample rates don't need more filtering; the cutoff is in Hz, not normalized).

---

## Code Examples

### Distance Processor Members in Engine.h

```cpp
// =========================================================================
// Phase 4: Distance Processing (DIST-01 through DIST-06)
// =========================================================================
dsp::FractionalDelayLine distDelayL_;  // propagation delay + Doppler, left ear
dsp::FractionalDelayLine distDelayR_;  // propagation delay + Doppler, right ear
dsp::OnePoleLP           airLPF_L_;   // air absorption LPF, left ear
dsp::OnePoleLP           airLPF_R_;   // air absorption LPF, right ear
dsp::OnePoleSmooth       distDelaySmooth_;  // smooth delay target (produces Doppler)
dsp::OnePoleSmooth       distGainSmooth_;   // smooth gain rolloff
```

### prepare() Additions in Engine.cpp

```cpp
// Phase 4: Distance delay lines
// Size for 300ms at max supported sample rate (192kHz) = 57,600 samples
// FractionalDelayLine rounds to next power-of-2 = 65,536 samples
// Memory: 65,536 * 4 bytes * 2 ears = ~512KB (pre-allocated, not audio-thread)
constexpr float kMaxSupportedSR = 192000.0f;
int distDelayCap = static_cast<int>(kDistDelayMaxMs * 0.001f * kMaxSupportedSR) + 8;
distDelayL_.prepare(distDelayCap);
distDelayR_.prepare(distDelayCap);
distDelayL_.reset();
distDelayR_.reset();

// Air absorption LPFs: initialized at maximum cutoff (no absorption at start)
airLPF_L_.setCoefficients(kAirAbsMaxHz, sr);
airLPF_R_.setCoefficients(kAirAbsMaxHz, sr);
airLPF_L_.reset();
airLPF_R_.reset();

// Smoothers: delay starts at minimum (2.0f samples), gain starts at max (1.0f)
distDelaySmooth_.prepare(kDistSmoothMs, sr);
distDelaySmooth_.reset(2.0f);  // 2.0f = minimum Hermite-safe delay
distGainSmooth_.prepare(kDefaultSmoothMs_Gain, sr);
distGainSmooth_.reset(1.0f);
```

### Per-Block Target Computation in Engine.cpp

```cpp
// Phase 4: per-block distance targets
// distNorm is already computed earlier in process() as dist

// Distance fraction in [0, 1]: 0 = closest, 1 = furthest
const float distFrac = (dist - kMinDistance) / (kSqrt3 - kMinDistance);

// Gain: inverse-square law, normalized so closest = unity
const float distGainTarget = std::clamp(kMinDistance / dist, 0.0f, 1.0f);

// Delay: 0ms at closest, distDelayMaxMs at furthest
const float delayTargetMs = distFrac * currentParams.distDelayMaxMs;
const float delayTargetSamples = std::max(2.0f,
    delayTargetMs * 0.001f * static_cast<float>(sampleRate));

// Air absorption cutoff: 22kHz at closest, 8kHz at furthest
const float airCutoffTarget = currentParams.airAbsMaxHz
    + (currentParams.airAbsMinHz - currentParams.airAbsMaxHz) * distFrac;

// Update LPF coefficients per-block only (setCoefficients() calls std::exp())
airLPF_L_.setCoefficients(airCutoffTarget, sr);
airLPF_R_.setCoefficients(airCutoffTarget, sr);

// Doppler enable: read once per block for branch prediction
const bool dopplerOn = currentParams.dopplerEnabled;
```

### Per-Sample Loop in Engine.cpp (distance section)

```cpp
// ----------------------------------------------------------------
// Phase 4: Distance Processing (DIST-01 through DIST-06)
// Runs after floor bounce, before outL/outR assignment.
// ----------------------------------------------------------------

// Gain attenuation (DIST-01): smooth then apply
const float distGain = distGainSmooth_.process(distGainTarget);
dL *= distGain;
dR *= distGain;

// Air absorption LPF (DIST-02): 6dB/oct, coefficients already set per-block
dL = airLPF_L_.process(dL);
dR = airLPF_R_.process(dR);

// Propagation delay + Doppler (DIST-03, DIST-04, DIST-05, DIST-06)
distDelayL_.push(dL);
distDelayR_.push(dR);

if (dopplerOn) {
    // Smooth the delay target — variation produces natural Doppler pitch shift (DIST-04)
    const float delaySamp = std::max(2.0f,
        distDelaySmooth_.process(delayTargetSamples));
    dL = distDelayL_.read(delaySamp);
    dR = distDelayR_.read(delaySamp);
} else {
    // Doppler OFF: read at minimum delay — no timing offset (DIST-05)
    // Smoother still processes to maintain correct state for when Doppler re-enables
    distDelaySmooth_.process(2.0f);  // ramp to min, keeping smoother state valid
    dL = distDelayL_.read(2.0f);
    dR = distDelayR_.read(2.0f);
}
```

### New Constants in Constants.h

```cpp
// ============================================================================
// Phase 4: Distance Processing
// ============================================================================

// Maximum propagation delay in milliseconds (DIST-03)
constexpr float kDistDelayMaxMs = 300.0f;

// Parameter smoother time constant for distance delay (controls Doppler feel)
// Longer than ITD (8ms) because 300ms range needs more ramp time to sound natural
constexpr float kDistSmoothMs = 30.0f;

// Air absorption LPF cutoff range (DIST-02)
// 22kHz at minimum distance (no absorption), 8kHz at maximum distance
constexpr float kAirAbsMaxHz = 22000.0f;
constexpr float kAirAbsMinHz = 8000.0f;
```

### New ParamIDs in ParamIDs.h

```cpp
// Phase 4: Distance Processing (DIST-07)
constexpr const char* DIST_DELAY_MAX_MS = "dist_delay_max_ms";
constexpr const char* DIST_SMOOTH_MS    = "dist_smooth_ms";
constexpr const char* DOPPLER_ENABLED   = "doppler_enabled";
constexpr const char* AIR_ABS_MAX_HZ    = "air_abs_max_hz";
constexpr const char* AIR_ABS_MIN_HZ    = "air_abs_min_hz";
```

### New ParamLayout Entries in ParamLayout.cpp

```cpp
// Phase 4: Distance processing tuning (DIST-07)
layout.add(std::make_unique<APF>(
    PID{ ParamID::DIST_DELAY_MAX_MS, 1 },
    "Dist Delay Max (ms)",
    NR(0.0f, 300.0f, 1.0f),
    xyzpan::kDistDelayMaxMs  // 300.0f
));

layout.add(std::make_unique<APF>(
    PID{ ParamID::DIST_SMOOTH_MS, 1 },
    "Dist Smooth (ms)",
    NR(1.0f, 200.0f, 1.0f),
    xyzpan::kDistSmoothMs   // 30.0f
));

layout.add(std::make_unique<juce::AudioParameterBool>(
    juce::ParameterID{ ParamID::DOPPLER_ENABLED, 1 },
    "Doppler",
    true  // enabled by default
));

layout.add(std::make_unique<APF>(
    PID{ ParamID::AIR_ABS_MAX_HZ, 1 },
    "Air Abs Max Hz",
    NR(1000.0f, 22000.0f, 100.0f),
    xyzpan::kAirAbsMaxHz  // 22000.0f
));

layout.add(std::make_unique<APF>(
    PID{ ParamID::AIR_ABS_MIN_HZ, 1 },
    "Air Abs Min Hz",
    NR(500.0f, 22000.0f, 100.0f),
    xyzpan::kAirAbsMinHz  // 8000.0f
));
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Separate Doppler DSP stage (pitch shifter) | Delay line modulation naturally produces pitch shift | Industry standard approach | Simpler, lower CPU, natural-sounding |
| dB/meter rolloff curves for distance | Inverse-square normalized to reference distance | Standard in spatial audio toolkits (3DTI, SAF) | Keeps gain at unity at reference distance |
| Fixed 44.1kHz delay line sizing | Size for max supported sample rate (192kHz) in prepare() | JUCE best practice since v6 | Zero reallocation when sample rate changes |
| Linear interpolation for delay modulation | Cubic Hermite (Catmull-Rom) interpolation | Established since codebase was created | Artifact-free Doppler modulation |

**Deprecated/outdated for this project:**
- Allpass interpolation for delay: Not used — Catmull-Rom already chosen and implemented
- Sinc interpolation: Too expensive per-sample; Catmull-Rom is sufficient for 300ms delay range

---

## Open Questions

1. **Order of gain vs. delay vs. LPF**
   - What we know: physically, signal travels through air (absorption + delay) and arrives attenuated
   - What's unclear: does applying gain before or after delay matter perceptually?
   - Recommendation: Apply gain FIRST (before delay), then delay, then LPF. The gain rolloff should not interact with the delay content. This means distant sources feed a quieter signal into the delay line, which is slightly more physically accurate.

2. **Doppler toggle: what happens to the delay line state on toggle OFF**
   - What we know: pushing into delay line even when reading at minimum keeps state valid
   - What's unclear: is there a click from the read position jump when toggling OFF?
   - Recommendation: Let `distDelaySmooth_` naturally ramp to 2.0f when OFF by continuing to process the smoother with target `2.0f` (not the actual distance delay). On toggle OFF event, no special handling needed — the smoother will ramp down over `kDistSmoothMs` milliseconds.

3. **Maximum pitch shift from Doppler at kDistSmoothMs = 30ms**
   - What we know: pitch shift = rate of delay change / sample period. If delay changes from 0 to 300ms over 30ms of smoother ramp, that is very fast movement and produces strong pitch shift.
   - What's unclear: what is the perceptually appropriate default for kDistSmoothMs?
   - Recommendation: Start with 30ms and expose via dev panel. The one-pole smoother's response means abrupt movements will produce a fast initial ramp (like Doppler) that tapers off. A time constant of 30ms means ~95% of the transition happens in ~90ms (3 time constants), which should sound natural for moderate source velocities.

---

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | Catch2 3.7+ |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `ctest --test-dir build --build-config Debug -R "Distance" --output-on-failure` |
| Full suite command | `ctest --test-dir build --build-config Debug --output-on-failure` |

### Phase Requirements -> Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| DIST-01 | Gain at far distance is significantly lower than near; doubling distance = -6dB | unit | `ctest --test-dir build -R "Distance" -VV` | No — Wave 0 |
| DIST-02 | LPF rolloff: high-frequency content attenuates more at far distances | unit (RMS comparison above/below cutoff) | `ctest --test-dir build -R "Distance" -VV` | No — Wave 0 |
| DIST-03 | Distance delay: impulse arrives N samples late, proportional to distance | unit (impulse timing) | `ctest --test-dir build -R "Distance" -VV` | No — Wave 0 |
| DIST-04 | Changing distance produces pitch shift (Doppler): output frequency differs from input when distance ramps | unit (frequency analysis of output) | `ctest --test-dir build -R "Distance" -VV` | No — Wave 0 |
| DIST-05 | Doppler OFF: no delay offset, no pitch shift | unit | `ctest --test-dir build -R "Distance" -VV` | No — Wave 0 |
| DIST-06 | Hermite interpolation: no clicks during modulation | unit (check output is finite, no NaN/Inf) | `ctest --test-dir build -R "Distance" -VV` | No — Wave 0 |
| DIST-07 | Dev panel params flow from EngineParams into DSP | unit (set param, verify effect) | `ctest --test-dir build -R "Distance" -VV` | No — Wave 0 |

### Sampling Rate
- **Per task commit:** `ctest --test-dir build --build-config Debug --output-on-failure`
- **Per wave merge:** `ctest --test-dir build --build-config Debug --output-on-failure`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps

- [ ] `tests/engine/TestDistanceProcessing.cpp` — covers DIST-01 through DIST-07
  - Inverse-square gain test: white noise at distance 0.1 vs 0.2 (double) — expect ~6dB difference
  - Air absorption test: RMS above/below 8kHz at far distance vs near distance
  - Delay timing test: impulse input, measure output peak position
  - Doppler test: linearly ramping distance, verify output pitch shifts (complex — may use simpler proxy: output is not identical to input when distance changes)
  - Toggle test: Doppler OFF produces no delay offset (verify impulse arrives at sample 2+epsilon, not 300ms later)
  - Finite output test: stress test with extreme params, verify no NaN/Inf

---

## Sources

### Primary (HIGH confidence)
- Codebase analysis: `engine/include/xyzpan/dsp/FractionalDelayLine.h` — Catmull-Rom interpolation implementation
- Codebase analysis: `engine/src/Engine.cpp` — established patterns for per-block targets, per-sample smoothers, minimum delay guard
- Codebase analysis: `engine/include/xyzpan/Constants.h` — existing constants and naming conventions
- Codebase analysis: `plugin/ParamIDs.h`, `plugin/ParamLayout.cpp`, `plugin/PluginProcessor.cpp` — 4-file extension pattern
- `.planning/research/ARCHITECTURE.md` — sample-rate-aware DSP, prepare() allocation pattern (section 4.4)
- `.planning/research/PITFALLS.md` — delay interpolation artifacts, Doppler smoothing (Pitfall 2)

### Secondary (MEDIUM confidence)
- Inverse-square law for sound: standard acoustics — 6dB per distance doubling for amplitude
- Air absorption frequency model: 8kHz cutoff at maximum distance is physically plausible (air absorbs ~2dB/100m at 1kHz, more at high frequencies) — the exact cutoff value is a creative/perceptual choice

### Tertiary (LOW confidence)
- Default kDistSmoothMs = 30ms: educated estimate based on ITD smoother (8ms) and the larger delay range; validate by ear during dev panel tuning

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all components exist in codebase, patterns are established
- Architecture: HIGH — directly extends Phase 3 pattern with no new concepts
- Pitfalls: HIGH — delay modulation, Doppler, and minimum delay guard all documented in existing pitfalls research and confirmed in codebase (ITD delay uses same guard)

**Research date:** 2026-03-12
**Valid until:** Stable — no external dependencies; all primitives are in-codebase
