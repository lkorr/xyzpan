---
phase: 03-depth-and-elevation
plan: 01
subsystem: dsp
tags: [cpp, dsp, feedback-comb-filter, svf, biquad, one-pole, audio-eq-cookbook, catch2]

# Dependency graph
requires:
  - phase: 02-binaural-panning-core
    provides: SVFLowPass, FractionalDelayLine, OnePoleSmooth patterns; EngineParams Phase 2 fields
provides:
  - FeedbackCombFilter: IIR comb filter with hard-clamped feedback [-0.95, 0.95]
  - SVFFilter: generalised TPT SVF with LP/HP/BP/Notch mode selection
  - BiquadFilter: Audio EQ Cookbook peaking EQ, high shelf, low shelf (Direct Form II)
  - OnePoleLP: first-order 6 dB/oct lowpass parameterised by cutoff Hz
  - Constants.h Phase 3 block: kMaxCombFilters, kCombDefaultDelays_ms, kCombDefaultFeedback, kCombMaxWet, kCombMaxDelay_ms, all elevation defaults
  - EngineParams Phase 3 fields: combDelays_ms[10], combFeedback[10], combWetMax, and 7 elevation parameters
  - 13 unit tests for all 4 DSP primitives (40 total, 27 existing)
affects: [03-02-engine-integration, 03-03-apvts-phase3]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Power-of-2 ring buffer with bitmask wraparound (FeedbackCombFilter, same pattern as FractionalDelayLine)"
    - "Audio EQ Cookbook Direct Form II biquad with per-block coefficient update"
    - "TPT SVF generalised to LP/HP/BP/Notch via switch on type (parallel to SVFLowPass)"
    - "First-order IIR LP via a=exp(-2pi*fc/sr), b=1-a (distinct from OnePoleSmooth's time-constant API)"

key-files:
  created:
    - engine/include/xyzpan/dsp/FeedbackCombFilter.h
    - engine/include/xyzpan/dsp/SVFFilter.h
    - engine/include/xyzpan/dsp/BiquadFilter.h
    - engine/include/xyzpan/dsp/OnePoleLP.h
    - tests/engine/TestDepthAndElevation.cpp
  modified:
    - engine/include/xyzpan/Constants.h
    - engine/include/xyzpan/Types.h
    - tests/CMakeLists.txt

key-decisions:
  - "FeedbackCombFilter uses integer-only delay (no fractional interpolation) — comb filters don't require sub-sample accuracy"
  - "BiquadFilter uses Direct Form II with per-block coefficient updates — transcendental functions (cos/sin/pow/sqrt) are too expensive for per-sample use"
  - "SVFFilter is a parallel class to SVFLowPass, not a replacement — Phase 2 engine still uses SVFLowPass; changing it would risk regression"
  - "OnePoleLP is distinct from OnePoleSmooth: cutoff-Hz API vs smoothing-ms API — same math, different parameterisation"
  - "EngineParams array defaults hardcoded as inline initializer lists (not referencing constexpr arrays directly) — C++ does not allow constexpr array as default member initializer"

patterns-established:
  - "TDD RED/GREEN pattern: build compilation as RED signal for headers-only tasks"
  - "Skip 64-512 transient samples in RMS measurements to avoid startup artifacts"
  - "Feedback clamp verification: process 1000 samples and verify maxAbs < 50 (divergence test)"

requirements-completed: [DEPTH-01, DEPTH-02, DEPTH-04]

# Metrics
duration: 7min
completed: 2026-03-12
---

# Phase 3 Plan 01: DSP Primitives and Types Extension Summary

**Four header-only DSP classes (FeedbackCombFilter, SVFFilter, BiquadFilter, OnePoleLP) with 13 unit tests — all Phase 3 building blocks ready for Engine integration in Plan 02**

## Performance

- **Duration:** 7 min
- **Started:** 2026-03-12T06:09:55Z
- **Completed:** 2026-03-12T06:16:52Z
- **Tasks:** 2
- **Files modified:** 7 (4 created headers, 1 created test file, 2 modified existing headers, 1 modified CMakeLists)

