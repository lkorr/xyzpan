# Phase 5: Creative Tools - Research

**Researched:** 2026-03-13
**Domain:** Algorithmic reverb (FDN/Schroeder) + per-axis LFO modulation in a hand-rolled C++ DSP engine
**Confidence:** HIGH

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| VERB-01 | Algorithmic reverb applied at end of signal chain | FDN reverb integrated as final stereo stage in Engine::process(); appended after DIST section |
| VERB-02 | Pre-delay scales with distance: 0ms (closest) to 50ms (furthest) | Pre-delay tapped from FractionalDelayLine before reverb diffuser; distFrac drives read position |
| VERB-03 | Reverb parameters (size, decay, damping, wet/dry) exposed as plugin parameters | EngineParams fields + ParamIDs constants + ParamLayout entries + PluginProcessor wiring, matching prior-phase pattern |
| VERB-04 | Reverb is sparse and mix-friendly (not convolution) | Feedback Delay Network (FDN) or Schroeder topology — both are algorithmic; FDN with 4-8 delays is the established sparse choice |
| LFO-01 | One LFO per axis (X, Y, Z) — 3 total | Three independent LFO instances in engine; tick per sample before coordinate conversion |
| LFO-02 | Each LFO has selectable waveform: sine, triangle, saw, square | Phase accumulator drives waveform selector; all four are trivial math, no lookup table needed |
| LFO-03 | Each LFO has rate, depth, and phase controls | Rate sets accumulator increment; depth is output scale; phase is accumulator start offset |
| LFO-04 | LFOs modulate position offset (add/subtract around fixed position) | modX = baseX + lfo[0].tick() * depthX; coordinate clamping to [-1, 1] prevents instability |
| LFO-05 | LFO rate syncs to host tempo when tempo sync is enabled | juce::AudioPlayHead::PositionInfo::bpm read in processBlock(); BPM passed into EngineParams |
</phase_requirements>

---

## Summary

Phase 5 adds two independent DSP subsystems to the existing engine: an algorithmic reverb as the final stage of the signal chain, and a per-axis LFO system that modulates X/Y/Z coordinates before coordinate conversion. Both are hand-rolled pure C++ with no JUCE dependency in the engine layer, consistent with all prior phases.

The reverb is the more complex of the two. A Feedback Delay Network (FDN) with 4-8 delays is the right topology for "sparse and mix-friendly" (VERB-04). Schroeder networks are simpler but tend to be phasier and more metallic. The FDN naturally produces diffuse late-field energy without washy density buildup. Pre-delay (VERB-02) is trivially implemented by tapping the existing `FractionalDelayLine` infrastructure before the reverb diffuser.

The LFO system is straightforward: a phase accumulator pattern per axis, four waveforms computed analytically (no lookup table), optional phase offset, and host tempo sync via `juce::AudioPlayHead`. The LFO ticks per sample and its output is added to the base X/Y/Z position before coordinate conversion — this is already diagrammed in ARCHITECTURE.md. The APVTS wiring follows the same pattern used in every prior phase.

**Primary recommendation:** Implement a 4-delay FDN reverb with a pre-delay line and a Moorer-style damping lowpass in each feedback loop. LFO uses a phase accumulator with per-waveform branching and a float-cast waveform selector stored in EngineParams.

---

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++20 (pure) | — | Engine DSP code | Established constraint: no JUCE headers in engine/ |
| JUCE APVTS | 8.0.12 | Parameter wiring in plugin layer | Consistent with Phase 2-4 wiring pattern |
| FractionalDelayLine (existing) | — | Pre-delay for reverb; LFO delay lines | Already implemented with Hermite interpolation |
| OnePoleSmooth (existing) | — | Smooth reverb wet/dry and LFO depth transitions | Already implemented with prepare()/reset()/process() API |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| juce::AudioPlayHead | 8.0.12 | Read host BPM for tempo sync | Only for LFO-05: tempo-sync path in processBlock() |
| std::cmath | — | sin/cos/fabs for LFO waveforms | Zero new dependencies |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| FDN reverb | Schroeder network (allpass chain + comb) | Schroeder is simpler code but produces metallic, phasier sound; FDN is the modern standard for quality |
| FDN reverb | JUCE dsp::Reverb | JUCE Reverb is Freeverb clone (Schroeder), would introduce JUCE dependency in engine, breaks engine isolation contract |
| Analytic waveforms | Lookup table for LFO | Lookup table adds memory and init complexity; sin/cos at LFO rates (<100 Hz) is negligible CPU |

