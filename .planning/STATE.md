---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
status: completed
stopped_at: Completed 07.5-02-PLAN.md
last_updated: "2026-03-21T03:36:10.329Z"
last_activity: 2026-03-17 -- Completed plan 07.2-01 (SineLUT, orbit cos/sin LUT, zero-LFO fast path, worst-case benchmark, 4 SineLUT + 7 LFO + 4 perf tests green)
progress:
  total_phases: 13
  completed_phases: 9
  total_plans: 26
  completed_plans: 23
  percent: 88
---

---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
status: completed
stopped_at: "07.4-02 Task 2 checkpoint: human-verify preset system end-to-end"
last_updated: "2026-03-18T08:58:48.321Z"
last_activity: 2026-03-17 -- Completed plan 07.2-01 (SineLUT, orbit cos/sin LUT, zero-LFO fast path, worst-case benchmark, 4 SineLUT + 7 LFO + 4 perf tests green)
progress:
  [█████████░] 88%
  completed_phases: 9
  total_plans: 23
  completed_plans: 22
  percent: 100
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-12)

**Core value:** Accurate real-time binaural rendering of 3D spatial audio positioning
**Current focus:** Phase 6: UI and Parameter System (Custom OpenGL Editor)

## Current Position

Phase: 07.2 of 9 (Optimization Round 2: DSP and UI CPU Optimization) -- complete (2/2 plans done)
Plan: 1 of 2 in current phase -- completed (07.2-01 SineLUT and zero-LFO fast path done)
Status: Phase 07.2 complete — DSP: SineLUT replaces all per-sample trig in LFO+orbits; zero-LFO fast path skips sqrt; GL: frame throttle 30/60fps + draw batching
Last activity: 2026-03-17 -- Completed plan 07.2-01 (SineLUT, orbit cos/sin LUT, zero-LFO fast path, worst-case benchmark, 4 SineLUT + 7 LFO + 4 perf tests green)

Progress: [██████████] 100%

## Performance Metrics

**Velocity:**
- Total plans completed: 5
- Average duration: 10 min
- Total execution time: 0.87 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| Phase 1: Project Scaffolding | 3/3 | 26 min | 9 min |
| Phase 2: Binaural Panning Core | 2/3 | 26 min | 13 min |
| Phase 3: Depth and Elevation | 2/3 | 13 min | 6 min |

**Recent Trend:**
- Last 5 plans: 4 min, 18 min, 8 min, 7 min, 6 min
- Trend: Stable

*Updated after each plan completion*
| Phase 01-project-scaffolding P01 | 14 | 3 tasks | 22 files |
| Phase 01-project-scaffolding P02 | 8 | 2 tasks | 1 file |
| Phase 01-project-scaffolding P03 | 4 | 2 tasks | 3 files |
| Phase 02-binaural-panning-core P01 | 18 | 2 tasks | 9 files |
| Phase 02-binaural-panning-core P02 | 8 | 1 tasks | 4 files |
| Phase 03-depth-and-elevation P01 | 7 | 2 tasks | 8 files |
| Phase 03-depth-and-elevation P02 | 6 | 2 tasks | 3 files |
| Phase 03-depth-and-elevation P03 | 2 | 1 tasks | 4 files |
| Phase 04-distance-processing P01 | 11 | 2 tasks | 8 files |
| Phase 04-distance-processing P02 | 2 | 1 tasks | 4 files |
| Phase 05-creative-tools P01 | 11 | 3 tasks | 8 files |
| Phase 05-creative-tools P02 | 13 | 3 tasks | 12 files |
| Phase 06-ui-and-parameter-system P01 | 10 | 2 tasks | 10 files |
| Phase 06-ui-and-parameter-system P02 | 15 | 2 tasks | 16 files |
| Phase 06-ui-and-parameter-system P03 | 17 | 1 tasks | 9 files |
| Phase 06.1-bug-and-dsp-fixes P01 | 9 | 5 tasks | 7 files |
| Phase 07.1-optimization P01 | 9 | 2 tasks | 5 files |
| Phase 07.1-optimization P02 | 3 | 2 tasks | 3 files |
| Phase 07.2-optimization-round-2 P01 | 20 | 2 tasks | 6 files |
| Phase 07.4-01 P01 | 7 | 2 tasks | 4 files |
| Phase 07.4 P02 | 4 | 1 tasks | 2 files |
| Phase 07.5-vst-ui-design-overhaul P02 | 5 | 2 tasks | 2 files |

## Accumulated Context

### Roadmap Evolution

