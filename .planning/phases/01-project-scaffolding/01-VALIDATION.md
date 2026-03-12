---
phase: 1
slug: project-scaffolding
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-12
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 3.7.1 |
| **Config file** | None — CMake `catch_discover_tests()` handles registration |
| **Quick run command** | `ctest --test-dir build --build-config Release -R "coordinates" --output-on-failure` |
| **Full suite command** | `ctest --test-dir build --build-config Release --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `ctest --test-dir build --build-config Release --output-on-failure`
- **After every plan wave:** Run `ctest --test-dir build --build-config Release --output-on-failure` + pluginval
- **Before `/gsd:verify-work`:** Full suite must be green + pluginval strictness 5 passes
- **Max feedback latency:** 30 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 01-01-01 | 01 | 1 | SETUP-01 | Build smoke | `cmake --build build --config Release` exits 0 | ❌ W0 | ⬜ pending |
| 01-01-02 | 01 | 1 | SETUP-02 | Build isolation | `cmake --build build --config Release --target XYZPanTests` exits 0 without juce:: | ❌ W0 | ⬜ pending |
| 01-01-03 | 01 | 1 | SETUP-04 | Unit | `ctest --test-dir build --build-config Release` | ❌ W0 | ⬜ pending |
| 01-02-01 | 02 | 1 | COORD-01 | Unit | `ctest -R "clamping"` | ❌ W0 | ⬜ pending |
| 01-02-02 | 02 | 1 | COORD-02 | Unit | `ctest -R "cardinal"` | ❌ W0 | ⬜ pending |
| 01-02-03 | 02 | 1 | COORD-03 | Unit | `ctest -R "cardinal"` | ❌ W0 | ⬜ pending |
| 01-02-04 | 02 | 1 | COORD-04 | Unit | `ctest -R "distance"` | ❌ W0 | ⬜ pending |
| 01-02-05 | 02 | 1 | COORD-05 | Unit | `ctest -R "independent"` | ❌ W0 | ⬜ pending |
| 01-03-01 | 03 | 2 | SETUP-03 | Manual / pluginval | pluginval validates audio path end-to-end | ❌ W0 | ⬜ pending |
| 01-03-02 | 03 | 2 | SETUP-05 | Plugin validation | `pluginval.exe --strictness-level 5 XYZPan.vst3` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/CMakeLists.txt` — test executable definition + `catch_discover_tests()`
- [ ] `tests/engine/TestCoordinates.cpp` — ~15-20 Catch2 test cases for COORD-01 through COORD-05
- [ ] `engine/CMakeLists.txt` — static library with no JUCE links (proves SETUP-02)
- [ ] `engine/include/xyzpan/Types.h` — EngineParams, SphericalCoord structs
- [ ] `engine/include/xyzpan/Constants.h` — kMinDistance, kMaxInputXYZ
- [ ] `engine/include/xyzpan/Coordinates.h` — toSpherical(), computeDistance() declarations
- [ ] `engine/src/Coordinates.cpp` — implementation
- [ ] `engine/include/xyzpan/Engine.h` — XYZPanEngine class
- [ ] `engine/src/Engine.cpp` — prepare/process/reset implementation
- [ ] `plugin/CMakeLists.txt` — juce_add_plugin target
- [ ] `plugin/PluginProcessor.h/.cpp` — APVTS, processBlock, isBusesLayoutSupported
- [ ] `plugin/PluginEditor.h/.cpp` — GenericAudioProcessorEditor
- [ ] `plugin/ParamIDs.h` — X, Y, Z string constants
- [ ] `plugin/ParamLayout.cpp` — createParameterLayout()
- [ ] `ui/CMakeLists.txt` — INTERFACE library placeholder
- [ ] `cmake/CPM.cmake` — CPM script
- [ ] `CMakeLists.txt` — root CMake file

*Framework install: CPM fetches JUCE 8.0.12 and Catch2 3.7.1 automatically on first cmake configure.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Plugin loads in DAW | SETUP-03 | Requires running DAW | Load VST3 in REAPER/FL Studio, verify mono-in stereo-out |
| pluginval strictness 5 | SETUP-05 | External tool | `pluginval.exe --strictness-level 5 "build/XYZPan_artefacts/Release/VST3/XYZPan.vst3"` |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 30s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
