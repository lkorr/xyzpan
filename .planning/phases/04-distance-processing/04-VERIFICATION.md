---
phase: 04-distance-processing
verified: 2026-03-12T05:30:00Z
status: passed
score: 12/12 must-haves verified
re_verification: false
gaps: []
human_verification:
  - test: "Doppler quality check"
    expected: "Smoothly ramping distance near-to-far and back produces a natural pitch glide without clicks or zipper artifacts"
    why_human: "Perceptual quality of doppler feel cannot be verified by RMS/impulse tests; requires ears"
  - test: "Distance attenuation feel"
    expected: "Gain rolloff feels proportional to distance; not abrupt at close range or inaudibly subtle at far range"
    why_human: "Tuning/feel judgment; kMinDistance=0.1 inverse-square gain is physically correct but perceptual acceptability needs listening"
---

# Phase 4: Distance Processing Verification Report

**Phase Goal:** Implement the full distance processing signal chain in the engine and wire all distance parameters from APVTS through to EngineParams
**Verified:** 2026-03-12T05:30:00Z
**Status:** PASSED
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

| #  | Truth | Status | Evidence |
|----|-------|--------|----------|
| 1  | A source at maximum distance is significantly quieter than at minimum distance (inverse-square gain) | VERIFIED | `Engine.cpp` line 437: `distGain = distGainSmooth_.process(distGainTarget)` where `distGainTarget = kMinDistance/dist`; DIST-01 test passes (ratio 0.35–0.65 at double distance) |
| 2  | A source at maximum distance sounds duller than at minimum distance (air absorption LPF) | VERIFIED | `Engine.cpp` lines 293–296: `airCutoffTarget` lerps from `airAbsMaxHz` (22kHz) to `airAbsMinHz` (8kHz); DIST-02 test passes (12kHz sine RMS far < near, ratio < 0.5) |
| 3  | A source at maximum distance has audible timing offset relative to minimum distance (propagation delay) | VERIFIED | `Engine.cpp` lines 442–453: `distDelayL_/R_` push/read with `distDelaySmooth_`; DIST-03 test passes (near peak < sample 20, far peak > 100) |
| 4  | Rapidly changing distance produces pitch shift (doppler from delay modulation) | VERIFIED | `Engine.cpp` line 446: `distDelaySmooth_.process(delayTargetSamples)` ramps delay per sample; DIST-04 test passes (ramp output differs from static-distance reference, diffRms > 1e-6) |
| 5  | Toggling doppler off removes all distance delay (no timing offset) | VERIFIED | `Engine.cpp` lines 449–453: `dopplerOn=false` branch reads at 2.0f (minimum), still calls `distDelaySmooth_.process(2.0f)` to keep state valid; DIST-05 test passes (peak < sample 20 with doppler off at far distance) |
| 6  | Delay line uses Hermite interpolation with no artifacts during modulation | VERIFIED | `FractionalDelayLine.read()` uses Catmull-Rom Hermite interpolation (per plan interface); DIST-06 stability test passes 10,000 samples with rapid position changes, no NaN/Inf; near-zero distance stability test also passes |
| 7  | Close sources hardpan more than distant sources (ITD and head shadow scale with proximity) | VERIFIED | `Engine.cpp` lines 203–213: `itdTarget *= proximity` and `shadowCutoffTarget` uses `* proximity` factor; hardpan test passes (`closeLRDiff > farLRDiff`, far contributes < 30% of total L/R diff) |
| 8  | All distance parameters are tuneable via dev panel controls | VERIFIED | `ParamIDs.h`: 5 Phase 4 constants; `ParamLayout.cpp`: 5 registrations; `PluginProcessor.h`: 5 atomic members; `PluginProcessor.cpp`: init + jassert + processBlock snapshot; all wired end-to-end |
| 9  | Changing distDelayMaxMs in the generic editor changes the maximum delay | VERIFIED | `PluginProcessor.cpp` line 128: `params.distDelayMaxMs = distDelayMaxMsParam->load()`; `Engine.cpp` line 288: `delayTargetMs = distFrac * currentParams.distDelayMaxMs` |
| 10 | Toggling Doppler in the generic editor enables/disables distance delay | VERIFIED | `PluginProcessor.cpp` line 130: `params.dopplerEnabled = dopplerEnabledParam->load() >= 0.5f`; `Engine.cpp` line 298: `dopplerOn = currentParams.dopplerEnabled` |
| 11 | Changing air absorption Hz parameters in the generic editor changes the LPF cutoff | VERIFIED | `PluginProcessor.cpp` lines 131–132: `params.airAbsMaxHz/MinHz = airAbsMaxHz/MinHzParam->load()`; `Engine.cpp` lines 293–296: `airCutoffTarget` computed from `currentParams.airAbsMaxHz/MinHz` |
| 12 | getTailLengthSeconds returns a value >= 0.3 to prevent DAW tail truncation | VERIFIED | `PluginProcessor.h` line 26: `double getTailLengthSeconds() const override { return 0.320; }` |

