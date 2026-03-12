# Technology Stack

**Project:** XYZPan -- 3D Spatial Audio Panner VST Plugin
**Researched:** 2026-03-12
**Overall confidence:** HIGH

## Recommended Stack

### Core Framework

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| C++ | C++20 | Language standard | C++17 is JUCE's minimum. C++20 adds concepts (cleaner template DSP code), constexpr improvements, designated initializers, and std::span (safe buffer views). MSVC 2022 has mature C++20 support. Avoid C++23 -- JUCE hasn't adopted it and Projucer doesn't support it. |
| JUCE | 8.0.12 | Plugin framework (VST3/AU/Standalone wrapper) | Industry standard. Broadest format support (VST3, AU, AUv3, AAX, LV2). Massive community. JUCE 8 adds Direct2D renderer, animation framework, bundled AAX SDK. JUCE 9 (upcoming) adds CLAP support. |
| OpenGL | 3.2+ Core Profile | Custom 3D panner visualization | JUCE's juce_opengl module wraps OpenGL context management. GL 3.2 Core Profile is the sweet spot: supported on all modern GPUs, provides programmable shaders, and avoids deprecated fixed-function pipeline. Do NOT target higher -- macOS caps at GL 4.1 if you ever need cross-platform, and many DAW hosts have limited GL expectations. |
| CMake | 3.25+ | Build system | JUCE 8 has first-class CMake API (juce_add_plugin). Required by Pamplejuce template. CMake 3.25 adds presets support used by modern JUCE templates. |

### Compiler & IDE

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| Visual Studio 2022 | 17.8+ (latest stable) | Primary IDE & compiler | JUCE 8.0.12 defaults to VS 2022 in Projucer. MSVC is the only supported Windows compiler for JUCE (MinGW support was removed). Be cautious with VS updates -- v17.10.x caused plugin binary compatibility issues on other machines. Pin a known-good version. |
| MSVC | v143 toolset | C++ compiler | Required by JUCE on Windows. Mature C++20 support including concepts, ranges, coroutines. |
| Clang (optional) | 16+ | Alternative compiler for sanitizers | clang-cl works with Visual Studio projects. Useful for AddressSanitizer and UndefinedBehaviorSanitizer runs during development. Not primary -- MSVC for release builds. |

### DSP Libraries

| Library | Version | Purpose | Why |
|---------|---------|---------|-----|
| None (hand-rolled DSP) | -- | Core binaural processing | XYZPan's DSP (ITD delay lines, comb filters, biquad filters for head shadow/pinna, distance attenuation, doppler) is straightforward enough to implement from scratch. No heavy linear algebra or FFT needed for the core path. Hand-rolling gives full control over real-time safety and zero unnecessary dependencies. |
| JUCE DSP module | 8.0.12 | Utility DSP primitives | juce::dsp provides IIR/FIR filters, oscillators (for LFO), convolution, oversampling, and lookup tables. Use selectively -- e.g. juce::dsp::IIR::Filter for biquads, juce::dsp::Oscillator for LFOs. Avoid its convolution engine for HRTF (overkill for short IRs). |
| AudioFFT | latest | FFT abstraction (if needed later) | MIT license. Wraps FFTW3, Intel IPP, or Ooura with zero-allocation runtime. Only pull in if you add frequency-domain convolution for HRTF filters. Not needed for initial implementation using time-domain biquads. |

### Math & 3D

| Library | Version | Purpose | Why |
|---------|---------|---------|-----|
| GLM | 1.0+ | 3D math for OpenGL visualization | Header-only, MIT license, GLSL-compatible API. The de facto standard for OpenGL math in C++. Use for view/projection matrices, quaternion rotations, vector math in the 3D panner UI. Requires C++17. SIMD-optimized (SSE2/AVX2). |
| Standard <cmath> | -- | DSP math | For sin/cos/tan in LFO, distance calculations, interpolation. No external library needed for the DSP math -- keep it simple. |

### Testing

