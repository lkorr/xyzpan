# Phase 1: Project Scaffolding - Research

**Researched:** 2026-03-12
**Domain:** JUCE 8 CMake plugin scaffolding, pure C++ static engine library, XYZ-to-spherical coordinate math, Catch2 unit testing, pluginval validation
**Confidence:** HIGH

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **Project structure:** Start from Pamplejuce template, modify for XYZPan's structure
- **Three CMake library targets:** `xyzpan_engine` (pure C++, no JUCE), `xyzpan_ui` (OpenGL rendering, depends on JUCE OpenGL), `XYZPan` plugin (thin JUCE wrapper linking both)
- **CPM for dependency management:** JUCE 8.0.12, Catch2, GLM pinned versions
- **Placeholder company name:** e.g., "XYZAudio" for COMPANY_NAME and manufacturer code — changeable before release
- **Y-forward coordinate convention:** Y=1 = front, Y=-1 = behind, X=1 = right, X=-1 = left, Z=1 = above, Z=-1 = below
- **Hard clamp all XYZ inputs to [-1.0, 1.0] before processing** — LFO overshoot is the user's responsibility
- **Clamp to minimum distance at origin (e.g., 0.1 normalized)** — prevents division-by-zero and undefined azimuth/elevation
- **True spherical elevation:** elevation = atan2(Z, sqrt(X²+Y²)) — geometrically correct, not raw Z mapping
- **Engine handles stereo-to-mono summing internally** (accepts 1 or 2 input channels)
- **Full prepare/process/reset contract from day one** — reset() is no-op in Phase 1
- **EngineParams struct starts minimal** (X, Y, Z only in Phase 1) and grows per phase
- **Coordinate converter as free functions in `xyzpan` namespace** (stateless math, no class needed)
- **process() signature:** process(const float* const* input, int numInputChannels, float* outL, float* outR, int numSamples)
- **Phase 1 plugin copies mono input to both output channels unchanged** — proves signal path works end-to-end
- **X, Y, Z parameters registered in APVTS from Phase 1** (visible in DAW automation, saveable in presets)
- **GenericAudioProcessorEditor** used as the editor — auto-generates sliders for X/Y/Z, useful for dev testing
- **pluginval must pass at strictness level 5**
- **Coordinate conversion tested with ~15-20 test cases:** cardinal directions, diagonals, origin/minimum-distance behavior, boundary values

### Claude's Discretion
- Exact Pamplejuce template cleanup and restructuring approach
- CMake configuration details (compile definitions, warning flags, LTO settings)
- Header file organization within engine/include/xyzpan/
- SphericalCoord struct layout and naming
- Test file organization and Catch2 section structure
- .clang-format style choices

### Deferred Ideas (OUT OF SCOPE)
None — discussion stayed within phase scope
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| SETUP-01 | CMake project builds JUCE plugin (VST3 + Standalone) on Windows with MSVC | juce_add_plugin() with FORMATS VST3 Standalone; CMake 3.25+; MSVC v143 toolset |
| SETUP-02 | Pure C++ engine exists as separate static library with no JUCE headers | add_library(xyzpan_engine STATIC); JUCE headers in plugin/ only; verified by build isolation |
| SETUP-03 | JUCE PluginProcessor calls engine for all audio processing | PluginProcessor owns engine instance; processBlock() delegates entirely to engine.process() |
| SETUP-04 | Unit test target builds and runs via CTest (Catch2) | add_executable(XYZPanTests); catch_discover_tests(); ctest --test-dir build |
| SETUP-05 | pluginval validation passes at strictness level 5 | pluginval.exe --strictness-level 5 path/to/XYZPan.vst3 |
| COORD-01 | Plugin accepts X, Y, Z position inputs normalized -1.0 to 1.0 | APVTS AudioParameterFloat for X, Y, Z with range [-1, 1]; engine hard-clamps before use |
| COORD-02 | XYZ converted to azimuth angle (from X and Y) | azimuth = atan2(X, Y) with Y-forward convention |
| COORD-03 | XYZ converted to elevation angle (from azimuth and Z) | elevation = atan2(Z, sqrt(X*X + Y*Y)) |
| COORD-04 | Euclidean distance computed from X, Y, Z | distance = sqrt(X*X + Y*Y + Z*Z), clamped to [0.1, sqrt(3)] |
| COORD-05 | All coordinate conversions are sample-rate independent | Free functions using only <cmath>; no time-domain state |
</phase_requirements>

---

## Summary

Phase 1 is pure scaffolding: establish a CMake project that builds a loadable JUCE VST3/Standalone plugin with a zero-JUCE engine library, implement XYZ-to-spherical coordinate conversion, wire up pass-through audio, and pass pluginval at strictness 5. Nothing in this phase requires audio creativity — it is entirely about getting the build, architecture, and test infrastructure right so every future phase has a solid foundation.