**Installation:** No new packages. All DSP is hand-rolled in engine/src/.

---

## Architecture Patterns

### Signal Chain Position

Reverb slots in as the final stereo stage, after distance processing:

```
[LFO System] -> modX, modY, modZ
[Coordinate Converter]
[Binaural Panner]
[Depth Comb Filters]
[Elevation Filters]
[Distance Processor]
[Reverb]              <-- NEW: Phase 5
Stereo Out (L/R)
```

LFO ticks at the TOP of the per-sample loop, before coordinate conversion:

```cpp
// At top of per-sample loop:
float modX = baseXSmooth_.getNext() + lfo_[0].tick() * lfoDepthX_.getNext();
float modY = baseYSmooth_.getNext() + lfo_[1].tick() * lfoDepthY_.getNext();
float modZ = baseZSmooth_.getNext() + lfo_[2].tick() * lfoDepthZ_.getNext();
// Clamp to prevent coordinate instability
modX = std::clamp(modX, -1.0f, 1.0f);
modY = std::clamp(modY, -1.0f, 1.0f);
modZ = std::clamp(modZ, -1.0f, 1.0f);
auto sph = convertCoordinates(modX, modY, modZ);
// ... rest of chain uses sph
```

### Recommended Project Structure (additions for Phase 5)

```
engine/include/xyzpan/
├── dsp/
│   ├── [existing files unchanged]
│   ├── FDNReverb.h           # NEW: Feedback Delay Network reverb
│   └── LFO.h                 # NEW: Per-axis LFO with phase accumulator
engine/src/
├── [existing files unchanged]
├── FDNReverb.cpp             # NEW
└── LFO.cpp                   # NEW (or inline in .h if short)
plugin/
├── ParamIDs.h                # EXTEND: reverb + LFO param ID constants
├── ParamLayout.cpp           # EXTEND: add reverb + LFO parameters
├── PluginProcessor.h         # EXTEND: add atomic<float>* pointers
└── PluginProcessor.cpp       # EXTEND: snapshot and pass to engine
engine/include/xyzpan/
├── Types.h                   # EXTEND: EngineParams with reverb + LFO fields
└── Constants.h               # EXTEND: default reverb sizes, LFO rate range
tests/engine/
└── TestCreativeTools.cpp     # NEW: Catch2 tests for VERB-* and LFO-*
```

### Pattern 1: FDN Reverb — Feedback Delay Network

**What:** N mutually coupled delay lines (typically 4 or 8) with a unitary feedback matrix. Each delay line's output is fed back through the matrix into all delay lines. A damping lowpass in each feedback loop controls high-frequency decay. Wet/dry mix controlled globally.

**Why 4-delay FDN for this use case:** 4 delays gives sufficient diffusion without excessive CPU. 8 delays gives smoother late tail but doubles the delay memory. For a spatial panner where reverb is a supporting effect (not the main event), 4 delays hits the right balance.

**Delay lengths:** Use mutually prime lengths (no common factors) to avoid combing. Good set at 44100 Hz: {1367, 1871, 2293, 2797} samples (approximately 31ms, 42ms, 52ms, 63ms). Scale by sampleRate/44100 in prepare().

**Feedback matrix:** Householder reflection matrix is simplest to implement correctly:
```
H = I - (2/N) * ones_matrix
```
For N=4: each output = 0.5 * (all inputs) - input_i. Sum of all inputs times 0.5, minus own input.

**Damping:** One-pole lowpass in each feedback loop. Pole position controlled by `damping` parameter (0 = no damping, 1 = heavy damping). `y = (1-d)*x + d*y_prev` where d is the damping coefficient.

**Pre-delay:** A `FractionalDelayLine` before the FDN input. Length driven by `distFrac * verbPreDelayMaxMs`:

```cpp
// In Engine::process() per-sample loop, after distance processing:
preDelay_.push((dL + dR) * 0.5f);  // push mono sum
float preDOut = preDelay_.read(preDelaySamp_);  // preSamp drives pre-delay length
// Feed preDOut into FDN, get fdnL, fdnR out
// Mix: outL = dL + wetGain * fdnL; outR = dR + wetGain * fdnR
```