| Library | Version | Purpose | Why |
|---------|---------|---------|-----|
| Catch2 | 3.7+ | Unit & integration testing | De facto standard in JUCE ecosystem. Used by Pamplejuce (the most popular JUCE template). Section-based tests avoid fixture boilerplate. Built-in BENCHMARK macros for measuring processBlock performance. Integrates cleanly with CMake/CTest. |
| pluginval | 1.x | Plugin validation | Cross-platform plugin validator by Tracktion. Tests plugin stability across strictness levels 1-10 (level 5 = minimum for host compatibility). Supports CLI/headless mode for CI. New real-time safety checking mode in 2025. Essential -- catches crashes that only appear in specific DAW hosting scenarios. |

### Profiling & Debugging

| Tool | Purpose | Why |
|------|---------|-----|
| melatonin_perfetto | DSP & UI performance tracing | JUCE-specific module wrapping Google's Perfetto. TRACE_DSP() macros show every audio callback individually (not aggregated). Critical for finding worst-case latency spikes that cause glitches. Zero overhead when disabled (macros compile away). |
| Melatonin Inspector | UI component debugging | Inspect JUCE components like browser DevTools. Shows paint timing per component, color IDs, parent/child relationships. Invaluable for debugging the OpenGL + JUCE component layering. |
| Tracy Profiler | Low-level performance profiling | Open source frame profiler. ~50ns overhead per instrumentation point. Shows worst-case frame times, lock contention, memory allocations. Good complement to Perfetto for deeper analysis. |
| Visual Studio Profiler | CPU sampling & memory | Built-in, zero setup. Use for initial profiling passes before reaching for specialized tools. |

### CI/CD & Distribution

| Tool | Purpose | Why |
|------|---------|-----|
| GitHub Actions | CI/CD pipeline | Pamplejuce template provides ready-made workflows: build (Win/Mac/Linux), run Catch2 tests, run pluginval, produce artifacts. Free for public repos, generous minutes for private. |
| CPM (CMake Package Manager) | Dependency management | Fetches JUCE, Catch2, and other dependencies automatically. Avoids git submodule headaches. Used by all modern JUCE templates. |
| Inno Setup | Windows installer | Standard for Windows VST plugin installers. Copies .vst3 to C:\Program Files\Common Files\VST3. Free, scriptable, well-documented. |

## Project Template