The key structural challenge is the three-target CMake layout: `xyzpan_engine` (pure C++, no JUCE, tested by Catch2), `xyzpan_ui` (placeholder in Phase 1, JUCE-dependent), and `XYZPan` (the plugin target, thin JUCE wrapper). The engine must compile with zero JUCE headers — this constraint is verified by build isolation and is the architectural invariant that enables fast unit testing in all future phases.

The coordinate math is simple but has two decision points that must be locked in Phase 1: (1) Y-forward convention (Y=1 = front), which differs from many audio tools that use Z-forward, and (2) the minimum-distance clamp (0.1 normalized) that prevents division-by-zero at the origin. These are already decided in CONTEXT.md and the tests must encode them as ground truth.

**Primary recommendation:** Start from Pamplejuce, split immediately into the three-target structure, implement coordinate conversion as free functions with ~15-20 Catch2 test cases, use GenericAudioProcessorEditor for the dev UI, and run pluginval before declaring Phase 1 done.

---

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| JUCE | 8.0.12 | Plugin framework (VST3/Standalone wrapper, APVTS, AudioProcessor) | Industry standard; locked decision |
| CMake | 3.25+ | Build system | JUCE 8's recommended path; enables CPM, CTest, CI |
| MSVC | v143 (VS 2022) | Compiler | Only supported Windows compiler for JUCE; C++20 support |
| C++ | C++20 | Language standard | Designated initializers, std::span, concepts; locked decision |
| CPM | Latest in cmake/ | Dependency management | Replaces git submodules; used by Pamplejuce; locked decision |

### Dependencies

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Catch2 | 3.7.1 | Unit test framework | All engine tests; links to `xyzpan_engine` not to JUCE plugin target |
| GLM | 1.0.1 | 3D math (future UI) | Phase 1: add to CPM now, use in Phase 6; no Phase 1 usage |
| pluginval | Latest release | Plugin validation | Run after build to verify DAW compatibility at strictness 5 |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Pamplejuce template | Start from scratch | Pamplejuce provides CTest + Catch2 + CI + CPM prebuilt; starting from scratch adds days of CMake setup |
| GenericAudioProcessorEditor | Custom editor (Phase 6) | Generic editor is correct for Phase 1 dev testing; custom editor is Phase 6 work |
| add_library(STATIC) for engine | JUCE SharedCode interface library | Plain STATIC library gives cleaner separation; SharedCode is for multi-format ODR prevention in the plugin, not the engine |

**Installation:**
```bash
# Configure (Windows, VS 2022, x64)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# Build Debug
cmake --build build --config Debug

# Build Release
cmake --build build --config Release

# Run tests
ctest --test-dir build --build-config Release --output-on-failure

# Run pluginval
pluginval.exe --strictness-level 5 "build/XYZPan_artefacts/Release/VST3/XYZPan.vst3"
```

---

## Architecture Patterns

### Recommended Project Structure

```
xyzpan/
├── CMakeLists.txt                Root: sets up CPM, adds subdirectories
├── cmake/
│   └── CPM.cmake                 CPM dependency manager script
├── engine/
│   ├── CMakeLists.txt            add_library(xyzpan_engine STATIC ...)
│   ├── include/
│   │   └── xyzpan/
│   │       ├── Engine.h          XYZPanEngine class: prepare/process/reset
│   │       ├── Types.h           EngineParams, SphericalCoord structs
│   │       ├── Coordinates.h     Free function declarations in xyzpan namespace
│   │       └── Constants.h       constexpr physical constants
│   └── src/
│       ├── Engine.cpp
│       └── Coordinates.cpp
├── plugin/
│   ├── CMakeLists.txt            juce_add_plugin(XYZPan ...) links xyzpan_engine
│   ├── PluginProcessor.h
│   ├── PluginProcessor.cpp
│   ├── PluginEditor.h            Returns GenericAudioProcessorEditor in Phase 1
│   ├── PluginEditor.cpp
│   ├── ParamIDs.h                String constants for APVTS parameter IDs
│   └── ParamLayout.cpp           createParameterLayout() factory function
├── ui/
│   └── CMakeLists.txt            Placeholder: add_library(xyzpan_ui INTERFACE)
└── tests/
    ├── CMakeLists.txt            add_executable(XYZPanTests); catch_discover_tests()
    └── engine/
        └── TestCoordinates.cpp   ~15-20 test cases for coordinate conversion
```

**Phase 1 simplification:** `xyzpan_ui` is an INTERFACE (empty) library in Phase 1. It exists in the CMake graph so Phase 6 can add to it without changing the root CMakeLists.txt structure.

### Pattern 1: Engine as Pure C++ Static Library

**What:** `xyzpan_engine` is created with standard CMake `add_library(xyzpan_engine STATIC ...)`. It has no `target_link_libraries` dependency on any JUCE target. JUCE headers are used only inside the `plugin/` subdirectory.

