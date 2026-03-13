---
phase: 3
slug: depth-and-elevation
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-12
---

# Phase 3 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 3.7.1 |
| **Config file** | Root CMakeLists.txt (enable_testing() + catch_discover_tests()) |
| **Quick run command** | `ctest --test-dir build -R "DepthElevation" --output-on-failure` |
| **Full suite command** | `ctest --test-dir build --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `ctest --test-dir build --output-on-failure`
- **After every plan wave:** Run `ctest --test-dir build --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 10 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 03-01-01 | 01 | 1 | DEPTH-01 | unit | `ctest --test-dir build -R "CombFilter" --output-on-failure` | ❌ W0 | ⬜ pending |
| 03-01-02 | 01 | 1 | DEPTH-01 | unit | `ctest --test-dir build -R "CombFilter" --output-on-failure` | ❌ W0 | ⬜ pending |
| 03-01-03 | 01 | 1 | DEPTH-02 | unit | `ctest --test-dir build -R "CombDelay" --output-on-failure` | ❌ W0 | ⬜ pending |
| 03-01-04 | 01 | 1 | DEPTH-03 | unit | `ctest --test-dir build -R "CombWet" --output-on-failure` | ❌ W0 | ⬜ pending |
| 03-01-05 | 01 | 1 | DEPTH-04 | unit | `ctest --test-dir build -R "CombStability" --output-on-failure` | ❌ W0 | ⬜ pending |
| 03-02-01 | 02 | 1 | ELEV-01 | unit | `ctest --test-dir build -R "PinnaNotch" --output-on-failure` | ❌ W0 | ⬜ pending |
| 03-02-02 | 02 | 1 | ELEV-01 | unit | `ctest --test-dir build -R "HighShelf" --output-on-failure` | ❌ W0 | ⬜ pending |
| 03-02-03 | 02 | 1 | ELEV-02 | unit | `ctest --test-dir build -R "PinnaFreeze" --output-on-failure` | ❌ W0 | ⬜ pending |
| 03-03-01 | 03 | 1 | ELEV-03 | unit | `ctest --test-dir build -R "ChestBounce" --output-on-failure` | ❌ W0 | ⬜ pending |
| 03-03-02 | 03 | 1 | ELEV-03 | unit | `ctest --test-dir build -R "ChestBounce" --output-on-failure` | ❌ W0 | ⬜ pending |
| 03-03-03 | 03 | 1 | ELEV-04 | unit | `ctest --test-dir build -R "FloorBounce" --output-on-failure` | ❌ W0 | ⬜ pending |
| 03-03-04 | 03 | 1 | ELEV-05 | unit | `ctest --test-dir build -R "Phase3Integration" --output-on-failure` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/engine/TestDepthAndElevation.cpp` — stubs for DEPTH-01 through ELEV-05
- [ ] `tests/CMakeLists.txt` — add `engine/TestDepthAndElevation.cpp` to `XYZPanTests` target

*(No new framework install needed — Catch2 already wired)*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Front/back perceptually distinguishable | DEPTH-01 | Perceptual judgment | 1. Load audio source 2. Move Y from 1 to -1 3. Verify tonal change is audible |
| Above sounds different from ear level | ELEV-01 | Perceptual judgment | 1. Set Z=0 (ear level) 2. Set Z=1 (above) 3. Compare timbral difference |
| Below has chest bounce coloration | ELEV-03 | Perceptual judgment | 1. Set Z=-1 2. Verify chest bounce coloration is audible |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
