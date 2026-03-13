---
phase: 06-ui-and-parameter-system
plan: 02
subsystem: ui
tags: [opengl, juce, glm, camera, mesh, laf, glview, editor, position-bridge, cmake]

# Dependency graph
requires:
  - phase: 06-ui-and-parameter-system
    plan: 01
    provides: R parameter in APVTS, PositionBridge header, XYZPanPluginTests target
provides:
  - xyzpan_ui STATIC library (Camera, Mesh, Shaders, AlchemyLookAndFeel, XYZPanGLView)
  - Engine::ModulatedPosition struct + getLastModulatedPosition() getter
  - XYZPanEditor class replacing GenericAudioProcessorEditor
  - PositionBridge written in processBlock after engine.process()
  - Room wireframe and floor grid scaled by R via roomModelMatrix = glm::scale(mat4(1), vec3(r))
affects:
  - 06-03 (LFO controls strip below knobs; GL view already in place)
  - 06-04 (Factory presets; editor already open/close-safe)

# Tech tracking
tech-stack:
  added:
    - GLM 1.0.1 (already in CPM; now used by xyzpan_ui via glm::glm target)
    - juce::juce_opengl (OpenGL 3.2 Core context, shader program, helpers)
  patterns:
    - JUCE GL setup order: setOpenGLVersionRequired → setRenderer → setContinuousRepainting → attachTo (LAST)
    - JUCE GL teardown: glContext_.detach() FIRST in destructor
    - JUCE 8 shader uniforms: setUniform(name, ...) and setUniformMat4(name, ptr, count, transpose)
    - GL types (GLuint, GLsizei) are global typedefs; GL function calls are in juce::gl namespace
    - xyzpan_ui takes AudioProcessorValueTreeState& (not XYZPanProcessor&) to avoid circular dependency
    - APVTS writes from GL thread: MessageManager::callAsync with APVTS pointer capture
    - JUCE_DIRECT2D=0 required for Windows to prevent D2D/OpenGL context conflict

key-files:
  created:
    - ui/Shaders.h
    - ui/Camera.h
    - ui/Camera.cpp
    - ui/Mesh.h
    - ui/Mesh.cpp
    - ui/AlchemyLookAndFeel.h
    - ui/AlchemyLookAndFeel.cpp
    - ui/XYZPanGLView.h
    - ui/XYZPanGLView.cpp
    - plugin/PluginEditor.h (replaced stub)
    - plugin/PluginEditor.cpp (replaced stub)
  modified:
    - ui/CMakeLists.txt (INTERFACE → STATIC)
    - engine/include/xyzpan/Engine.h (ModulatedPosition struct + getter)
    - engine/src/Engine.cpp (lastModulated_ stored per sample)
    - plugin/PluginProcessor.h (positionBridge public member + include)
    - plugin/PluginProcessor.cpp (bridge write + createEditor returns XYZPanEditor + PluginEditor.h include)
    - plugin/CMakeLists.txt (xyzpan_ui, juce_opengl, JUCE_DIRECT2D=0)

key-decisions:
  - "XYZPanGLView takes juce::AudioProcessorValueTreeState& instead of XYZPanProcessor& to avoid circular include — xyzpan_ui is compiled before plugin/ and cannot include PluginProcessor.h"
  - "GL types (GLuint, GLint, etc.) are global typedef in juce::gl.h header; juce::gl namespace contains only function pointers and enum constants — use GLuint not juce::gl::GLuint"
  - "JUCE 8 OpenGLShaderProgram uses getUniformIDFromName() (not getUniformIDForName()); simpler to use setUniform(name,...) and setUniformMat4(name,...) methods directly"
  - "JUCE_DIRECT2D=0 added to plugin target_compile_definitions to prevent Direct2D/OpenGL context conflict on Windows NVIDIA drivers"
  - "Snap buttons are TextButton with setClickingTogglesState(false) — routing directly to glView_.setSnapView(); toggle state not needed for view snapping"