**Example FDN structure:**
```cpp
// engine/include/xyzpan/dsp/FDNReverb.h
class FDNReverb {
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    // Process one stereo sample through the reverb.
    // preDelaySamp: fractional delay for pre-delay line (distance-scaled)
    // Returns {wetL, wetR} — caller mixes with dry signal.
    void processSample(float inL, float inR,
                       float preDelaySamp,
                       float& wetL, float& wetR);

    void setSize(float sizeNorm);    // [0,1] -> scales all delay lengths
    void setDecay(float decayNorm);  // [0,1] -> T60 scaling on feedback gain
    void setDamping(float d);        // [0,1] -> pole position in damping LP
    void setWetDry(float wet);       // [0,1] mix (used by caller or internal)

private:
    static constexpr int kNumDelays = 4;
    FractionalDelayLine preDelayLine_;
    std::array<FractionalDelayLine, kNumDelays> delays_;
    std::array<float, kNumDelays> delayLengths_;  // in samples, scaled by size
    std::array<float, kNumDelays> dampState_;     // one-pole LP state per delay
    float feedbackGain_ = 0.0f;  // per-delay feedback scalar (set from decay)
    float damping_      = 0.0f;
    OnePoleSmooth wetSmooth_;
    double sampleRate_  = 44100.0;
};
```

### Pattern 2: LFO — Phase Accumulator

**What:** A running phase value in [0, 1) incremented by `freq/sampleRate` each sample. Waveform shapes are computed analytically from phase.

**Why phase accumulator:** Zero memory (single float per LFO), perfectly band-limited for sine/triangle, no aliasing issues at audio-rate LFO frequencies (max ~50 Hz for creative motion), easily synced to tempo.

**Waveforms (all from phase in [0,1)):**
```cpp
float sine     = std::sin(phase * 2.0f * 3.14159265f);
float triangle = 1.0f - 4.0f * std::abs(phase - 0.5f);
float saw      = 2.0f * phase - 1.0f;
float square   = phase < 0.5f ? 1.0f : -1.0f;
```

**Waveform selector:** Store as integer in EngineParams (0=sine, 1=triangle, 2=saw, 3=square). APVTS uses `AudioParameterChoice` or `AudioParameterFloat` with integer values.

**Phase offset:** The `phase` parameter offsets the accumulator start. Set once in prepare() or reset() from the phase param. Phase init: `accumulator_ = phaseOffset_` when reset is called or first tick.

**Tempo sync:** When enabled, rate is computed from BPM. BPM is passed in via EngineParams (read from playHead in processBlock). Beat subdivision is also a parameter (1/1, 1/2, 1/4, 1/8, etc.).

```cpp
// In processBlock() before engine.setParams():
if (auto* playHead = getPlayHead()) {
    if (auto pos = playHead->getPosition()) {
        if (pos->getBpm().hasValue())
            params.hostBpm = static_cast<float>(*pos->getBpm());
        params.isPlaying = pos->getIsPlaying();
    }
}

// In Engine or LFO, when tempo sync enabled:
// rate_hz = (hostBpm / 60.0f) * beatDivision
// beatDivision: 1.0 = quarter note, 0.5 = eighth, 2.0 = half, etc.
float rateHz = (hostBpm / 60.0f) * beatDivision;
float increment = rateHz / sampleRate;
```

**Example LFO class:**
```cpp
// engine/include/xyzpan/dsp/LFO.h
namespace xyzpan::dsp {

enum class LFOWaveform { Sine = 0, Triangle, Saw, Square };

class LFO {
public:
    void prepare(double sampleRate);
    void reset(float phaseOffsetNorm = 0.0f);  // resets accumulator to phaseOffset

    // Set rate directly in Hz (free-running mode)
    void setRateHz(float hz);

    // Tick one sample; returns value in [-1, 1]
    float tick();

    LFOWaveform waveform = LFOWaveform::Sine;

private:
    float accumulator_ = 0.0f;   // phase in [0, 1)
    float increment_   = 0.0f;   // freq / sampleRate
    double sampleRate_ = 44100.0;
};

} // namespace xyzpan::dsp
```

**Per-axis integration in Engine:**
```cpp
// Three LFO instances in Engine.h (no LFOSystem wrapper needed — just 3 flat members):
dsp::LFO lfoX_, lfoY_, lfoZ_;
// Plus smoothers for depth (so depth changes don't click):
dsp::OnePoleSmooth lfoDepthXSmooth_, lfoDepthYSmooth_, lfoDepthZSmooth_;
```

### Pattern 3: EngineParams Extension (established pattern)

Following the exact same pattern as prior phases:

