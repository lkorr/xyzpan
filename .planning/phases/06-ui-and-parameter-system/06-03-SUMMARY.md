---
phase: 06-ui-and-parameter-system
plan: 03
subsystem: ui
tags: [juce, lfo, waveform, apvts, sliderattachment, buttonattachment, viewport, devpanel, laf]

# Dependency graph
requires:
  - phase: 06-ui-and-parameter-system
    plan: 02
    provides: xyzpan_ui STATIC library, XYZPanEditor foundation, OpenGL spatial view
  - phase: 05-creative-tools
    plan: 02
    provides: LFO parameters (rate/depth/phase/waveform/beat_div per axis, tempo sync) in APVTS
  - phase: 05-creative-tools
    plan: 01
    provides: Reverb parameters (size/decay/damping/wet/pre_delay) in APVTS
provides:
  - LFOWaveformButton component (clickable waveform display cycling sine/triangle/saw/square)
  - LFOStrip component (per-axis LFO controls: waveform + rate/depth/phase knobs + SYNC button)
  - DevPanelComponent (scrollable Viewport with 40 dev params in 4 groups: Binaural/Comb/Elevation/Distance)
  - Extended XYZPanEditor with kStripH=200 bottom strip (LFO strips + reverb knobs + dev panel toggle)
  - All APVTS parameters now have corresponding UI controls
affects:
  - 06-04 (Factory presets; editor is fully wired)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - LFOWaveformButton: param IDs duplicated as anonymous-namespace constexpr strings (no plugin/ include in ui/)
    - DevPanelComponent: Viewport wrapping inner Component; SliderAttachments in vectors kept alive permanently
    - AudioProcessorParameter::getValue() returns normalized [0,1]; for LFO waveform param with range [0,3] getValue()*3 = int waveform
    - setValueNotifyingHost() takes normalized value: waveform/3.0f for waveform param with range [0,3]
    - LFOStrip constructs param IDs dynamically from axis char: "lfo_" + axisLower + "_rate"
    - DevPanelComponent: always alive, setVisible() controls appearance (SliderAttachment lifetime rule)
    - ToggleButton + ButtonAttachment used for DOPPLER_ENABLED (AudioParameterBool); Slider+SliderAttachment for all float params

key-files:
  created:
    - ui/LFOWaveformButton.h
    - ui/LFOWaveformButton.cpp
    - ui/LFOStrip.h
    - ui/LFOStrip.cpp
    - ui/DevPanelComponent.h
    - ui/DevPanelComponent.cpp
  modified:
    - ui/CMakeLists.txt (added LFOWaveformButton.cpp, LFOStrip.cpp, DevPanelComponent.cpp)
    - plugin/PluginEditor.h (extended with LFOStrip members, reverb SA, devToggle_, DevPanelComponent)
    - plugin/PluginEditor.cpp (full constructor, resized() with kStripH=200 layout)

key-decisions:
  - "Param ID strings duplicated as anonymous-namespace constexpr in ui/*.cpp to avoid plugin/ include dependency in xyzpan_ui STATIC (consistent with XYZPanGLView.cpp pattern)"
  - "LFOWaveformButton normalizes waveform index: getValue()*3 for read, index/3.0f for setValueNotifyingHost (param registered with range [0,3])"
  - "DevPanelComponent uses Viewport with setViewedComponent(&content_, false) — false means Viewport does not own content; content is a Component member so lifetimes match"
  - "kStripH extended from 80 to 200 in PluginEditor to accommodate 120px LFO strips below 80px position knobs"
  - "Dev panel overlays right 30% of GL view area (not full-window overlay) to allow GL view to remain visible"

patterns-established:
  - "ui/ static library param ID pattern: duplicate constexpr strings in anonymous namespace, never #include plugin headers"
  - "DevPanelComponent show/hide: addAndMakeVisible in constructor + setVisible(false); only setVisible() ever called after that"

requirements-completed: [UI-04, UI-05, UI-06, PARAM-02, PARAM-03]

# Metrics
duration: 20min
completed: 2026-03-13
---

# Phase 06 Plan 03: LFO Controls, Dev Panel, and Reverb UI Summary

**Full APVTS control surface: LFO strips (animated waveform button + rate/depth/phase knobs + SYNC) below each spatial axis knob, reverb knobs wired to APVTS, and scrollable dev panel overlay exposing all 40 DSP constant sliders in grouped layout.**

## Performance

