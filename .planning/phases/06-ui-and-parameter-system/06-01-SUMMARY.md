---
phase: 06-ui-and-parameter-system
plan: 01
subsystem: ui
tags: [juce, apvts, dsp, one-pole-smooth, position-bridge, catch2, cmake, lock-free]

# Dependency graph
requires:
  - phase: 05-creative-tools
    provides: LFO + reverb APVTS wiring, OnePoleSmooth used for rSmooth_
provides:
  - R (radius/scale) AudioParameterFloat in APVTS, range 0–2, default 1.0
  - rSmooth_ OnePoleSmooth member in PluginProcessor: 20ms smoothing, multiplies X/Y/Z before engine
  - PositionBridge header-only lock-free double-buffer for audio->GL position transfer
  - XYZPanPluginTests CMake target linking XYZPan + xyzpan_ui + juce::juce_audio_utils + Catch2
  - Three plugin-layer test files (PARAM-01, UI-07, PARAM-05 stub)
affects:
  - 06-02 (PluginEditor/OpenGL renderer — will use PositionBridge and XYZPanPluginTests)
  - 06-04 (Factory presets — will fill TestPresets.cpp stub)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - XYZPanPluginTests links XYZPan target (not XYZPan_SharedCode string) plus juce::juce_audio_utils to propagate JUCE include paths
    - rSmooth_.process() called once per block to multiply position parameters (not audio); per-block smoothing sufficient
    - PositionBridge double-buffer: writeIdx_ is the live index (not the write-ahead index); read() returns buf_[writeIdx_]

key-files:
  created:
    - ui/PositionBridge.h
    - tests/plugin/TestParameterLayout.cpp
    - tests/plugin/TestPositionBridge.cpp
    - tests/plugin/TestPresets.cpp
  modified:
    - plugin/ParamIDs.h
    - plugin/ParamLayout.cpp
    - plugin/PluginProcessor.h
    - plugin/PluginProcessor.cpp
    - ui/CMakeLists.txt
    - tests/CMakeLists.txt

key-decisions:
  - "XYZPanPluginTests links the XYZPan CMake target (not the string 'XYZPan_SharedCode') because JUCE's juce_add_plugin(XYZPan ...) creates a CMake target named XYZPan whose .lib output file happens to be named XYZPan_SharedCode.lib"
  - "juce::juce_audio_utils must be added to XYZPanPluginTests link libraries — XYZPan alone does not propagate JUCE module include paths to dependent test targets in CMake"
  - "rSmooth_ processes once per block at block rate (not per-sample) because R multiplies position coordinates not audio samples — block-rate smoothing eliminates zipper noise with negligible computational cost"

patterns-established:
  - "Plugin-layer test targets must link both the plugin target AND the relevant juce:: module targets for include paths"
  - "Lock-free double-buffer pattern for audio->GL: writer stores to the non-live slot, atomically updates index with memory_order_release; reader loads index with memory_order_acquire"

requirements-completed: [PARAM-01, PARAM-03, PARAM-04, UI-07]

# Metrics
duration: 10min
completed: 2026-03-13
---

# Phase 06 Plan 01: R Parameter, PositionBridge, and Plugin Test Infrastructure Summary

**R scale parameter (0–2, default 1.0) live in APVTS with OnePoleSmooth block-rate multiplier; lock-free PositionBridge header and XYZPanPluginTests target with 4 passing plugin-layer tests.**

## Performance

- **Duration:** 10 min
- **Started:** 2026-03-13T08:38:58Z
- **Completed:** 2026-03-13T08:49:00Z
- **Tasks:** 2
- **Files modified:** 10

## Accomplishments
- R AudioParameterFloat registered in APVTS (range 0.0–2.0, step 0.001, default 1.0) per PARAM-01
- processBlock applies smoothed R via rSmooth_.process() once per block — eliminates zipper noise on automation (PARAM-03)
- PositionBridge lock-free double-buffer header-only class ready for 06-02 OpenGL renderer
- XYZPanPluginTests CMake target builds and discovers 4 plugin-layer tests via CTest
- All 68 tests pass (64 original engine tests + 4 new plugin tests) — zero regressions

## Task Commits

1. **Task 1: R parameter in ParamIDs, ParamLayout, and PluginProcessor** - `fd4e749` (feat)
2. **Task 2: PositionBridge header, xyzpan_ui include path, and XYZPanPluginTests target** - `bc66f9c` (feat)

**Plan metadata:** (docs commit follows)

