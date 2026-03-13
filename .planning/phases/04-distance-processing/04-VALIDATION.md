---
phase: 4
slug: distance-processing
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-12
---

# Phase 4 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 3.7+ |
| **Config file** | `tests/CMakeLists.txt` |
| **Quick run command** | `ctest --test-dir build --build-config Debug -R "Distance" --output-on-failure` |
| **Full suite command** | `ctest --test-dir build --build-config Debug --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `ctest --test-dir build --build-config Debug -R "Distance" --output-on-failure`
- **After every plan wave:** Run `ctest --test-dir build --build-config Debug --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 10 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 04-01-01 | 01 | 1 | DIST-01 | unit | `ctest --test-dir build -R "Distance" -VV` | ❌ W0 | ⬜ pending |
| 04-01-02 | 01 | 1 | DIST-02 | unit (RMS comparison above/below cutoff) | `ctest --test-dir build -R "Distance" -VV` | ❌ W0 | ⬜ pending |
| 04-01-03 | 01 | 1 | DIST-03 | unit (impulse timing) | `ctest --test-dir build -R "Distance" -VV` | ❌ W0 | ⬜ pending |
| 04-01-04 | 01 | 1 | DIST-06 | unit (finite output, no NaN/Inf) | `ctest --test-dir build -R "Distance" -VV` | ❌ W0 | ⬜ pending |
| 04-02-01 | 02 | 2 | DIST-04 | unit (frequency analysis) | `ctest --test-dir build -R "Distance" -VV` | ❌ W0 | ⬜ pending |
| 04-02-02 | 02 | 2 | DIST-05 | unit | `ctest --test-dir build -R "Distance" -VV` | ❌ W0 | ⬜ pending |
| 04-02-03 | 02 | 2 | DIST-07 | unit (set param, verify effect) | `ctest --test-dir build -R "Distance" -VV` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/engine/TestDistanceProcessing.cpp` — stubs for DIST-01 through DIST-07
  - Inverse-square gain test: white noise at distance 0.1 vs 0.2 (double) — expect ~6dB difference
  - Air absorption test: RMS above/below 8kHz at far distance vs near distance
  - Delay timing test: impulse input, measure output peak position
  - Doppler test: linearly ramping distance, verify output pitch shifts
  - Toggle test: Doppler OFF produces no delay offset
  - Finite output test: stress test with extreme params, verify no NaN/Inf
  - Dev panel params test: set param, verify effect on output

*If none: "Existing infrastructure covers all phase requirements."*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Doppler sounds natural, not glitchy | DIST-04 | Perceptual quality judgment | Move source rapidly near→far and back in dev panel; listen for smooth pitch shift without clicks or artifacts |
| Distance attenuation feels natural | DIST-01 | Perceptual tuning | Compare near vs far positions; gain should feel proportional to distance |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
