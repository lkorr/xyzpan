# Phase 6: UI and Parameter System - Research

**Researched:** 2026-03-13
**Domain:** JUCE OpenGL custom editor, APVTS parameter system, 3D spatial visualization, factory presets
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**OpenGL View — Visual Design**
- Hermetic alchemy-inspired color scheme: earthy browns, ochres, aged parchment tones — warm and dark
- Claude has full creative latitude to make the design striking; "wow factor" is the explicit goal
- Perspective 3D view with a box/room wireframe and floor grid for spatial orientation
- Click-drag on empty view area orbits the camera (rotates perspective)
- Click-drag on the source node moves the source on the current view plane (projected onto whichever two axes face the camera)
- Three preset view buttons for orthographic snaps: XY (top-down), XZ (side), YZ (front)
- Camera rotation shows strong perspective cues (vanishing point lines, wireframe foreshortening)

**Source Node Appearance**
- Node size = perspective distance from the camera (not just Z) — shrinks as source recedes in 3D
- Node opacity maps to acoustic distance gain: at minimum distance gain (furthest), opacity ~10%; at closest, fully opaque
- Node brightens + cursor changes to grab icon on hover/drag
- Listener node centered, non-moveable

**R (Radius/Scale) Parameter**
- New APVTS parameter: R multiplies XYZ before engine: effectiveX = X*R, effectiveY = Y*R, effectiveZ = Z*R
- R is DAW-automatable (PARAM-01 compliant)
- Visually: grid/room wireframe scales with R — the space appears bigger/smaller, source stays proportionally positioned
- R range: 0.0–2.0 (or similar), default 1.0

**Controls Layout**
- Plugin window is resizable; OpenGL view fills the majority of the window
- Bottom strip below the OpenGL view: four knobs in a row — X, Y, Z, R — spaced apart
- LFO controls sit below each respective knob (X LFO under X knob, Y LFO under Y knob, Z LFO under Z knob; R has no LFO)
- LFO per axis: waveform display (clickable — cycles sine → triangle → saw → square), frequency knob, depth knob, phase knob
- Small SYNC button next to each LFO frequency knob; when active, knob switches to beat-division display (e.g., "1/4")
- Reverb controls (size, decay, damping, wet) accessible in the main UI — placement Claude's discretion

