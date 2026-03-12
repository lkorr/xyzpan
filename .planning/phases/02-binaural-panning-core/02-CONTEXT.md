# Phase 2: Binaural Panning Core - Context

**Gathered:** 2026-03-12
**Status:** Ready for planning

<domain>
## Phase Boundary

ITD (interaural time difference) and head shadow filtering to create a convincing left/right binaural image from azimuth. Mono input split to stereo L/R at the panning stage. Stereo input summed to mono (already implemented in Phase 1). Smooth, click-free panning during parameter automation. Front/back depth perception (comb filters) and elevation are Phase 3 concerns.

</domain>

<decisions>
## Implementation Decisions

### Head Shadow Filter
- Single low-pass filter (SVF) per ear, cutoff modulated by azimuth X component
- At X=0 (center): LPF fully open on both ears — inaudible
- At X=-1 (hard left): right ear LPF active (darkened), left ear LPF fully open
- At X=+1 (hard right): left ear LPF active (darkened), right ear LPF fully open
- Filter only — no level reduction from head shadow itself
- Exaggerated/dramatic character: ~10-15 dB cut starting at 1-2 kHz at full azimuth

### Distance-Dependent Level Reduction (ILD)
- Far ear gets attenuated based on proximity — near ear stays at unity
- Close source + full azimuth = significant level difference (hard panning feel)
- Far source = negligible level difference (both ears roughly equal)
- Distance gain attenuation (Phase 4) will handle overall level naturally when combined with this
- Max attenuation value exposed in dev panel for tuning

### ITD (Interaural Time Difference)
- Sinusoidal scaling with floor function — NOT a crossover model
- At X=0: both ears have 0 delay
- X moving from 0 to -1: right ear delay increases sinusoidally from 0 to 0.72ms, left ear stays at 0
- X moving from 0 to +1: left ear delay increases sinusoidally from 0 to 0.72ms, right ear stays at 0
- Only the far ear gets delayed; near ear is always at 0
- Delay line uses cubic (Hermite) interpolation (decided pre-project)
- 0.72ms default max — dev panel range at Claude's discretion

### Rear Shadow Hint
- Slight high-frequency rolloff applied to BOTH ears equally when source is behind
- Scales linearly with Y: no shadow at Y=0, full shadow at Y=-1
- No effect in front hemisphere (Y=0 to Y=1)
- Independent from azimuth-based head shadow (does not stack on far ear)
- Provides a hint of front/back distinction before Phase 3 comb filters arrive

### Parameter Smoothing
- As fast as possible without inducing clicks — artifact avoidance is the priority
- Per-parameter smoothing if needed (different rates for ITD delay vs filter cutoff vs level)
- Target ~5ms baseline, Claude's discretion on exact per-parameter values
- Smoothing time(s) exposed in dev panel for tuning

### Spatial Character
- Realistic baseline using physically accurate head model values
- Dev panel allows pushing beyond realistic values for creative use

### Claude's Discretion
- Head shadow filter topology (single LPF vs LPF + high shelf — pick what sounds best for the exaggerated character)
- ILD direction: far ear quieter (near ear unity) — confirmed, but exact curve shape is Claude's choice
- Max ILD attenuation at closest distance + full azimuth (reasonable value, exposed in dev panel)
- Max ITD dev panel range (default 0.72ms, upper bound Claude's choice)
- Per-parameter smoothing time values (optimize for artifact-free, expose in dev panel)
- Rear shadow filter cutoff and depth (subtle hint, not dramatic)

</decisions>

<specifics>
## Specific Ideas

- "The head shadow needs to be such that at azimuth X=0 the LPF is all the way up and inaudible"
- "When the object has a large distance, level reduction should be negligible — when close, decent hard panning"
- "ITD should use a floor function — both speakers at 0 delay when X=0, only the far ear increases"
- Distance gain from Phase 4 will complement the ILD naturally — no need to duplicate attenuation logic

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `XYZPanEngine` (engine/include/xyzpan/Engine.h): Main processing class with prepare/setParams/process/reset lifecycle. Phase 2 extends this with delay lines and filters.
- `toSpherical()` (engine/include/xyzpan/Coordinates.h): Already computes azimuth from X/Y — Phase 2 uses azimuth to drive ITD and head shadow.
- `computeDistance()` (engine/include/xyzpan/Coordinates.h): Returns normalized distance — Phase 2 uses this for distance-dependent ILD.
- `monoBuffer` (Engine.h private): Pre-allocated mono buffer, stereo-to-mono summing already implemented.
- `EngineParams` (Types.h): Currently has x, y, z — Phase 2 will add smoothing targets.

### Established Patterns
- Pure C++ engine with no JUCE headers — all new DSP classes must follow this
- Pre-allocation in `prepare()`, zero allocation in `process()`
- Parameter snapshot from APVTS atomics passed via `setParams()` each block
- SVF (State Variable Filter) chosen for modulated filters (project-wide decision)
- Cubic Hermite interpolation for delay lines (project-wide decision)

### Integration Points
- `Engine::process()` currently does pass-through — Phase 2 replaces this with ITD + head shadow pipeline
- `EngineParams` struct needs new fields for any Phase 2-specific dev panel parameters
- `ParamLayout.cpp` needs new APVTS parameters for dev panel exposure
- `ParamIDs.h` needs new parameter ID constants

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 02-binaural-panning-core*
*Context gathered: 2026-03-12*
