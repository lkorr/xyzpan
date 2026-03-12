# Architecture Patterns

**Domain:** 3D Spatial Audio Panner VST Plugin (Binaural)
**Researched:** 2026-03-12

---

## 1. System Overview

```
+-----------------------------------------------------------------------+
|  DAW Host (Reaper, Ableton, Logic, etc.)                              |
|                                                                       |
|  +---------------------------+   +------------------------------+     |
|  |  JUCE Plugin Wrapper      |   |  JUCE Plugin Editor          |     |
|  |  (PluginProcessor.cpp)    |   |  (PluginEditor.cpp)          |     |
|  |                           |   |                              |     |
|  |  - processBlock()  -------+---+--> OpenGL Renderer (GL thread)|    |
|  |  - prepareToPlay()        |   |    - 3D source visualization  |    |
|  |  - APVTS params <---------+---+--> JUCE controls (msg thread) |    |
|  |                           |   +------------------------------+     |
|  |  +---------------------+  |                                        |
|  |  |  XYZPanEngine       |  |   Audio Thread                        |
|  |  |  (pure C++, no JUCE)|  |   ================================    |
|  |  |                     |  |                                        |
|  |  |  Mono In            |  |                                        |
|  |  |    |                |  |                                        |
|  |  |    v                |  |                                        |
|  |  |  [LFO System]------+--+-- modulates X, Y, Z before convert     |
|  |  |    |                |  |                                        |
|  |  |    v                |  |                                        |
|  |  |  [Coordinate Conv]  |  |  XYZ -> azimuth, elevation, distance  |
|  |  |    |                |  |                                        |
|  |  |    v                |  |                                        |
|  |  |  [Binaural Panner]  |  |  ITD + ILD (head shadow)              |
|  |  |    |                |  |  Mono -> Stereo split here             |
|  |  |    v                |  |                                        |
|  |  |  [Depth Comb]       |  |  L/R comb filters for depth cues      |
|  |  |    |                |  |                                        |
|  |  |    v                |  |                                        |
|  |  |  [Elevation Filter] |  |  Pinna + chest bounce + floor bounce  |
|  |  |    |                |  |                                        |
|  |  |    v                |  |                                        |
|  |  |  [Distance Proc]    |  |  Attenuation + LPF + delay + doppler  |
|  |  |    |                |  |                                        |
|  |  |    v                |  |                                        |
|  |  |  [Reverb]           |  |  Early reflections + late tail         |
|  |  |    |                |  |                                        |
|  |  |    v                |  |                                        |
|  |  |  Stereo Out (L/R)   |  |                                        |
|  |  +---------------------+  |                                        |
|  +---------------------------+                                        |
+-----------------------------------------------------------------------+
```

### Thread Model

```
Audio Thread (realtime, hard deadline)
  |
  +-- PluginProcessor::processBlock()
  |     +-- Read atomic params from APVTS
  |     +-- engine.process(buffer, numSamples)
  |           +-- LFO tick -> modulated XYZ
  |           +-- Coordinate conversion
  |           +-- DSP chain: panner -> comb -> elevation -> distance -> reverb
  |
Message Thread (UI, non-realtime)
  |
  +-- PluginEditor: JUCE controls, sliders, buttons
  |     +-- APVTS attachments write atomic params
  |
OpenGL Thread (rendering, non-realtime)
  |
  +-- OpenGLRenderer::renderOpenGL()
        +-- Reads source position from atomic/ring buffer
        +-- Renders 3D scene (head, source dot, axes)
        +-- NEVER takes MessageManagerLock
```

**Confidence: HIGH** -- Based on JUCE official documentation for threading model and APVTS.

---

## 2. Component Responsibilities

### Engine Core (`XYZPanEngine`)

| Aspect | Detail |
|--------|--------|
| **Purpose** | Owns the full DSP chain. Single entry point for audio processing. |
| **JUCE dependency** | None. Pure C++ with `<cmath>`, `<atomic>`, `<array>`. |
| **Interface** | `prepare(sampleRate, maxBlockSize)`, `process(float* in, float** out, int numSamples)`, `reset()` |
| **Owns** | All DSP processor instances, LFO system, coordinate converter |
| **Thread safety** | Parameters set via atomic floats. No locks. No allocations in `process()`. |

### Coordinate Converter

| Aspect | Detail |
|--------|--------|
| **Purpose** | Converts Cartesian (X, Y, Z) to spherical (azimuth, elevation, distance) |
| **Math** | `azimuth = atan2(x, z)`, `elevation = atan2(y, sqrt(x*x + z*z))`, `distance = sqrt(x*x + y*y + z*z)` |
| **Output** | Struct with `{azimuth, elevation, distance}` used by all downstream processors |
| **Update rate** | Per-sample (required because LFO modulation changes XYZ per sample) |