patterns-established:
  - "xyzpan_ui STATIC library pattern: link juce_opengl + juce_audio_processors + glm; plugin links xyzpan_ui to gain all GL and alchemy theme symbols"
  - "Line geometry upload: flat [x,y,z] VBO with stride=3*sizeof(float), attrib 0=position"
  - "Sphere geometry: interleaved [x,y,z,nx,ny,nz] VBO (stride=6*sizeof(float)), attrib 0=position, attrib 1=normal, indexed draw"

requirements-completed: [UI-01, UI-02, UI-03, UI-07]

# Metrics
duration: 15min
completed: 2026-03-13
---

# Phase 06 Plan 02: OpenGL Spatial View and XYZPanEditor Summary

**OpenGL 3.2 Core alchemy-themed spatial view with perspective room wireframe, draggable source node, orbit camera, and three snap buttons; XYZPanEditor replaces GenericAudioProcessorEditor; PositionBridge wired from processBlock.**

## Performance

- **Duration:** 15 min
- **Started:** 2026-03-13T08:52:17Z
- **Completed:** 2026-03-13T09:07:26Z
- **Tasks:** 2
- **Files modified:** 16

## Accomplishments

- `xyzpan_ui` STATIC library builds with Camera, Mesh, Shaders, AlchemyLookAndFeel, XYZPanGLView
- GL 3.2 Core rendering: room wireframe (bronze), floor grid (dark earth), listener sphere (warm gold), source sphere (bright gold, opacity by distance)
- Room wireframe and floor grid scale with R via `roomModelMatrix = glm::scale(mat4(1), vec3(r))` — space appears bigger/smaller as R changes
- Source node drag updates X/Y/Z parameters via `MessageManager::callAsync` (never from GL thread)
- Camera orbit on empty drag; three snap buttons (XY=TopDown, XZ=Side, YZ=Front)
- `Engine::ModulatedPosition` struct + `getLastModulatedPosition()` getter — stores last-sample modulated position
- PositionBridge written in every processBlock after engine.process() with modulated XYZ + distance
- `XYZPanEditor` with `AlchemyLookAndFeel`, rotary knobs (X/Y/Z/R), SliderAttachments, and snap buttons
- All 68 tests pass — zero regressions

## Task Commits

1. **Task 1: xyzpan_ui STATIC library** - `2b19686` (feat)
2. **Task 2: XYZPanEditor class and plugin wiring** - `d3bd2ce` (feat)

**Plan metadata:** (docs commit follows)

## Files Created/Modified

- `ui/CMakeLists.txt` — converted from INTERFACE to STATIC, added juce_opengl + juce_audio_processors + glm
- `ui/Shaders.h` — GL 3.2 Core vertex/fragment shader strings for lines and spheres
- `ui/Camera.h` + `ui/Camera.cpp` — orbit camera with yaw/pitch/dist, snap presets, applyMouseDrag
- `ui/Mesh.h` + `ui/Mesh.cpp` — buildUnitSphere, buildRoomWireframe, buildFloorGrid geometry helpers
- `ui/AlchemyLookAndFeel.h` + `ui/AlchemyLookAndFeel.cpp` — LookAndFeel_V4 with alchemy palette (drawRotarySlider, drawButtonBackground, drawLabel)
- `ui/XYZPanGLView.h` + `ui/XYZPanGLView.cpp` — OpenGL 3.2 renderer component (takes APVTS& + AudioProcessor* + PositionBridge&)
- `engine/include/xyzpan/Engine.h` — ModulatedPosition struct + getLastModulatedPosition() + lastModulated_ private member
- `engine/src/Engine.cpp` — stores lastModulated_ each sample in per-sample loop
- `plugin/PluginProcessor.h` — added positionBridge (public) + PositionBridge.h include
- `plugin/PluginProcessor.cpp` — writes positionBridge after engine.process(); createEditor() returns XYZPanEditor; includes PluginEditor.h
- `plugin/CMakeLists.txt` — added xyzpan_ui, juce_opengl, JUCE_DIRECT2D=0
- `plugin/PluginEditor.h` — XYZPanEditor class declaration
- `plugin/PluginEditor.cpp` — XYZPanEditor constructor, paint, resized implementations

