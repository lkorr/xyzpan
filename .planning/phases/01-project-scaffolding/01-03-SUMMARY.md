---
phase: 01-project-scaffolding
plan: 03
subsystem: audio-engine
tags: [engine, passthrough, pluginval, vst3, audio-processing, nan-fix]

requires:
  - phase: 01-project-scaffolding/01-01
    provides: "Engine.h API, PluginProcessor framework, CMake build system"
  - phase: 01-project-scaffolding/01-02
    provides: "Coordinates.h/cpp full tested implementation, toSpherical()"

provides:
  - Engine.cpp: working prepare/process/reset with stereo-to-mono summing and mono-to-stereo pass-through
  - PluginProcessor.cpp: APVTS param snapshot and engine delegation in processBlock
  - pluginval strictness-5 clean pass (0 failures, exit 0)
  - toSpherical() call in Engine::process() validates coordinate wiring (SETUP-03)

affects:
  - all future DSP phases (Engine pass-through is the signal path foundation)
  - Phase 2 onwards: replace pass-through with actual spatial processing

tech-stack:
  added:
    - pluginval 1.0.4 (Windows, download-only, not committed to repo)
  patterns:
    - "Use getTotalNumInputChannels() for engine numIn, NOT buffer.getNumChannels() — mono-in/stereo-out buffer has 2 slots but only slot 0 is valid input"
    - "toSpherical() called in process() with (void) result to prove coordinate wiring without optimizer stripping"
    - "releaseResources() delegates to engine.reset() — JUCE lifecycle fully wired"

key-files:
  created: []
  modified:
    - engine/src/Engine.cpp
    - plugin/PluginProcessor.cpp
    - .gitignore

key-decisions:
  - "buffer.getNumChannels() returns total channel slots (input+output); getTotalNumInputChannels() returns actual input count — must use the latter when building the engine inputs[] array"
  - "pluginval binaries excluded from git via .gitignore (tools/pluginval*.exe, tools/pluginval*.zip, tools/pluginval_output/)"

patterns-established:
  - "Pattern: getTotalNumInputChannels() for engine input count, getTotalNumOutputChannels() for output validation"
  - "Pattern: monoBuffer pre-sized in prepare() — zero allocations in process() confirmed by pluginval"

requirements-completed: [SETUP-03, SETUP-05]

duration: 4min
completed: 2026-03-12
---

# Phase 1 Plan 3: Pass-Through Audio and pluginval Validation Summary

**Engine pass-through wired to PluginProcessor with toSpherical() coordinate validation; pluginval 1.0.4 passes all strictness-5 tests after fixing mono-in/stereo-out channel count bug**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-12T22:56:13Z
- **Completed:** 2026-03-12T23:00:00Z
- **Tasks:** 2
- **Files modified:** 3 (Engine.cpp, PluginProcessor.cpp, .gitignore)

## Accomplishments

- Engine.cpp completed: `#include "xyzpan/Coordinates.h"` added, `toSpherical()` called in `process()` with `(void)spherical` to validate SETUP-03 coordinate wiring
- PluginProcessor.cpp: `releaseResources()` now calls `engine.reset()`, completing the full JUCE lifecycle delegation
- pluginval 1.0.4 downloaded from GitHub releases, placed in `tools/` (not committed to git)
- pluginval strictness-5 passes with 0 failures across: Open plugin (cold/warm), Plugin info, Editor, Audio processing (15 sample rate/block size combos), Plugin state, Automation, Editor Automation, Automatable Parameters, Bus layout tests
- All 5 CTest coordinate conversion tests continue to pass

## Task Commits

1. **Task 1: Engine pass-through and PluginProcessor bridge** - `3fc1f53` (feat)
2. **Task 2: pluginval strictness-5 pass + NaN auto-fix** - `48422d4` (feat)

## Files Created/Modified

