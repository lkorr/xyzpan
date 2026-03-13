---
phase: 6
slug: ui-and-parameter-system
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-13
---

# Phase 6 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 3.7.1 |
| **Config file** | tests/CMakeLists.txt (catch_discover_tests) |
| **Quick run command** | `ctest --test-dir build -R XYZPanTests -C Debug` |
| **Full suite command** | `ctest --test-dir build -C Debug` |
| **Estimated runtime** | ~15 seconds |

---

## Sampling Rate

- **After every task commit:** Run `ctest --test-dir build -C Debug`
- **After every plan wave:** Run `ctest --test-dir build -C Debug`
- **Before `/gsd:verify-work`:** Full suite must be green + pluginval passes
- **Max feedback latency:** ~15 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 06-01-01 | 01 | 0 | UI-07 | unit | `ctest --test-dir build -R TestPositionBridge -C Debug` | ❌ W0 | ⬜ pending |
| 06-01-02 | 01 | 0 | PARAM-01, UI-06 | unit | `ctest --test-dir build -R TestParameterLayout -C Debug` | ❌ W0 | ⬜ pending |
| 06-01-03 | 01 | 0 | PARAM-05 | unit | `ctest --test-dir build -R TestPresets -C Debug` | ❌ W0 | ⬜ pending |
| 06-01-04 | 01 | 1 | PARAM-01 | unit | `ctest --test-dir build -R TestParameterLayout -C Debug` | ❌ W0 | ⬜ pending |
| 06-01-05 | 01 | 1 | PARAM-04 | manual | pluginval session restore | n/a | ⬜ pending |
| 06-02-01 | 02 | 2 | UI-01 | unit/smoke | `ctest --test-dir build -R TestPluginProcessor -C Debug` | ❌ W0 | ⬜ pending |
| 06-02-02 | 02 | 2 | UI-02 | manual | Visual inspection: listener node at origin | n/a | ⬜ pending |
| 06-02-03 | 02 | 2 | UI-03 | manual | Drag source; verify X/Y/Z params update in DAW | n/a | ⬜ pending |
| 06-02-04 | 02 | 2 | UI-07 | unit | `ctest --test-dir build -R TestPositionBridge -C Debug` | ❌ W0 | ⬜ pending |
| 06-03-01 | 03 | 3 | UI-04 | manual | Visual inspection: LFO controls visible, waveform cycles | n/a | ⬜ pending |
| 06-03-02 | 03 | 3 | UI-05 | manual | Toggle dev panel; verify all 27+ params accessible | n/a | ⬜ pending |
| 06-03-03 | 03 | 3 | PARAM-02 | manual | Dev panel: change value, confirm audible effect | n/a | ⬜ pending |
| 06-03-04 | 03 | 3 | PARAM-03 | manual | Automate X param; listen for zipper noise | n/a | ⬜ pending |
| 06-04-01 | 04 | 4 | PARAM-05 | unit | `ctest --test-dir build -R TestPresets -C Debug` | ❌ W0 | ⬜ pending |
| 06-04-02 | 04 | 4 | UI-07 | unit | `ctest --test-dir build -R TestPositionBridge -C Debug` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/plugin/TestParameterLayout.cpp` — R parameter registration (PARAM-01), all param IDs present (UI-06)
- [ ] `tests/plugin/TestPresets.cpp` — factory preset XML load/restore round-trip (PARAM-05)
- [ ] `tests/plugin/TestPositionBridge.cpp` — lock-free atomic write/read correctness (UI-07)
- [ ] `tests/plugin/CMakeLists.txt` — `XYZPanPluginTests` target linking `XYZPan_SharedCode` or equivalent JUCE-aware target (required because existing `XYZPanTests` links only `xyzpan_engine`)

*These stubs must exist and compile (failing or skipped) before any Plan 06-01 implementation tasks begin.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Listener node visible at origin | UI-02 | OpenGL rendering — no headless GL in CI | Open editor in DAW or standalone; confirm static sphere at center |
| Source drag updates X/Y/Z in real time | UI-03 | Requires physical mouse interaction + visual feedback | Drag source node; confirm knob values update; confirm audio pans |
| LFO waveform display cycles on click | UI-04 | Requires visual inspection of paint output | Click waveform icon; confirm sine→tri→saw→square cycle |
| Dev panel toggles and shows all params | UI-05 | UI visibility / component show/hide | Toggle dev panel button; scroll to confirm all 27+ sliders present |
| Parameter automation: no zipper noise | PARAM-03 | Requires subjective listening | Automate X from −1 to 1; listen for clicks/pops at various rates |
| State save/restore across DAW close | PARAM-04 | Requires full DAW session lifecycle | Save project; close DAW; reopen; confirm all param values match |
| pluginval full validation | PARAM-01 | Requires pluginval binary execution | Run `pluginval --strictnessLevel 5 --validateInProcess "XYZPan.vst3"` |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 30s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
