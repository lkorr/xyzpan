---
phase: 01-project-scaffolding
plan: 01
subsystem: infra
tags: [cmake, juce, catch2, glm, cpm, vst3, c++20, static-library]

requires: []

provides:
  - CMake project with CPM dependency management (JUCE 8.0.12, Catch2 3.7.1, GLM 1.0.1)
  - xyzpan_engine static library with zero JUCE dependency (SETUP-02 invariant)
  - XYZPan VST3 + Standalone plugin binary
  - Catch2 test target (XYZPanTests) with 5 test cases across coordinate math
  - Engine header API: Engine.h, Types.h, Constants.h, Coordinates.h
  - Full XYZ-to-spherical coordinate conversion implementation
  - APVTS with X/Y/Z parameters (range [-1,1], Y default=1.0f)

affects:
  - 01-02-coordinates (uses engine headers and test infrastructure)
  - 01-03-passthrough (uses plugin and engine build targets)
  - all future phases (depend on xyzpan_engine static library)

tech-stack:
  added:
    - JUCE 8.0.12 (juce_audio_utils, juce_dsp)
    - Catch2 3.7.1 (Catch2WithMain target)
    - GLM 1.0.1 (future use in Phase 6 UI)
    - CPM 0.40.2 (cmake package manager)
  patterns:
    - Three-target CMake layout: xyzpan_engine (pure C++), XYZPan plugin (JUCE), xyzpan_ui (placeholder)
    - Engine-has-no-JUCE-headers architectural invariant enforced by build isolation
    - APVTS parameter layout in separate createParameterLayout() factory function
    - Catch2 catch_discover_tests() with enable_testing() at root CMakeLists.txt
    - monoBuffer pre-allocated in prepare(), never in process()

key-files:
  created:
    - CMakeLists.txt
    - cmake/CPM.cmake
    - engine/CMakeLists.txt
    - engine/include/xyzpan/Engine.h
    - engine/include/xyzpan/Types.h
    - engine/include/xyzpan/Constants.h
    - engine/include/xyzpan/Coordinates.h
    - engine/src/Engine.cpp
    - engine/src/Coordinates.cpp
    - plugin/CMakeLists.txt
    - plugin/PluginProcessor.h
    - plugin/PluginProcessor.cpp
    - plugin/PluginEditor.h
    - plugin/PluginEditor.cpp
    - plugin/ParamIDs.h
    - plugin/ParamLayout.h
    - plugin/ParamLayout.cpp
    - ui/CMakeLists.txt
    - tests/CMakeLists.txt
    - tests/engine/TestCoordinates.cpp
    - .clang-format
    - .gitignore
  modified: []

key-decisions:
  - "Y default = 1.0f (front in Y-forward convention, not 0.0f)"
  - "Bus layout: mono input + stereo output only, enforced in both BusesProperties constructor and isBusesLayoutSupported()"
  - "COPY_PLUGIN_AFTER_BUILD FALSE to avoid permission issues; run pluginval against artefacts/ path"
  - "enable_testing() at root CMakeLists.txt required for ctest --test-dir build to discover tests"
  - "Catch2 extras module path must be set via list(APPEND CMAKE_MODULE_PATH) before include(Catch)"
  - "GenericAudioProcessorEditor returned directly from createEditor() — no custom PluginEditor class in Phase 1"

patterns-established:
  - "Pattern: engine/CMakeLists.txt has NO juce:: links — xyzpan_engine is pure C++20 with <cmath> only"
  - "Pattern: Test target links xyzpan_engine + Catch2::Catch2WithMain, zero juce:: — this is the SETUP-02 verification"
  - "Pattern: processBlock() starts with juce::ScopedNoDenormals, snapshots atomics to EngineParams, calls engine.setParams() then engine.process()"
  - "Pattern: monoBuffer is a std::vector<float> member of XYZPanEngine, resized in prepare() never in process()"

requirements-completed: [SETUP-01, SETUP-02, SETUP-04, COORD-01]

duration: 14min
completed: 2026-03-12
---

# Phase 1 Plan 1: Project Scaffolding Summary

**Three-target CMake project (engine/plugin/ui) with JUCE 8.0.12 VST3+Standalone, pure C++ engine static library, Catch2 5-test suite — all build clean and pass**

## Performance

- **Duration:** 14 min
- **Started:** 2026-03-12T22:30:54Z
- **Completed:** 2026-03-12T22:44:24Z
- **Tasks:** 3
- **Files created:** 22

## Accomplishments

- CMake project configures and builds with Visual Studio 17 2022 generator on Windows
- `xyzpan_engine` static library compiles with zero JUCE headers — SETUP-02 invariant enforced
- VST3 artifact at `build/plugin/XYZPan_artefacts/Release/VST3/XYZPan.vst3`, Standalone at `build/plugin/XYZPan_artefacts/Release/Standalone/XYZPan.exe`
- CTest discovers and runs 5 test cases across 5 TEST_CASE blocks — all pass
- APVTS registers X/Y/Z with range [-1,1], Y default=1.0f (Y-forward convention)
- Full XYZ-to-spherical coordinate implementation with distance clamping to kMinDistance=0.1f

## Task Commits

1. **Task 1: CMake build system, engine library stubs, test target** - `872e090` (feat)
2. **Task 2: JUCE plugin target with APVTS and GenericEditor** - `4e67982` (feat)
3. **Task 3: Build validation, enable_testing fix** - `6ce5139` (feat)

## Files Created/Modified

