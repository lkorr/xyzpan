---
phase: 02-binaural-panning-core
plan: "02"
subsystem: plugin
tags: [juce, apvts, vst3, parameters, binaural, itd, ild, head-shadow, smoothing]

# Dependency graph
requires:
  - phase: 02-binaural-panning-core
    plan: "01"
    provides: EngineParams struct with 7 dev panel fields and DSP pipeline that reads them
provides:
  - 7 APVTS parameter IDs (ITD_MAX_MS, HEAD_SHADOW_HZ, ILD_MAX_DB, REAR_SHADOW_HZ, SMOOTH_ITD_MS, SMOOTH_FILTER_MS, SMOOTH_GAIN_MS)
  - APVTS registration for all 7 dev panel parameters with correct ranges, defaults, skew factors
  - PluginProcessor atomic pointer caching and processBlock snapshot for all 7 fields
  - Complete APVTS -> EngineParams wiring for all binaural panning tuning parameters
affects: [02-03, 06-custom-ui, any plan that reads or extends EngineParams]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "NormalisableRange skew factor 0.3 for Hz parameters in generic editor (log-like feel)"
    - "APVTS atomic pointer cached in constructor with jassert, snapshot each processBlock"
    - "Dev panel parameters versioned at version 1 (PID{id, 1}) alongside spatial params"

key-files:
  created: []
  modified:
    - plugin/ParamIDs.h
    - plugin/ParamLayout.cpp
    - plugin/PluginProcessor.h
    - plugin/PluginProcessor.cpp

key-decisions:
  - "NormalisableRange skew 0.3 for all Hz parameters (HEAD_SHADOW_HZ, REAR_SHADOW_HZ) for log-like feel in the generic editor without requiring a custom UI"
  - "All 7 dev panel parameters follow existing pattern: atomic<float>* cached in constructor with jassert, load() per processBlock"

patterns-established:
  - "All parameter additions follow the same 3-layer pattern: ParamIDs.h constant -> ParamLayout.cpp registration -> PluginProcessor.h pointer + constructor init + processBlock snapshot"
  - "Hz-range parameters use NR skew 0.3 for perceptually linear feel in linear slider controls"

requirements-completed: [PAN-04, PAN-05]

# Metrics
duration: 8min
completed: 2026-03-13
---

# Phase 2 Plan 02: APVTS Parameter Wiring Summary

**7 binaural panning tuning parameters wired from APVTS atomics to EngineParams each processBlock, with skewed Hz ranges visible in the DAW generic editor**

## Performance

- **Duration:** ~8 min
- **Started:** 2026-03-13T00:38:00Z
- **Completed:** 2026-03-13T00:46:00Z
- **Tasks:** 1
- **Files modified:** 4

## Accomplishments
- Added 7 parameter ID constants to ParamIDs.h (ITD_MAX_MS, HEAD_SHADOW_HZ, ILD_MAX_DB, REAR_SHADOW_HZ, SMOOTH_ITD_MS, SMOOTH_FILTER_MS, SMOOTH_GAIN_MS)
- Registered all 7 parameters in ParamLayout.cpp with appropriate ranges: linear for ms/dB, skew 0.3 for Hz params
- Added 7 atomic<float>* members to PluginProcessor.h, initialized with jasserts in constructor
- Snapshot all 7 new fields into EngineParams each processBlock — complete APVTS-to-engine wiring
- Build passes, all 27 engine+DSP tests pass (no regressions), pluginval strictness-5 SUCCESS

## Task Commits

Each task was committed atomically:

1. **Task 1: Add dev panel parameter IDs, APVTS registration, and PluginProcessor wiring** - `be56673` (feat)

**Plan metadata:** (docs commit follows)

## Files Created/Modified
- `plugin/ParamIDs.h` - Added 7 new parameter ID constants for dev panel binaural tuning
- `plugin/ParamLayout.cpp` - Registered 7 new APVTS AudioParameterFloat entries with ranges and defaults
- `plugin/PluginProcessor.h` - Added 7 atomic<float>* pointer members for new parameters
- `plugin/PluginProcessor.cpp` - Constructor initialization + processBlock snapshot for all 7 new fields

## Decisions Made
- NormalisableRange skew factor 0.3 for HEAD_SHADOW_HZ and REAR_SHADOW_HZ — gives a log-like response in the generic editor's linear slider so mid-range frequency values aren't compressed into a small slider region
- All 7 parameters use version 1 in ParameterID (consistent with existing X/Y/Z)

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All binaural panning parameters are now tuneable from the DAW generic editor
- EngineParams is fully populated from APVTS atomics each processBlock
- Phase 2 Plan 03 can now build the stereo summing + any remaining Phase 2 work on top of this complete parameter layer
- pluginval strictness-5 confirmed: plugin is distribution-ready at current feature level

## Self-Check: PASSED

- plugin/ParamIDs.h: FOUND
- plugin/ParamLayout.cpp: FOUND
- plugin/PluginProcessor.h: FOUND
- plugin/PluginProcessor.cpp: FOUND
- .planning/phases/02-binaural-panning-core/02-02-SUMMARY.md: FOUND
- Commit be56673: FOUND
- Build: SUCCESS (MSBuild)
- Tests: 27/27 PASSED
- pluginval strictness-5: SUCCESS

---
*Phase: 02-binaural-panning-core*
*Completed: 2026-03-13*