### Binaural Panner

| Aspect | Detail |
|--------|--------|
| **Purpose** | Splits mono to stereo using ITD and ILD (head shadow) |
| **ITD** | Fractional delay line per ear. Delay = `(headRadius / speedOfSound) * (azimuth + sin(azimuth))` (Woodworth formula) |
| **ILD / Head Shadow** | Low-pass filter on contralateral ear. Frequency-dependent shadowing (higher frequencies shadowed more). |
| **Implementation** | Two fractional delay lines (allpass interpolation), one shelving filter per ear |
| **Crossfade** | Smooth transitions when source moves to prevent clicks |

### Depth Comb Filters

| Aspect | Detail |
|--------|--------|
| **Purpose** | Add depth perception via early reflection simulation |
| **Implementation** | Short comb filters with distance-dependent delay times and feedback |
| **Stereo** | Independent L/R with slightly different delay times for width |

### Elevation Filters

| Aspect | Detail |
|--------|--------|
| **Purpose** | Model pinna (outer ear), chest bounce, and floor bounce spectral cues |
| **Pinna** | Notch/peak filters whose center frequencies shift with elevation angle |
| **Chest bounce** | Short delay + comb for sources below the ear plane |
| **Floor bounce** | Distance-dependent delay + LP filter for ground reflection |
| **Implementation** | Parametric EQ bands with elevation-mapped coefficients |

### Distance Processor

| Aspect | Detail |
|--------|--------|
| **Purpose** | Models the effect of distance: volume, HF absorption, propagation delay, Doppler |
| **Attenuation** | `1 / distance` (inverse law), clamped to prevent infinity at zero |
| **Air absorption LPF** | Cutoff decreases with distance (6dB/octave rolloff characteristic) |
| **Propagation delay** | `distance / speedOfSound` in samples. Fractional delay line. |
| **Doppler** | Rate of change of distance modulates delay line read speed |

### Reverb

| Aspect | Detail |
|--------|--------|
| **Purpose** | Environment simulation. Distance-dependent wet/dry mix. |
| **Implementation** | Feedback delay network (FDN) or Schroeder reverb topology |
| **Distance coupling** | Farther sources = more reverb (wet/dry ratio increases with distance) |
| **Pre-delay** | Tied to simulated room size, not source distance |

### Parameter System

| Aspect | Detail |
|--------|--------|
| **Purpose** | Thread-safe parameter bridge between DAW automation, UI, and audio engine |
| **Implementation** | JUCE `AudioProcessorValueTreeState` (APVTS) on the JUCE side |
| **Audio thread reads** | Via `getRawParameterValue()` returning `std::atomic<float>*` |
| **Smoothing** | Every parameter feeding the DSP chain uses `SmoothedValue<float>` to prevent zipper noise |
| **Engine side** | Engine has its own `EngineParams` struct of plain floats. PluginProcessor copies atomic values into this struct at the top of each `processBlock()` call. |

### LFO System

| Aspect | Detail |
|--------|--------|
| **Purpose** | Modulate X, Y, Z coordinates with periodic waveforms |
| **Waveforms** | Sine, triangle, saw, square, random (S&H) |
| **Architecture** | 3 independent LFOs, one per axis. Each has rate, depth, waveform, phase. |
| **Update rate** | Per-sample (LFO output adds to base XYZ before coordinate conversion) |
| **Sync** | Optional tempo sync via host BPM (read from `processBlock` position info) |
| **Implementation** | Phase accumulator pattern: `phase += freq / sampleRate; if (phase >= 1.0) phase -= 1.0;` |

### UI Renderer (OpenGL)

| Aspect | Detail |
|--------|--------|
| **Purpose** | 3D visualization of source position relative to listener head |
| **Thread** | JUCE `OpenGLContext` runs on its own background GL thread |
| **Renderer class** | Implements `juce::OpenGLRenderer` with `renderOpenGL()`, `newOpenGLContextCreated()`, `openGLContextClosing()` |
| **Data from audio** | Reads modulated XYZ position from a lock-free ring buffer or atomic struct |
| **Interaction** | Mouse drag on 3D view writes new XYZ back to APVTS params (via message thread) |
| **Profile** | OpenGL 3.2 Core (broadest hardware compatibility per JUCE docs) |
| **WARNING** | Never take `MessageManagerLock` inside `renderOpenGL()` -- causes deadlock on macOS |

