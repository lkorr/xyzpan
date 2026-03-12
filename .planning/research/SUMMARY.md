# Research Summary

**Project:** XYZPan — 3D Spatial Audio Panner VST
**Researched:** 2026-03-12
**Sources:** STACK.md, FEATURES.md, ARCHITECTURE.md, PITFALLS.md

## Key Findings

### Stack
- **C++20 + JUCE 8.0.12 + CMake** — the standard modern audio plugin stack
- **Hand-rolled DSP** — no external spatial audio libraries needed; the binaural processing (ITD, biquads, comb filters, delay lines) is straightforward enough to implement directly
- **OpenGL 3.2 Core Profile** for UI visualization, **GLM** for 3D math
- **Catch2 + pluginval** for testing/validation
- Start from **Pamplejuce** CMake template

### Market Opportunity
- **DearVR Pro 2 was discontinued March 2025** and made free — the most popular mid-tier binaural panner vacated the market
- **Front/back confusion is the #1 user complaint** across all binaural plugins — XYZPan's tuneable comb filters directly address this
- **HRTF coloration is a persistent complaint** — XYZPan's parametric approach sidesteps it entirely
- **Per-axis LFO + doppler is rare** — no competitor offers both

### Architecture
- **Engine must be a separate CMake static library** with zero JUCE headers — enables unit testing without DAW
- **Per-sample processing** is correct for this topology (LFO modulates coordinates every sample)
- **Three-layer parameter flow:** APVTS atomics → per-block snapshot → engine's own SmoothedParam
- **State Variable Filters** over direct-form biquads for any modulated filter (coefficient interpolation stability)
- **Audio→GL via double-buffer** (only need latest position, not history)

### Critical Pitfalls
1. **Audio thread safety** — zero allocations in processBlock, pre-allocate everything in prepareToPlay
2. **Delay interpolation for doppler** — use cubic (Hermite) interpolation + lowpass-smoothed control signal; crossfade for jumps
3. **OpenGL in JUCE plugins** — render to FBO, blit to JUCE Image, have software fallback
4. **State save/restore** — DAW-specific chaos; use APVTS with version-tagged state
5. **Comb filter stability** — hard-clamp feedback to 0.999, soft clipper in feedback path, NaN guard

### MVP Phasing (from Features research)
1. **Phase 1:** Binaural core (ITD + head shadow + XY position) — validate fundamental spatialization
2. **Phase 2:** Distance + Elevation (attenuation, LPF, pinna notch, comb filters) — prove differentiators
3. **Phase 3:** Creative tools (LFOs, doppler, reverb) — make it a creative instrument
4. **Phase 4:** Dev mode + polish (runtime constant editing, presets, UI polish)

## Anti-Recommendations
- No HRTF database/SOFA files — undermines the parametric differentiator
- No multi-format output (Ambisonics, surround) — binaural stereo only
- No head tracking/OSC — niche, additive later
- No convolution reverb — algorithmic only
- No C++23 — JUCE hasn't adopted it

## Open Questions for Requirements
1. **300ms distance delay = 300ms reported latency** — significant for live use. Accept or cap lower?
2. **Stereo input handling** — collapse to mono before processing, or omit stereo input for v1?
3. **Comb filter count** — spec says ~10, research suggests 4-6 may suffice for MVP
4. **macOS support** — OpenGL deprecated on macOS; relevant for v1 or defer?