```cpp
// In Types.h (additions to EngineParams):

// =========================================================================
// Phase 5: Reverb (VERB-01 through VERB-04)
// =========================================================================
float verbSize        = kVerbDefaultSize;        // [0,1] — room size
float verbDecay       = kVerbDefaultDecay;       // [0,1] — T60 in seconds (mapped)
float verbDamping     = kVerbDefaultDamping;     // [0,1] — HF damping
float verbWet         = kVerbDefaultWet;         // [0,1] — wet/dry ratio
float verbPreDelayMax = kVerbPreDelayMaxMs;      // ms — max pre-delay (at max distance)

// =========================================================================
// Phase 5: LFO (LFO-01 through LFO-05)
// =========================================================================
// Per-axis: rate (Hz), depth (position units), phase offset (normalized 0-1),
// waveform (int: 0=sine 1=triangle 2=saw 3=square), enabled flag.
float lfoXRate       = kLFODefaultRate;   float lfoYRate       = kLFODefaultRate;   float lfoZRate       = kLFODefaultRate;
float lfoXDepth      = 0.0f;              float lfoYDepth      = 0.0f;              float lfoZDepth      = 0.0f;
float lfoXPhase      = 0.0f;              float lfoYPhase      = 0.0f;              float lfoZPhase      = 0.0f;
int   lfoXWaveform   = 0;                 int   lfoYWaveform   = 0;                 int   lfoZWaveform   = 0;
// Tempo sync
bool  lfoTempoSync   = false;
float hostBpm        = 120.0f;            // passed from processBlock
float lfoXBeatDiv    = 1.0f;             float lfoYBeatDiv    = 1.0f;             float lfoZBeatDiv    = 1.0f;
```

### Pattern 4: Waveform Selector as AudioParameterFloat

JUCE's `AudioParameterChoice` introduces a different parameter type. Prior phases used only `AudioParameterFloat` and `AudioParameterBool`. For consistency, use `AudioParameterFloat` with integer snapping for waveform (range 0-3, step 1). Cast with `static_cast<int>(std::round(...))` in processBlock.

```cpp
// ParamLayout.cpp:
layout.add(std::make_unique<APF>(
    PID{ ParamID::LFO_X_WAVEFORM, 1 },
    "LFO X Waveform",
    NR(0.0f, 3.0f, 1.0f),  // step=1 snaps to integer values
    0.0f  // default: sine
));

// processBlock.cpp:
params.lfoXWaveform = static_cast<int>(std::round(lfoXWaveformParam->load()));
```

Alternatively, `AudioParameterChoice` is cleaner semantically and allowed by prior-phase precedent for `AudioParameterBool`. Either works — the integer-float approach avoids a new parameter type.

### Pattern 5: getTailLengthSeconds Update

The reverb produces a tail after the signal ends. `getTailLengthSeconds()` must be updated again. Current value is 0.320 (300ms distance delay + 20ms floor bounce). Add reverb decay:

```cpp
double getTailLengthSeconds() const override {
    // Prior phases: 300ms distance delay + 20ms floor bounce = 320ms
    // Phase 5 reverb: add T60 (up to ~5 seconds at decay=1.0) + pre-delay max (50ms)
    return 5.350;  // 0.320 + 5.0 max decay + 0.050 pre-delay max, rounded up
}
```

Note: The actual value depends on the maximum configurable decay. Use the dev panel decay range upper limit.

### Anti-Patterns to Avoid

- **Don't use JUCE dsp::Reverb:** Freeverb implementation — pulls JUCE into engine layer, breaks the no-JUCE-in-engine contract.
- **Don't allocate in process():** All FDN delay lines must be sized in prepare(). Size for worst-case sample rate (192kHz) if delay lengths are not sample-rate scaled.
- **Don't update FDN delay lengths per-sample:** The `size` parameter controls delay length. Changing delay length mid-playback causes pitch artifacts (same as the delay line doppler effect). Either keep delay lengths fixed after prepare() and only scale feedback gain, or implement smooth length transitions. Simplest: only allow size to affect feedback gain, not actual delay lengths. Delay lengths set once in prepare() and never changed.
- **Don't skip clamping LFO output:** LFO depth can push modX/modY/modZ outside [-1, 1]. Always clamp before coordinate conversion to prevent unstable distance computation (division by near-zero).
- **Don't reset LFO phase on every processBlock:** LFO phase must advance continuously across blocks. Only reset in Engine::reset() or when the user explicitly changes the phase parameter.
- **Don't read hostBpm without checking isPlaying:** Some hosts report BPM even when stopped, which is fine. But isPlaying should gate whether tempo sync should advance the accumulator — if not playing and tempo sync is on, freeze the accumulator or let it free-run (creative choice: document which).

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Hermite-interpolated pre-delay line | New delay line class | Existing `FractionalDelayLine` | Already implemented, tested, proven in Phases 2-4 |
| Parameter smoothing | New smoother | Existing `OnePoleSmooth` | Already implemented, exact API: prepare(ms, sr), reset(val), process(target) |
| Sample-rate-agnostic filter state | New LP filter | Existing `OnePoleLP` | Already implemented for air absorption |
| Thread-safe parameter bridge | Custom atomics | APVTS atomic pointers (existing pattern) | 4 prior phases have established the exact pattern |

