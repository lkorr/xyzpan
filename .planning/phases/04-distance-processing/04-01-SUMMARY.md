---
phase: 04-distance-processing
plan: 01
subsystem: dsp
tags: [distance, doppler, hermite, delay-line, one-pole-lp, inverse-square, air-absorption, proximity]

requires:
  - phase: 03-depth-and-elevation
    provides: Engine.cpp signal chain with floor bounce, OnePoleLP, FractionalDelayLine
  - phase: 02-binaural-panning-core
    provides: ITD/ILD/head shadow binaural split, proximity computation in process()
provides:
  - "Inverse-square gain attenuation: distGainSmooth (OnePoleSmooth) applied per sample"
  - "Air absorption LPF: OnePoleLP per-block coefficient update (airLPF_L_, airLPF_R_)"
  - "Propagation delay + doppler: FractionalDelayLine (distDelayL_, distDelayR_) with distDelaySmooth"
  - "dopplerEnabled flag: disables long delay (reads at 2.0f), keeps smoother state valid"
  - "Proximity-scaled ITD and head shadow: itdTarget and shadowCutoffTarget both multiplied by proximity"
  - "Phase 4 constants: kDistDelayMaxMs, kDistSmoothMs, kAirAbsMaxHz, kAirAbsMinHz"
  - "Phase 4 EngineParams fields: distDelayMaxMs, distSmoothMs, dopplerEnabled, airAbsMaxHz, airAbsMinHz"
  - "55 passing tests (47 prior + 8 new DIST-01 through DIST-06)"
affects: [05-ui-engine-bridge, 06-juce-plugin-wrapper, 07-release-packaging]

tech-stack:
  added: []
  patterns:
    - "Per-block coefficient update for OnePoleLP (matches BiquadFilter pattern from Phase 3)"
    - "Delay smoother re-prepare on distSmoothMs change (matches smoothMs_ITD/Filter/Gain pattern)"
    - "dopplerEnabled=false in Phase 3 integration tests to avoid delay smoother ramp interference"
    - "settleAndProcess helper in distance tests: settle all smoothers before measuring"

key-files:
  created:
    - "engine/include/xyzpan/Constants.h -- Phase 4 block with 4 new constants"
    - "engine/include/xyzpan/Types.h -- Phase 4 EngineParams fields (5 new fields)"
    - "engine/include/xyzpan/Engine.h -- Phase 4 private members (7 new members)"
    - "engine/src/Engine.cpp -- Phase 4 prepare/process/reset sections"
    - "tests/engine/TestDistanceProcessing.cpp -- 8 test cases for DIST-01 through DIST-06"
  modified:
    - "tests/engine/TestBinauralPanning.cpp -- dopplerEnabled=false in 3 ITD/panning tests"
    - "tests/engine/TestDepthAndElevation.cpp -- dopplerEnabled=false in runEngine helper"
    - "tests/CMakeLists.txt -- added TestDistanceProcessing.cpp"

key-decisions:
  - "Proximity scaling applied to ITD and head shadow targets (itdTarget * proximity, shadowCutoffTarget uses * proximity factor) so close sources hardpan more than distant"
  - "Signal chain order: gain -> delay+doppler -> air LPF (gain first so quieter signal enters delay, LPF after to filter the arriving signal per research recommendation)"
  - "Distance delay lines sized for 192kHz worst case (57608 samples) regardless of runtime sample rate"
  - "dopplerEnabled=false reads delay at 2.0f but still calls distDelaySmooth_.process(2.0f) to keep smoother state valid (prevents jump on re-enable)"
  - "Existing Phase 2/3 integration tests updated with dopplerEnabled=false to prevent delay smoother ramp from zeroing 2048-4096 sample output windows"
  - "settleAndProcess test helper provides stable output by converging smoothers before measurement"

patterns-established:
  - "Distance test pattern: settleAndProcess helper with 4096-44100 sample settle window depending on expected delay"
  - "Phase 3 integration tests require dopplerEnabled=false when testing DSP effects at y=1 (dist=1.0) positions"

requirements-completed: [DIST-01, DIST-02, DIST-03, DIST-04, DIST-05, DIST-06]

duration: 11min
completed: 2026-03-13
---

# Phase 4 Plan 01: Distance Processing Summary

**Inverse-square gain, air absorption one-pole LPF, Hermite-interpolated propagation delay with doppler, and proximity-scaled ITD/head shadow — all distance cues implemented with 8 new tests, 55 total passing**

## Performance

- **Duration:** 11 min
- **Started:** 2026-03-13T04:59:14Z
- **Completed:** 2026-03-13T05:10:00Z
- **Tasks:** 2
- **Files modified:** 8

## Accomplishments

- Distance processing stage added after floor bounce in Engine::process(): gain attenuation (DIST-01), propagation delay + doppler (DIST-03/04/05/06), air absorption LPF (DIST-02)
- ITD and head shadow now scale with proximity — close sources hardpan more, distant sources collapse toward center (matching ILD behavior that already did this)
- Full test suite: 8 new test cases covering all DIST requirements, 55/55 total tests passing

