---
phase: 03-depth-and-elevation
plan: "03"
subsystem: plugin
tags: [juce, apvts, parameters, comb-filter, elevation, depth, dsp]

# Dependency graph
requires:
  - phase: 03-02
    provides: Phase 3 engine DSP (comb bank, pinna EQ, chest/floor bounce) with EngineParams fields
  - phase: 02-02
    provides: Established 3-layer APVTS wiring pattern (ParamIDs -> ParamLayout -> PluginProcessor)
provides:
  - 28 Phase 3 APVTS parameter ID constants in ParamIDs.h
  - 28 Phase 3 parameters registered in ParamLayout.cpp with correct ranges and defaults
  - 17 atomic pointer members in PluginProcessor.h for Phase 3 parameters
  - processBlock snapshot wiring all Phase 3 EngineParams fields from APVTS
affects: [04-distance, 05-opengl-ui, 06-custom-ui]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "constexpr const char* arrays in namespace for indexed parameter IDs (COMB_DELAY[10], COMB_FB[10])"
    - "Loop-based APVTS registration for per-filter parameters"
    - "Loop-based atomic pointer init and processBlock snapshot for array params"

key-files:
  created: []
  modified:
    - plugin/ParamIDs.h
    - plugin/ParamLayout.cpp
    - plugin/PluginProcessor.h
    - plugin/PluginProcessor.cpp

key-decisions:
  - "constexpr const char* arrays (COMB_DELAY[10], COMB_FB[10]) in header are safe — ParamIDs.h is only included by two .cpp TUs, so no ODR violation"
  - "ParamLayout.cpp loop uses xyzpan::kMaxCombFilters and kCombDefaultDelays_ms/kCombDefaultFeedback directly from Constants.h for defaults"
  - "Hz-domain elevation params (PINNA_NOTCH_HZ, PINNA_SHELF_HZ) use NormalisableRange skew 0.3 — consistent with Phase 2 head/rear shadow Hz params"

patterns-established:
  - "Indexed param arrays: constexpr const char* ARRAY[N] = {...} in namespace for loop-addressable IDs"
  - "Per-filter loop: for (int i = 0; i < N; ++i) { init, jassert, snapshot } in constructor and processBlock"

requirements-completed: [DEPTH-05, ELEV-05]

# Metrics
duration: 2min
completed: 2026-03-13
---

# Phase 3 Plan 03: Phase 3 APVTS Parameter Wiring Summary

**28 Phase 3 comb-filter and elevation parameters wired from APVTS to EngineParams via the established 3-layer pattern, enabling runtime tuning of all depth/elevation DSP from the DAW generic editor**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-13T03:50:50Z
- **Completed:** 2026-03-13T03:53:02Z
- **Tasks:** 1
- **Files modified:** 4

## Accomplishments
- All 28 Phase 3 parameters (10 comb delays, 10 comb feedbacks, 1 comb wet max, 7 elevation) registered in APVTS with correct ranges and defaults from Constants.h
- All 17 atomic pointer members added to PluginProcessor.h and initialized with jasserts in constructor
- processBlock snapshots all Phase 3 EngineParams fields from APVTS atomics every block
- Build passes, all 47 tests green with no regressions

## Task Commits

Each task was committed atomically:

1. **Task 1: Wire Phase 3 APVTS parameters to EngineParams** - `ec548e0` (feat)

**Plan metadata:** (docs commit follows)

## Files Created/Modified
- `plugin/ParamIDs.h` - Added 28 Phase 3 param ID constants: COMB_DELAY[10], COMB_FB[10], COMB_WET_MAX, PINNA_NOTCH_HZ, PINNA_NOTCH_Q, PINNA_SHELF_HZ, CHEST_DELAY_MS, CHEST_GAIN_DB, FLOOR_DELAY_MS, FLOOR_GAIN_DB
- `plugin/ParamLayout.cpp` - Added Constants.h include; loop-registers 20 comb params + 1 wet max + 7 elevation params with correct NormalisableRange and defaults
- `plugin/PluginProcessor.h` - Added combDelayParam[10], combFbParam[10], combWetMaxParam, and 7 elevation atomic pointer members
- `plugin/PluginProcessor.cpp` - Constructor loop-inits all Phase 3 pointers with jasserts; processBlock loop-snapshots comb arrays and scalar elevation fields

## Decisions Made
- `constexpr const char*` arrays in the `ParamID` namespace are safe since ParamIDs.h is only included by two .cpp files (no ODR issues)
- Hz-domain elevation parameters use NormalisableRange skew 0.3 for log-like feel, matching Phase 2 convention for HEAD_SHADOW_HZ and REAR_SHADOW_HZ

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 3 is complete: engine DSP (03-01, 03-02) and parameter wiring (03-03) are all done
- All 47 tests pass including Phase 3 integration tests
- Ready for Phase 4: Distance (distance attenuation, Doppler delay, air absorption)

## Self-Check: PASSED

- plugin/ParamIDs.h: FOUND, contains COMB_WET_MAX
- plugin/ParamLayout.cpp: FOUND, contains COMB_WET_MAX
- plugin/PluginProcessor.h: FOUND, contains combWetMaxParam
- plugin/PluginProcessor.cpp: FOUND, contains combWetMax
- .planning/phases/03-depth-and-elevation/03-03-SUMMARY.md: FOUND
- Commit ec548e0: FOUND in git log

---
*Phase: 03-depth-and-elevation*
*Completed: 2026-03-13*