**Key insight:** The Phase 5 DSP classes (`FDNReverb`, `LFO`) are new files, but they are assembled entirely from patterns and primitives already proven in prior phases. No new DSP infrastructure needs to be invented.

---

## Common Pitfalls

### Pitfall 1: FDN Feedback Matrix Instability
**What goes wrong:** Feedback gain exceeds 1.0 due to the Householder matrix summing multiple delay outputs, causing exponential energy growth and clipping/silence from denormalization.
**Why it happens:** The Householder matrix for N=4 has values of ±0.5. Combined with the per-delay feedback gain, the total loop gain exceeds 1.0 if the feedback scalar isn't set correctly.
**How to avoid:** The feedback scalar (applied per-delay to control T60) must satisfy `|feedbackGain * householder_eigenvalue| < 1`. For Householder, eigenvalues are ±1 and -1/(N-1). Keep feedbackGain < 1.0. For decay control, map decay parameter to feedbackGain via: `feedbackGain = pow(10.0f, -3.0f * maxDelayMs / (1000.0f * decayT60))`. Hard-clamp to 0.999.
**Warning signs:** Output grows unboundedly or becomes NaN within seconds of enabling reverb.

### Pitfall 2: FDN Delay Line Sizing for Sample Rate Changes
**What goes wrong:** FDN delay lines sized for 44100 Hz are too short at 96kHz or 192kHz. prepareToPlay() is called on sample rate changes, which must reallocate the delay lines.
**Why it happens:** Delay lengths in samples scale linearly with sample rate. If sized for 44100 Hz, a 44-sample delay is ~1ms; at 96kHz the same 1ms requires ~96 samples.
**How to avoid:** Size delay lines based on `delayLengthMs * sampleRate / 1000.0f`. Round up and add 4 for Hermite headroom. Or use the worst-case 192kHz sizing upfront (larger memory, avoids reallocation). The distance delay lines already use this 192kHz worst-case approach.

### Pitfall 3: LFO Phase Discontinuity When Rate Changes
**What goes wrong:** Changing LFO rate causes an audible click because the accumulator increment changes abruptly.
**Why it happens:** At the new rate, the next tick produces a very different value than the current phase would suggest.
**How to avoid:** The rate change only affects the increment; the accumulator continues from its current phase. This is naturally click-free with the phase accumulator pattern — just update the increment and let the phase evolve. No crossfade or smoothing needed for rate changes. The output continuity is preserved because the waveform is continuous and the phase doesn't jump.

### Pitfall 4: Tempo Sync Not Available at Plugin Load
**What goes wrong:** hostBpm defaults to 0 or garbage when the DAW hasn't started transport or hasn't provided BPM. LFO rate computation produces NaN or 0 Hz.
**Why it happens:** `getPlayHead()` may return nullptr, or `getPosition()->getBpm()` may return `std::nullopt` (it returns `std::optional<double>`).
**How to avoid:** Default `hostBpm` to 120.0f in EngineParams. Check that `pos->getBpm().hasValue()` before reading. Use the free-running rate as fallback when tempo sync is on but BPM is not available:
```cpp
if (auto* ph = getPlayHead())
    if (auto pos = ph->getPosition())
        if (pos->getBpm().hasValue())
            params.hostBpm = static_cast<float>(*pos->getBpm());
// else: params.hostBpm stays at its previous/default value
```