- Phase 06.1 inserted after Phase 6: Bug and dsp fixes (URGENT)
- Phase 07.1 inserted after Phase 7: Optimization (URGENT)
- Phase 07.2 inserted after Phase 7: Optimization Round 2 DSP and UI CPU Optimization (URGENT)
- Phase 07.3 inserted after Phase 7: refactoring and codebase cleanup - remove redundancies and increase legibility and understandability of the codebase while retaining 100% functionality with tests (URGENT)
- Phase 07.4 inserted after Phase 7: Presets - VST preset system for all twistable knobs with LFO parameters, user-saveable and factory preset support. Safety-critical: no audio-thread memory allocation (URGENT)
- Phase 07.5 inserted after Phase 7: VST UI Design Overhaul (URGENT)

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- Distance delay is a creative effect (NOT latency-compensated)
- Stereo input accepted, summed to mono before processing
- All DSP features from PROJECT.md included in v1 (no deferrals)
- Custom OpenGL UI in v1
- Pure C++ engine as separate static library (no JUCE headers)
- Windows only for v1
- enable_testing() must be at root CMakeLists.txt for ctest --test-dir build to discover tests
- COPY_PLUGIN_AFTER_BUILD FALSE to avoid Windows permission issues
- Y parameter default = 1.0f (front in Y-forward convention)
- Catch2 MODULE_PATH must include Catch2_SOURCE_DIR/extras before include(Catch)
- [Phase 01-project-scaffolding]: enable_testing() at root CMakeLists.txt required for ctest --test-dir build to discover Catch2 tests
- [Phase 01-project-scaffolding]: COPY_PLUGIN_AFTER_BUILD FALSE to avoid Windows VST3 folder permission issues
- [Phase 01-project-scaffolding]: Y parameter default = 1.0f (front in Y-forward convention, not 0.0f)
- [Phase 01-project-scaffolding]: Plan 01-01 delivered full Coordinates.cpp implementation (not stubs), so plan 01-02 TDD RED phase was not applicable — 22-section test suite confirmed GREEN immediately
- [Phase 01-project-scaffolding]: Boundary clamping tests compare over-range input vs clamped-range input (no hardcoded magic constants)
- [Phase 01-project-scaffolding]: getTotalNumInputChannels() required for engine input count — buffer.getNumChannels() returns total slots (in+out), causing NaN from reading uninitialized output channel as input
- [Phase 01-project-scaffolding]: pluginval binaries not committed to git — download from GitHub releases and place in tools/ (added to .gitignore)
- [Phase 02-binaural-panning-core]: kHeadShadowFullOpenHz = 16000 Hz (not 20000): SVF g=6.3 at 20kHz/44100Hz causes state transients >1.5x input during per-sample coefficient changes; 16000 Hz gives g=2.25 — safe and inaudible
- [Phase 02-binaural-panning-core]: kMinDelay = 2.0f in Engine process(): Hermite C and D points at base+1/base+2 read future ring buffer positions when delay<2; minimum 2-sample offset ensures all 4 Hermite points are valid past samples
- [Phase 02-binaural-panning-core]: OnePoleSmooth::prepare() does NOT reset z_ — allows live time constant changes without audible click; reset(value) is the separate "snap to value" API
- [Phase 02-binaural-panning-core]: NormalisableRange skew 0.3 for Hz parameters (HEAD_SHADOW_HZ, REAR_SHADOW_HZ) gives log-like generic editor feel without custom UI
- [Phase 03-depth-and-elevation]: FeedbackCombFilter uses integer-only delay (no fractional interpolation) — comb filters don't require sub-sample accuracy
- [Phase 03-depth-and-elevation]: BiquadFilter coefficients updated per-block only — std::cos/sin/pow/sqrt too expensive at audio rate; engine integration must respect this
- [Phase 03-depth-and-elevation]: SVFFilter is a parallel class to SVFLowPass (not a replacement) — Phase 2 engine uses SVFLowPass; changing it would risk regression
- [Phase 03-depth-and-elevation]: OnePoleLP uses setCoefficients(cutoffHz, sampleRate) API (vs OnePoleSmooth's smoothingMs) — same math kernel, different parameterisation
- [Phase 03-depth-and-elevation]: EngineParams array defaults hardcoded inline (not referencing constexpr arrays) — C++ disallows constexpr array as default member initializer in struct
- [Phase 03-depth-and-elevation]: Bounce delay guard uses std::max(2.0f, delaySamp) + gain threshold — plan formula gives 0ms delay at Z=-1 (max gain position); clamping to 2 samples minimum ensures bounce is audible everywhere gain > 0
- [Phase 03-depth-and-elevation]: Chest bounce uses original mono (not pinna-EQ'd monoEQ) — physical chest reflection bypasses the pinna path
- [Phase 03-depth-and-elevation]: Per-block biquad setCoefficients() strictly maintained — std::cos/sin/pow/sqrt too expensive at audio rate
- [Phase 03-depth-and-elevation]: constexpr const char* arrays for COMB_DELAY[10]/COMB_FB[10] in ParamIDs.h namespace safe — only included by two .cpp TUs
- [Phase 03-depth-and-elevation]: Hz-domain elevation params use NormalisableRange skew 0.3, consistent with Phase 2 HEAD_SHADOW_HZ/REAR_SHADOW_HZ convention
- [Phase 04-distance-processing]: Proximity scaling applied to ITD and head shadow (itdTarget * proximity, shadowCutoffTarget * proximity) to match ILD's existing proximity behavior — close sources hardpan more than distant
- [Phase 04-distance-processing]: Signal chain order: gain -> delay+doppler -> air LPF (gain first per physical accuracy, LPF last to filter arriving signal)
- [Phase 04-distance-processing]: Distance delay lines sized for 192kHz worst case (57608 samples) regardless of runtime sample rate
- [Phase 04-distance-processing]: Existing Phase 2/3 integration tests updated with dopplerEnabled=false: delay smoother ramp during 2048-4096 sample windows would zero output at y=1 (dist=1.0) positions
- [Phase 04-distance-processing]: AudioParameterBool uses getRawParameterValue same as APF — returns std::atomic<float>* with value 0.0f or 1.0f; cast to bool via >= 0.5f in processBlock
- [Phase 04-distance-processing]: getTailLengthSeconds updated to 0.320 to prevent DAW tail truncation: 300ms max distance delay + 20ms floor bounce
- [Phase 05-creative-tools]: FDNReverb internal wetGain_ fixed at 1.0; Engine applies smoothed verbWetSmooth_ externally for click-free transitions
- [Phase 05-creative-tools]: VERB-02 test uses wet-minus-dry subtraction to isolate reverb onset from dry signal (dry path arrives at same time regardless of pre-delay)
- [Phase 05-creative-tools]: Pre-delay distFrac computed from block-rate dist — consistent with other distance parameters and avoids per-sample distance recomputation
- [Phase 05-creative-tools]: Triangle LFO formula peaks at half-cycle (acc=0.5), not quarter — 1.0 - 4.0*|acc-0.5| formula confirmed
- [Phase 05-creative-tools]: LFO per-sample binaural target recomputation from modX enables true audio-rate spatial modulation
- [Phase 05-creative-tools]: lfoXPhase applied only at reset (not per-block) to avoid phase-jump clicks in running LFO
- [Phase 06-01]: XYZPanPluginTests links XYZPan CMake target (not XYZPan_SharedCode string) — JUCE names the .lib output XYZPan_SharedCode.lib but the CMake target is XYZPan
- [Phase 06-01]: juce::juce_audio_utils must be linked explicitly to XYZPanPluginTests to propagate JUCE include paths — XYZPan target alone does not propagate them to test executables
- [Phase 06-01]: rSmooth_ processes at block rate (not per-sample) — R multiplies position coordinates not audio; per-block smoothing eliminates zipper noise with negligible cost
- [Phase 06-02]: XYZPanGLView takes juce::AudioProcessorValueTreeState& instead of XYZPanProcessor& to avoid circular include — xyzpan_ui compiles before plugin/ and cannot include PluginProcessor.h
- [Phase 06-02]: GL types (GLuint, GLsizei, etc.) are global typedefs; juce::gl namespace contains only function pointers and enum constants
- [Phase 06-02]: JUCE 8 OpenGLShaderProgram uses setUniform(name,...) and setUniformMat4(name,...) directly; getUniformIDForName() does not exist
- [Phase 06-02]: JUCE_DIRECT2D=0 required in plugin CMakeLists.txt to prevent D2D/OpenGL context conflict on Windows NVIDIA
- [Phase 06-ui-and-parameter-system]: Param ID strings duplicated as anonymous-namespace constexpr in ui/*.cpp to avoid plugin/ include dependency in xyzpan_ui STATIC (consistent with XYZPanGLView.cpp pattern from Phase 06-02)
- [Phase 06-ui-and-parameter-system]: LFOWaveformButton normalizes waveform: getValue()*3 for read (APVTS range [0,3], getValue() returns [0,1]); index/3.0f for setValueNotifyingHost
- [Phase 06-ui-and-parameter-system]: kStripH changed 80→200 in PluginEditor to accommodate 120px LFO strips below 80px position knobs
- [Phase 06.1-bug-and-dsp-fixes]: kDistGainRef=1.0 so unity gain at dist=1.0 (default source position Y=1)
- [Phase 06.1-bug-and-dsp-fixes]: kDistGainMax=2.0 caps close-source boost at +6dB to prevent explosion
- [Phase 06.1-bug-and-dsp-fixes]: kDistSmoothMs increased 30ms to 150ms; kDopplerMaxDeltaSamp=2.0 per-sample clamp prevents zipper artifacts
- [Phase 06.1-bug-and-dsp-fixes]: glView_ added FIRST in PluginEditor constructor — JUCE paints later-added children on top (devPanel_ z-order fix)
- [Phase 06.1-bug-and-dsp-fixes]: LFO strip 2-row layout fits all controls in 100px column width without reducing knobs below 40px diameter
- [Phase 07.1-optimization]: Per-block coefficients use block-start position not LFO-modulated position — LFO delta within 64-128 samples is negligible for EQ coefficients
- [Phase 07.1-optimization]: SVF cutoff steps once per block instead of smoothly — acceptable at 1.45ms block size, below 5ms smoothing constant
- [Phase 07.1-optimization]: /fp:fast safe with std::isfinite() guards: MSVC /fp:fast documents isfinite() still returns correct results
- [Phase 07.1-optimization]: Angular smoother ticked once per block: 5ms time constant >> 1.45ms block period, inaudible difference
- [Phase 07.1-optimization]: XZ/YZ orbit cos/sin remain per-sample: depend on per-sample LFO output
- [Phase 07.1-optimization]: Performance test budget: 10% CPU mono (290us), 20% stereo (580us) at 128/44.1kHz
- [Phase 07.2-01-dsp-optimization]: SineLUT is header-only with static inline constexpr IIFE table — no .cpp required, zero runtime startup cost
- [Phase 07.2-01-dsp-optimization]: Block-level preamble orbit cos/sin also replaced with SineLUT (plan acceptance criteria required no std::cos(angXZ) anywhere in Engine.cpp)
- [Phase 07.2-01-dsp-optimization]: Zero-LFO fast path detects inactive LFOs via smoothed depthX/Y/Z > 1e-7f; LFOs still ticked every sample to maintain phase accumulation
- [Phase 07.2-01-dsp-optimization]: blkHorizMag reused in zero-LFO fast path (already computed as sqrt(blkX^2+blkY^2) at block start)
- [Phase 07.2-optimization-round-2]: Frame throttle: early-return inside setContinuousRepainting(true) loop (30fps idle, 60fps active) — simpler than toggling repainting, avoids race conditions
- [Phase 07.2-optimization-round-2]: GL batch pattern: bind sphereShader + upload projection/view/lightDir once before all sphere/cone draws; each draw helper sets only model/color/opacity uniforms
- [Phase 07.2-optimization-round-2]: isDraggingCamera_ tracks orbit drag state separately from isDraggingSource_ to keep 60fps during camera orbit regardless of position change detection
- [Phase 07.2-optimization-round-2]: drawCone switches VAO mid-batch (vaoCone_) then restores vaoSphere_ to maintain batch invariant
- [Phase 07.4-01]: Beat div default index = 6 (kBeatDivDefaultIndex, quarter note); plan comment said 5 — corrected in Presets.h
- [Phase 07.4-01]: Factory preset XMLs include all ~110 parameter IDs to prevent state bleed between presets (Pitfall 4 from research)
- [Phase 07.4-01]: setCurrentProgram calls apvts.replaceState() on message thread only; processBlock reads only pre-cached atomics (INFRA-04)
- [Phase 07.4-02]: Preset bar uses removeFromTop(kPresetBarH=32) as first resized() carve-out; leftColTop=leftCol.getY() anchors left-column Y after preset bar removal; XmlElement::writeTo() replaces deprecated writeToFile(); ComboBox deselected after user preset load
- [Phase 07.5-02]: drawToggleButton renders as text-only button: gold border+text ON, bronze border+parchment text OFF — no tick box
- [Phase 07.5-02]: drawComboBox triangle arrow proportional to height (30%) for legibility across sizes
- [Phase 07.5-02]: drawPopupMenuItem uses filled ellipse gold dot (not checkmark) for ticked items — consistent with Alchemy aesthetic

### Pending Todos

None.

### Blockers/Concerns

None.

## Session Continuity

Last session: 2026-03-21T03:36:10.324Z
Stopped at: Completed 07.5-02-PLAN.md
Resume file: None