**Confidence: HIGH** -- Component responsibilities derived from the stated signal flow and JUCE documentation.

---

## 3. Recommended Project Structure

```
xyzpan/
|
+-- CMakeLists.txt                    Root build config
+-- VERSION                           Single source of truth for version
+-- .clang-format                     Code style
+-- .github/workflows/                CI (build + test + pluginval)
|
+-- JUCE/                             JUCE as git submodule
|
+-- engine/                           Pure C++ DSP engine (NO JUCE dependency)
|   +-- CMakeLists.txt                Builds as static library: xyzpan_engine
|   +-- include/
|   |   +-- xyzpan/
|   |       +-- Engine.h              Top-level engine interface
|   |       +-- Types.h               EngineParams, SphericalCoord, etc.
|   |       +-- CoordinateConverter.h
|   |       +-- BinauralPanner.h
|   |       +-- DepthCombFilter.h
|   |       +-- ElevationFilter.h
|   |       +-- DistanceProcessor.h
|   |       +-- Reverb.h
|   |       +-- LFO.h
|   |       +-- LFOSystem.h
|   |       +-- DelayLine.h           Shared utility: fractional delay
|   |       +-- SmoothedParam.h       Lightweight parameter smoother
|   |       +-- Constants.h           Speed of sound, head radius, etc.
|   +-- src/
|       +-- Engine.cpp
|       +-- CoordinateConverter.cpp
|       +-- BinauralPanner.cpp
|       +-- DepthCombFilter.cpp
|       +-- ElevationFilter.cpp
|       +-- DistanceProcessor.cpp
|       +-- Reverb.cpp
|       +-- LFO.cpp
|       +-- LFOSystem.cpp
|       +-- DelayLine.cpp
|
+-- plugin/                           JUCE wrapper (depends on engine + JUCE)
|   +-- CMakeLists.txt                juce_add_plugin(...) linking xyzpan_engine
|   +-- PluginProcessor.h
|   +-- PluginProcessor.cpp           Bridges APVTS <-> Engine
|   +-- PluginEditor.h
|   +-- PluginEditor.cpp              Owns OpenGL context + JUCE control panel
|   +-- ParamIDs.h                    String IDs for all APVTS parameters
|   +-- ParamLayout.h/.cpp            createParameterLayout() factory
|
+-- ui/                               OpenGL rendering (depends on JUCE OpenGL module)
|   +-- CMakeLists.txt                Builds as static library: xyzpan_ui
|   +-- GLRenderer.h/.cpp             Implements juce::OpenGLRenderer
|   +-- Scene3D.h/.cpp                3D head + source dot + grid rendering
|   +-- Shaders.h                     Vertex/fragment shader source strings
|   +-- Camera.h/.cpp                 View matrix, mouse orbit controls
|   +-- Meshes.h                      Sphere, grid geometry data
|
+-- tests/                            Unit + integration tests
|   +-- CMakeLists.txt                Test executable linking xyzpan_engine
|   +-- engine/
|   |   +-- TestCoordinateConverter.cpp
|   |   +-- TestBinauralPanner.cpp
|   |   +-- TestDelayLine.cpp
|   |   +-- TestLFO.cpp
|   |   +-- TestDistanceProcessor.cpp
|   |   +-- TestEngineIntegration.cpp
|   +-- plugin/
|       +-- TestParameterLayout.cpp
|       +-- TestPluginProcessor.cpp   Requires JUCE test harness
|
+-- benchmarks/                       Performance benchmarks
|   +-- CMakeLists.txt
|   +-- BenchEngine.cpp               Full-chain throughput measurement
|
+-- resources/                        Non-code assets
    +-- presets/                       Factory presets (XML or JSON)
    +-- icon.png                       Plugin icon
```

### Why This Structure

**Engine as a separate static library** is the single most important architectural decision. It provides:

1. **Testability** -- Engine links to the test executable without pulling in JUCE. Unit tests run in milliseconds, no DAW needed.
2. **Portability** -- Engine could be used in a non-JUCE host, a command-line tool, or a different UI framework.
3. **Compile times** -- Changing engine code does not trigger JUCE recompilation and vice versa.
4. **Enforced separation** -- If engine code accidentally includes a JUCE header, the build fails immediately.

The `SharedCode` interface library pattern (from pamplejuce template) links the engine to both the plugin target and the test target, preventing ODR violations across AU/VST3/AAX builds.

**Confidence: HIGH** -- Derived from pamplejuce template, Chowdhury-DSP template, and WolfSound tutorial patterns.