## Files Created/Modified
- `plugin/ParamIDs.h` - Added constexpr const char* R = "r" to ParamID namespace
- `plugin/ParamLayout.cpp` - Registered R AudioParameterFloat (0.0–2.0, step 0.001, default 1.0)
- `plugin/PluginProcessor.h` - Added rParam atomic pointer, rSmooth_ OnePoleSmooth, OnePoleSmooth.h include
- `plugin/PluginProcessor.cpp` - rParam init in constructor, rSmooth_.prepare in prepareToPlay, r=rSmooth_.process in processBlock
- `ui/PositionBridge.h` - Lock-free double-buffer SourcePositionSnapshot + PositionBridge (UI-07)
- `ui/CMakeLists.txt` - Added target_include_directories INTERFACE to expose PositionBridge.h
- `tests/CMakeLists.txt` - Added XYZPanPluginTests target with JUCE-aware linking
- `tests/plugin/TestParameterLayout.cpp` - PARAM-01 automated coverage (R registration, range, default)
- `tests/plugin/TestPositionBridge.cpp` - UI-07 automated coverage (write/read round-trip, defaults)
- `tests/plugin/TestPresets.cpp` - PARAM-05 stub (compiles; real content in 06-04)

## Decisions Made
- The CMake target for XYZPanPluginTests links `XYZPan` (the juce_add_plugin target), not the string `XYZPan_SharedCode`. JUCE names the .lib output `XYZPan_SharedCode.lib` internally but the CMake target name is `XYZPan`.
- `juce::juce_audio_utils` must be added explicitly to XYZPanPluginTests — the XYZPan target alone does not propagate JUCE include paths through CMake's dependency graph to test executables.
- rSmooth_ processes at block rate (not per-sample) because R scales position coordinates, not audio samples. Block-rate smoothing is sufficient and avoids redundant computation.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added juce::juce_audio_utils to XYZPanPluginTests link libraries**
- **Found during:** Task 2 (XYZPanPluginTests CMake target)
- **Issue:** TestParameterLayout.cpp includes PluginProcessor.h which includes juce_audio_utils/juce_audio_utils.h. Linking only XYZPan_SharedCode (later XYZPan) did not propagate JUCE module include paths to the test target, causing fatal error C1083.
- **Fix:** Added `juce::juce_audio_utils` to target_link_libraries for XYZPanPluginTests. This propagates the necessary JUCE include directories via CMake's dependency mechanism.
- **Files modified:** tests/CMakeLists.txt
- **Verification:** Build succeeded; all 68 tests pass including the 4 new plugin tests.
- **Committed in:** bc66f9c (Task 2 commit)

**2. [Rule 3 - Blocking] Used XYZPan CMake target instead of XYZPan_SharedCode string**
- **Found during:** Task 2 (XYZPanPluginTests CMake target)
- **Issue:** Linking `XYZPan_SharedCode` as a target name failed: "No target XYZPan_SharedCode". JUCE creates the shared code target with the plugin name `XYZPan`, and only renames the .lib output file to `XYZPan_SharedCode.lib`.
- **Fix:** Changed target_link_libraries to use `XYZPan` (the actual CMake target) instead of `XYZPan_SharedCode`.
- **Files modified:** tests/CMakeLists.txt
- **Verification:** Build succeeded; linker found XYZPan_SharedCode.lib via the correct library directory.
- **Committed in:** bc66f9c (Task 2 commit)

---

**Total deviations:** 2 auto-fixed (both Rule 3 - blocking)
**Impact on plan:** Both fixes necessary to build the test target. No scope creep — both are direct consequences of JUCE's CMake internal target naming conventions.

## Issues Encountered
- JUCE's `juce_add_plugin(XYZPan ...)` creates a static library CMake target named `XYZPan` with output file renamed to `XYZPan_SharedCode.lib`. This naming split is a JUCE internal convention. Test targets must link the CMake target name (`XYZPan`), not the output file basename string.

## Next Phase Readiness
- R parameter is live in APVTS and automatable — 06-02 can use it immediately
- PositionBridge is ready for use in the PluginEditor and OpenGL renderer (06-02)
- XYZPanPluginTests target is established — all subsequent plugin-layer tests add to this target
- No blockers for 06-02

## Self-Check: PASSED

All created files verified present. Both task commits verified in git log (fd4e749, bc66f9c).

---
*Phase: 06-ui-and-parameter-system*
*Completed: 2026-03-13*
