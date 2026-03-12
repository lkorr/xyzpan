---
phase: 2
slug: binaural-panning-core
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-12
---

# Phase 2 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 3.7.1 |
| **Config file** | Root CMakeLists.txt (enable_testing() + catch_discover_tests()) |
| **Quick run command** | `ctest --test-dir build -R "binaural" --output-on-failure` |
| **Full suite command** | `ctest --test-dir build --output-on-failure` |
| **Estimated runtime** | ~2 seconds |

---

## Sampling Rate

- **After every task commit:** Run `ctest --test-dir build --output-on-failure`
- **After every plan wave:** Run `ctest --test-dir build --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 02-01-01 | 01 | 1 | PAN-01 | unit | `ctest --test-dir build -R "ITD" --output-on-failure` | ❌ W0 | ⬜ pending |
| 02-01-02 | 01 | 1 | PAN-01 | unit | `ctest --test-dir build -R "DelayLine" --output-on-failure` | ❌ W0 | ⬜ pending |
| 02-01-03 | 01 | 1 | PAN-02 | unit | `ctest --test-dir build -R "HeadShadow" --output-on-failure` | ❌ W0 | ⬜ pending |
| 02-01-04 | 01 | 1 | PAN-03 | unit | `ctest --test-dir build -R "PannerOutput" --output-on-failure` | ❌ W0 | ⬜ pending |
| 02-02-01 | 02 | 1 | PAN-04 | unit | `ctest --test-dir build -R "StereoSum" --output-on-failure` | ✅ existing | ⬜ pending |
| 02-02-02 | 02 | 1 | PAN-05 | unit | `ctest --test-dir build -R "Automation" --output-on-failure` | ❌ W0 | ⬜ pending |
| 02-02-03 | 02 | 1 | PAN-05 | unit | `ctest --test-dir build -R "Smoother" --output-on-failure` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/engine/TestBinauralPanning.cpp` — stubs for PAN-01 through PAN-05 (ITD, delay line, head shadow, panner output, automation, smoother)
- [ ] `tests/CMakeLists.txt` — add `engine/TestBinauralPanning.cpp` to `XYZPanTests` target

*No new framework install needed — Catch2 already wired.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Spatial image sounds convincing and natural | PAN-01, PAN-02 | Perceptual quality — no automated metric for "convincing" | Listen with headphones; pan source left/right; verify subjective spatial width and timbre darkening |
| Automation sweep sounds smooth without clicks | PAN-05 | Perceptual artifact detection — clicks below automated threshold may still be audible | Automate X from -1 to +1 over 100ms; listen for clicks, pops, or zipper noise |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