---

## 4. Architectural Patterns

### 4.1 DSP Processing Chain Pattern

Each DSP module follows the **prepare / process / reset** contract:

```cpp
class DspProcessor {
public:
    virtual ~DspProcessor() = default;

    // Called when sample rate or block size changes.
    // Pre-allocate ALL memory here. Recalculate coefficients.
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;

    // Called every audio callback. MUST be realtime-safe.
    // No allocations, no locks, no I/O, no unbounded loops.
    virtual void process(float* left, float* right, int numSamples) = 0;

    // Called when playback stops/restarts. Clear delay lines, reset state.
    virtual void reset() = 0;
};
```

The engine chains processors explicitly rather than using a generic graph (overkill for a fixed signal path):

```cpp
void Engine::process(const float* monoIn, float* left, float* right, int numSamples) {
    ScopedNoDenormals noDenormals;  // Flush denormals to zero

    for (int i = 0; i < numSamples; ++i) {
        // 1. LFO modulation -> modulated XYZ
        float mx = baseX + lfoSystem.tickX();
        float my = baseY + lfoSystem.tickY();
        float mz = baseZ + lfoSystem.tickZ();

        // 2. Coordinate conversion
        auto spherical = coordConverter.convert(mx, my, mz);

        // 3. Binaural panning (mono -> stereo)
        float L, R;
        panner.processSample(monoIn[i], spherical, L, R);

        // 4-7. Stereo chain
        depthComb.processSample(L, R, spherical.distance);
        elevationFilter.processSample(L, R, spherical.elevation);
        distanceProc.processSample(L, R, spherical.distance);
        reverb.processSample(L, R, spherical.distance);

        left[i]  = L;
        right[i] = R;
    }
}
```

**Why per-sample rather than per-block for the inner loop:** The LFO modulates XYZ per sample, which changes the coordinate conversion, which changes filter parameters for every downstream processor. Block-based processing would require sub-dividing blocks at LFO update boundaries, adding complexity for no benefit. Per-sample processing is simpler and correct. For processors with internal block-based needs (like FFT-based reverb), they accumulate internally and output per-sample.

**Alternative considered:** `juce::dsp::ProcessorChain` -- rejected because it requires JUCE dependency in the engine and its template-based chaining is inflexible for the per-sample coordinate dependency pattern here.

### 4.2 Parameter System Pattern

Three-layer parameter flow: **APVTS (atomic) -> PluginProcessor snapshot -> Engine params struct**.

```cpp
// ParamIDs.h -- string constants, used by both APVTS and UI
namespace ParamID {
    constexpr const char* X       = "x";
    constexpr const char* Y       = "y";
    constexpr const char* Z       = "z";
    constexpr const char* LfoXRate = "lfo_x_rate";
    // ... all parameters
}

// PluginProcessor.h
class XYZPanProcessor : public juce::AudioProcessor {
    juce::AudioProcessorValueTreeState apvts;

    // Cached atomic pointers -- read on audio thread
    std::atomic<float>* xParam   = nullptr;
    std::atomic<float>* yParam   = nullptr;
    std::atomic<float>* zParam   = nullptr;
    // ... one per parameter

    XYZPanEngine engine;

    void prepareToPlay(double sr, int bs) override {
        engine.prepare(sr, bs);

        // Cache raw parameter pointers (do this once, not per-block)
        xParam = apvts.getRawParameterValue(ParamID::X);
        yParam = apvts.getRawParameterValue(ParamID::Y);
        zParam = apvts.getRawParameterValue(ParamID::Z);
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override {
        juce::ScopedNoDenormals noDenormals;

        // Snapshot all params into engine's param struct (atomic reads)
        EngineParams p;
        p.x = xParam->load();
        p.y = yParam->load();
        p.z = zParam->load();
        // ... all other params

        engine.setParams(p);  // Engine copies into SmoothedValues
        engine.process(buffer.getReadPointer(0),
                       buffer.getWritePointer(0),
                       buffer.getWritePointer(buffer.getNumChannels() > 1 ? 1 : 0),
                       buffer.getNumSamples());
    }
};
```

Inside the engine, `setParams()` updates target values on `SmoothedParam` objects (a lightweight custom smoother, not `juce::SmoothedValue`, since the engine has no JUCE dependency):