**Score:** 12/12 truths verified

---

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `engine/include/xyzpan/Constants.h` | Phase 4 constants (kDistDelayMaxMs, kDistSmoothMs, kAirAbsMaxHz, kAirAbsMinHz) | VERIFIED | Lines 116–126: all 4 constants present with correct values (300.0f, 30.0f, 22000.0f, 8000.0f) |
| `engine/include/xyzpan/Types.h` | Phase 4 EngineParams fields (distDelayMaxMs, distSmoothMs, dopplerEnabled, airAbsMaxHz, airAbsMinHz) | VERIFIED | Lines 56–60: all 5 fields with correct defaults from Constants.h |
| `engine/include/xyzpan/Engine.h` | Phase 4 private members (distDelayL_, distDelayR_, airLPF_L_, airLPF_R_, distDelaySmooth_, distGainSmooth_) | VERIFIED | Lines 129–135: all 7 members (6 DSP objects + `lastDistSmoothMs_` tracker); signal flow docstring updated at lines 29–36 |
| `engine/src/Engine.cpp` | Distance processing in per-sample loop (gain, LPF, delay+doppler) | VERIFIED | prepare() lines 109–127; process() per-block lines 272–298 and per-sample lines 430–458; reset() lines 510–516; `distGainSmooth_` present |
| `tests/engine/TestDistanceProcessing.cpp` | Unit tests for DIST-01 through DIST-06, >= 100 lines | VERIFIED | 538 lines; 8 TEST_CASEs covering DIST-01 through DIST-06 + hardpan test; all 8 pass (tests 48–55 in ctest) |
| `plugin/ParamIDs.h` | Phase 4 parameter ID constants | VERIFIED | Lines 43–47: DIST_DELAY_MAX_MS, DIST_SMOOTH_MS, DOPPLER_ENABLED, AIR_ABS_MAX_HZ, AIR_ABS_MIN_HZ |
| `plugin/ParamLayout.cpp` | Phase 4 APVTS parameter registrations | VERIFIED | Lines 185–217: 5 registrations; AudioParameterBool for DOPPLER_ENABLED; NR skew 0.3 on Hz params |
| `plugin/PluginProcessor.h` | Phase 4 atomic pointer members | VERIFIED | Lines 74–78: 5 atomic pointer members; getTailLengthSeconds = 0.320 at line 26 |
| `plugin/PluginProcessor.cpp` | Phase 4 processBlock snapshot wiring | VERIFIED | Lines 63–74: init + jassert all 5; lines 127–132: processBlock snapshot with float->bool conversion for dopplerEnabled |
| `tests/CMakeLists.txt` | TestDistanceProcessing.cpp added to test target | VERIFIED | Line 5: `engine/TestDistanceProcessing.cpp` present in XYZPanTests source list |

---

### Key Link Verification

#### Plan 04-01 Key Links

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `engine/src/Engine.cpp` | `engine/include/xyzpan/Types.h` | `currentParams.distDelayMaxMs`, `currentParams.dopplerEnabled` | WIRED | Lines 288, 293–294, 298: `currentParams.dist*` and `currentParams.airAbs*` used in per-block targets; `currentParams.dopplerEnabled` at line 298 |
| `engine/src/Engine.cpp` | `engine/include/xyzpan/Constants.h` | `kDistDelayMaxMs`, `kAirAbsMaxHz`, `kAirAbsMinHz`, `kDistSmoothMs` | WIRED | prepare() lines 113, 119–120, 123, 127 use all 4 Phase 4 constants |
| `engine/src/Engine.cpp` | `FractionalDelayLine`, `OnePoleLP`, `OnePoleSmooth` | `distDelayL_.push/read`, `airLPF_L_.process`, `distDelaySmooth_.process` | WIRED | Lines 442–453: `distDelayL_.push(dL)`, `distDelayL_.read(delaySamp)`; lines 457–458: `airLPF_L_.process(dL)`; line 446: `distDelaySmooth_.process(...)` |

#### Plan 04-02 Key Links

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `plugin/PluginProcessor.cpp` | `plugin/ParamIDs.h` | `getRawParameterValue(ParamID::DIST_DELAY_MAX_MS)` | WIRED | Lines 64–68: all 5 `getRawParameterValue(ParamID::DIST_*)` calls |
| `plugin/PluginProcessor.cpp` | `engine/include/xyzpan/Types.h` | `params.distDelayMaxMs = distDelayMaxMsParam->load()` | WIRED | Lines 128–132: all 5 fields assigned from loaded atomics; `dopplerEnabledParam->load() >= 0.5f` float->bool conversion at line 130 |
| `plugin/ParamLayout.cpp` | `engine/include/xyzpan/Constants.h` | `kDistDelayMaxMs`, `kDistSmoothMs`, `kAirAbsMaxHz`, `kAirAbsMinHz` defaults | WIRED | Lines 189, 196, 203, 209, 215: all 4 constants used as APVTS parameter defaults |