## Decisions Made

- `XYZPanGLView` takes `juce::AudioProcessorValueTreeState&` and `juce::AudioProcessor*` instead of `XYZPanProcessor&`. `xyzpan_ui` is compiled as a STATIC library before the plugin layer; including `PluginProcessor.h` from `XYZPanGLView.cpp` would require a circular build dependency. Using the base-class APVTS reference avoids this cleanly.
- GL types (`GLuint`, `GLsizei`, etc.) are global typedefs in JUCE's `juce_gl.h` header. The `juce::gl` namespace contains only function pointers and enum constants. Code must use `GLuint` (global), not `juce::gl::GLuint`.
- JUCE 8 `OpenGLShaderProgram` uses `getUniformIDFromName()` (not `getUniformIDForName()`). The simpler approach is `setUniform(name, ...)` and `setUniformMat4(name, ptr, count, transpose)` directly on the shader object.
- `JUCE_DIRECT2D=0` added to `plugin/CMakeLists.txt` to prevent the known D2D/OpenGL context conflict on Windows with NVIDIA drivers (per RESEARCH.md and plan spec).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Changed XYZPanGLView to take AudioProcessorValueTreeState& instead of XYZPanProcessor&**
- **Found during:** Task 1 (designing XYZPanGLView)
- **Issue:** Plan specified `XYZPanGLView(XYZPanProcessor&, ...)` but `xyzpan_ui` is compiled as a STATIC library before the plugin layer. Including `PluginProcessor.h` from the ui library would require the plugin include path to be present at xyzpan_ui compile time, creating a circular build dependency.
- **Fix:** Changed constructor to `XYZPanGLView(juce::AudioProcessorValueTreeState& apvts, juce::AudioProcessor* proc, PositionBridge& bridge)`. All APVTS access and parameter writes work identically — only the type of the first argument changed.
- **Files modified:** `ui/XYZPanGLView.h`, `ui/XYZPanGLView.cpp`, `plugin/PluginEditor.cpp`
- **Committed in:** 2b19686 / d3bd2ce

**2. [Rule 3 - Blocking] Fixed juce::gl::GLuint → GLuint and getUniformIDForName → setUniform API**
- **Found during:** Task 1 (first build attempt)
- **Issue 1:** `juce::gl::GLuint` does not exist — `GLuint` is a global typedef, `juce::gl` namespace only contains function pointers.
- **Issue 2:** `OpenGLShaderProgram::getUniformIDForName` does not exist — the correct method is `setUniform(name, ...)` / `setUniformMat4(name, ...)`.
- **Fix:** Changed all `juce::gl::GLuint` to `GLuint`; replaced manual uniform location queries with JUCE's built-in `setUniform`/`setUniformMat4` methods.
- **Files modified:** `ui/XYZPanGLView.h`, `ui/XYZPanGLView.cpp`
- **Committed in:** 2b19686

---

**Total deviations:** 2 auto-fixed (Rule 1: design correction; Rule 3: build-blocking API mismatches)
**Impact on plan:** Functionally identical — same rendering behavior, same parameter update path. The APVTS interface change is a stricter, cleaner coupling than the original plan's processor reference.

## Issues Encountered

- JUCE 8 `OpenGLShaderProgram` uniform API differs from what was specified in the plan interfaces. Plan referenced `getUniformIDForName` which does not exist; the actual API is `setUniform(const char*, ...)` and `setUniformMat4(const char*, ...)` — no manual uniform ID lookup needed.
- GL types are in the global namespace (not `juce::gl`) — this is the standard pattern for all JUCE OpenGL code.
- Circular include between xyzpan_ui and plugin/ requires using APVTS base class rather than full processor type.

## Next Phase Readiness

- OpenGL spatial view is live — 06-03 (LFO controls) adds knob rows below the X/Y/Z/R strip
- XYZPanGLView snap buttons and camera orbit are functional
- PositionBridge is populated every processBlock — GL thread reads are live
- AlchemyLookAndFeel is the default look and feel — 06-03 inherits it automatically
- All 68 tests pass — no regressions

## Self-Check: PASSED

