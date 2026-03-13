---
phase: 05-creative-tools
plan: 01
subsystem: engine-dsp
tags: [reverb, fdn, spatial-audio, dsp, tdd]
dependency_graph:
  requires: [04-01, 04-02]
  provides: [fdn-reverb-engine, verb-constants, verb-params]
  affects: [Engine.cpp, Types.h, Constants.h]
tech_stack:
  added: [FDNReverb (4-delay Householder FDN), one-pole LP damping per loop, Hermite pre-delay line]
  patterns: [T60 decay formula, Householder feedback matrix, distance-scaled pre-delay, Engine-level wet/dry smoothing]
key_files:
  created:
    - engine/include/xyzpan/dsp/FDNReverb.h
    - engine/src/FDNReverb.cpp
    - tests/engine/TestCreativeTools.cpp
  modified:
    - engine/include/xyzpan/Constants.h
    - engine/include/xyzpan/Types.h
    - engine/include/xyzpan/Engine.h
    - engine/src/Engine.cpp
    - engine/CMakeLists.txt
    - tests/CMakeLists.txt
decisions:
  - FDNReverb internal wetGain_ fixed at 1.0; Engine applies smoothed verbWetSmooth_ externally for click-free transitions
  - VERB-02 test uses wet-minus-dry subtraction to isolate reverb onset from dry signal
  - Pre-delay distFrac computed from block-rate dist (not per-sample) — consistent with other distance parameters
  - setSize() updates delayLengths_ immediately; not called per-block to avoid pitch artifacts
metrics:
  duration: 11 min
  completed: "2026-03-13"
  tasks_completed: 3
  files_created: 3
  files_modified: 5
---

# Phase 5 Plan 01: FDN Reverb Engine — Summary

FDN algorithmic reverb integrated as the final stereo stage of the signal chain, with distance-scaled pre-delay and full stability at T60=5s.

## What Was Built

### FDNReverb DSP Primitive (`engine/include/xyzpan/dsp/FDNReverb.h`, `engine/src/FDNReverb.cpp`)
4-delay Householder feedback delay network with:
- Pre-delay line (FractionalDelayLine) scaled by source distance for VERB-02
- Householder feedback matrix: `fb[i] = sum*0.5 - x[i]` (energy-preserving)
- One-pole LP damping per delay loop for `verbDamping` control
- T60 feedback gain formula: `feedbackGain = pow(10, -3 * maxDelayMs / (1000 * t60))`, clamped to [0, 0.999]
- All delay lines sized for 192kHz worst case — no reallocation on sample rate change

### Constants/Types Extension
- Phase 5 reverb constants added to `Constants.h`: `kVerbDefaultSize`, `kVerbDefaultDecay`, `kVerbDefaultDamping`, `kVerbDefaultWet`, `kVerbPreDelayMaxMs`, `kVerbMaxDecayT60_s`, `kFDNDelayMs[4]`
- `EngineParams` extended with: `verbSize`, `verbDecay`, `verbDamping`, `verbWet`, `verbPreDelayMax`

### Engine Integration (`Engine.h`, `Engine.cpp`)
- FDN reverb appended as step 8 in the per-sample signal chain after air LPF
- `verbWetSmooth_` (OnePoleSmooth) provides click-free wet/dry transitions
- Per-block: `reverb_.setDecay()` and `reverb_.setDamping()` updated from `currentParams`
- Pre-delay scales with distance: `distFrac * verbPreDelayMax * sampleRate / 1000`
- Phase 5 reset in `reset()`: clears FDN state and snaps wet smoother to 0.0

### Test Scaffold (`tests/engine/TestCreativeTools.cpp`)
- VERB-01: confirms wet output measurably differs from dry (sample-level comparison)
- VERB-02: wet-minus-dry subtraction isolates reverb onset; far (2891) >> near (688) samples
- VERB-03: EngineParams field accessors confirmed (stub — APVTS test deferred to Plan 05-02)
- VERB-04: 100000 samples at decay=1.0; growth ratio 0.018x (decaying, not growing)
- LFO-01..05: FAIL stubs for Plan 05-02

## Test Results

```
64 test cases | 59 passed | 5 failed (LFO stubs — expected)
VERB-01: PASS  (reverb output differs from dry)
VERB-02: PASS  (near onset=688, far onset=2891 samples)
VERB-03: PASS  (EngineParams fields verified)
VERB-04: PASS  (no NaN/Inf; growth ratio=0.018, stable)
LFO-01..05: FAIL (not yet implemented — Plan 05-02 stubs)
All 55 prior-phase tests: GREEN
```

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] FDNReverb internal wetGain_ was 0.0 after prepare(), zeroing all wet output**
- **Found during:** Task 3 verification (VERB-01 failing — anyDiff was false)
- **Issue:** `reverb_.setWetDry(kVerbDefaultWet)` in prepare() set wetGain_=0.0. The Engine's wet mixing used `wetGain * wetL` where `wetL` already had wetGain_=0.0 baked in. Net result: always zero wet output.
- **Fix:** Changed prepare() to call `reverb_.setWetDry(1.0f)` so FDNReverb returns raw reverb signal. Engine's `verbWetSmooth_` provides the only wet gain multiplier.
- **Files modified:** `engine/src/Engine.cpp`
- **Commit:** c430858

**2. [Rule 1 - Bug] VERB-02 test onset detection captured dry signal (same for near and far), not reverb onset**
- **Found during:** Task 3 verification (VERB-02 failing — farOnset == nearOnset == 4)
- **Issue:** Both near and far engines produced dry signal at sample ~4 (from ITD/ILD delays), making onset detection identical regardless of pre-delay settings.
- **Fix:** Redesigned test to subtract dry-only output from wet output, isolating the reverb contribution. Search starts at sample 20 (past dry impulse). Near onset=688, far onset=2891 — clear and correct difference.
- **Files modified:** `tests/engine/TestCreativeTools.cpp`
- **Commit:** c430858

## Self-Check: PASSED

Files verified:
- FOUND: engine/include/xyzpan/dsp/FDNReverb.h
- FOUND: engine/src/FDNReverb.cpp
- FOUND: tests/engine/TestCreativeTools.cpp
- FOUND: .planning/phases/05-creative-tools/05-01-SUMMARY.md

Commits verified:
- FOUND: d491d5a (test scaffold)
- FOUND: 54b083c (FDNReverb + Constants/Types)
- FOUND: c430858 (engine integration)