```cpp
// engine/include/xyzpan/SmoothedParam.h
class SmoothedParam {
    float current = 0.0f;
    float target  = 0.0f;
    float step    = 0.0f;
    int   stepsRemaining = 0;

public:
    void prepare(double sampleRate, double rampTimeMs = 5.0) {
        // Pre-calculate for a given ramp time
        maxSteps = static_cast<int>(sampleRate * rampTimeMs * 0.001);
    }

    void setTarget(float newTarget) {
        target = newTarget;
        step = (target - current) / static_cast<float>(maxSteps);
        stepsRemaining = maxSteps;
    }

    float getNext() {
        if (stepsRemaining > 0) {
            current += step;
            --stepsRemaining;
        } else {
            current = target;
        }
        return current;
    }

private:
    int maxSteps = 256;
};
```

**Why this three-layer approach:**
- APVTS handles DAW automation, preset save/load, and UI synchronization.
- The snapshot in `processBlock()` reads all atomics once per block (not per sample), which is efficient.
- The engine's `SmoothedParam` smooths the step from old to new over ~5ms, preventing zipper noise without JUCE dependency.

**Confidence: HIGH** -- This is the standard JUCE pattern documented in official tutorials and community best practices.

### 4.3 Lock-Free Communication Between Audio and UI Threads

**Parameters (UI -> Audio):** Handled entirely by APVTS atomic reads. No additional mechanism needed.

**Visualization data (Audio -> UI):** The OpenGL renderer needs the current (modulated) source position for the 3D display. Use a lock-free atomic struct:

```cpp
// Shared between audio thread (writer) and GL thread (reader)
struct alignas(16) SourcePosition {
    float x, y, z;
    float azimuth, elevation, distance;  // derived values for UI display
};

// In PluginProcessor or a shared state object:
// Use a double-buffer pattern with an atomic index
class PositionBridge {
    SourcePosition buffers[2];
    std::atomic<int> writeIndex{0};

public:
    // Called from audio thread
    void write(const SourcePosition& pos) {
        int idx = 1 - writeIndex.load(std::memory_order_relaxed);
        buffers[idx] = pos;
        writeIndex.store(idx, std::memory_order_release);
    }

    // Called from GL thread
    SourcePosition read() const {
        int idx = writeIndex.load(std::memory_order_acquire);
        return buffers[idx];
    }
};
```

This is simpler than a FIFO because the GL thread only cares about the *most recent* position, not every intermediate value. Dropped frames are irrelevant for visualization.

**Metering data (Audio -> UI):** If you add level meters, use `juce::AbstractFifo` wrapping a ring buffer of meter snapshots. The UI timer callback polls this at 30-60Hz.

**Confidence: HIGH** -- Double-buffer pattern is standard for audio-to-visualization bridges. JUCE `AbstractFifo` documented for streaming data.

### 4.4 Sample-Rate-Aware DSP

Every DSP module that uses time-dependent constants must recalculate when `prepare()` is called:

```cpp
class DistanceProcessor : public DspProcessor {
    double sampleRate = 44100.0;
    float maxDelaySamples = 0.0f;
    DelayLine delayL, delayR;
    SmoothedParam cutoffSmooth;
    // ...

public:
    void prepare(double newSampleRate, int maxBlockSize) override {
        sampleRate = newSampleRate;

        // Recalculate max delay for 100m distance
        constexpr float maxDistance = 100.0f;   // meters
        constexpr float speedOfSound = 343.0f;  // m/s
        maxDelaySamples = static_cast<float>((maxDistance / speedOfSound) * sampleRate);

        // Allocate delay lines to max size (allocation happens HERE, never in process)
        delayL.prepare(static_cast<int>(maxDelaySamples) + 4);
        delayR.prepare(static_cast<int>(maxDelaySamples) + 4);

        // Recalculate smoother step sizes
        cutoffSmooth.prepare(sampleRate, 5.0);
    }

    void reset() override {
        delayL.clear();
        delayR.clear();
    }
};
```

**Key rules:**
1. **All memory allocation in `prepare()`**, never in `process()`.
2. **Store `sampleRate` as a member** -- needed for coefficient recalculation when parameters change at runtime.
3. **`prepare()` can be called multiple times** without a matching `reset()` -- handle gracefully.
4. **Delay line sizes** depend on sample rate. A 100m distance at 192kHz needs ~56,000 samples. Pre-allocate for the worst case.
5. **Filter coefficients** that depend on sample rate (LPF cutoff for air absorption, pinna notch frequencies) must be recalculated in `prepare()` and also when their controlling parameter changes during `process()`.

**Confidence: HIGH** -- JUCE DSP module pattern is the standard, confirmed by official tutorials.

---

## 5. Data Flow

### 5.1 Audio Buffer Flow

