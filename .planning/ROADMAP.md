# Roadmap: XYZPan

## Overview

XYZPan is built from the ground up in 7 phases, starting with a pure C++ engine and coordinate system, then layering binaural DSP stages in signal-flow order (panning, depth, elevation, distance), adding creative tools (reverb, LFOs), building the OpenGL UI and parameter system, and finishing with DAW integration validation. Each phase delivers an audibly verifiable capability -- the spatial illusion becomes progressively more convincing as phases complete.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: Project Scaffolding** - Build system, engine library, coordinate conversion, and test infrastructure (completed 2026-03-12)
- [x] **Phase 2: Binaural Panning Core** - ITD, head shadow, and mono-to-stereo split -- the fundamental spatial illusion (completed 2026-03-13)
- [x] **Phase 3: Depth and Elevation** - Comb filters for front/back and pinna/chest/floor bounce for elevation (completed 2026-03-13)
- [x] **Phase 4: Distance Processing** - Gain attenuation, air absorption LPF, delay, and doppler shift (completed 2026-03-13)
- [x] **Phase 5: Creative Tools** - Algorithmic reverb with distance-scaled pre-delay and per-axis LFO modulation (completed 2026-03-13)
- [ ] **Phase 6: UI and Parameter System** - Custom OpenGL renderer, dev panel, automation parameters, state save/restore, presets
- [x] **Phase 7: Integration and Validation** - DAW compatibility testing, pluginval, multi-instance, final hardening (completed 2026-03-17)

## Phase Details

### Phase 1: Project Scaffolding
**Goal**: A building CMake project with a pure C++ engine library that converts XYZ coordinates to spherical and passes audio through, verified by unit tests
**Depends on**: Nothing (first phase)
**Requirements**: SETUP-01, SETUP-02, SETUP-03, SETUP-04, SETUP-05, COORD-01, COORD-02, COORD-03, COORD-04, COORD-05
**Success Criteria** (what must be TRUE):
  1. CMake builds produce a loadable VST3 and Standalone binary on Windows (MSVC)
  2. Engine library compiles with zero JUCE headers -- verified by include-what-you-use or build isolation
  3. Unit tests for coordinate conversion pass: XYZ inputs produce correct azimuth, elevation, and distance values
  4. Plugin loads in a DAW, accepts mono input, and produces stereo output (pass-through at this stage)
  5. pluginval passes at strictness level 5 with the scaffolded plugin
**Plans**: 3 plans

Plans:
- [x] 01-01: CMake build system with JUCE, engine static library, and Catch2 test target
- [x] 01-02: Coordinate converter (XYZ to azimuth/elevation/distance) with unit tests
- [x] 01-03: Engine-to-PluginProcessor bridge, parameter stubs, and pluginval pass

### Phase 2: Binaural Panning Core
**Goal**: A mono signal placed in 3D space produces a convincing left/right binaural image via ITD and head shadow filtering
**Depends on**: Phase 1
**Requirements**: PAN-01, PAN-02, PAN-03, PAN-04, PAN-05
**Success Criteria** (what must be TRUE):
  1. A sound hard-panned left (X=-1) arrives earlier and louder in the left ear; hard-panned right (X=1) arrives earlier and louder in the right ear
  2. Head shadow filter audibly darkens the contralateral ear at extreme azimuth positions
  3. Stereo input is accepted and summed to mono before binaural processing (no crash, no phase issues)
  4. Sweeping X position via automation produces smooth, click-free spatial movement
**Plans**: 2 plans

Plans:
- [x] 02-01-PLAN.md — DSP primitives (FractionalDelayLine, SVFLowPass, OnePoleSmooth), binaural pipeline in Engine::process(), and integration tests
- [x] 02-02-PLAN.md — Dev panel parameter IDs, APVTS registration, PluginProcessor wiring, and pluginval pass

### Phase 3: Depth and Elevation
**Goal**: Front/back depth perception via comb filters and vertical positioning via pinna notch, chest bounce, and floor bounce
**Depends on**: Phase 2
**Requirements**: DEPTH-01, DEPTH-02, DEPTH-03, DEPTH-04, DEPTH-05, ELEV-01, ELEV-02, ELEV-03, ELEV-04, ELEV-05
**Success Criteria** (what must be TRUE):
  1. Moving a source from front (Y=1) to back (Y=-1) produces an audible tonal/timbral change from the comb filters -- front/back is perceptually distinguishable
  2. Comb filter feedback remains stable at all parameter values (no runaway resonance or clipping)
  3. A source above (Z=1) sounds perceptibly different from a source at ear level (Z=0) due to pinna notch and high shelf
  4. A source below (Z=-1) has audible chest bounce coloration and floor bounce delay
  5. All comb filter and elevation parameters are tuneable via dev panel controls
**Plans**: 3 plans

