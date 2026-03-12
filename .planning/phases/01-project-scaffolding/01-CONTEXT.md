# Phase 1: Project Scaffolding - Context

**Gathered:** 2026-03-12
**Status:** Ready for planning

<domain>
## Phase Boundary

Build system (CMake + JUCE), pure C++ engine static library with coordinate conversion (XYZ to spherical), JUCE plugin wrapper with pass-through audio, and Catch2 test infrastructure. Delivers a building, loadable, pluginval-passing skeleton that all future phases build on.

</domain>

<decisions>
## Implementation Decisions

### Project structure
- Start from Pamplejuce template, modify for XYZPan's structure
- Three CMake library targets: `xyzpan_engine` (pure C++, no JUCE), `xyzpan_ui` (OpenGL rendering, depends on JUCE OpenGL), `XYZPan` plugin (thin JUCE wrapper linking both)
- CPM for dependency management (JUCE 8.0.12, Catch2, GLM pinned versions)
- Placeholder company name (e.g., "XYZAudio") for COMPANY_NAME and manufacturer code — changeable before release

### Coordinate system behavior
- Y-forward convention: Y=1 = front, Y=-1 = behind, X=1 = right, X=-1 = left, Z=1 = above, Z=-1 = below
- Hard clamp all XYZ inputs to [-1.0, 1.0] before processing — LFO overshoot is the user's responsibility
- Clamp to minimum distance at origin (e.g., 0.1 normalized) — prevents division-by-zero and undefined azimuth/elevation
- True spherical elevation: elevation = atan2(Z, sqrt(X²+Y²)) — geometrically correct, not raw Z mapping

### Engine API surface
- Engine handles stereo-to-mono summing internally (accepts 1 or 2 input channels)
- Full prepare(sampleRate, maxBlockSize) / process() / reset() contract from day one — reset() is no-op in Phase 1
- EngineParams struct starts minimal (X, Y, Z only in Phase 1) and grows per phase as DSP is added
- Coordinate converter implemented as free functions in `xyzpan` namespace (stateless math, no class needed)
- process() signature: process(const float* const* input, int numInputChannels, float* outL, float* outR, int numSamples)

### Pass-through and plugin behavior
- Phase 1 plugin copies mono input to both output channels unchanged — proves signal path works end-to-end
- X, Y, Z parameters registered in APVTS from Phase 1 (visible in DAW automation, saveable in presets)
- GenericAudioProcessorEditor used as the editor — auto-generates sliders for X/Y/Z, useful for dev testing
- pluginval must pass at strictness level 5

### Test coverage
- Coordinate conversion tested with core cases + edge cases: cardinal directions (front, back, left, right, up, down), diagonals, origin/minimum-distance behavior, boundary values (-1, 0, 1) — ~15-20 test cases

### Claude's Discretion
- Exact Pamplejuce template cleanup and restructuring approach
- CMake configuration details (compile definitions, warning flags, LTO settings)
- Header file organization within engine/include/xyzpan/
- SphericalCoord struct layout and naming
- Test file organization and Catch2 section structure
- .clang-format style choices

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- No existing code — greenfield project

### Established Patterns
- Research recommends Pamplejuce template as starting point (CMake + CPM + Catch2 + CI preconfigured)
- Architecture research defines engine/plugin/ui separation pattern in detail (ARCHITECTURE.md)
- STACK.md has CMake configuration sketch with juce_add_plugin() call ready to adapt

### Integration Points
- Engine static library links to both plugin target and test target
- APVTS parameter IDs defined in shared ParamIDs.h header used by both plugin and (later) UI

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 01-project-scaffolding*
*Context gathered: 2026-03-12*
