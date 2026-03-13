---
phase: 5
slug: creative-tools
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-13
---

# Phase 5 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 3.7+ |
| **Config file** | tests/CMakeLists.txt (existing) |
| **Quick run command** | `ctest --test-dir build -R "creative" -C Release` |
| **Full suite command** | `ctest --test-dir build -C Release` |
| **Estimated runtime** | ~15 seconds |

---

## Sampling Rate

- **After every task commit:** Run `ctest --test-dir build -R "creative" -C Release`
- **After every plan wave:** Run `ctest --test-dir build -C Release`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** ~15 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 5-01-01 | 01 | 0 | VERB-01, LFO-01 | unit/integration | `ctest --test-dir build -R "creative" -C Release` | ❌ W0 | ⬜ pending |
| 5-01-02 | 01 | 1 | VERB-04 | unit | `ctest --test-dir build -R "VERB-04" -C Release` | ❌ W0 | ⬜ pending |
| 5-01-03 | 01 | 1 | VERB-02 | integration | `ctest --test-dir build -R "VERB-02" -C Release` | ❌ W0 | ⬜ pending |
| 5-01-04 | 01 | 1 | VERB-01 | integration | `ctest --test-dir build -R "VERB-01" -C Release` | ❌ W0 | ⬜ pending |
| 5-02-01 | 02 | 1 | LFO-01, LFO-02 | unit | `ctest --test-dir build -R "LFO-01\|LFO-02" -C Release` | ❌ W0 | ⬜ pending |
| 5-02-02 | 02 | 1 | LFO-03 | unit | `ctest --test-dir build -R "LFO-03" -C Release` | ❌ W0 | ⬜ pending |
| 5-02-03 | 02 | 1 | LFO-04 | integration | `ctest --test-dir build -R "LFO-04" -C Release` | ❌ W0 | ⬜ pending |
| 5-02-04 | 02 | 1 | LFO-05 | unit | `ctest --test-dir build -R "LFO-05" -C Release` | ❌ W0 | ⬜ pending |
| 5-02-05 | 02 | 2 | VERB-03 | integration | `ctest --test-dir build -R "VERB-03" -C Release` | ❌ W0 (stub in 05-01 Task 1) | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

> **5-02-05 note:** The `[VERB-03]` TEST_CASE is created in Plan 05-01 Task 1 as part of the Wave 0 scaffold (alongside VERB-01/02/04). It instantiates PluginProcessor and asserts getRawParameterValue returns non-null for verb_size, verb_decay, verb_damping, verb_wet. It will fail to link until Plan 05-02 Task 2a adds the APVTS entries — that is expected RED-phase behavior. The test turns green after Task 2a completes.

---

## Wave 0 Requirements

- [ ] `tests/engine/TestCreativeTools.cpp` — test stubs for VERB-01 through VERB-04, VERB-03 param-pointer check, and LFO-01 through LFO-05
- [ ] Register new test file in `tests/CMakeLists.txt`

*No new fixture files needed — existing `settleAndProcess()` / `makeNoise()` / `makeSine()` helpers from TestDistanceProcessing.cpp are sufficient.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Reverb sounds sparse and mix-friendly (not washy) on sustained material | VERB-04 | Perceptual quality judgement cannot be automated | Load plugin in DAW, play sustained pad at distance=1.0, enable reverb at decay=1.0 for 30s, confirm no muddy buildup |
| Combining LFOs on X+Y produces circular motion in UI | LFO-04 | Visual/spatial trajectory requires human observation | Set lfoX=sine/0.5Hz/depth=0.5, lfoY=sine/0.5Hz/depth=0.5/phase=0.25 — confirm circular orbit in 3D display |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 15s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