**When to use:** Always. This is the non-negotiable architectural invariant. If engine code needs time or math utilities, use `<cmath>`, `<chrono>`, `<array>` — never JUCE.

**Enforcement:** The `engine/CMakeLists.txt` must NOT link against any `juce::` target. The test target links only `xyzpan_engine` + `Catch2::Catch2WithMain`. If the test target builds cleanly without JUCE, the invariant holds.

```cmake
# engine/CMakeLists.txt
add_library(xyzpan_engine STATIC
    src/Engine.cpp
    src/Coordinates.cpp
)

target_include_directories(xyzpan_engine
    PUBLIC include
)

target_compile_features(xyzpan_engine PUBLIC cxx_std_20)

# NO juce:: link here — intentional
```

```cmake
# plugin/CMakeLists.txt
juce_add_plugin(XYZPan
    COMPANY_NAME "XYZAudio"
    PLUGIN_MANUFACTURER_CODE Xyza
    PLUGIN_CODE Xzpn
    FORMATS VST3 Standalone
    PRODUCT_NAME "XYZPan"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

target_sources(XYZPan PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
    ParamLayout.cpp
)

target_compile_definitions(XYZPan PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(XYZPan
    PRIVATE
        xyzpan_engine
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
```

### Pattern 2: Coordinate Conversion as Free Functions

**What:** XYZ-to-spherical math implemented as stateless free functions in `namespace xyzpan`. Returns a `SphericalCoord` struct. No class, no state.

**When to use:** Coordinate conversion has no state — calling it with the same XYZ always produces the same result. Free functions are testable in isolation and have zero setup overhead.

```cpp
// engine/include/xyzpan/Types.h
namespace xyzpan {

struct EngineParams {
    float x = 0.0f;  // [-1, 1], Y-forward convention
    float y = 1.0f;  // [-1, 1], Y=1 is front
    float z = 0.0f;  // [-1, 1], Z=1 is above
};

struct SphericalCoord {
    float azimuth;    // radians, 0 = front (Y=1), +PI/2 = right (X=1)
    float elevation;  // radians, +PI/2 = above, -PI/2 = below
    float distance;   // normalized [kMinDistance, sqrt(3)]
};

} // namespace xyzpan
```

```cpp
// engine/include/xyzpan/Constants.h
namespace xyzpan {
    constexpr float kMinDistance  = 0.1f;   // prevents division-by-zero at origin
    constexpr float kMaxInputXYZ  = 1.0f;   // hard clamp for all XYZ inputs
} // namespace xyzpan
```

```cpp
// engine/include/xyzpan/Coordinates.h
namespace xyzpan {

// Clamp XYZ inputs to [-1, 1], apply minimum distance, convert to spherical.
// Y-forward convention: Y=1 = front, X=1 = right, Z=1 = above.
// elevation = atan2(Z, sqrt(X*X + Y*Y))  -- true spherical elevation
// azimuth   = atan2(X, Y)                -- 0 = front, clockwise positive
SphericalCoord toSpherical(float x, float y, float z);

// Euclidean distance, clamped to [kMinDistance, sqrt(3)]
float computeDistance(float x, float y, float z);

} // namespace xyzpan
```

```cpp
// engine/src/Coordinates.cpp
#include "xyzpan/Coordinates.h"
#include "xyzpan/Constants.h"
#include <cmath>
#include <algorithm>

namespace xyzpan {

SphericalCoord toSpherical(float x, float y, float z) {
    // Hard clamp inputs
    x = std::clamp(x, -kMaxInputXYZ, kMaxInputXYZ);
    y = std::clamp(y, -kMaxInputXYZ, kMaxInputXYZ);
    z = std::clamp(z, -kMaxInputXYZ, kMaxInputXYZ);

    // Minimum distance guard
    const float dist = computeDistance(x, y, z);

    // Elevation: atan2(Z, sqrt(X^2 + Y^2)) — true spherical
    const float horizontalMag = std::sqrt(x * x + y * y);
    const float elevation = std::atan2(z, horizontalMag);

    // Azimuth: atan2(X, Y) — Y-forward, clockwise = positive
    // At origin, azimuth is 0 (front) by convention
    const float azimuth = (dist > kMinDistance) ? std::atan2(x, y) : 0.0f;

    return { azimuth, elevation, dist };
}

float computeDistance(float x, float y, float z) {
    x = std::clamp(x, -kMaxInputXYZ, kMaxInputXYZ);
    y = std::clamp(y, -kMaxInputXYZ, kMaxInputXYZ);
    z = std::clamp(z, -kMaxInputXYZ, kMaxInputXYZ);
    const float raw = std::sqrt(x * x + y * y + z * z);
    return std::max(raw, kMinDistance);
}

} // namespace xyzpan
```

### Pattern 3: APVTS Parameter Layout + GenericEditor