- **Duration:** ~20 min
- **Started:** 2026-03-13T09:12:10Z
- **Completed:** 2026-03-13T09:32:00Z
- **Tasks:** 1 of 2 (Task 2 is human-verify checkpoint)
- **Files modified:** 9

## Accomplishments
- LFOWaveformButton: clickable mini waveform display with 64-point path rendering, cycles sine → triangle → saw → square on each click, reads/writes APVTS waveform parameter
- LFOStrip: one per spatial axis (X/Y/Z), wires rate/depth/phase knobs via SliderAttachment and tempo sync toggle via ButtonAttachment; shown below position knobs in 120px zone
- DevPanelComponent: Viewport-based scrollable panel, 40 dev params in 4 groups, DOPPLER_ENABLED as ToggleButton, all other params as LinearHorizontal Slider + SliderAttachment; always alive (never recreated)
- PluginEditor extended: kStripH=200, reverb knobs (Size/Decay/Damping/Wet) right of R knob, DEV toggle button toggles devPanel_ visibility, devPanel_ overlays right 30% of GL view

## Task Commits

1. **Task 1: LFOWaveformButton, LFOStrip, DevPanelComponent, PluginEditor wiring** - `d7261ac` (feat)

**Plan metadata:** (pending)

## Files Created/Modified
- `ui/LFOWaveformButton.h` - Clickable waveform display component declaration
- `ui/LFOWaveformButton.cpp` - 64-point waveform path paint; mouseUp cycles waveform; mouseEnter/Exit hover highlight
- `ui/LFOStrip.h` - Per-axis LFO controls group (waveform + knobs + sync)
- `ui/LFOStrip.cpp` - Constructs param IDs from axis char; SliderAttachment for rate/depth/phase; ButtonAttachment for SYNC
- `ui/DevPanelComponent.h` - Scrollable dev panel with 40 param sliders in 4 groups
- `ui/DevPanelComponent.cpp` - Builds all 40 sliders/toggles in constructor; Viewport-based scroll
- `ui/CMakeLists.txt` - Added three new .cpp files to xyzpan_ui STATIC
- `plugin/PluginEditor.h` - Full extended editor with LFO/reverb/devPanel members
- `plugin/PluginEditor.cpp` - Constructor, resized() with kStripH=200 column layout

## Decisions Made
- Param ID strings duplicated in ui/*.cpp anonymous namespace to avoid plugin/ circular dependency (matches XYZPanGLView pattern from Phase 06-02)
- LFO waveform normalization: APVTS range is [0,3] with step 1; getValue() returns [0,1] normalized; multiply by 3 for display, divide by 3 for setValueNotifyingHost()
- DevPanelComponent show/hide: added in constructor with setVisible(false); only visibility toggled, never destroyed — satisfies SliderAttachment lifetime requirement
- kStripH changed 80 → 200 (80 for position knob + 120 for LFO strip below)

## Deviations from Plan

**1. [Rule 3 - Blocking] Replaced #include "ParamIDs.h" with local constexpr strings in LFOStrip.cpp and DevPanelComponent.cpp**
- **Found during:** Task 1 (initial review of include structure)
- **Issue:** Plan specified `#include "ParamIDs.h"` in ui/*.cpp files, but ParamIDs.h is in plugin/ which is not in xyzpan_ui's include paths (by design, to prevent circular dependency). XYZPanGLView.cpp already established the pattern of duplicating needed param ID strings locally.
- **Fix:** Replaced all ParamID:: constant references with inline anonymous-namespace constexpr strings matching plugin/ParamIDs.h exactly. LFOStrip.cpp constructs IDs dynamically from axis char ("lfo_" + axisLower + "_rate") without any include.
- **Files modified:** ui/LFOStrip.cpp, ui/DevPanelComponent.cpp
- **Verification:** All three new .obj files compiled; full XYZPan plugin (VST3 + Standalone) built with 0 errors, 0 warnings
- **Committed in:** d7261ac (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (Rule 3 - blocking include resolved)
**Impact on plan:** Required for correct compilation. No scope creep.

## Issues Encountered
None beyond the deviation above.

## User Setup Required
None.

## Next Phase Readiness
- All APVTS parameters have corresponding UI controls (requirements UI-04, UI-05, UI-06, PARAM-02, PARAM-03 met)
- Editor is fully wired; ready for Phase 6-04 (factory presets)
- Dev panel exposes all 40 DSP constants for runtime tuning

---
*Phase: 06-ui-and-parameter-system*
*Completed: 2026-03-13*