- `CMakeLists.txt` — Root: CPM deps, enable_testing(), four subdirectories
- `cmake/CPM.cmake` — CPM v0.40.2 package manager script
- `engine/CMakeLists.txt` — xyzpan_engine STATIC, no juce:: links
- `engine/include/xyzpan/Engine.h` — XYZPanEngine class: prepare/setParams/process/reset
- `engine/include/xyzpan/Types.h` — EngineParams and SphericalCoord structs
- `engine/include/xyzpan/Constants.h` — kMinDistance=0.1f, kMaxInputXYZ=1.0f
- `engine/include/xyzpan/Coordinates.h` — toSpherical() and computeDistance() declarations
- `engine/src/Coordinates.cpp` — Full implementation: clamp, azimuth=atan2(X,Y), elevation=atan2(Z,sqrt(X²+Y²))
- `engine/src/Engine.cpp` — Phase 1 pass-through: sums stereo-to-mono, copies to outL+outR
- `plugin/CMakeLists.txt` — juce_add_plugin VST3+Standalone, links xyzpan_engine
- `plugin/ParamIDs.h` — ParamID::X/Y/Z string constants
- `plugin/ParamLayout.h/.cpp` — createParameterLayout() factory, Y default=1.0f
- `plugin/PluginProcessor.h/.cpp` — APVTS, mono-in/stereo-out bus, delegates to engine
- `plugin/PluginEditor.h/.cpp` — createEditor() returns GenericAudioProcessorEditor
- `ui/CMakeLists.txt` — xyzpan_ui INTERFACE placeholder for Phase 6
- `tests/CMakeLists.txt` — XYZPanTests with Catch2WithMain, no juce::
- `tests/engine/TestCoordinates.cpp` — 18 assertions across 5 TEST_CASE blocks
- `.gitignore` — build/, .vs/, .cache/, CMakeUserPresets.json, etc.
- `.clang-format` — LLVM-based, 4-space indent, 120 column limit

## Decisions Made

- `enable_testing()` must be at root `CMakeLists.txt`, not just in `tests/CMakeLists.txt`, for `ctest --test-dir build` to discover tests via the generated top-level `CTestTestfile.cmake`
- Catch2 `include(Catch)` requires `list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")` before the include
- `COPY_PLUGIN_AFTER_BUILD FALSE` to avoid Windows permission issues writing to `C:\Program Files\Common Files\VST3`
- `createEditor()` returns `new juce::GenericAudioProcessorEditor(*this)` directly — no custom PluginEditor class needed in Phase 1

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Plugin files created in Task 1 to enable CMake configure**
- **Found during:** Task 1 verification (cmake configure)
- **Issue:** CMake configure failed because `plugin/CMakeLists.txt` didn't exist yet (it was planned for Task 2), but `add_subdirectory(plugin)` in the root requires the file
- **Fix:** Created all plugin files during the initial file creation pass so configure could succeed. Task 2 commit captured them as its deliverable.
- **Files modified:** plugin/CMakeLists.txt, plugin/PluginProcessor.h/.cpp, plugin/PluginEditor.h/.cpp, plugin/ParamIDs.h, plugin/ParamLayout.h/.cpp
- **Verification:** CMake configure succeeded, all plugin files committed in Task 2 commit
- **Committed in:** 4e67982 (Task 2 commit)

**2. [Rule 3 - Blocking] Fixed CTest discovery by adding enable_testing() to root CMakeLists.txt**
- **Found during:** Task 3 verification (ctest --test-dir build showed "No tests were found")
- **Issue:** `include(CTest)` in `tests/CMakeLists.txt` alone doesn't generate a root-level `CTestTestfile.cmake`, so `ctest --test-dir build` found zero tests
- **Fix:** Added `enable_testing()` before the subdirectory declarations in root `CMakeLists.txt`; removed duplicate `include(CTest)` from `tests/CMakeLists.txt`
- **Files modified:** CMakeLists.txt, tests/CMakeLists.txt
- **Verification:** Root `CTestTestfile.cmake` generated; `ctest --test-dir build --build-config Release` shows 5/5 tests passed
- **Committed in:** 6ce5139 (Task 3 commit)

---

**Total deviations:** 2 auto-fixed (both Rule 3 - Blocking)
**Impact on plan:** Both fixes essential for correctness. Fix 1 was a natural sequencing issue (configure needs the file to exist). Fix 2 is a known CMake/CTest pattern requirement. No scope creep.

## Issues Encountered

- CMake path quoting in bash required careful handling on Windows (spaces in VS path). Used `/c/Program Files/...` unix-style path consistently throughout.

## User Setup Required

None — no external service configuration required. The build is self-contained via CPM which downloads JUCE/Catch2/GLM on first configure (requires internet access).

## Next Phase Readiness

- Build infrastructure complete; all three targets (engine, plugin, tests) build clean
- Engine API surface defined and stable: `prepare()`, `setParams()`, `process()`, `reset()`
- Coordinate conversion implementation is complete (not just stubs) — Plan 01-02 can extend the test suite
- APVTS wired up with correct parameter IDs and defaults
- Plan 01-02 (coordinate conversion tests) can begin immediately
- Plan 01-03 (pass-through audio wiring) can begin after 01-02

## Self-Check: PASSED

- CMakeLists.txt: FOUND
- engine/include/xyzpan/Engine.h: FOUND
- plugin/PluginProcessor.cpp: FOUND
- tests/engine/TestCoordinates.cpp: FOUND
- build/plugin/XYZPan_artefacts/Release/VST3/XYZPan.vst3 (directory bundle): FOUND
- build/plugin/XYZPan_artefacts/Release/Standalone/XYZPan.exe: FOUND
- .planning/phases/01-project-scaffolding/01-01-SUMMARY.md: FOUND
- Commit 872e090: FOUND
- Commit 4e67982: FOUND
- Commit 6ce5139: FOUND

---
*Phase: 01-project-scaffolding*
*Completed: 2026-03-12*