**What:** X, Y, Z registered as `AudioParameterFloat` in APVTS with range [-1, 1]. `createEditor()` returns `GenericAudioProcessorEditor`. This gives immediate dev UI and DAW automation support with minimal code.

```cpp
// plugin/ParamIDs.h
namespace ParamID {
    constexpr const char* X = "x";
    constexpr const char* Y = "y";
    constexpr const char* Z = "z";
}

// plugin/ParamLayout.cpp
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    return {
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { ParamID::X, 1 },
            "X Position",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f),
            0.0f
        ),
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { ParamID::Y, 1 },
            "Y Position",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f),
            1.0f  // Default: front (Y=1)
        ),
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { ParamID::Z, 1 },
            "Z Position",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f),
            0.0f
        ),
    };
}

// plugin/PluginEditor.cpp
juce::AudioProcessorEditor* XYZPanProcessor::createEditor() {
    return new juce::GenericAudioProcessorEditor(*this);
}
```

### Pattern 4: Bus Layout Declaration (Mono-in / Stereo-out)

**What:** Override `isBusesLayoutSupported()` to declare exactly mono-in / stereo-out. Reject all other configurations. This is required for pluginval to pass.

```cpp
bool XYZPanProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    // Require exactly mono input
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono())
        return false;
    // Require exactly stereo output
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}
```

**Note:** Also set the default bus layout in the constructor via `AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::mono()).withOutput("Output", juce::AudioChannelSet::stereo()))`.

### Pattern 5: Pass-Through processBlock (Phase 1)

**What:** `processBlock()` sums stereo-to-mono if needed, copies mono input to both output channels. Engine is called even though it does nothing in Phase 1 — this validates the call chain is wired correctly.

```cpp
void XYZPanProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    // Snapshot params from APVTS atomics
    EngineParams params;
    params.x = xParam->load();
    params.y = yParam->load();
    params.z = zParam->load();

    // Build input channel pointer array
    const float* inputs[2] = {
        buffer.getNumChannels() > 0 ? buffer.getReadPointer(0) : nullptr,
        buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : nullptr
    };
    const int numIn = buffer.getNumChannels();

    // Ensure output has 2 channels
    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getWritePointer(1);

    engine.setParams(params);
    engine.process(inputs, numIn, outL, outR, buffer.getNumSamples());
}
```

```cpp
// Phase 1 Engine::process — pass-through implementation
void XYZPanEngine::process(const float* const* inputs, int numInputChannels,
                            float* outL, float* outR, int numSamples) {
    // Sum to mono if stereo input
    const float* monoIn = inputs[0];
    if (numInputChannels >= 2 && inputs[1] != nullptr) {
        for (int i = 0; i < numSamples; ++i)
            monoBuffer[i] = 0.5f * (inputs[0][i] + inputs[1][i]);
        monoIn = monoBuffer.data();
    }

    // Pass-through: copy mono to both channels
    for (int i = 0; i < numSamples; ++i) {
        outL[i] = monoIn[i];
        outR[i] = monoIn[i];
    }
}
```

**Note:** `monoBuffer` must be pre-allocated in `prepare()` to `maxBlockSize` samples. No allocation in `process()`.

### Pattern 6: Catch2 Test Structure for Coordinate Conversion

**What:** One test file, ~15-20 cases organized with Catch2 `SECTION` blocks. No JUCE dependency — links only `xyzpan_engine`.

```cmake
# tests/CMakeLists.txt
add_executable(XYZPanTests
    engine/TestCoordinates.cpp
)

target_link_libraries(XYZPanTests
    PRIVATE
        xyzpan_engine
        Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(XYZPanTests)
```