## Accomplishments

- Created 4 header-only DSP classes covering all Phase 3 filter types (feedback comb, generalised SVF, peaking/shelf biquad, one-pole LP)
- Extended `Constants.h` with 12 new Phase 3 constants (comb bank + elevation defaults)
- Extended `EngineParams` in `Types.h` with all 10 Phase 3 depth and elevation fields
- Wrote 13 unit tests covering all new DSP primitives with RMS-based frequency-domain verification
- 40 total tests pass with zero regressions (27 existing + 13 new)

## Task Commits

Each task was committed atomically:

1. **Task 1: Create DSP primitives and extend Types/Constants** - `fd09da5` (feat)
2. **Task 2: Unit tests for all Phase 3 DSP primitives** - `89124c4` (test)

**Plan metadata:** (docs commit — see below)

## Files Created/Modified

- `engine/include/xyzpan/dsp/FeedbackCombFilter.h` - IIR feedback comb: y[n] = x[n] + g*y[n-M], hard-clamped feedback
- `engine/include/xyzpan/dsp/SVFFilter.h` - Generalised TPT SVF with LP/HP/BP/Notch mode enum
- `engine/include/xyzpan/dsp/BiquadFilter.h` - Audio EQ Cookbook peaking EQ + shelf filters (Direct Form II)
- `engine/include/xyzpan/dsp/OnePoleLP.h` - First-order 6 dB/oct lowpass parameterised by cutoff Hz
- `engine/include/xyzpan/Constants.h` - Phase 3 comb bank and elevation default constants added
- `engine/include/xyzpan/Types.h` - EngineParams extended with Phase 3 depth and elevation fields
- `tests/engine/TestDepthAndElevation.cpp` - 13 unit tests for all 4 DSP primitives
- `tests/CMakeLists.txt` - TestDepthAndElevation.cpp added to XYZPanTests target

## Decisions Made

- FeedbackCombFilter uses integer-only delay — comb filters don't require sub-sample accuracy, simplifying the implementation vs FractionalDelayLine
- BiquadFilter coefficient update is marked "per-block only" in the header comment because `std::cos/sin/pow/sqrt` are expensive at audio rate; Phase 3 engine integration must respect this
- SVFFilter is a parallel class, not a replacement for SVFLowPass — this isolates Phase 2's head/rear shadow filters from any Phase 3 changes
- EngineParams array default initializers are hardcoded inline (not referencing `kCombDefaultDelays_ms` directly) because C++ disallows constexpr array as default member initializer in a struct
- OnePoleLP uses `setCoefficients(float cutoffHz, float sampleRate)` to distinguish it from `OnePoleSmooth::prepare(float smoothingMs, float sampleRate)` — same mathematical kernel, different parameterisation

## Deviations from Plan

None — plan executed exactly as written.

## Issues Encountered

None — all 4 headers compiled first attempt, all 13 tests passed first run.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

- All 4 DSP primitives ready for Plan 02 Engine integration
- EngineParams has all required Phase 3 fields with correct defaults
- Constants.h has all required Phase 3 defaults referenced by FeedbackCombFilter and the future engine
- No blockers or concerns

## Self-Check: PASSED

- FOUND: engine/include/xyzpan/dsp/FeedbackCombFilter.h
- FOUND: engine/include/xyzpan/dsp/SVFFilter.h
- FOUND: engine/include/xyzpan/dsp/BiquadFilter.h
- FOUND: engine/include/xyzpan/dsp/OnePoleLP.h
- FOUND: tests/engine/TestDepthAndElevation.cpp
- FOUND commit fd09da5 (feat: DSP primitives)
- FOUND commit 89124c4 (test: unit tests)
- All 40 tests pass (27 existing + 13 new)

---
*Phase: 03-depth-and-elevation*
*Completed: 2026-03-12*