### Pitfall 5: Reverb Tail in getTailLengthSeconds
**What goes wrong:** DAW truncates reverb tail on track end — last notes sound unnaturally chopped.
**Why it happens:** DAW uses `getTailLengthSeconds()` to determine how long to keep calling processBlock after the input silence. If the return value is too short, the reverb tail is cut off.
**How to avoid:** Return the maximum possible decay time (max of the dev panel decay range) plus pre-delay max plus some margin. Update this in plan 05-02 (APVTS wiring), same as getTailLengthSeconds was updated in plan 04-02.

### Pitfall 6: Waveform Selector Causes Click During Change
**What goes wrong:** Switching waveform during playback causes a discontinuous jump in LFO output.
**Why it happens:** At the moment of waveform switch, the current phase position may produce a very different value for the new waveform (e.g., sine near 0.5 produces ~1.0; switching to square at phase 0.5 still produces 1.0 but transition mid-period may be jarring).
**How to avoid:** Waveform changes happen between blocks (param snapshot at block start), so the discontinuity is at most one block boundary. This is acceptable for an LFO — no crossfade needed. Document this as expected behavior.

---

## Code Examples

### FDN Reverb — Householder Feedback (4-channel)

```cpp
// Source: established FDN reverb pattern (Smith, Schlecht, Valimaki literature)
// In FDNReverb::processSample():

// Read from each delay line
float x[4];
for (int i = 0; i < 4; ++i)
    x[i] = delays_[i].read(delayLengths_[i]);

// Apply Householder reflection: y_i = sum(x) * 0.5f - x_i
// (for N=4, factor = 2/N = 0.5)
float sum = x[0] + x[1] + x[2] + x[3];
float fb[4];
for (int i = 0; i < 4; ++i) {
    fb[i] = sum * 0.5f - x[i];
    // Apply damping lowpass: y = (1-d)*fb + d*dampState
    dampState_[i] = (1.0f - damping_) * fb[i] + damping_ * dampState_[i];
    // Scale by feedback gain (decay control), push back
    delays_[i].push(inMono + dampState_[i] * feedbackGain_);
}

// Stereo output: mix two pairs of delays
wetL = (x[0] + x[1]) * 0.5f;
wetR = (x[2] + x[3]) * 0.5f;
```

### LFO Phase Accumulator Tick

```cpp
// Source: standard audio DSP — phase accumulator pattern
float LFO::tick() {
    float out;
    switch (waveform) {
        case LFOWaveform::Sine:
            out = std::sin(accumulator_ * 6.28318530f);
            break;
        case LFOWaveform::Triangle:
            out = 1.0f - 4.0f * std::abs(accumulator_ - 0.5f);
            break;
        case LFOWaveform::Saw:
            out = 2.0f * accumulator_ - 1.0f;
            break;
        case LFOWaveform::Square:
            out = accumulator_ < 0.5f ? 1.0f : -1.0f;
            break;
        default:
            out = 0.0f;
    }
    accumulator_ += increment_;
    if (accumulator_ >= 1.0f) accumulator_ -= 1.0f;
    return out;
}
```

### Tempo Sync Rate Computation

```cpp
// Source: JUCE AudioPlayHead docs (8.0.x)
// In PluginProcessor::processBlock():
if (auto* ph = getPlayHead()) {
    if (auto pos = ph->getPosition()) {
        if (pos->getBpm().hasValue())
            params.hostBpm = static_cast<float>(*pos->getBpm());
    }
}

// In Engine, for LFO rate when tempoSync is true:
// lfoXBeatDiv: 1.0 = quarter note, 2.0 = half note, 0.5 = eighth note
float rateHz = (hostBpm / 60.0f) * lfoXBeatDiv;
lfoX_.setRateHz(rateHz);
```

### APVTS Parameter IDs for Phase 5