```cpp
// tests/engine/TestCoordinates.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "xyzpan/Coordinates.h"
#include "xyzpan/Constants.h"

using namespace xyzpan;
using Catch::Matchers::WithinAbs;
constexpr float kPi = 3.14159265f;

TEST_CASE("Coordinate conversion - cardinal directions", "[coordinates]") {
    SECTION("Front (Y=1)") {
        auto s = toSpherical(0.0f, 1.0f, 0.0f);
        REQUIRE_THAT(s.azimuth,   WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(s.elevation, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(s.distance,  WithinAbs(1.0f, 0.001f));
    }
    SECTION("Right (X=1)") {
        auto s = toSpherical(1.0f, 0.0f, 0.0f);
        REQUIRE_THAT(s.azimuth,   WithinAbs(kPi / 2.0f, 0.001f));
        REQUIRE_THAT(s.elevation, WithinAbs(0.0f, 0.001f));
    }
    SECTION("Left (X=-1)") {
        auto s = toSpherical(-1.0f, 0.0f, 0.0f);
        REQUIRE_THAT(s.azimuth,   WithinAbs(-kPi / 2.0f, 0.001f));
    }
    SECTION("Behind (Y=-1)") {
        auto s = toSpherical(0.0f, -1.0f, 0.0f);
        // atan2(0, -1) = pi
        REQUIRE_THAT(std::abs(s.azimuth), WithinAbs(kPi, 0.001f));
        REQUIRE_THAT(s.elevation, WithinAbs(0.0f, 0.001f));
    }
    SECTION("Above (Z=1)") {
        auto s = toSpherical(0.0f, 0.0f, 1.0f);
        REQUIRE_THAT(s.elevation, WithinAbs(kPi / 2.0f, 0.001f));
    }
    SECTION("Below (Z=-1)") {
        auto s = toSpherical(0.0f, 0.0f, -1.0f);
        REQUIRE_THAT(s.elevation, WithinAbs(-kPi / 2.0f, 0.001f));
    }
}

TEST_CASE("Coordinate conversion - origin and minimum distance", "[coordinates]") {
    SECTION("Exact origin clamped to kMinDistance") {
        auto s = toSpherical(0.0f, 0.0f, 0.0f);
        REQUIRE(s.distance >= kMinDistance);
        REQUIRE_THAT(s.azimuth, WithinAbs(0.0f, 0.001f));  // convention: front
    }
    SECTION("Near-origin also clamped") {
        auto s = toSpherical(0.001f, 0.001f, 0.001f);
        REQUIRE(s.distance >= kMinDistance);
    }
}

TEST_CASE("Coordinate conversion - boundary clamping", "[coordinates]") {
    SECTION("Values beyond 1.0 are clamped") {
        auto sOver  = toSpherical(2.0f,  0.0f, 0.0f);
        auto sExact = toSpherical(1.0f,  0.0f, 0.0f);
        REQUIRE_THAT(sOver.azimuth,   WithinAbs(sExact.azimuth,   0.001f));
        REQUIRE_THAT(sOver.distance,  WithinAbs(sExact.distance,  0.001f));
    }
    SECTION("Values below -1.0 are clamped") {
        auto sOver  = toSpherical(-2.0f, 0.0f, 0.0f);
        auto sExact = toSpherical(-1.0f, 0.0f, 0.0f);
        REQUIRE_THAT(sOver.azimuth, WithinAbs(sExact.azimuth, 0.001f));
    }
}

TEST_CASE("Coordinate conversion - diagonals", "[coordinates]") {
    SECTION("Front-right diagonal (X=1, Y=1)") {
        auto s = toSpherical(1.0f, 1.0f, 0.0f);
        REQUIRE_THAT(s.azimuth, WithinAbs(kPi / 4.0f, 0.001f));  // 45 degrees right of front
        REQUIRE_THAT(s.elevation, WithinAbs(0.0f, 0.001f));
    }
    SECTION("Front-up diagonal (Y=1, Z=1)") {
        auto s = toSpherical(0.0f, 1.0f, 1.0f);
        REQUIRE_THAT(s.azimuth,   WithinAbs(0.0f, 0.001f));         // still front
        REQUIRE_THAT(s.elevation, WithinAbs(kPi / 4.0f, 0.001f));   // 45 degrees up
    }
}

TEST_CASE("Coordinate conversion - distance is sample-rate independent", "[coordinates]") {
    SECTION("computeDistance produces consistent result regardless of call order") {
        float d1 = computeDistance(0.5f, 0.5f, 0.5f);
        float d2 = computeDistance(0.5f, 0.5f, 0.5f);
        REQUIRE_THAT(d1, WithinAbs(d2, 1e-6f));
        REQUIRE(d1 >= kMinDistance);
    }
    SECTION("Full unit sphere corner gives sqrt(3)") {
        float d = computeDistance(1.0f, 1.0f, 1.0f);
        REQUIRE_THAT(d, WithinAbs(std::sqrt(3.0f), 0.001f));
    }
}
```

### Anti-Patterns to Avoid

