---
phase: 04-distance-processing
plan: 02
subsystem: plugin
tags: [juce, apvts, parameters, distance, doppler, air-absorption, vst3]

# Dependency graph
requires:
  - phase: 04-distance-processing/04-01
    provides: Phase 4 EngineParams fields (distDelayMaxMs, distSmoothMs, dopplerEnabled, airAbsMaxHz, airAbsMinHz) and Constants.h kDist*/kAirAbs* values
provides:
  - 5 Phase 4 APVTS parameter ID constants in ParamIDs.h
  - 5 Phase 4 APVTS parameter registrations in ParamLayout.cpp (AudioParameterBool for Doppler, NR skew 0.3 for Hz params)
  - 5 atomic pointer members in PluginProcessor.h, initialized and jasserted in constructor
  - processBlock snapshot of all 5 Phase 4 fields to EngineParams (float->bool for dopplerEnabled)
  - getTailLengthSeconds() = 0.320 (300ms distance delay + 20ms floor bounce)
affects: [05-custom-ui, 06-state-persistence]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "AudioParameterBool registered same as APF but with getRawParameterValue returning float (0/1) — cast via >= 0.5f"
    - "Hz parameters use NR skew 0.3 for log-like generic editor feel (consistent with Phase 2/3 convention)"
    - "getTailLengthSeconds accounts for maximum DSP latency tail: distance delay + floor bounce"

key-files:
  created: []
  modified:
    - plugin/ParamIDs.h
    - plugin/ParamLayout.cpp
    - plugin/PluginProcessor.h
    - plugin/PluginProcessor.cpp

key-decisions:
  - "AudioParameterBool uses getRawParameterValue same as APF — returns std::atomic<float>* with value 0.0f or 1.0f; cast to bool via >= 0.5f in processBlock"
  - "getTailLengthSeconds updated to 0.320 to prevent DAW tail truncation: 300ms max distance delay + 20ms floor bounce"

patterns-established:
  - "Phase 4 3-layer wiring follows identical pattern to Phases 2 and 3: ParamIDs.h -> ParamLayout.cpp -> PluginProcessor.h/cpp"

requirements-completed: [DIST-07]

# Metrics
duration: 2min
completed: 2026-03-12
---

# Phase 4 Plan 02: Distance Processing APVTS Wiring Summary

**5 Phase 4 distance parameters wired through the 3-layer APVTS pattern (ParamIDs -> ParamLayout -> PluginProcessor) with AudioParameterBool for Doppler toggle and getTailLengthSeconds updated to 0.320**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-12T04:13:46Z
- **Completed:** 2026-03-12T04:15:55Z
- **Tasks:** 1
- **Files modified:** 4

## Accomplishments
- Added DIST_DELAY_MAX_MS, DIST_SMOOTH_MS, DOPPLER_ENABLED, AIR_ABS_MAX_HZ, AIR_ABS_MIN_HZ to ParamIDs.h
- Registered all 5 parameters in ParamLayout.cpp with correct ranges and defaults from Constants.h (Doppler as AudioParameterBool, Hz params with NR skew 0.3)
- Added 5 atomic pointer members to PluginProcessor.h with constructor initialization and jasserts
- processBlock snapshots all 5 fields to EngineParams (dopplerEnabledParam->load() >= 0.5f for bool conversion)
- getTailLengthSeconds updated from 0.0 to 0.320 to prevent DAW tail truncation

## Task Commits

Each task was committed atomically:

1. **Task 1: APVTS parameter wiring and tail length for Phase 4 distance** - `0430f24` (feat)

**Plan metadata:** (docs commit follows)

## Files Created/Modified
- `plugin/ParamIDs.h` - Added 5 Phase 4 parameter ID constants after FLOOR_GAIN_DB
- `plugin/ParamLayout.cpp` - Added 5 Phase 4 parameter registrations before return layout
- `plugin/PluginProcessor.h` - Added 5 atomic pointer members; updated getTailLengthSeconds to 0.320
- `plugin/PluginProcessor.cpp` - Constructor: init + jassert all 5; processBlock: snapshot all 5

## Decisions Made
- AudioParameterBool stores as float 0/1 in the APVTS atomic — getRawParameterValue works the same as for APF; cast to bool in processBlock via `>= 0.5f`
- getTailLengthSeconds set to 0.320 (300ms + 20ms) to ensure DAW does not cut off distance-delayed signal tails

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All Phase 4 distance parameters are now tunable via the DAW generic editor
- Distance DSP (Plan 04-01) fully connected to APVTS (Plan 04-02)
- Phase 4 is complete — ready for Phase 5 (Custom UI)

## Self-Check: PASSED

- FOUND: plugin/ParamIDs.h (5 Phase 4 constants)
- FOUND: plugin/ParamLayout.cpp (5 Phase 4 registrations)
- FOUND: plugin/PluginProcessor.h (5 atomic members, getTailLengthSeconds=0.320)
- FOUND: plugin/PluginProcessor.cpp (constructor init + jasserts, processBlock snapshot)
- FOUND: 04-02-SUMMARY.md
- FOUND: commit 0430f24

---
*Phase: 04-distance-processing*
*Completed: 2026-03-12*