## Task Commits

1. **Task 1: Constants, Types, Engine members, and DSP implementation** - `eb10c89` (feat)
2. **Task 2: Distance processing integration tests** - `caa7ee2` (feat)

**Plan metadata:** (created in final commit)

## Files Created/Modified

- `engine/include/xyzpan/Constants.h` — Phase 4 block: kDistDelayMaxMs (300ms), kDistSmoothMs (30ms), kAirAbsMaxHz (22000Hz), kAirAbsMinHz (8000Hz)
- `engine/include/xyzpan/Types.h` — 5 new EngineParams fields for Phase 4 distance control
- `engine/include/xyzpan/Engine.h` — 7 new private members (distDelayL/R, airLPF_L/R, distDelaySmooth, distGainSmooth, lastDistSmoothMs), updated signal flow docstring
- `engine/src/Engine.cpp` — Phase 4 sections in prepare(), process() (per-block targets + per-sample loop), reset()
- `tests/engine/TestDistanceProcessing.cpp` — 8 test cases: DIST-01 through DIST-06 + hardpan test
- `tests/engine/TestBinauralPanning.cpp` — dopplerEnabled=false in 3 ITD/panning tests to prevent delay ramp interference
- `tests/engine/TestDepthAndElevation.cpp` — dopplerEnabled=false in runEngine helper for same reason
- `tests/CMakeLists.txt` — added TestDistanceProcessing.cpp to test executable

## Decisions Made

- Proximity scaling applied to ITD and head shadow: `itdTarget * proximity` and `shadowCutoffTarget` interpolated with `* proximity` factor. Brings these two cues in line with ILD which already used proximity.
- Signal chain order: gain -> delay+doppler -> LPF. Gain first (physically more accurate, quieter signal into delay), LPF last (air absorption filters the arriving signal, matching how sound travels through air).
- Delay lines sized for 192kHz worst case (300ms * 192000 + 8 = 57608 samples) regardless of runtime sample rate, matching the Phase 2 pattern of over-provisioning for future sample rates.
- `dopplerEnabled=false` still processes distGainSmooth and airLPF — it only skips the long propagation delay. Smoother state is kept valid by calling `distDelaySmooth_.process(2.0f)` even when OFF.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Existing Phase 2/3 integration tests broken by distance delay smoother ramp**
- **Found during:** Task 1 verification (running ctest after Engine.cpp changes)
- **Issue:** 8 existing tests failed because the distance delay smoother (kDistSmoothMs=30ms, 1323 samples at 44100Hz) starts at 2.0f and ramps toward large values (e.g. 7276 samples at y=1, dist=1.0). During a 2048-4096 sample test window, the delay grows to exceed the buffer length, zeroing all output. Tests using impulse-peak-finding produced peakL=0, peakR=0. RMS-based tests got near-zero values, failing both absolute and ratio assertions.
- **Fix:** Added `dopplerEnabled=false` to 3 TestBinauralPanning.cpp test params (ITD delay tests and L!=R test) and to the `runEngine` helper in TestDepthAndElevation.cpp. With doppler off, the delay reads at 2.0f (minimum), so all signal passes through. The Phase 3 tests verify comb/pinna/elevation behavior — they don't need distance delay.
- **Files modified:** `tests/engine/TestBinauralPanning.cpp`, `tests/engine/TestDepthAndElevation.cpp`
- **Verification:** All 47 prior tests pass after fix (ctest 100%)
- **Committed in:** `eb10c89` (Task 1 commit, part of infrastructure changes)

---

**Total deviations:** 1 auto-fixed (Rule 1 - Bug)
**Impact on plan:** Fix was necessary for correctness — distance delay is designed behavior, not a regression in Engine.cpp. The test fix preserves each test's original intent (test the Phase 3 DSP, not distance delay behavior).

## Issues Encountered

None beyond the deviation above.

## Next Phase Readiness

- Full distance cue chain complete: gain rolloff, spectral darkening, timing offset, doppler
- All 6 DIST requirements verified by automated tests
- Engine.h/Types.h Phase 4 fields ready for Phase 5 APVTS parameter wiring
- dopplerEnabled, distDelayMaxMs, distSmoothMs, airAbsMaxHz, airAbsMinHz all need APVTS params in Phase 5

## Self-Check: PASSED

- engine/include/xyzpan/Constants.h: FOUND (kDistDelayMaxMs present)
- engine/include/xyzpan/Types.h: FOUND (distDelayMaxMs present)
- engine/include/xyzpan/Engine.h: FOUND (distDelayL_ present)
- engine/src/Engine.cpp: FOUND (distGainSmooth_ present)
- tests/engine/TestDistanceProcessing.cpp: FOUND (538 lines, > 100 minimum)
- Commits eb10c89 and caa7ee2: VERIFIED in git log
- 55/55 tests passing: VERIFIED by ctest

---
*Phase: 04-distance-processing*
*Completed: 2026-03-13*