**Dev Panel**
- Toggled open/closed from a button (side slide-out or overlay panel — Claude's discretion)
- Shows only DSP tuning constants — NOT X/Y/Z/R/LFO/reverb (those are in main UI)
- Grouped by DSP section: Binaural (head shadow, ILD, ITD, smoothing), Comb Filters (10x delay+FB + wet max), Elevation (pinna notch/Q/shelf, chest, floor), Distance (delay max, smooth, air abs)
- Each group is collapsible or clearly sectioned
- Scrollable — all 27+ params accessible without truncation
- Plain functional appearance (not the alchemy theme); serves as a dev/diagnostic surface

### Claude's Discretion
- Exact alchemy color palette — earthy browns, ochres, dark backgrounds encouraged; find references in hermetic/alchemical manuscript aesthetics
- Typography, icon design, knob style (rotary visual style)
- Reverb controls placement in main UI
- Dev panel open/close animation and exact layout
- Grid line density and room wireframe proportions
- How perspective rotation speed feels (mouse sensitivity)
- OpenGL shader details and rendering approach
- xyzpan_ui CMake target structure (planned since Phase 1, not yet created)
- Plugin window default size and aspect ratio

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PARAM-01 | All DSP parameters exposed as VST automation parameters | R parameter registration in ParamLayout + ParamIDs; APVTS SliderAttachment pattern; processor reads R, multiplies before engine call |
| PARAM-02 | Dev panel exposes all internal constants (filter frequencies, delay ranges, dB values, comb tunings) | 27+ dev params already registered in APVTS Phases 2-4; dev panel just needs SliderAttachment or direct atomic reads + custom knobs |
| PARAM-03 | Parameter changes are smoothed to prevent zipper noise (no clicks on automation) | Already handled in engine OnePoleSmooth; UI must use SliderAttachment (which writes APVTS atomics, not direct to engine); R new param needs same smoothing treatment in processor |
| PARAM-04 | Plugin state saves and restores correctly across DAW sessions | getStateInformation/setStateInformation already wired via apvts.copyState()/replaceState(); needs session testing; factory presets use same mechanism |
| PARAM-05 | Factory presets demonstrating spatial positions and LFO patterns | XML-based presets stored as binary resources or inline strings; loaded via apvts.replaceState(ValueTree::fromXml(...)) |
| UI-01 | Custom OpenGL renderer showing 2D projection of 3D space | OpenGLContext attached to child component; OpenGLRenderer renderOpenGL(); perspective projection via GLM glm::perspective; wireframe room via GL_LINES |
| UI-02 | Listener node centered in the view | Static sphere mesh at world origin; drawn with separate pass so it's never draggable |
| UI-03 | Object node draggable in X/Y, with Z shown as size change (depth perspective) | Hit-test against projected source position in mouseDown; mouseDrag computes view-plane delta, posts to APVTS via message thread; node size = perspective-projected size |
| UI-04 | LFO controls visible per axis (waveform, rate, depth, phase) | JUCE Sliders with SliderAttachment for rate/depth/phase; custom waveform icon component cycling 0-3; SYNC button = ButtonAttachment for tempo sync |
| UI-05 | Dev panel toggleable showing all tuneable DSP constants | Overlay or side-panel Component; contains ScrollableComponent with grouped Sliders + SliderAttachments for all 27+ dev params |
| UI-06 | All parameter controls also accessible as standard UI elements | Bottom strip knobs (X/Y/Z/R) + reverb sliders + LFO controls all use SliderAttachment; dev panel provides full APVTS coverage |
| UI-07 | UI updates at display rate, not audio rate (double-buffer pattern for audio→GL) | Atomic PositionBridge struct; audio thread writes modulated XYZ after each engine call; GL thread reads in renderOpenGL(); APVTS atomics for base position read by message thread |
</phase_requirements>

---

## Summary

Phase 6 is the complete custom editor replacement. The existing `PluginProcessor.cpp` returns `GenericAudioProcessorEditor` — Phase 6 replaces `createEditor()` with a real `XYZPanEditor` class. The APVTS, state save/restore, and all 27+ dev parameters are already registered from Phases 1-5. The primary new work is: (1) creating the `xyzpan_ui` CMake target with real sources, (2) building the `XYZPanEditor` class with its OpenGL spatial view and JUCE control strip, (3) adding the `R` radius/scale parameter to ParamIDs + ParamLayout + PluginProcessor, and (4) implementing factory presets via APVTS XML.

The critical design challenge is the OpenGL/JUCE integration: the `OpenGLContext` must be attached to a child `Component` (not the editor root) to avoid host-specific glitches, `detach()` must be called in the component destructor, and all audio-to-GL data transfer must use a lock-free atomic struct (no MessageManagerLock inside `renderOpenGL()`). The camera orbit, source drag, and node projection are all standard 3D math via GLM — straightforward once the context is set up correctly.

The alchemy aesthetic is a design goal, not a technical uncertainty. Color palette, shader uniforms, and JUCE LookAndFeel customization are the tools. The LFO waveform display is a custom `Component` that paints a mini waveform shape; the dev panel is a scrollable `Component` with grouped sliders using SliderAttachment — functional and uncomplicated.

**Primary recommendation:** Build 06-01 (APVTS R parameter + state) first, then 06-02 (OpenGL renderer core), then 06-03 (LFO controls + dev panel), then 06-04 (presets + double-buffer polish). This ordering keeps each plan independently verifiable.

---

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| JUCE juce_opengl | 8.0.12 | OpenGLContext, OpenGLRenderer, OpenGLShaderProgram | Already in project; juce::juce_opengl links OpenGL system libraries on Windows automatically |
| JUCE juce_audio_processors | 8.0.12 | APVTS, SliderAttachment, ButtonAttachment, AudioProcessorEditor | All parameter/UI binding; standard JUCE plugin editor pattern |
| GLM | 1.0.1 | glm::mat4, glm::vec3, glm::perspective, glm::lookAt | Already in CMakeLists.txt; GLSL-compatible math for view/projection matrices |
| OpenGL | 3.2 Core | Vertex/fragment shaders, VBOs, wireframe rendering | Broadest hardware support; GL 3.2 Core is the JUCE-recommended profile |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| std::atomic | C++20 stdlib | PositionBridge: lock-free audio→GL data transfer | For the double-buffer struct that carries modulated XYZ from audio thread to GL thread |
| juce::ScrollableComponent / Viewport | 8.0.12 | Dev panel scrollable area | Viewport wraps a tall inner component for the 27+ dev sliders |
| juce::LookAndFeel | 8.0.12 | Alchemy color scheme, custom knob painting | Subclass to override drawRotarySlider, drawButtonBackground, etc. |
| juce::ComponentBoundsConstrainer | 8.0.12 | Resizable editor with aspect-ratio constraints | setResizeLimits() on editor constructor |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Custom OpenGL shaders | JUCE software renderer | Software renderer cannot do 3D perspective at 60fps; locked decision |
| APVTS SliderAttachment | juce::Value listeners | SliderAttachment handles thread safety, undo, automation recording automatically; always prefer it |
| GLM for math | JUCE Matrix3D/Vector3D | JUCE has Matrix3D but GLM is already in the project and has more complete API (glm::perspective, glm::lookAt, quaternions) |

**Installation (no new dependencies needed):**
```bash
# juce_opengl already in project — just add to plugin CMakeLists.txt:
target_link_libraries(XYZPan PRIVATE juce::juce_opengl ...)
# GLM already in root CMakeLists.txt via CPMAddPackage
# xyzpan_ui target: convert from INTERFACE to real static library in ui/CMakeLists.txt
```

---

## Architecture Patterns

### Recommended Project Structure Changes for Phase 6
```
ui/
├── CMakeLists.txt            # Convert INTERFACE -> real static library with sources
├── XYZPanGLView.h/.cpp       # OpenGLRenderer — renderOpenGL(), mouse, camera
├── PositionBridge.h          # Lock-free atomic struct for audio→GL data transfer
├── Shaders.h                 # Vertex/fragment shader source strings (GL 3.2)
├── Camera.h/.cpp             # Orbit camera: yaw/pitch, view matrix, ortho snaps
├── Mesh.h/.cpp               # Sphere (listener/source), wireframe box, floor grid
└── AlchemyLookAndFeel.h/.cpp # Custom LookAndFeel: knob style, colors, fonts

plugin/
├── PluginEditor.h            # XYZPanEditor — real class replacing Phase 1 stub
├── PluginEditor.cpp          # createEditor() returns new XYZPanEditor(*this)
├── ParamIDs.h                # Add: R = "r"
└── ParamLayout.cpp           # Add: APF for R, range 0.0–2.0, default 1.0
```

### Pattern 1: OpenGL Context Attached to Child Component (Not Editor Root)

**What:** `XYZPanGLView` is a `juce::Component` that owns the `OpenGLContext` and implements `juce::OpenGLRenderer`. The `XYZPanEditor` adds it as a child component. The `OpenGLContext` is attached to `XYZPanGLView`, not to the editor.

**When to use:** Always in a plugin context. Attaching to the editor root causes host-specific glitches (broken NSWindows on macOS, D2D conflicts on Windows).

**Example:**
```cpp
// Source: JUCE docs + forum recommendation
// ui/XYZPanGLView.h
class XYZPanGLView : public juce::Component,
                     public juce::OpenGLRenderer {
public:
    XYZPanGLView() {
        glContext.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
        glContext.setRenderer(this);
        glContext.setContinuousRepainting(true);
        // CRITICAL: configure BEFORE attachTo()
        glContext.attachTo(*this);
    }
    ~XYZPanGLView() override {
        glContext.detach(); // MUST call before component destructs
    }
    void newOpenGLContextCreated() override { /* compile shaders, create VBOs */ }
    void renderOpenGL() override { /* draw scene */ }
    void openGLContextClosing() override { /* release GL resources */ }
private:
    juce::OpenGLContext glContext;
    // ... shaders, VBOs, position bridge
};
```

### Pattern 2: Lock-Free Audio-to-GL Position Bridge (UI-07)

**What:** A double-buffer struct with an atomic index. Audio thread writes the modulated XYZ; GL thread reads the latest written value. No mutex, no MessageManagerLock.

**When to use:** Any time the audio thread needs to send data to the UI/GL thread (modulated position, LFO visualization state).

**Example:**
```cpp
// Source: JUCE ARCHITECTURE.md in project research (established pattern)
// ui/PositionBridge.h
struct SourcePositionSnapshot {
    float x = 0.0f, y = 1.0f, z = 0.0f;     // modulated position
    float azimuth = 0.0f, elevation = 0.0f;   // derived (for display readouts)
    float distance = 1.0f;
};

class PositionBridge {
public:
    // Called from audio thread after engine.process()
    void write(const SourcePositionSnapshot& pos) {
        int idx = 1 - writeIdx.load(std::memory_order_relaxed);
        buf[idx] = pos;
        writeIdx.store(idx, std::memory_order_release);
    }
    // Called from GL thread in renderOpenGL()
    SourcePositionSnapshot read() const {
        return buf[writeIdx.load(std::memory_order_acquire)];
    }
private:
    SourcePositionSnapshot buf[2];
    std::atomic<int> writeIdx{0};
};
```

Note: The base (non-modulated) X/Y/Z are already available to the UI via APVTS atomics. PositionBridge only needs to carry the modulated position (base + LFO offset) which the GL thread cannot read from APVTS because the engine applies LFO internally.

### Pattern 3: APVTS SliderAttachment for All Knobs

**What:** Every slider/knob in the editor is backed by a `std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>`. The attachment handles range mapping, thread-safe writes, automation recording, and undo/redo automatically.

**When to use:** All JUCE plugin controls. Never write directly to APVTS parameter values from the UI; always go through attachment.

**Example:**
```cpp
// Source: JUCE official tutorial
// plugin/PluginEditor.h (partial)
class XYZPanEditor : public juce::AudioProcessorEditor {
public:
    explicit XYZPanEditor(XYZPanProcessor&);
    ~XYZPanEditor() override;
    void resized() override;
    void paint(juce::Graphics&) override;
private:
    XYZPanProcessor& proc;

    // GL view (fills most of editor)
    XYZPanGLView glView;

    // Bottom strip knobs
    juce::Slider xKnob, yKnob, zKnob, rKnob;
    using SlAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SlAtt> xAtt, yAtt, zAtt, rAtt;

    // Dev panel toggle + overlay
    juce::TextButton devPanelButton;
    std::unique_ptr<DevPanelComponent> devPanel;
    // ...
};
```

### Pattern 4: R Parameter Integration (PARAM-01)

**What:** R multiplies X/Y/Z in the processor before calling `engine.setParams()`. R itself is never passed to the engine. The engine struct has no R field.

**Implementation path:**
1. Add `constexpr const char* R = "r";` to `ParamIDs.h`
2. Add `APF(PID{ParamID::R, 1}, "R Scale", NR(0.0f, 2.0f, 0.001f), 1.0f)` to `ParamLayout.cpp`
3. Add `std::atomic<float>* rParam = nullptr;` and `rParam = apvts.getRawParameterValue(ParamID::R);` to `PluginProcessor`
4. In `processBlock()`: `params.x = xParam->load() * rParam->load(); params.y = yParam->load() * rParam->load(); params.z = zParam->load() * rParam->load();`
5. Clamp result to [-1, 1] before passing to engine (R can amplify positions beyond the unit cube — may want soft clamping or allow it per design intent)

**Clamp decision:** The CONTEXT.md does not specify explicit clamping behavior. Since the engine already clamps inputs, allowing R to push beyond ±1 is valid — it will be clamped internally. This is the correct behavior: R > 1 pushes source to the "walls" of the spatial field.

### Pattern 5: Factory Presets (PARAM-05)

**What:** Factory presets are XML strings stored as inline `const char*` arrays or compiled as binary resources. Loading is done via `apvts.replaceState(juce::ValueTree::fromXml(*xml))` on the message thread.

**Example:**
```cpp
// Source: JUCE tutorial + forum patterns
// Inline preset XML structure (generated from apvts.copyState().createXml())
static const char* kPresetFront = R"(
<XYZPanState>
  <PARAM id="x" value="0.0"/>
  <PARAM id="y" value="1.0"/>
  <PARAM id="z" value="0.0"/>
  <PARAM id="r" value="1.0"/>
  <!-- ... all params at default ... -->
</XYZPanState>
)";

void XYZPanProcessor::loadPreset(const char* xmlString) {
    if (auto xml = juce::XmlDocument::parse(xmlString))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}
```

**Preset content plan** (5 factory presets):
1. "Front Center" — X=0, Y=1, Z=0, R=1, all LFOs off
2. "Overhead" — X=0, Y=0, Z=0.8, R=1, all LFOs off
3. "Behind & Far" — X=0, Y=-0.8, Z=0, R=1.5, all LFOs off
4. "Orbiting XY" — X=0, Y=0.5, Z=0, R=1, LFO X rate=0.25Hz depth=0.8, LFO Y rate=0.25Hz depth=0.8 (phase offset 90deg → circular orbit)
5. "Deep Space" — R=2.0, X=0, Y=0.5, Z=0, reverb wet high, distance delay max

### Pattern 6: Resizable Editor

**What:** `setResizable(true, true)` with `setResizeLimits()` in the editor constructor. The GL view component uses `setBoundsRelative()` or responds to `resized()` to fill the top portion; the bottom strip has fixed height.

**Example:**
```cpp
XYZPanEditor::XYZPanEditor(XYZPanProcessor& p)
    : AudioProcessorEditor(&p), proc(p) {
    setResizable(true, true);
    setResizeLimits(500, 400, 1600, 1200);
    setSize(800, 600); // default
    // ... add children
}

void XYZPanEditor::resized() {
    auto bounds = getLocalBounds();
    const int stripH = 180; // bottom controls height
    glView.setBounds(bounds.removeFromTop(bounds.getHeight() - stripH));
    // layout bottom strip...
}
```

### Pattern 7: Camera Orbit Implementation

**What:** Camera maintains yaw and pitch angles. Mouse drag on empty space (not on source node) updates these angles. View matrix recomputed each frame via `glm::lookAt`. Three orthographic snap buttons override to fixed view matrices.

**Example:**
```cpp
// Source: LearnOpenGL camera pattern + GLM API
struct Camera {
    float yaw   = 45.0f;   // degrees
    float pitch = 30.0f;   // degrees — slight elevation for 3D feel
    float dist  = 4.0f;    // camera radius from origin

    glm::mat4 getViewMatrix() const {
        float yr = glm::radians(yaw), pr = glm::radians(pitch);
        glm::vec3 eye = {
            dist * std::cos(pr) * std::sin(yr),
            dist * std::sin(pr),
            dist * std::cos(pr) * std::cos(yr)
        };
        return glm::lookAt(eye, glm::vec3(0), glm::vec3(0,1,0));
    }

    // Ortho snap presets
    glm::mat4 getTopDownView()  const { return glm::lookAt({0,dist,0}, {0,0,0}, {0,0,-1}); }
    glm::mat4 getSideView()     const { return glm::lookAt({0,0,dist}, {0,0,0}, {0,1,0}); }
    glm::mat4 getFrontView()    const { return glm::lookAt({dist,0,0}, {0,0,0}, {0,1,0}); }
};
```

**Mouse orbit integration:**
```cpp
void XYZPanGLView::mouseDrag(const juce::MouseEvent& e) {
    if (isDraggingSource) {
        // Project drag delta to current view plane, update X/Y/Z via APVTS
    } else {
        // Orbit camera
        camera.yaw   += e.getDistanceFromDragStartX() * 0.3f - lastDragX * 0.3f;
        camera.pitch  = juce::jlimit(-89.0f, 89.0f, camera.pitch - e.getDistanceFromDragStartY() * 0.3f + lastDragY * 0.3f);
        lastDragX = e.getDistanceFromDragStartX();
        lastDragY = e.getDistanceFromDragStartY();
    }
}
```

### Pattern 8: Source Node Hit-Testing and View-Plane Drag

**What:** On `mouseDown`, project the 3D source position to screen coordinates. If mouse position is within the projected node radius, set `isDraggingSource = true`. On `mouseDrag`, un-project the delta from screen to world space on the view-facing plane through the source, then write new X/Y/Z to APVTS on the message thread.

**The key math:**
```cpp
// Project 3D world pos to 2D screen pos
glm::vec4 clip = projMatrix * viewMatrix * glm::vec4(worldPos, 1.0f);
glm::vec3 ndc = glm::vec3(clip) / clip.w;
float screenX = (ndc.x * 0.5f + 0.5f) * viewportW;
float screenY = (1.0f - (ndc.y * 0.5f + 0.5f)) * viewportH; // flip Y

// Post parameter update to message thread (NEVER write APVTS from GL thread directly)
juce::MessageManager::callAsync([this, newX, newY, newZ]() {
    if (auto* param = proc.apvts.getParameter(ParamID::X))
        param->setValueNotifyingHost(proc.apvts.getParameterRange(ParamID::X).convertTo0to1(newX));
    // ... similarly for Y, Z
});
```

### Anti-Patterns to Avoid

- **Taking MessageManagerLock in renderOpenGL():** Causes deadlock on macOS. All GL thread code must be lock-free.
- **Attaching OpenGLContext to editor root:** Causes host-specific rendering glitches. Attach to a child component.
- **Forgetting glContext.detach() in destructor:** Background GL thread continues running after component is deleted → crash.
- **Writing to APVTS from GL thread:** APVTS listeners may call JUCE functions that are not thread-safe from the GL thread. Use `MessageManager::callAsync()` to bounce writes to the message thread.
- **Compiling shaders every frame:** Compile shaders once in `newOpenGLContextCreated()`, cache the `OpenGLShaderProgram`.
- **Recreating VBOs every frame:** Create VBOs in `newOpenGLContextCreated()`, update uniform data (position, transform matrices) per frame instead.
- **Using R > 1 to produce X*R > 1 and not communicating this:** The engine clamps at ±1, so high R values saturate. This is the intended behavior (source "sticks to the wall" of the spatial field), but the visual (grid scaling) should make this clear.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Slider-to-parameter binding | Custom observer/callback system | APVTS `SliderAttachment` | Handles thread safety, undo, automation recording, value range mapping automatically |
| Parameter state XML serialization | Custom XML parser | `apvts.copyState().createXml()` + `ValueTree::fromXml()` | APVTS state is already a ValueTree; one-liner serialization |
| View/projection matrix math | Hand-written matrix multiply | GLM `glm::perspective`, `glm::lookAt`, `glm::mat4` | GLM is already in project; battle-tested, GLSL-compatible |
| Knob paint logic from scratch | Custom `Graphics::fillEllipse` arc drawing | Custom `LookAndFeel::drawRotarySlider` override | Override one method to restyle all rotary sliders globally |
| Scrollable dev panel layout | Manual scroll tracking + component offsets | `juce::Viewport` wrapping a tall `Component` | Viewport handles scroll bar, clipping, and mouse wheel automatically |
| Mouse-drag scroll in dev panel | Raw mouse event tracking | `juce::Viewport` | Viewport handles drag-scrolling automatically |

**Key insight:** The APVTS + SliderAttachment pattern does 80% of the "parameter system" work automatically. The main Phase 6 code is the OpenGL rendering and visual design.

---

## Common Pitfalls

### Pitfall 1: OpenGL Context Lifecycle Crash on Editor Close/Reopen
**What goes wrong:** DAW closes and reopens the plugin editor (common in every DAW). If `glContext.detach()` is not called in the component destructor, the GL background thread continues running and accesses freed memory. On the next open, `attachTo()` is called again but the thread from the previous context is still alive.

**Why it happens:** JUCE's `OpenGLContext` starts a background thread. If the owning component is destroyed before `detach()` runs, the thread has a dangling reference.

**How to avoid:** `~XYZPanGLView()` must call `glContext.detach()` as the first line. Order is critical: detach before any member variables are destroyed.

**Warning signs:** Intermittent crashes on project close, JUCE ASSERT failures in debug about GL thread activity after context close.

### Pitfall 2: GL Thread Writing APVTS Parameters Directly
**What goes wrong:** Source drag logic runs in `mouseDrag()` (called on the JUCE message thread when the Component is the responder, but the GL context's mouse event forwarding can call it from the GL thread in some configurations). Writing to APVTS from the wrong thread triggers JUCE assertions and can cause UI-thread-only listeners to be called from the audio thread.

**Why it happens:** JUCE `OpenGLContext` mouse events are forwarded through the component system but the originating thread depends on context attachment target.

**How to avoid:** Always use `juce::MessageManager::callAsync()` to bounce any APVTS writes from drag events. The lambda captures new X/Y/Z values and calls `param->setValueNotifyingHost()` on the message thread.

**Warning signs:** JUCE assertions about thread safety in AudioProcessorParameter; DAW automation lane shows no recording despite dragging.

### Pitfall 3: R Parameter Causing Unexpected Phase 1-5 State Invalidation
**What goes wrong:** Adding the R parameter to ParamLayout changes the APVTS ValueTree structure. If a session was saved with Phase 5 plugin state (no R param), loading it with Phase 6 plugin (has R param) may cause `setStateInformation` to fail or partially restore.

**Why it happens:** APVTS `replaceState()` only replaces parameters present in the XML. Missing parameters use their registered defaults. This is safe and correct for forward compatibility.

**How to avoid:** The existing `setStateInformation` implementation (`apvts.replaceState(juce::ValueTree::fromXml(*xml))`) already handles this correctly — missing parameters default. No extra migration code needed. Confirm via testing with a pre-R session file.

**Warning signs:** R knob shows unexpected values after session load; other parameters reset unexpectedly (would indicate a bug in the XML tag name check).

### Pitfall 4: Source Node Hit-Test Using Stale Screen Projection
**What goes wrong:** The hit-test for `mouseDown` uses a screen projection computed during `renderOpenGL()` (which runs on the GL thread). If the GL thread has not rendered a frame since the editor was resized, the stored projected position is stale.

**Why it happens:** Screen projection depends on viewport size. If viewport size changes between render frames, the stored projection is wrong.

**How to avoid:** Recompute the projection in `mouseDown()` using the current viewport size and the latest position from PositionBridge. This recomputation is cheap (one matrix multiply per click).

### Pitfall 5: Dev Panel SliderAttachment Lifetime Mismatch
**What goes wrong:** The dev panel is shown/hidden by toggling visibility. If SliderAttachments are created and destroyed with the panel visibility, the parameters still exist in APVTS and may receive automation. Mismatched lifetimes cause dangling references.

**Why it happens:** SliderAttachment holds a pointer to the Slider. If Slider is destroyed while Attachment exists (or vice versa), crash.

**How to avoid:** Create all dev panel Sliders and their SliderAttachments in the DevPanelComponent constructor. Keep them alive as long as the DevPanelComponent exists. Show/hide the DevPanelComponent rather than creating/destroying it.

### Pitfall 6: JUCE 8 OpenGL + Direct2D Conflict on Windows
**What goes wrong:** JUCE 8 introduced Direct2D rendering on Windows. When OpenGL is also active, some NVIDIA drivers produce GUI sluggishness and DAW freezes. This was a known JUCE 8.0.8+ issue.

**Why it happens:** JUCE 8 Direct2D renderer and OpenGL context compete for the window handle. Thread synchronization between them can cause stalls.

**How to avoid:** Add `JUCE_DIRECT2D=0` compile definition to disable Direct2D and force software renderer for non-GL components. The GL view handles its own hardware rendering; the rest of the UI (knobs, labels) can use software rendering at typical update rates without issue.
```cmake
# In plugin/CMakeLists.txt
target_compile_definitions(XYZPan PUBLIC JUCE_DIRECT2D=0)
```

**Warning signs:** Plugin renders fine in isolation but causes DAW meter drops or frame rate lag when plugin editor is open.

---

## Code Examples

Verified patterns from JUCE official sources and project research:

### CMake: Converting xyzpan_ui from INTERFACE to Real Library
```cmake
# ui/CMakeLists.txt — Phase 6 replaces the Phase 1 placeholder
add_library(xyzpan_ui STATIC
    XYZPanGLView.cpp
    Camera.cpp
    Mesh.cpp
    AlchemyLookAndFeel.cpp
)

target_include_directories(xyzpan_ui
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(xyzpan_ui
    PUBLIC
        juce::juce_opengl
        juce::juce_audio_processors
        glm::glm
)
```

```cmake
# plugin/CMakeLists.txt — add xyzpan_ui and juce_opengl
target_link_libraries(XYZPan
    PRIVATE
        xyzpan_engine
        xyzpan_ui          # <-- Phase 6 addition
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl  # <-- Phase 6 addition
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
```

### XYZPanEditor: createEditor() and Constructor Pattern
```cpp
// plugin/PluginEditor.h — Phase 6 real implementation
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "XYZPanGLView.h"

class XYZPanProcessor;

class XYZPanEditor : public juce::AudioProcessorEditor {
public:
    explicit XYZPanEditor(XYZPanProcessor&);
    ~XYZPanEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    XYZPanProcessor& proc;
    XYZPanGLView glView;  // owns OpenGLContext

    // X/Y/Z/R knobs in bottom strip
    juce::Slider xKnob, yKnob, zKnob, rKnob;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SA> xAtt, yAtt, zAtt, rAtt;

    // ... LFO controls, reverb controls, dev panel
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XYZPanEditor)
};
```

```cpp
// plugin/PluginProcessor.cpp — Phase 6: replace createEditor()
juce::AudioProcessorEditor* XYZPanProcessor::createEditor() {
    return new XYZPanEditor(*this);  // replaces GenericAudioProcessorEditor
}
```

### OpenGL Shader Strings (Minimal 3D Scene)
```glsl
// Source: JUCE OpenGL tutorial + LearnOpenGL
// Vertex shader (GL 3.2 Core, GLSL 150)
const char* vertexShader = R"(
#version 150 core
in vec3 position;
in vec3 color;
uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform float opacity;
out vec4 fragColor;
void main() {
    gl_Position = projection * view * model * vec4(position, 1.0);
    fragColor = vec4(color, opacity);
}
)";

// Fragment shader
const char* fragmentShader = R"(
#version 150 core
in vec4 fragColor;
out vec4 outColor;
void main() {
    outColor = fragColor;
}
)";
```

### Wiring PositionBridge: Audio Thread Write
```cpp
// plugin/PluginProcessor.cpp — add after engine.process() call
void XYZPanProcessor::processBlock(juce::AudioBuffer<float>& buffer, ...) {
    // ... existing param snapshot and engine.setParams(params) ...
    engine.process(...);

    // Phase 6: write modulated position to bridge for GL thread
    // The engine exposes getLastModulatedPosition() (to be added to engine API)
    // OR: read the APVTS base position + let GL thread know it's "base only"
    // Simplest approach: post base XYZ (LFO visualization happens via timer on msg thread)
    SourcePositionSnapshot snap;
    snap.x = params.x;  // base position (already R-multiplied)
    snap.y = params.y;
    snap.z = params.z;
    positionBridge.write(snap);
}
```

Note: Full LFO-modulated position requires engine to expose last computed modulated XYZ. If that's not added to the engine API in Plan 06-02, the GL view shows base position only (which still animates correctly during automation; LFO causes the displayed position to lag the audio by one block — acceptable for visualization).

### LFO Waveform Icon Component
```cpp
// Custom component that draws a mini waveform and cycles on click
class LFOWaveformButton : public juce::Component {
    int waveform = 0; // 0=sine, 1=tri, 2=saw, 3=square
    std::function<void(int)> onChange;
public:
    void paint(juce::Graphics& g) override {
        // Draw waveform shape using Graphics::startNewSubPath / lineTo
        // for the current waveform type
        auto bounds = getLocalBounds().toFloat().reduced(4.0f);
        juce::Path p;
        const int N = 64;
        for (int i = 0; i <= N; ++i) {
            float t = (float)i / N;
            float y = computeWaveformValue(t, waveform); // [-1,1]
            float px = bounds.getX() + t * bounds.getWidth();
            float py = bounds.getCentreY() - y * bounds.getHeight() * 0.4f;
            if (i == 0) p.startNewSubPath(px, py);
            else p.lineTo(px, py);
        }
        g.setColour(juce::Colours::orange.withAlpha(0.8f));
        g.strokePath(p, juce::PathStrokeType(1.5f));
    }
    void mouseUp(const juce::MouseEvent&) override {
        waveform = (waveform + 1) % 4;
        if (onChange) onChange(waveform);
        repaint();
    }
};
```

### Dev Panel with Viewport + Scrollable Content
```cpp
// DevPanelComponent: scrollable, grouped sliders for all 27+ dev params
class DevPanelComponent : public juce::Component {
    juce::Viewport viewport;
    juce::Component content;  // tall inner component
    // ... arrays of sliders and attachments for all dev params
public:
    DevPanelComponent(juce::AudioProcessorValueTreeState& apvts) {
        addAndMakeVisible(viewport);
        viewport.setViewedComponent(&content, false);

        // Create all 27+ sliders with SliderAttachments
        // Grouped: Binaural (7), Comb (21), Elevation (7), Distance (5)
        // contentHeight = sum of group heights + spacing
        content.setSize(contentWidth, contentHeight);
    }
    void resized() override {
        viewport.setBounds(getLocalBounds());
    }
};
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Projucer-generated UI | CMake + custom editor class | JUCE 6+ | Full control over editor class hierarchy |
| GenericAudioProcessorEditor | Custom AudioProcessorEditor with OpenGL | Phase 6 | Replaces auto-generated UI with production UI |
| Fixed-size plugin editors | setResizable() + setResizeLimits() | JUCE 7+ | Hosts that support resizing (Reaper, Ableton) get flexible layout |
| OpenGL fixed-function pipeline (glBegin/glEnd) | GLSL shaders + VAO/VBO | GL 3.2 Core | Required for GL Core profile; more control over rendering |
| JUCE 7 OpenGL rendering | JUCE 8 OpenGL + potential D2D conflict | JUCE 8.0.8 | Add JUCE_DIRECT2D=0 to avoid conflict on Windows |

**Deprecated/outdated:**
- `glBegin()`/`glEnd()` fixed-function pipeline: not available in GL 3.2 Core profile; use VAO + VBO + shaders
- `OpenGLAppComponent` as base class: fine for standalone apps, but for plugins use `Component` + `OpenGLRenderer` attachment pattern
- JUCE `Matrix3D<float>` for 3D transforms: works, but GLM is already in the project and has better API coverage; use GLM

---

## Open Questions

1. **Engine modulated position exposure**
   - What we know: The engine processes LFO internally per sample. PluginProcessor calls `engine.process()` but does not get back the modulated XYZ.
   - What's unclear: Does Plan 06-02 need to add an `engine.getLastModulatedXYZ()` method, or is showing base position in the GL view acceptable?
   - Recommendation: Add `xyzpan::XYZPanEngine::getLastModulatedPosition()` returning a `{float x, float y, float z}` struct. This is a one-line addition to Engine.h and the implementation stores last computed values in the per-sample loop. Enables the GL view to correctly animate LFO motion.

2. **JUCE_DIRECT2D=0 impact on other JUCE components**
   - What we know: JUCE 8 D2D/OpenGL conflict is documented on Windows NVIDIA.
   - What's unclear: Does disabling D2D for the entire XYZPan target affect rendering quality of the non-GL JUCE components (sliders, labels, buttons)?
   - Recommendation: Test both with and without JUCE_DIRECT2D=0. Software renderer on non-GL components at 60fps for a few dozen components is not measurable overhead. If D2D issues manifest, disable. Otherwise leave default.

3. **Preset XML format: inline strings vs. binary resources**
   - What we know: JUCE supports `BinaryData` (via `juce_add_binary_data`) to compile files as C++ arrays.
   - What's unclear: Is there a preference for inline strings vs. compiled binary data for 5 presets?
   - Recommendation: Use inline `const char*` strings for 5 presets. At this scale, BinaryData adds CMake complexity for negligible benefit. If preset count grows to 20+, switch to BinaryData.

---

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Catch2 3.7.1 |
| Config file | tests/CMakeLists.txt (catch_discover_tests) |
| Quick run command | `ctest --test-dir build -R XYZPanTests -C Debug` |
| Full suite command | `ctest --test-dir build -C Debug` |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| PARAM-01 | R parameter registered in APVTS, appears in automation lane | Manual (DAW) | pluginval run | ❌ Wave 0 |
| PARAM-02 | All 27+ dev params accessible in dev panel UI | Manual (visual) | n/a — UI inspection | n/a |
| PARAM-03 | No clicks/zipper noise during automation | Manual (audio) | n/a — listening test | n/a |
| PARAM-04 | State saves and restores across DAW close/reopen | Manual (DAW) | pluginval state test | n/a |
| PARAM-05 | Factory presets load correctly, all params set | unit | `ctest --test-dir build -R TestPresets -C Debug` | ❌ Wave 0 |
| UI-01 | OpenGL renderer creates context, renders without crash | unit/smoke | `ctest --test-dir build -R TestPluginProcessor -C Debug` | ❌ Wave 0 |
| UI-02 | Listener node visible at origin | Manual (visual) | n/a | n/a |
| UI-03 | Source drag updates X/Y/Z params in real time | Manual (visual+audio) | n/a | n/a |
| UI-04 | LFO controls visible, waveform display cycles correctly | Manual (visual) | n/a | n/a |
| UI-05 | Dev panel toggles, all params accessible | Manual (visual) | n/a | n/a |
| UI-06 | All params accessible as standard controls | unit | `ctest --test-dir build -R TestParameterLayout -C Debug` | ❌ Wave 0 |
| UI-07 | PositionBridge: audio write/GL read thread safety | unit | `ctest --test-dir build -R TestPositionBridge -C Debug` | ❌ Wave 0 |

**Note on UI test philosophy:** OpenGL rendering tests and visual layout tests are manually verified. The automated tests cover: (a) APVTS parameter registration including R, (b) PositionBridge lock-free correctness, (c) factory preset XML load/restore round-trip, (d) pluginval validation.

### Sampling Rate
- **Per task commit:** `ctest --test-dir build -C Debug` (existing 64 tests; adding new tests in Wave 0)
- **Per wave merge:** `ctest --test-dir build -C Debug` (full suite)
- **Phase gate:** Full suite green + pluginval passes before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/plugin/TestParameterLayout.cpp` — covers PARAM-01 (R registration), PARAM-06 coverage check
- [ ] `tests/plugin/TestPresets.cpp` — covers PARAM-05 (preset XML round-trip)
- [ ] `tests/plugin/TestPositionBridge.cpp` — covers UI-07 (atomic write/read correctness)
- [ ] Plugin test infrastructure: test CMakeLists.txt needs JUCE link for plugin-layer tests

**Note on plugin test target:** Existing `XYZPanTests` links `xyzpan_engine` only (SETUP-02 enforcement — no JUCE in engine tests). PARAM-01 and PARAM-05 tests require JUCE (they test APVTS). A separate `XYZPanPluginTests` target is needed that links `XYZPan_SharedCode` or similar. This is a Wave 0 task for Plan 06-01.

---

## Sources

### Primary (HIGH confidence)
- JUCE docs: `juce::OpenGLContext` — context lifecycle, attachTo/detach, threading
- JUCE docs: `juce::OpenGLRenderer` — renderOpenGL, newOpenGLContextCreated, openGLContextClosing
- JUCE docs: `juce::AudioProcessorValueTreeState::SliderAttachment` — parameter binding API
- JUCE official tutorial: [Saving and loading plug-in state](https://juce.com/tutorials/tutorial_audio_processor_value_tree_state/) — copyState/replaceState pattern
- JUCE official tutorial: [Build an OpenGL application](https://juce.com/tutorials/tutorial_open_gl_application/) — shader setup, perspective matrix, Matrix3D
- Project research ARCHITECTURE.md — PositionBridge double-buffer pattern (pre-established)
- Project source files — existing APVTS structure, ParamIDs.h, ParamLayout.cpp (actual codebase)

### Secondary (MEDIUM confidence)
- JUCE forum: [OpenGLContext for Plugins — Attach to PluginEditor or Child Component?](https://forum.juce.com/t/openglcontext-for-plugins-attach-to-plugineditor-or-child-component/46795) — recommends child component attachment
- JUCE forum: [setOpaque and OpenGL JUCE 8](https://forum.juce.com/t/setopaque-and-opengl-juce-8/68235) — JUCE 8 behavior change
- JUCE forum: [Significant GUI Sluggishness JUCE 8 OpenGL D2D Conflict](https://forum.juce.com/t/significant-gui-sluggishness-daw-freezes-on-windows-nvidia-suspected-juce-8-opengl-d2d-conflict/67863) — JUCE_DIRECT2D=0 mitigation
- [LearnOpenGL Camera](https://learnopengl.com/Getting-started/Camera) — Euler angle orbit camera pattern
- [Arcball camera implementation](https://nerdhut.de/2019/12/04/arcball-camera-opengl/) — orbit camera math

### Tertiary (LOW confidence — for design reference only)
- Hermetic/alchemical manuscript aesthetics: general reference for color palette (earthy browns, ochres, aged parchment — not a technical source)

---

## Metadata

**Confidence breakdown:**
- APVTS parameter system (R param, SliderAttachment, state save): HIGH — existing codebase already follows this pattern for 40+ params
- OpenGL context lifecycle and setup: HIGH — JUCE official docs confirmed
- Camera orbit + source drag math: HIGH — standard 3D math, GLM already in project
- JUCE 8 D2D/OpenGL conflict: MEDIUM — forum-confirmed bug, mitigation verified
- LFO waveform display implementation: HIGH — standard JUCE custom component pattern
- Dev panel scrollable layout: HIGH — juce::Viewport is documented API
- Factory preset XML format: HIGH — matches existing getStateInformation/setStateInformation pattern

**Research date:** 2026-03-13
**Valid until:** 2026-06-13 (90 days — JUCE releases are quarterly; check for 8.x point releases before starting)