```cpp
// ParamIDs.h additions (following existing pattern):
namespace ParamID {
    // Reverb (Phase 5) — VERB-03
    constexpr const char* VERB_SIZE        = "verb_size";
    constexpr const char* VERB_DECAY       = "verb_decay";
    constexpr const char* VERB_DAMPING     = "verb_damping";
    constexpr const char* VERB_WET         = "verb_wet";
    constexpr const char* VERB_PRE_DELAY   = "verb_pre_delay";  // max pre-delay ms

    // LFO X (Phase 5) — LFO-01 through LFO-05
    constexpr const char* LFO_X_RATE      = "lfo_x_rate";
    constexpr const char* LFO_X_DEPTH     = "lfo_x_depth";
    constexpr const char* LFO_X_PHASE     = "lfo_x_phase";
    constexpr const char* LFO_X_WAVEFORM  = "lfo_x_waveform";
    // LFO Y
    constexpr const char* LFO_Y_RATE      = "lfo_y_rate";
    constexpr const char* LFO_Y_DEPTH     = "lfo_y_depth";
    constexpr const char* LFO_Y_PHASE     = "lfo_y_phase";
    constexpr const char* LFO_Y_WAVEFORM  = "lfo_y_waveform";
    // LFO Z
    constexpr const char* LFO_Z_RATE      = "lfo_z_rate";
    constexpr const char* LFO_Z_DEPTH     = "lfo_z_depth";
    constexpr const char* LFO_Z_PHASE     = "lfo_z_phase";
    constexpr const char* LFO_Z_WAVEFORM  = "lfo_z_waveform";
    // Tempo sync (shared)
    constexpr const char* LFO_TEMPO_SYNC  = "lfo_tempo_sync";
    // Beat divisions per axis (when tempo sync enabled)
    constexpr const char* LFO_X_BEAT_DIV  = "lfo_x_beat_div";
    constexpr const char* LFO_Y_BEAT_DIV  = "lfo_y_beat_div";
    constexpr const char* LFO_Z_BEAT_DIV  = "lfo_z_beat_div";
}
```

### Constants.h Additions

```cpp
// engine/include/xyzpan/Constants.h additions:

// ============================================================================
// Phase 5: Reverb
// ============================================================================
constexpr float kVerbDefaultSize      = 0.5f;    // normalized room size
constexpr float kVerbDefaultDecay     = 0.5f;    // normalized decay (~2s T60)
constexpr float kVerbDefaultDamping   = 0.5f;    // normalized damping
constexpr float kVerbDefaultWet       = 0.0f;    // reverb off by default
constexpr float kVerbPreDelayMaxMs    = 50.0f;   // max pre-delay at max distance
constexpr float kVerbMaxDecayT60_s    = 5.0f;    // maximum T60 at decay=1.0

// FDN delay lengths (prime-like, at 44100 Hz — scaled in prepare())
constexpr float kFDNDelayMs[4]        = { 30.98f, 42.40f, 51.95f, 63.45f };

// ============================================================================
// Phase 5: LFO
// ============================================================================
constexpr float kLFODefaultRate       = 0.5f;    // Hz, free-running default
constexpr float kLFOMinRate           = 0.01f;   // Hz minimum (very slow)
constexpr float kLFOMaxRate           = 50.0f;   // Hz maximum
constexpr float kLFOMaxDepth          = 1.0f;    // max depth = full axis range
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Schroeder network (allpass + comb chains) | FDN with unitary feedback matrix | Mid-1990s (Jot 1991) | Denser, smoother late tail; less metallic character |
| Fixed LFO rates | Tempo-synced LFOs with beat subdivision | Standard since VST2 era | Required for rhythmic spatial effects in DAW context |
| LFO with lookup table | Phase accumulator + analytic waveforms | 1980s embedded DSP | No memory overhead, perfectly continuous, no aliasing at sub-audio rates |

**Deprecated/outdated:**
- JUCE `dsp::Reverb`: Freeverb (Schroeder-based, 1999). Sounds dated. Not usable here anyway (engine isolation).
- Convolution reverb for room simulation: Explicitly excluded by REQUIREMENTS.md (Out of Scope).

---

## Open Questions

1. **Beat division options for tempo sync**
   - What we know: LFO-05 says "syncs to host tempo" but doesn't specify what subdivisions are available
   - What's unclear: should the planner expose 1/16, 1/8, 1/4, 1/2, 1/1, 2/1 as options, or just a ratio?
   - Recommendation: Use a float multiplier (0.25 = 1/4 note, 0.5 = 1/2, 1.0 = quarter, 2.0 = half, 4.0 = whole). Store as `AudioParameterFloat` with sensible snap values. This gives flexibility without a Choice parameter type.

2. **FDN stereo input vs mono sum**
   - What we know: The signal entering reverb is stereo (L/R after distance processing)
   - What's unclear: should the FDN have 4 stereo delay lines (8 total) or take a mono sum and decorrelate output?
   - Recommendation: Sum L+R to mono before FDN input; decorrelate output by mixing delay outputs differently for L vs R (x[0]+x[1] for L, x[2]+x[3] for R). This gives realistic stereo spread without doubling complexity.

3. **LFO depth parameter range**
   - What we know: depth scales the LFO output before adding to base position; base positions are in [-1, 1]
   - What's unclear: should depth=1.0 mean the LFO can swing the full [-1, 1] range (aggressive) or something more musical like 0.5?
   - Recommendation: depth range [0, 1] where 1.0 = full ±1 swing. Let users decide via the parameter. Clamp the final modX/modY/modZ to [-1, 1] regardless.

---

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Catch2 3.7+ |
| Config file | tests/CMakeLists.txt (existing) |
| Quick run command | `ctest --test-dir build -R "creative" -C Release` |
| Full suite command | `ctest --test-dir build -C Release` |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| VERB-01 | Reverb stage present at end of chain; output differs from input | integration | `ctest --test-dir build -R "VERB-01" -C Release` | ❌ Wave 0 |
| VERB-02 | Pre-delay increases with distance (far = longer pre-delay) | integration | `ctest --test-dir build -R "VERB-02" -C Release` | ❌ Wave 0 |
| VERB-03 | Reverb params exposed in APVTS; changing wet/dry has audible effect | integration | `ctest --test-dir build -R "VERB-03" -C Release` | ❌ Wave 0 |
| VERB-04 | Reverb is stable (no NaN/Inf); no washy buildup at decay=1.0 | integration | `ctest --test-dir build -R "VERB-04" -C Release` | ❌ Wave 0 |
| LFO-01 | Three LFO instances exist; each modulates its axis independently | unit | `ctest --test-dir build -R "LFO-01" -C Release` | ❌ Wave 0 |
| LFO-02 | All four waveforms produce distinct output at same phase/rate | unit | `ctest --test-dir build -R "LFO-02" -C Release` | ❌ Wave 0 |
| LFO-03 | Rate, depth, phase controls all affect LFO output as expected | unit | `ctest --test-dir build -R "LFO-03" -C Release` | ❌ Wave 0 |
| LFO-04 | LFO modulates position: X LFO with sine causes oscillating panning | integration | `ctest --test-dir build -R "LFO-04" -C Release` | ❌ Wave 0 |
| LFO-05 | Tempo sync: at BPM=120, rate=1.0 beat, LFO completes cycle in 0.5s | unit | `ctest --test-dir build -R "LFO-05" -C Release` | ❌ Wave 0 |

### Sampling Rate
- **Per task commit:** `ctest --test-dir build -R "creative" -C Release`
- **Per wave merge:** `ctest --test-dir build -C Release` (full suite)
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/engine/TestCreativeTools.cpp` — all VERB-* and LFO-* test cases
- [ ] No new fixture files needed — existing settleAndProcess() helper pattern from TestDistanceProcessing.cpp is sufficient