```
DAW calls processBlock(buffer, midi)
  |
  |  buffer: [numChannels x numSamples] float
  |  Input: channel 0 = mono source audio
  |  Output: channel 0 = Left ear, channel 1 = Right ear
  |
  +-> Snapshot all APVTS params into EngineParams struct (atomic reads)
  +-> engine.setParams(params)   // updates SmoothedParam targets
  +-> engine.process(in, outL, outR, numSamples)
        |
        |  Per-sample loop (i = 0..numSamples-1):
        |
        |  [LFO System]
        |    lfoX.tick() -> dx     (phase accumulator, waveform lookup)
        |    lfoY.tick() -> dy
        |    lfoZ.tick() -> dz
        |    modX = baseX.getNext() + dx * depthX.getNext()
        |    modY = baseY.getNext() + dy * depthY.getNext()
        |    modZ = baseZ.getNext() + dz * depthZ.getNext()
        |
        |  [Coordinate Converter]
        |    {az, el, dist} = convert(modX, modY, modZ)
        |
        |  [Binaural Panner]  (mono -> stereo)
        |    ITD delay: delayL/R computed from azimuth + head radius
        |    ILD filter: contralateral ear LP filter from azimuth
        |    Output: L, R (two float samples)
        |
        |  [Depth Comb Filters]  (stereo -> stereo)
        |    Short comb delays scaled by distance
        |    Slightly different L/R for stereo width
        |
        |  [Elevation Filters]  (stereo -> stereo)
        |    Pinna notch: center freq from elevation angle
        |    Chest bounce: enabled when elevation < 0
        |    Floor bounce: delay from distance + elevation
        |
        |  [Distance Processor]  (stereo -> stereo)
        |    Gain = clamp(1.0 / dist, 0.0, 1.0)
        |    Air absorption LPF: cutoff decreases with dist
        |    Propagation delay: dist / 343.0 * sampleRate samples
        |    Doppler: delay line read speed from d(dist)/dt
        |
        |  [Reverb]  (stereo -> stereo)
        |    Wet/dry = f(distance) -- more reverb when farther
        |    FDN or Schroeder topology
        |
        |  outL[i] = L
        |  outR[i] = R
        |
  +-> Write modulated position to PositionBridge (for GL thread)
```

### 5.2 Parameter Update Flow

```
Source A: DAW Automation
  |
  +-> APVTS atomic<float> updated by host
  |
Source B: UI Slider/Knob
  |
  +-> SliderAttachment / ButtonAttachment
  +-> APVTS atomic<float> updated on message thread
  |
Source C: OpenGL 3D drag
  |
  +-> Mouse event on GL thread
  +-> Post to message thread via AsyncUpdater or MessageManager::callAsync
  +-> Set APVTS parameter value on message thread
  +-> APVTS atomic<float> updated
  |
  v
processBlock() reads all atomics into EngineParams
  |
  v
engine.setParams(params) sets SmoothedParam targets
  |
  v
SmoothedParam::getNext() produces smoothed value per sample in process()
```

### 5.3 UI Rendering Flow

```
OpenGL Thread (background, ~60fps):
  |
  +-> renderOpenGL() called by JUCE OpenGLContext
  |     |
  |     +-> Read SourcePosition from PositionBridge (lock-free)
  |     +-> Update camera from stored mouse state
  |     +-> glClear, set viewport
  |     +-> Draw grid floor
  |     +-> Draw listener head (sphere at origin)
  |     +-> Draw source position (sphere at modulated XYZ)
  |     +-> Draw connecting line / trajectory trail
  |     +-> SwapBuffers (handled by JUCE)
  |
Message Thread (UI controls):
  |
  +-> PluginEditor::resized() lays out JUCE components
  +-> Sliders/combos for all DSP params (APVTS attachments)
  +-> Timer callback at 30Hz: update readouts (az, el, dist labels)
  +-> Mouse events on GL viewport -> convert to XYZ -> post to APVTS
```

---

## 6. Patterns to Follow

### Pattern 1: Denormal Protection

**What:** Floating-point denormals (numbers very close to zero) cause 10-100x CPU spikes in filter feedback loops.

**When:** Every `processBlock()` call.

```cpp
void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override {
    juce::ScopedNoDenormals noDenormals;  // FIRST LINE, always
    // ... rest of processing
}
```

The engine itself should also guard at the per-filter level by adding a tiny DC offset or flushing to zero:

```cpp
inline float flushDenormal(float x) {
    return (std::abs(x) < 1e-15f) ? 0.0f : x;
}
```

### Pattern 2: RAII Resource Management