Start from **Pamplejuce** (https://github.com/sudara/pamplejuce) because it provides:
- JUCE 8 + CMake + CPM preconfigured
- Catch2 test target with JUCE initialization boilerplate
- pluginval integration in CI
- GitHub Actions workflows for Windows/macOS/Linux
- Melatonin Inspector included
- Intel IPP configuration (optional)
- .clang-format for consistent style

Modify to add: juce_opengl module, GLM dependency, custom DSP engine as a separate static library target.

## CMake Configuration Sketch

```cmake
cmake_minimum_required(VERSION 3.25)
project(XYZPan VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies via CPM
include(cmake/cpm.cmake)
CPMAddPackage("gh:juce-framework/JUCE#8.0.12")
CPMAddPackage("gh:catchorg/Catch2@3.7.1")
CPMAddPackage("gh:g-truc/glm#1.0.1")

# Plugin target
juce_add_plugin(XYZPan
    COMPANY_NAME "YourCompany"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Xyzp
    PLUGIN_CODE Xzpn
    FORMATS VST3 Standalone        # AU on macOS
    PRODUCT_NAME "XYZPan"
)

target_sources(XYZPan PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/dsp/BinauralEngine.cpp
    source/dsp/DistanceModel.cpp
    source/dsp/DopplerShift.cpp
    source/dsp/HeadShadow.cpp
    source/dsp/LFOModulator.cpp
    source/ui/PannerView3D.cpp
)

target_compile_definitions(XYZPan PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0   # Requires paid license
)

target_link_libraries(XYZPan
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl
        glm::glm
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
```

## Licensing Considerations

| Component | License | Cost | Notes |
|-----------|---------|------|-------|
| JUCE 8 | AGPLv3 / Commercial | Free (Starter, <$20K revenue) or $40/mo Indie (<$500K) or $175/mo Pro | Starter tier works for development and initial release. Upgrade to Indie when revenue exceeds $20K. Per-seat licensing in JUCE 8 -- every developer modifying JUCE-dependent code needs a license. |
| VST3 SDK | MIT | Free | Bundled with JUCE. No separate licensing needed. Steinberg moved to MIT in 2024. |
| GLM | MIT | Free | No restrictions. |
| Catch2 | BSL-1.0 | Free | No restrictions. |
| OpenGL | Royalty-free | Free | Khronos standard. |

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| Plugin framework | JUCE 8 | iPlug2 | iPlug2 has BSD license (cheaper) and smaller binaries (~100KB vs ~5MB), but it's maintained by one person, Linux support is immature, and community/documentation is far smaller. For a complex plugin with custom OpenGL UI, JUCE's mature OpenGLContext integration and massive community support wins. |
| Plugin framework | JUCE 8 | DPF (DISTRHO) | DPF is great for Linux-focused FOSS plugins. Lacks the breadth of JUCE's GUI system, app framework, and community. Wrong tool for a Windows-primary commercial plugin with complex 3D UI. |
| Plugin framework | JUCE 8 | Raw VST3 SDK | VST3 SDK alone gives you the wire protocol but zero GUI, zero cross-platform audio device abstraction, zero utility. You'd be writing thousands of lines of boilerplate that JUCE handles. Only makes sense if you need absolute minimal binary size or hate frameworks. |
| Plugin framework | JUCE 8 | CLAP + raw | CLAP is promising (open source, simpler than VST3) but ecosystem support in DAWs is still growing. JUCE 9 will add CLAP output -- wait for that rather than going raw CLAP now. |
| 3D math | GLM | Eigen | Eigen is superior for linear algebra/DSP (decompositions, sparse matrices) but overkill for 3D visualization math. GLM's GLSL-compatible API is purpose-built for OpenGL. Use Eigen only if you add heavy matrix DSP later (e.g., ambisonics decoding). |
| FFT | AudioFFT (if needed) | FFTW | FFTW is GPL (or commercial license). AudioFFT wraps FFTW/IPP/Ooura behind MIT license and guarantees no runtime allocations. For a plugin that may never need FFT (time-domain biquad filters suffice for binaural), AudioFFT is the safer choice if the need arises. |
| FFT | AudioFFT (if needed) | KFR | KFR is faster than FFTW but GPL (commercial license available). Full DSP framework is more than needed. Too heavy a dependency for optional FFT usage. |
| Testing | Catch2 | Google Test | Both work. Catch2 wins in JUCE ecosystem: Pamplejuce uses it, section-based tests are cleaner for DSP testing, built-in benchmarking. Google Test's main advantage (Google Mock) isn't critical for audio plugin testing where you're mostly testing signal flow, not mocking complex interfaces. |
| Profiling | melatonin_perfetto | Superluminal | Superluminal is excellent (zero-code sampling) but costs E59+. melatonin_perfetto is free, JUCE-specific, and designed exactly for audio callback profiling. Use Superluminal as a supplement if you need deep thread contention analysis. |
| Build system | CMake | Projucer | Projucer is JUCE's legacy project generator. CMake is now JUCE's recommended path. CMake enables CI/CD, works with any IDE, and allows proper dependency management via CPM. Projucer locks you into its UI and generates IDE-specific files. |

## What NOT to Use

| Technology | Why Avoid |
|------------|-----------|
| VST2 SDK | Steinberg stopped issuing VST2 licenses years ago. SDK is no longer distributed. VST3 is the standard. No reason to support VST2 for a new plugin. |
| MinGW | JUCE explicitly removed MinGW support. Use MSVC. |
| Projucer (for build) | Legacy tool. CMake is the modern path. Projucer doesn't support CPM, CI/CD integration, or modern dependency management. Fine for quick prototyping but wrong for a serious project. |
| JUCE's software renderer for 3D | CPU-only rendering. Cannot handle 60fps 3D visualization. Use OpenGL via juce_opengl. |
| VSTGUI | Steinberg's GUI toolkit. Inferior to JUCE's component system for anything beyond basic knobs. No 3D support. If you're already using JUCE, there's no reason to pull in VSTGUI. |
| Qt for plugin UI | Massive dependency. Not designed for plugin hosting contexts. JUCE handles the plugin window lifecycle; Qt would fight it at every step. |
| Boost | Huge dependency for marginal benefit. Modern C++20 covers most of what you'd want from Boost (std::span, std::format, concepts, ranges). If you need a specific Boost library, reconsider whether a lighter alternative exists. |
| C++23 / C++26 | JUCE hasn't adopted C++23. Compiler support varies. C++20 is the sweet spot -- mature tooling, full MSVC support, and all the features you actually need. |
| FFTW (directly) | GPL license is problematic for closed-source plugins. If you need FFT, use AudioFFT (MIT) which can wrap FFTW or use the built-in Ooura implementation. |
| OpenGL 4.x features | macOS caps at OpenGL 4.1, and many integrated GPUs on laptops have limited GL 4.x support. GL 3.2 Core Profile gives you everything needed for a 3D panner visualization. |
| ASIO SDK (bundled) | JUCE handles ASIO driver integration for standalone builds. You don't need to separately manage the ASIO SDK -- JUCE wraps it. Just enable JUCE_ASIO in compile definitions for standalone target. |

## HRTF / Spatial Audio Reference Libraries

These are NOT dependencies but reference implementations worth studying for algorithm design:

| Library | What to Learn | License |
|---------|---------------|---------|
| 3DTI Toolkit / BRT | Binaural rendering architecture: HRIR convolution, ITD management, ILD near-field simulation, pinna filtering | GPLv3 |
| Spatial Audio Framework (SAF) | HRTF interpolation techniques, diffuse-field EQ, distance filtering | ISC/GPLv2 |
| libspatialaudio | Frequency-domain HRTF convolution (overlap-add), SOFA file loading | LGPL v2.1 |

Study these for algorithm approaches. Do NOT link them as dependencies -- XYZPan's stated architecture is a custom DSP engine. Implementing your own gives you control over real-time safety, filter topology, and parameter smoothing that library code often lacks.

## Installation Commands

```bash
# Initial project setup (from Pamplejuce template)
gh repo create xyzpan --template sudara/pamplejuce --private --clone

# Or manual setup
mkdir xyzpan && cd xyzpan
git init

# CMake configure (Windows, VS 2022)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# CMake build
cmake --build build --config Release

# Run tests
ctest --test-dir build --build-config Release

# Run pluginval (after build)
pluginval --validate-in-process --strictness-level 5 "build/XYZPan_artefacts/Release/VST3/XYZPan.vst3"
```

## Version Pinning Strategy

Pin exact versions in CPM to avoid surprise breakage:

| Dependency | Pinned Version | Update Cadence |
|------------|---------------|----------------|
| JUCE | 8.0.12 | Check quarterly. Read changelogs carefully -- point releases sometimes break things. |
| Catch2 | 3.7.1 | Stable. Update when convenient. |
| GLM | 1.0.1 | Stable. Rarely needs updating. |
| pluginval | latest release | Update before each release to catch new validation rules. |

## Sources

- JUCE releases: https://github.com/juce-framework/JUCE/releases
- JUCE CMake API: https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md
- JUCE 8 features: https://juce.com/releases/whats-new/
- JUCE licensing: https://juce.com/get-juce/
- Pamplejuce template: https://github.com/sudara/pamplejuce
- pluginval: https://github.com/Tracktion/pluginval
- Melatonin Perfetto: https://github.com/sudara/melatonin_perfetto
- Melatonin Inspector: https://github.com/sudara/melatonin_inspector
- GLM: https://github.com/g-truc/glm
- AudioFFT: https://github.com/HiFi-LoFi/AudioFFT
- Tracy Profiler: https://github.com/wolfpld/tracy
- Spatial Audio Framework: https://github.com/leomccormack/Spatial_Audio_Framework
- 3DTI Toolkit: https://github.com/3DTune-In/3dti_AudioToolkit
- libspatialaudio: https://github.com/videolan/libspatialaudio
- CMake + JUCE guide: https://melatonin.dev/blog/how-to-use-cmake-with-juce/
- Perfetto for JUCE DSP: https://melatonin.dev/blog/using-perfetto-with-juce-for-dsp-and-ui-performance-tuning/
- JUCE OpenGL tutorial: https://juce.com/tutorials/tutorial_open_gl_application/
- Catch2: https://catch2.org/
- KVR iPlug2 vs JUCE: https://www.kvraudio.com/forum/viewtopic.php?t=565161
