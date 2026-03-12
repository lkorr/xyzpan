# XYZPan

## What This Is

A 3D spatial audio panner VST plugin that positions a sound object in 3D space (X/Y/Z) and applies binaural processing to create realistic spatial perception. One object per plugin instance, mono input to stereo binaural output. Built as a pure C++ engine with custom UI, wrapped in JUCE for VST3/AU/Standalone distribution.

## Core Value

Accurate real-time binaural rendering of 3D spatial audio positioning — the spatial illusion must be convincing and the processing must be artifact-free.

## Requirements

### Validated

(None yet — ship to validate)

### Active

- [ ] Binaural panning with ITD (interaural time difference) — 0.7ms max delay, head shadow filter
- [ ] Depth/front-back processing via ~10 tunable comb filters (0-1.5ms delays, 0-30% wet)
- [ ] Elevation processing: pinna filter (notch at 8kHz), chest bounce delay, floor bounce delay
- [ ] Distance modeling: gain attenuation (inverse square), low-pass filter (22kHz→8kHz), delay (0-300ms)
- [ ] Doppler shift from distance changes over time (toggleable)
- [ ] Reverb with distance-scaled pre-delay (0-50ms)
- [ ] LFO modulation: 1 per axis, selectable waveform (sine/tri/saw/square), rate/depth/phase controls
- [ ] XYZ coordinate input with conversion to azimuth + elevation angles
- [ ] Custom OpenGL UI: 2D projection of 3D space, listener node centered, draggable object node
- [ ] Dev panel: all filter/delay/dB constants exposed as tunable parameters
- [ ] All dev parameters also exposed as VST automation parameters
- [ ] Pure C++ DSP engine (no JUCE dependency in core)
- [ ] JUCE wrapper for VST3/AU/Standalone output formats
- [ ] DAW-adaptive sample rate handling

### Out of Scope

- Multiple sound objects per instance — use multiple plugin instances instead
- Ambisonics output — binaural stereo only
- Room simulation beyond basic reverb — this is a panner, not a room modeler
- Mobile/embedded targets — desktop DAW plugin only

## Context

- **Architecture:** Pure C++ engine handles all DSP and coordinate math. Custom UI renders via OpenGL. JUCE PluginProcessor calls the engine; JUCE handles plugin format wrapping only.
- **Signal flow:** Mono in → coordinate conversion (XYZ → azimuth/elevation/distance) → binaural panning (ITD + head shadow) → depth comb filters → elevation filters (pinna + chest bounce + floor bounce) → distance processing (attenuation + LPF + delay/doppler) → reverb → stereo binaural out
- **Coordinate system:** X/Y mapped to azimuth, Z to elevation. All angle components normalized -1.0 to 1.0.
- **Dev workflow:** All DSP constants (filter frequencies, delay ranges, gain curves, comb filter tuning) must be runtime-adjustable during development for tuning. These become fixed or preset-based in production.
- **CPU priority:** Quality first, but don't waste cycles. Reasonable performance — no need for extreme optimization at the cost of code clarity.

## Constraints

- **Tech stack**: Pure C++ engine + JUCE wrapper — no other frameworks in DSP path
- **Plugin formats**: VST3, AU, Standalone via JUCE
- **I/O**: Mono in, stereo binaural out
- **Instances**: One sound object per plugin instance
- **Sample rate**: Must adapt to DAW session sample rate
- **UI rendering**: OpenGL for 3D visualization

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Pure C++ engine separate from JUCE | Keep DSP portable and testable without JUCE dependency | — Pending |
| One object per instance | Simplifies DSP pipeline and UI; multiple instances for multiple sources | — Pending |
| OpenGL for UI | Need 3D-projected visualization of object position | — Pending |
| XYZ coordinates → azimuth/elevation conversion | Matches user mental model (3D space) while enabling angle-based DSP processing | — Pending |
| All constants as dev parameters | Enables runtime tuning during development | — Pending |

---
*Last updated: 2026-03-12 after initial project setup*