**What:** All DSP resources (delay lines, filter states, buffers) are owned by the engine as members, not heap-allocated during processing.

**When:** Design all processor classes.

```cpp
class BinauralPanner {
    // Fixed-size members, no dynamic allocation needed during process
    DelayLine delayL;   // pre-allocated in prepare()
    DelayLine delayR;
    float filterStateL = 0.0f;
    float filterStateR = 0.0f;
    // NO std::vector that might resize, NO new/delete in process
};
```

### Pattern 3: Const-Correct Immutable Config

**What:** DSP configuration constants (speed of sound, head radius, max distance) defined as `constexpr` in a single header.

```cpp
// engine/include/xyzpan/Constants.h
namespace xyzpan {
    constexpr float kSpeedOfSound  = 343.0f;    // m/s at 20C
    constexpr float kHeadRadius    = 0.0875f;   // meters (average human)
    constexpr float kMaxDistance   = 100.0f;     // meters
    constexpr float kMinDistance   = 0.1f;       // meters (prevents division by zero)
    constexpr float kDefaultRampMs = 5.0;        // parameter smoothing time
}
```

---

## 7. Anti-Patterns to Avoid

### Anti-Pattern 1: Memory Allocation on the Audio Thread

**What:** Using `new`, `malloc`, `std::vector::push_back()`, `std::string`, or any STL container that may allocate inside `processBlock()` or any function called from it.

**Why bad:** Heap allocation invokes the OS allocator which takes locks, has unbounded timing, and causes priority inversion. Result: audio dropouts, clicks, glitches.

**Instead:** Pre-allocate everything in `prepare()`. Use fixed-size arrays (`std::array`), pre-sized delay lines, and stack-allocated temporaries.

### Anti-Pattern 2: Locking a Mutex on the Audio Thread

**What:** Using `std::mutex::lock()`, or even `std::mutex::try_lock()`, on the audio thread.

**Why bad:** `lock()` blocks with unbounded wait time. Even `try_lock()` is not safe -- if another thread is waiting, the audio thread interacts with the OS scheduler (a system call). `juce::ScopedLock` in APVTS listener callbacks is a known source of this.

**Instead:** Use `std::atomic<float>` for parameters. Use lock-free FIFOs or double-buffering for complex data. Design so that shared mutable state does not exist.

### Anti-Pattern 3: Recalculating Coefficients Every Sample When Unnecessary

**What:** Computing `sin()`, `cos()`, `tan()`, `exp()` for filter coefficients on every sample even when the parameter hasn't changed.

**Why bad:** Transcendental functions are expensive. A panner with 6 filters recalculating per sample wastes significant CPU.

**Instead:** Recalculate coefficients only when the controlling parameter's `SmoothedParam` is still ramping. Once it reaches its target, skip recalculation until the next parameter change:

```cpp
if (cutoffSmooth.isSmoothing()) {
    float cutoff = cutoffSmooth.getNext();
    recalcCoefficients(cutoff, sampleRate);
} else {
    cutoffSmooth.getNext(); // still call to keep consistent, returns target
}
```

### Anti-Pattern 4: Using Direct Form Biquads for Modulated Filters

**What:** Direct form I or II biquad implementations where filter coefficients change rapidly (e.g., from LFO modulation or source movement).

**Why bad:** Coefficient interpolation on direct form biquads can produce transient instability and audible artifacts. The transfer function changes non-linearly when you linearly interpolate coefficients.

**Instead:** Use state variable filters (SVF) or transposed direct form II (TDF-II), which handle modulation smoothly. SVFs allow direct modulation of frequency/Q without coefficient recalculation artifacts.

### Anti-Pattern 5: OpenGL Calls Taking MessageManagerLock

**What:** Inside `renderOpenGL()`, calling any JUCE function that acquires the message manager lock, or accessing Component methods that aren't thread-safe.

**Why bad:** On macOS, the OpenGL context is locked during `renderOpenGL()`. The main thread may also try to lock the GL context. Taking a MessageManagerLock creates a lock ordering inversion -> deadlock.

**Instead:** Read only from lock-free data structures (atomics, PositionBridge). Pre-compute all rendering data before entering the GL call. Use `juce::OpenGLContext::executeOnGLThread()` for resource allocation/deallocation housekeeping only.

### Anti-Pattern 6: Unbounded Loops in the Audio Callback

**What:** Loops whose iteration count depends on runtime data (e.g., processing a variable-length event list without bounds, while-loops waiting for a condition).

**Why bad:** Violates the hard realtime constraint. Worst-case execution time must be predictable.