Plans:
- [x] 03-01-PLAN.md — DSP primitives (FeedbackCombFilter, SVFFilter, BiquadFilter, OnePoleLP), Types/Constants extensions, and unit tests
- [x] 03-02-PLAN.md — Engine integration: comb bank depth, pinna notch/shelf, chest bounce, floor bounce, and integration tests
- [x] 03-03-PLAN.md — APVTS parameter wiring for all Phase 3 dev panel controls

### Phase 4: Distance Processing
**Goal**: Distance from the listener produces natural gain rolloff, high-frequency absorption, propagation delay, and optional doppler shift
**Depends on**: Phase 3
**Requirements**: DIST-01, DIST-02, DIST-03, DIST-04, DIST-05, DIST-06, DIST-07
**Success Criteria** (what must be TRUE):
  1. A source moved from near to far gets progressively quieter (inverse-square) and duller (LPF rolloff)
  2. Distance delay is audible as a timing offset (up to 300ms at maximum distance) and is NOT latency-compensated
  3. Rapidly changing distance produces a pitch-shifted doppler effect that sounds natural, not glitchy
  4. Toggling doppler off removes the distance delay entirely (no timing offset, no pitch shift)
  5. All distance parameters are tuneable via dev panel controls
**Plans**: 2 plans

Plans:
- [ ] 04-01-PLAN.md — Engine distance DSP: inverse-square gain, air absorption LPF, propagation delay with Hermite interpolation, doppler via delay modulation, toggle, and integration tests
- [ ] 04-02-PLAN.md — APVTS parameter wiring for all Phase 4 dev panel controls and getTailLengthSeconds update

### Phase 5: Creative Tools
**Goal**: Algorithmic reverb adds spatial depth and per-axis LFOs create automated 3D movement patterns
**Depends on**: Phase 4
**Requirements**: VERB-01, VERB-02, VERB-03, VERB-04, LFO-01, LFO-02, LFO-03, LFO-04, LFO-05
**Success Criteria** (what must be TRUE):
  1. Reverb adds audible spatial depth at the end of the signal chain; moving a source farther increases the reverb pre-delay
  2. Reverb sounds sparse and mix-friendly -- not washy or muddy on sustained material
  3. Enabling an LFO on the X axis causes the source to oscillate left-right at the set rate and depth
  4. Combining LFOs on all three axes produces complex 3D orbital motion (e.g., sine on X + sine on Y at different rates = circle)
  5. LFO rate syncs to host tempo when tempo sync is enabled
**Plans**: 2 plans

Plans:
- [x] 05-01-PLAN.md — FDN reverb DSP primitive, engine integration (final stereo stage), distance-scaled pre-delay, and test scaffold
- [ ] 05-02-PLAN.md — LFO DSP primitive, engine LFO integration, APVTS wiring for all Phase 5 parameters, getTailLengthSeconds update

### Phase 6: UI and Parameter System
**Goal**: A custom OpenGL interface for spatial visualization and control, a complete parameter system with automation and state persistence, and a dev panel for DSP constant tuning
**Depends on**: Phase 5
**Requirements**: PARAM-01, PARAM-02, PARAM-03, PARAM-04, PARAM-05, UI-01, UI-02, UI-03, UI-04, UI-05, UI-06, UI-07
**Success Criteria** (what must be TRUE):
  1. OpenGL renderer shows a 2D projection of 3D space with a centered listener and a draggable source node whose size changes with elevation (Z)
  2. Dragging the source in the OpenGL view updates the X/Y/Z parameters in real time; automation playback moves the source in the view
  3. All DSP parameters are automatable from the DAW (visible in automation lane) with smooth, click-free response
  4. Dev panel toggle reveals all internal DSP constants; changing a value has immediate audible effect
  5. Plugin state saves and restores correctly across DAW sessions (close and reopen session, all parameters match)
**Plans**: 4 plans

Plans:
- [x] 06-01-PLAN.md — R parameter (APVTS + processBlock), PositionBridge header, XYZPanPluginTests CMake target, Wave 0 test scaffolds
- [x] 06-02-PLAN.md — xyzpan_ui STATIC library, alchemy-themed OpenGL renderer, XYZPanEditor replacing stub, PositionBridge wired in processBlock
- [ ] 06-03-PLAN.md — LFO strips (waveform button + knobs), dev panel overlay (40 DSP constant sliders), reverb controls
- [ ] 06-04-PLAN.md — Factory presets (5 XML strings + preset buttons), TestPresets unit tests, state save/restore verification

### Phase 06.1: Bug and dsp fixes (INSERTED)

**Goal**: Fix five critical bugs: signal explosion at extreme parameter values (distance gain formula and floor-bounce feedback path), proximity-panning inversion (distance gain miscalibration causing -20dB attenuation at Y=1), doppler glitchiness (smoother too fast for large jumps), missing main-UI doppler toggle, and dev panel / LFO strip layout failures (invisible dev panel, LFO knobs too small to use)
**Depends on:** Phase 6
**Plans:** 1/1 plans complete