---

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| DIST-01 | 04-01 | Gain attenuation follows inverse-square law | SATISFIED | `Engine.cpp`: `distGainTarget = kMinDistance/dist`; smoothed per sample; DIST-01 test (ctest #48) passes |
| DIST-02 | 04-01 | LPF rolls off 22kHz (closest) to 8kHz (furthest) at 6dB/oct | SATISFIED | `Engine.cpp`: `airCutoffTarget` lerp from `airAbsMaxHz` to `airAbsMinHz`; OnePoleLP applied per sample; DIST-02 test (#49) passes |
| DIST-03 | 04-01 | Distance delay ranges 0ms (closest) to 300ms (furthest) | SATISFIED | `Engine.cpp`: `delayTargetMs = distFrac * currentParams.distDelayMaxMs`; FractionalDelayLine with distDelaySmooth; DIST-03 test (#50) passes |
| DIST-04 | 04-01 | Doppler shift from delay modulation over time | SATISFIED | `Engine.cpp`: `distDelaySmooth_.process(delayTargetSamples)` ramps delay per sample creating pitch shift; DIST-04 test (#51) passes |
| DIST-05 | 04-01 | Doppler shift is toggleable (off = no distance delay) | SATISFIED | `Engine.cpp`: `dopplerOn=false` branch reads at 2.0f; smoother state kept valid; DIST-05 test (#52) passes |
| DIST-06 | 04-01 | Delay line uses cubic (Hermite) interpolation | SATISFIED | FractionalDelayLine.read() uses Catmull-Rom; DIST-06 stability tests (#53, #54) pass; no NaN/Inf at extreme params |
| DIST-07 | 04-02 | All distance parameters tuneable via dev panel | SATISFIED | 5 APVTS params registered (ParamLayout.cpp); 5 atomics initialized (PluginProcessor.cpp); processBlock snapshots all 5 to EngineParams |

**All 7 Phase 4 requirements: SATISFIED**

No orphaned requirements — REQUIREMENTS.md maps exactly DIST-01 through DIST-07 to Phase 4, all covered by plans 04-01 and 04-02.

---

### Anti-Patterns Found

No anti-patterns detected in any Phase 4 modified files:
- No TODO/FIXME/PLACEHOLDER comments in engine or plugin layer
- No stub implementations (no empty handlers, no `return {}`, no `return null`)
- No unconnected wiring (all 5 parameters flow from APVTS through processBlock to EngineParams)
- Signal chain order (gain -> delay -> LPF) is intentional and documented in code comments

---

### Test Results

**Build:** Succeeds (XYZPanTests.exe built without errors)
**Total tests:** 55/55 passing (100%)
**Phase 4 tests (ctest #48–55):** 8/8 passing
**Phase 1–3 regression tests (ctest #1–47):** 47/47 passing

Notable: Phase 2/3 tests were patched to set `dopplerEnabled=false` to prevent distance delay smoother ramp from zeroing output during short measurement windows. This is correct — those tests verify binaural/elevation DSP, not distance delay behavior.

---

### Human Verification Required

#### 1. Doppler naturalness

**Test:** In a DAW, load XYZPan. Enable doppler. Automate Y from 0.1 to 1.0 over 2 seconds, then back. Listen on headphones.
**Expected:** Smooth pitch descend (source receding) then ascend (source approaching); no clicks, zipper noise, or abrupt transitions
**Why human:** kDistSmoothMs=30ms (1323 samples at 44100Hz) controls the doppler feel; the test verifies there is doppler, not whether it sounds natural

#### 2. Distance gain feel

**Test:** In a DAW, move source from Y=0.1 (closest) to Y=kSqrt3 (max). Listen to how gain rolls off.
**Expected:** Gain rolloff feels proportional and usable; close sources not unreasonably loud relative to far
**Why human:** Inverse-square law is physically correct but kMinDistance=0.1 means gain=1.0 at closest — perceptual balance is a tuning judgment

---

### Gaps Summary

No gaps. All 12 observable truths verified, all 10 required artifacts exist and are substantive and wired, all 7 key links confirmed, all 7 DIST requirements satisfied by ctest evidence. The automated test suite (55/55 passing) provides direct behavioral verification of all requirements.

---

_Verified: 2026-03-12T05:30:00Z_
_Verifier: Claude (gsd-verifier)_