- **Including any juce:: header in engine/ files:** Causes JUCE to appear as a transitive dependency of the engine test target and defeats SETUP-02.
- **Using std::vector in Engine::prepare() monoBuffer allocation without sizing:** Always resize the vector to `maxBlockSize` in `prepare()`, not in `process()`.
- **Setting default Y parameter to 0.0f:** Y=0 means the source is equidistant between front and back (at the listener's side). The default should be Y=1.0f (front).
- **Calling GenericAudioProcessorEditor in a non-message-thread context:** Always return it from `createEditor()` which is called on the message thread.
- **Forgetting `juce::ScopedNoDenormals` as the first line of processBlock():** Without this, filter feedback loops (future phases) will get denormal CPU spikes.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Dependency management | Custom git submodule scripts | CPM (already in Pamplejuce) | CPM handles version pinning, caching, network fetching with one line |
| Plugin parameter boilerplate | Custom param serialization | JUCE APVTS | APVTS handles thread-safe atomics, XML serialization, DAW automation, and undo/redo |
| Test runner main() | Custom Catch2 main | `Catch2::Catch2WithMain` CMake target | Catch2WithMain provides a correct main(); custom mains only needed for JUCE GUI init (not needed for pure-engine tests) |
| CTest integration | Custom test registration | `catch_discover_tests()` | Automatically finds all TEST_CASE macros and registers with CTest |
| Warning flags | Per-compiler flag lists | `juce::juce_recommended_warning_flags` | JUCE provides compiler-appropriate flags for MSVC/Clang/GCC |
| LTO configuration | Per-compiler LTO flags | `juce::juce_recommended_lto_flags` | Same pattern, avoids MSVC vs GCC LTO flag differences |

**Key insight:** Pamplejuce already solves the boilerplate problems. The engineering work in Phase 1 is the three-target CMake split and the coordinate math — not the plugin scaffold itself.

---

## Common Pitfalls

### Pitfall 1: JUCE Header Leakage into Engine
**What goes wrong:** A `#include "juce_core/juce_core.h"` or similar slips into an engine header. The test target then requires JUCE to build, breaking the separation invariant.

**Why it happens:** Easy to accidentally include a JUCE convenience header, especially when copy-pasting from examples.

**How to avoid:** After `engine/CMakeLists.txt` is written, verify the test target builds with ONLY `xyzpan_engine` and `Catch2::Catch2WithMain` — no `juce::` targets. If it builds clean, SETUP-02 is satisfied.

**Warning signs:** Test target build errors mentioning `JuceHeader.h`, `juce_audio_basics`, or any `juce::` symbol.

### Pitfall 2: Wrong isBusesLayoutSupported Causes pluginval Failure
**What goes wrong:** Plugin doesn't declare mono-in/stereo-out bus layout correctly. pluginval sends various channel configurations and the plugin crashes or misbehaves.

**Why it happens:** Default JUCE plugin template allows stereo-in/stereo-out. Changing to mono-in without also updating the `AudioProcessor` constructor's `BusesProperties` causes inconsistent state.

**How to avoid:** Set `BusesProperties` in the processor constructor AND override `isBusesLayoutSupported`. Both must agree.

**Warning signs:** pluginval reports "Failed to set bus layout" or the plugin fails at strictness level 1.

### Pitfall 3: Default Y=0 Makes Pass-Through Appear Broken
**What goes wrong:** Default parameter value for Y is 0.0f. When distance = 0 and azimuth is undefined, the first APVTS snapshot feeds nearly-zero XYZ into the engine. With future DSP, this would produce no output. Phase 1 pass-through would still work, but the default position is confusing.

**Why it happens:** Default value of 0 is the obvious choice numerically. Y=1 (front) is the correct perceptual default.

**How to avoid:** Set Y parameter default to 1.0f in `createParameterLayout()`.

### Pitfall 4: VST3 Path Trailing Slash Breaks pluginval
**What goes wrong:** Running `pluginval.exe --strictness-level 5 "path/to/XYZPan.vst3/"` (note trailing slash) silently produces "No types found" and reports nothing.

**Why it happens:** VST3 bundles are directories on Windows. pluginval needs the path without trailing slash.

**How to avoid:** Use `"build/XYZPan_artefacts/Release/VST3/XYZPan.vst3"` (no trailing slash).

### Pitfall 5: Catch2 v3 API Difference From v2
**What goes wrong:** Test code uses Catch2 v2 macros (`REQUIRE_THAT` with `Catch::WithinAbs` without the namespace) that changed in v3. Build fails on missing symbols.

**Why it happens:** Many online JUCE Catch2 examples predate v3 (released 2022). Pamplejuce uses v3.7.1.

**How to avoid:** Use `#include <catch2/catch_test_macros.hpp>` and `#include <catch2/matchers/catch_matchers_floating_point.hpp>`. Matchers are in `Catch::Matchers::WithinAbs`, not `Catch::WithinAbs`.

### Pitfall 6: monoBuffer Not Pre-Allocated in prepare()
**What goes wrong:** `engine.process()` declares `std::vector<float> monoBuffer(numSamples)` inside the function body, causing a heap allocation on the audio thread.

**Why it happens:** Convenient to allocate inline; easy to miss when writing the Phase 1 pass-through quickly.

**How to avoid:** Declare `std::vector<float> monoBuffer;` as a member of `XYZPanEngine`. In `prepare(sampleRate, maxBlockSize)`, call `monoBuffer.resize(maxBlockSize)`. Never allocate in `process()`.

---

## Code Examples

### Root CMakeLists.txt Skeleton

```cmake
cmake_minimum_required(VERSION 3.25)
project(XYZPan VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# CPM
include(cmake/CPM.cmake)

# Dependencies
CPMAddPackage("gh:juce-framework/JUCE#8.0.12")
CPMAddPackage("gh:catchorg/Catch2@3.7.1")
CPMAddPackage("gh:g-truc/glm#1.0.1")

# Subdirectories (order matters: engine before plugin)
add_subdirectory(engine)
add_subdirectory(plugin)
add_subdirectory(ui)
add_subdirectory(tests)
```

### Engine Interface (Phase 1 Minimal)

```cpp
// engine/include/xyzpan/Engine.h
#pragma once
#include "xyzpan/Types.h"
#include <vector>

namespace xyzpan {

class XYZPanEngine {
public:
    XYZPanEngine() = default;
    ~XYZPanEngine() = default;

    // Called before processing begins. Pre-allocate all resources here.
    void prepare(double sampleRate, int maxBlockSize);

    // Set current parameters (called once per processBlock, not per sample).
    void setParams(const EngineParams& params);

    // Process audio. Accepts 1 or 2 input channels, produces stereo output.
    // Phase 1: pass-through only. outL and outR receive mono input.
    void process(const float* const* inputs, int numInputChannels,
                 float* outL, float* outR, int numSamples);

    // Reset all internal state (delay lines, filter states).
    // Phase 1: no-op.
    void reset();

private:
    EngineParams currentParams;
    std::vector<float> monoBuffer;  // pre-allocated in prepare()
    double sampleRate = 44100.0;
    int maxBlockSize  = 512;
};

} // namespace xyzpan
```

### pluginval Command (Windows)

```bash
# Download from GitHub releases and run:
pluginval.exe --strictness-level 5 "build/XYZPan_artefacts/Release/VST3/XYZPan.vst3"

# With verbose output for CI:
pluginval.exe --strictness-level 5 --verbose "build/XYZPan_artefacts/Release/VST3/XYZPan.vst3"
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Projucer for project management | CMake + CPM | JUCE 6-7 era | CMake enables CI, dependency management, multi-target builds |
| Git submodules for JUCE | CPM (CMake Package Manager) | ~2021 | One-line dep declarations, automatic caching, no submodule mess |
| Catch2 v2 macros | Catch2 v3 API (different headers) | 2022 | Header paths changed; `Catch::WithinAbs` moved to `Catch::Matchers::WithinAbs` |
| All plugin code in PluginProcessor | Engine/plugin/UI separation | Ongoing best practice | Testability, compile time, enforced separation |
| GenericAudioProcessorEditor removed | Still valid for dev scaffolding | N/A | Use in Phase 1, replace with custom editor in Phase 6 |

**Deprecated/outdated:**
- **Projucer-generated CMakeLists:** JUCE's CMake API is now the primary path; Projucer is legacy for new projects
- **Catch2 v2:** Pamplejuce pins Catch2 3.7.1; v2 examples online are incorrect for this project
- **`juce_generate_juce_header()`:** Not needed for the engine target and not usable for plain `add_library` targets

---

## Open Questions

1. **Pamplejuce Structural Divergence**
   - What we know: Pamplejuce uses a `source/` flat structure with a SharedCode INTERFACE library; XYZPan needs `engine/`, `plugin/`, `ui/` subdirectory structure.
   - What's unclear: How much of Pamplejuce's cmake/ module system (PamplejuceVersion, JUCEDefaults, etc.) carries over cleanly when reorganizing into subdirectories.
   - Recommendation: Use Pamplejuce's cmake/ modules and CPM setup verbatim; replace its `source/` with the three-target structure. The cmake/ helpers are source-location-agnostic.

2. **COPY_PLUGIN_AFTER_BUILD on CI**
   - What we know: `COPY_PLUGIN_AFTER_BUILD TRUE` copies the .vst3 to the system VST3 folder after build.
   - What's unclear: Whether this causes permission issues on Windows CI (GitHub Actions runners may not have write access to `C:\Program Files\Common Files\VST3`).
   - Recommendation: Set `COPY_PLUGIN_AFTER_BUILD FALSE` for CI builds via a CMake cache variable; keep it TRUE for developer local builds. Or always FALSE and run pluginval against the artefacts/ path directly.

---

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | Catch2 3.7.1 |
| Config file | None — CMake `catch_discover_tests()` handles registration |
| Quick run command | `ctest --test-dir build --build-config Release -R "coordinates" --output-on-failure` |
| Full suite command | `ctest --test-dir build --build-config Release --output-on-failure` |

### Phase Requirements to Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| SETUP-01 | CMake builds VST3 + Standalone | Build smoke | `cmake --build build --config Release` exits 0 | Wave 0 |
| SETUP-02 | Engine has no JUCE headers | Build isolation | `cmake --build build --config Release --target XYZPanTests` exits 0 without juce:: targets | Wave 0 |
| SETUP-03 | PluginProcessor delegates to engine | Manual / pluginval | pluginval validates audio path end-to-end | Wave 0 |
| SETUP-04 | Unit tests build and run via CTest | Unit | `ctest --test-dir build --build-config Release` | ❌ Wave 0 |
| SETUP-05 | pluginval passes at strictness 5 | Plugin validation | `pluginval.exe --strictness-level 5 XYZPan.vst3` | ❌ Wave 0 |
| COORD-01 | X,Y,Z accepted -1.0 to 1.0 | Unit | clamping tests in TestCoordinates.cpp | ❌ Wave 0 |
| COORD-02 | XYZ → azimuth | Unit | cardinal direction tests | ❌ Wave 0 |
| COORD-03 | XYZ → elevation | Unit | above/below tests | ❌ Wave 0 |
| COORD-04 | XYZ → distance | Unit | distance boundary tests | ❌ Wave 0 |
| COORD-05 | Conversions are sample-rate independent | Unit | pure math, no time state | ❌ Wave 0 |

### Sampling Rate

- **Per task commit:** `ctest --test-dir build --build-config Release --output-on-failure`
- **Per wave merge:** Full suite + pluginval validation
- **Phase gate:** All CTest tests green + pluginval strictness 5 passes before marking Phase 1 complete

### Wave 0 Gaps

- [ ] `tests/CMakeLists.txt` — test executable definition + `catch_discover_tests()`
- [ ] `tests/engine/TestCoordinates.cpp` — ~15-20 Catch2 test cases for COORD-01 through COORD-05
- [ ] `engine/CMakeLists.txt` — static library definition with no JUCE links
- [ ] `engine/include/xyzpan/Types.h` — EngineParams, SphericalCoord structs
- [ ] `engine/include/xyzpan/Constants.h` — kMinDistance, kMaxInputXYZ
- [ ] `engine/include/xyzpan/Coordinates.h` — toSpherical(), computeDistance() declarations
- [ ] `engine/src/Coordinates.cpp` — implementation
- [ ] `engine/include/xyzpan/Engine.h` — XYZPanEngine class
- [ ] `engine/src/Engine.cpp` — prepare/process/reset implementation
- [ ] `plugin/CMakeLists.txt` — juce_add_plugin target
- [ ] `plugin/PluginProcessor.h/.cpp` — APVTS, processBlock, isBusesLayoutSupported
- [ ] `plugin/PluginEditor.h/.cpp` — GenericAudioProcessorEditor
- [ ] `plugin/ParamIDs.h` — X, Y, Z string constants
- [ ] `plugin/ParamLayout.cpp` — createParameterLayout() with X/Y/Z AudioParameterFloat
- [ ] `ui/CMakeLists.txt` — INTERFACE library placeholder
- [ ] `cmake/CPM.cmake` — CPM script (from Pamplejuce)
- [ ] `CMakeLists.txt` — root CMake file

Framework install: CPM fetches JUCE 8.0.12 and Catch2 3.7.1 automatically on first cmake configure. Requires internet access on first run. Subsequent runs use CPM cache.

---

## Sources

### Primary (HIGH confidence)
- [JUCE CMake API docs](https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md) — juce_add_plugin parameters, bus layout
- [Pamplejuce template](https://github.com/sudara/pamplejuce) — CPM setup, Catch2 integration, project structure
- [Catch2 CMake integration docs](https://github.com/catchorg/Catch2/blob/devel/docs/cmake-integration.md) — catch_discover_tests, CMake targets
- [pluginval README](https://github.com/Tracktion/pluginval) — CLI flags, strictness levels, exit codes
- Project research files: `.planning/research/STACK.md`, `ARCHITECTURE.md`, `PITFALLS.md`

### Secondary (MEDIUM confidence)
- [How to use CMake with JUCE — Melatonin](https://melatonin.dev/blog/how-to-use-cmake-with-juce/) — verified against official JUCE docs
- [JUCE Forum: Setting up a Catch2 Test target with CMake](https://forum.juce.com/t/setting-up-a-catch2-test-target-with-cmake/41863) — community pattern, verified against Catch2 docs
- [JUCE Forum: isBusesLayoutSupported mono/mono configuration](https://forum.juce.com/t/isbuseslayoutsupported-auval-and-mono-mono-configuration-solved/34326) — mono-in/stereo-out bus declaration pattern
- [JUCE Bus Layouts Tutorial](https://juce.com/tutorials/tutorial_audio_bus_layouts/) — isBusesLayoutSupported pattern

### Tertiary (LOW confidence)
- Various JUCE forum posts on static library CMake issues — flagged: the recommended pattern (plain add_library STATIC + target_link_libraries) appears straightforward but CMake quirks with ARCHIVE_OUTPUT_DIRECTORY on some versions may require explicit path settings

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — locked decisions verified against existing project research (STACK.md HIGH confidence)
- Architecture: HIGH — three-target CMake split well-documented in JUCE/Pamplejuce ecosystem
- Coordinate math: HIGH — pure trigonometry with locked conventions from CONTEXT.md
- Pitfalls: HIGH — sourced from PITFALLS.md which cross-references JUCE forums and KVR

**Research date:** 2026-03-12
**Valid until:** 2026-09-12 (stable stack — JUCE 8.0.12 pinned, CMake patterns are stable)