- `engine/src/Engine.cpp` — Added `#include "xyzpan/Coordinates.h"`, added `toSpherical()` call in `process()` to prove coordinate wiring (SETUP-03)
- `plugin/PluginProcessor.cpp` — `releaseResources()` calls `engine.reset()`; fixed `numIn` from `buffer.getNumChannels()` to `getTotalNumInputChannels()` to prevent NaN from reading uninitialized output channel as input
- `.gitignore` — Added `tools/pluginval*.exe`, `tools/pluginval*.zip`, `tools/pluginval_output/`

## Decisions Made

- `getTotalNumInputChannels()` must be used instead of `buffer.getNumChannels()` when building the engine's input pointer array. For mono-in/stereo-out, JUCE allocates a 2-channel buffer: channel 0 = mono input, channel 1 = uninitialized stereo output R. Using `buffer.getNumChannels()` (= 2) caused the engine to treat the uninitialized output channel as a second input, summing garbage into the mono buffer and producing NaN outputs. `getTotalNumInputChannels()` correctly returns 1.
- pluginval binary not committed to git — added to `.gitignore` with `tools/pluginval*.exe` pattern. Developers download from https://github.com/Tracktion/pluginval/releases and place in `tools/`.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed NaN output from incorrect input channel count in processBlock**
- **Found during:** Task 2 verification (first pluginval run)
- **Issue:** `processBlock` used `buffer.getNumChannels()` (= 2 for mono-in/stereo-out) to determine the number of engine input channels. This caused `inputs[1]` to be set to `buffer.getReadPointer(1)`, which is the uninitialized stereo output R channel, not a second audio input. The engine then summed `0.5 * (valid_input + NaN_garbage)` into monoBuffer, producing NaN output. pluginval reported 90 failed tests (NaNs found in buffer) across 15 block-size configurations.
- **Fix:** Changed `const int numIn = buffer.getNumChannels()` to `const int numIn = getTotalNumInputChannels()`. For mono-in/stereo-out, `getTotalNumInputChannels()` returns 1, so `inputs[1]` is set to `nullptr` and the engine correctly uses the mono input path.
- **Files modified:** `plugin/PluginProcessor.cpp`
- **Commit:** `48422d4`
- **Verification:** pluginval re-run shows 0 failures, exit code 0

---

**Total deviations:** 1 auto-fixed (Rule 1 - Bug, found during pluginval validation)
**Impact on plan:** Correctness fix required for pluginval to pass. No scope creep.

## Issues Encountered

- pluginval output is not captured via normal stdout redirect because it's a JUCE GUI app. Used `Start-Process -Wait -PassThru -NoNewWindow` in PowerShell to get output to terminal.

## User Setup Required

- To run pluginval manually: Download from https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Windows.zip, extract to `tools/`, then run:
  ```
  tools\pluginval.exe --strictness-level 5 "build\plugin\XYZPan_artefacts\Release\VST3\XYZPan.vst3"
  ```

## Next Phase Readiness

- Full signal path proven end-to-end: DAW -> PluginProcessor -> Engine -> stereo output
- pluginval strictness-5 baseline established — regression test for all future DSP work
- Engine API is complete and stable: `prepare()`, `setParams()`, `process()`, `reset()` all wired and tested
- APVTS X/Y/Z parameters save/restore correctly (Plugin state test passes)
- Phase 1 complete — all 3 plans done. Phase 2 (DSP core) can begin.

## Self-Check: PASSED

- engine/src/Engine.cpp: FOUND
- plugin/PluginProcessor.cpp: FOUND
- .gitignore: FOUND
- toSpherical in Engine.cpp: FOUND (line 44)
- engine.process in PluginProcessor.cpp: FOUND (line 56)
- getTotalNumInputChannels in PluginProcessor.cpp: FOUND
- build/plugin/XYZPan_artefacts/Release/VST3/XYZPan.vst3: FOUND
- Commit 3fc1f53: FOUND
- Commit 48422d4: FOUND

---
*Phase: 01-project-scaffolding*
*Completed: 2026-03-12*