---

## Sources

### Primary (HIGH confidence)
- Codebase: Engine.h, Engine.cpp, Types.h, Constants.h, ParamIDs.h, ParamLayout.cpp, PluginProcessor.cpp — direct inspection; all patterns verified
- Codebase: FractionalDelayLine.h, OnePoleSmooth.h, OnePoleLP.h — verified available and suitable for Phase 5 reuse
- Codebase: TestDistanceProcessing.cpp — established test helper patterns (settleAndProcess, makeNoise, makeSine)
- ARCHITECTURE.md (project research) — LFO architecture and signal chain position already specified

### Secondary (MEDIUM confidence)
- Julius O. Smith III, "Physical Audio Signal Processing" — FDN topology, Householder matrix, feedback gain calculation for T60 (https://ccrma.stanford.edu/~jos/pasp/Feedback_Delay_Networks.html)
- Jot & Chaigne 1991, "Analysis and synthesis of room reverberation based on a statistical time-frequency model" — FDN reverb theoretical basis
- JUCE AudioPlayHead documentation (8.0.x) — `getPosition()`, `getBpm()`, `std::optional<double>` return type

### Tertiary (LOW confidence)
- None — all findings are either directly from the codebase or verifiable DSP theory

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — derived entirely from existing codebase inspection
- Architecture: HIGH — reverb placement and LFO integration already documented in ARCHITECTURE.md; FDN topology is well-established DSP
- Pitfalls: HIGH — feedback instability, delay sizing, tempo sync nullopt handling are known categories verified by codebase review of prior-phase pitfalls
- Test map: HIGH — existing Catch2 infrastructure is confirmed present; test commands verified against existing build structure

**Research date:** 2026-03-13
**Valid until:** 2026-06-13 (JUCE 8.0.12 stable; FDN theory is timeless)