**Instead:** All loops in the audio path must have fixed, known maximum iteration counts. For the main processing loop, `numSamples` is bounded by `maxBlockSize` from `prepare()`.

### Anti-Pattern 7: Sharing the Engine Between Threads Without a Clear Protocol

**What:** Both the UI and audio thread directly calling methods on the engine, or the engine storing state that both threads read/write.

**Why bad:** Data races. Undefined behavior. Intermittent crashes that are nearly impossible to reproduce.

**Instead:** The engine is owned exclusively by the audio thread. The only input to the engine is the `EngineParams` struct (populated from atomics at the top of `processBlock()`). The only output is audio buffers and the `PositionBridge` write. The UI never touches the engine directly.

---

## 8. Scalability Considerations

| Concern | This Plugin (1 instance) | Multiple Instances (32+) | Notes |
|---------|--------------------------|--------------------------|-------|
| CPU per instance | ~2-5% at 44.1kHz (target) | Scales linearly per instance | Per-sample processing is the bottleneck; profile early |
| Memory per instance | ~200KB (delay lines, filter states) | ~6.4MB for 32 instances | Acceptable for modern systems |
| Sample rate sensitivity | All delay line sizes scale with SR | 192kHz = 4.35x buffer sizes vs 44.1kHz | Pre-allocate for worst case in `prepare()` |
| SIMD potential | Limited (per-sample dependencies) | N/A | Could SIMD-ize independent L/R processing, but gains are modest for this topology |
| OpenGL overhead | Single context per editor | One per open editor window | DAWs typically only show one editor at a time |

---

## Sources

- [JUCE AudioProcessorValueTreeState Documentation](https://docs.juce.com/master/classjuce_1_1AudioProcessorValueTreeState.html)
- [JUCE OpenGLRenderer Documentation](https://docs.juce.com/master/classOpenGLRenderer.html)
- [JUCE OpenGLContext Documentation](https://docs.juce.com/master/classjuce_1_1OpenGLContext.html)
- [JUCE Introduction to DSP Tutorial](https://docs.juce.com/master/tutorial_dsp_introduction.html)
- [JUCE Cascading Plugin Effects Tutorial](https://juce.com/tutorials/tutorial_audio_processor_graph/)
- [JUCE SmoothedValue Documentation](https://docs.juce.com/master/classSmoothedValue.html)
- [Pamplejuce JUCE Plugin Template](https://github.com/sudara/pamplejuce)
- [Chowdhury-DSP JUCE Plugin Template](https://github.com/Chowdhury-DSP/JUCEPluginTemplate)
- [NTHN Template Plugin (Realtime-Safe State Management)](https://github.com/ncblair/NTHN_TEMPLATE_PLUGIN)
- [Using Locks in Real-Time Audio Processing, Safely -- timur.audio](https://timur.audio/using-locks-in-real-time-audio-processing-safely)
- [Four Common Mistakes in Audio Development -- A Tasty Pixel](https://atastypixel.com/four-common-mistakes-in-audio-development/)
- [3D Tune-In Toolkit Architecture](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0211899)
- [Spatial Audio Framework (SAF)](https://github.com/leomccormack/Spatial_Audio_Framework)
- [HALO 3D Pan -- Nathan Blair Binaural Panner](https://www.kvraudio.com/product/halo-3d-pan-by-nathan-blair)
- [Nathan Blair -- Developing Audio Plugins (Thesis)](https://nthnblair.com/thesis/)
- [WolfSound -- How To Build Audio Plugin with JUCE, CMake, Unit Tests](https://thewolfsound.com/how-to-build-audio-plugin-with-juce-cpp-framework-cmake-and-unit-tests/)
- [How to Use CMake with JUCE -- Melatonin](https://melatonin.dev/blog/how-to-use-cmake-with-juce/)
- [JUCE Forum -- Architectural Best Practices](https://forum.juce.com/t/architectural-best-practices-workflow-tips-tricks/43341)
- [JUCE Forum -- Understanding Lock in Audio Thread](https://forum.juce.com/t/understanding-lock-in-audio-thread/60007)
- [JUCE Forum -- APVTS Lock-Free Best Practices](https://forum.juce.com/t/latest-recommended-best-practices-wrt-audioprocessorvaluetreestate-lock-free-queues-multithreading/27295)
- [JUCE Forum -- How to Handle Modulations](https://forum.juce.com/t/how-to-handle-modulations/36660)
- [OpenGLRealtimeVisualization4JUCE Module](https://github.com/JanosGit/OpenGLRealtimeVisualization4JUCE)