Plans:
- [ ] 06.1-01-PLAN.md — Fix all five bugs: distance gain formula, floor-bounce feedback, doppler smoother + hard limiter, doppler UI toggle, dev panel bounds, LFO strip width

### Phase 7: Integration and Validation
**Goal**: Plugin works reliably across target DAWs, passes validation, and handles edge cases (sample rate changes, multiple instances)
**Depends on**: Phase 6
**Requirements**: INFRA-01, INFRA-02, INFRA-03, INFRA-04, INFRA-05, INFRA-06, INFRA-07
**Success Criteria** (what must be TRUE):
  1. VST3 and Standalone builds load and produce correct spatial audio on Windows
  2. Changing DAW sample rate (e.g., 44.1kHz to 96kHz) does not break processing -- all filter coefficients and delay lines recalculate correctly
  3. Zero memory allocations occur on the audio thread during normal operation (verified by instrumentation or analysis)
  4. pluginval passes at strictness level 5 with the complete plugin
  5. Plugin works correctly in Ableton, FL Studio, and Reaper; multiple simultaneous instances produce independent output
**Plans**: 2 plans

Plans:
- [ ] 07-01: Audio thread safety audit, sample rate adaptation verification, and memory allocation check
- [ ] 07-02: DAW compatibility testing (Ableton, FL Studio, Reaper), multi-instance test, and final pluginval pass

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Project Scaffolding | 3/3 | Complete   | 2026-03-12 |
| 2. Binaural Panning Core | 2/2 | Complete   | 2026-03-13 |
| 3. Depth and Elevation | 3/3 | Complete   | 2026-03-13 |
| 4. Distance Processing | 2/2 | Complete   | 2026-03-13 |
| 5. Creative Tools | 2/2 | Complete   | 2026-03-13 |
| 6. UI and Parameter System | 3/4 | In Progress|  |
| 7. Integration and Validation | 2/2 | Complete   | 2026-03-17 |

### Phase 07.4: Presets (INSERTED)

**Goal:** Implement a safe VST preset system with 7 factory presets (embedded XML strings in Presets.h), PluginProcessor program methods (getNumPrograms/setCurrentProgram), in-editor preset dropdown with user save/load, and comprehensive round-trip tests -- all with zero audio-thread memory allocation
**Requirements**: PARAM-04, PARAM-05, INFRA-04
**Depends on:** Phase 7
**Plans:** 1/2 plans executed

Plans:
- [ ] 07.4-01-PLAN.md — Factory preset backend: Presets.h with 7 embedded XML presets, PluginProcessor program methods, TestPresets.cpp round-trip and factory preset tests
- [ ] 07.4-02-PLAN.md — Preset UI: ComboBox factory preset selector, user Save/Load buttons with FileChooser, human verification checkpoint

### Phase 07.3: refactoring and codebase cleanup - remove redundancies and increase legibility and understandability of the codebase while retaining 100% functionality with tests (INSERTED)

**Goal:** [Urgent work - to be planned]
**Requirements**: TBD
**Depends on:** Phase 7
**Plans:** 0 plans

Plans:
- [ ] TBD (run /gsd:plan-phase 07.3 to break down)

### Phase 07.2: Optimization Round 2 DSP and UI CPU Optimization (INSERTED)

**Goal:** Comprehensive CPU optimization pass covering both DSP engine hot paths and OpenGL UI rendering -- SineLUT for LFO/orbit trig, zero-LFO fast path for sqrt elimination, GL frame rate throttling and draw call batching
**Requirements**: INFRA-03
**Depends on:** Phase 07.1
**Plans:** 2/2 plans complete

Plans:
- [ ] 07.2-01-PLAN.md — SineLUT header, LFO sine + orbit cos/sin LUT integration, zero-LFO fast path, TestSineLUT + worst-case benchmark
- [ ] 07.2-02-PLAN.md — GL frame rate throttle (30fps idle / 60fps active), sphere/cone draw call batching

### Phase 07.1: Optimization (INSERTED)

**Goal:** Reduce audio-thread CPU usage through profiling-driven DSP optimization -- SIMD vectorization, algorithm tuning, and hot-path elimination -- so the plugin runs comfortably at low buffer sizes (64-128 samples) on mid-range hardware
**Requirements**: INFRA-03
**Depends on:** Phase 7
**Plans:** 2/2 plans complete

Plans:
- [ ] 07.1-01-PLAN.md — Per-block coefficient pre-computation refactor, CMake Release optimization flags, sample-rate adaptation test
- [ ] 07.1-02-PLAN.md — Per-block stereo orbit transcendentals, performance microbenchmark test
