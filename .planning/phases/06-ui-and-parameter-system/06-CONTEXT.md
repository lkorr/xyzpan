# Phase 6: UI and Parameter System - Context

**Gathered:** 2026-03-13
**Status:** Ready for planning

<domain>
## Phase Boundary

Custom OpenGL spatial visualization with perspective camera, full parameter control surface, scrollable dev panel for DSP tuning, R (radius scale) parameter, factory presets, and state persistence. Replaces GenericAudioProcessorEditor with a production custom editor.

</domain>

<decisions>
## Implementation Decisions

### OpenGL View — Visual Design
- Hermetic alchemy–inspired color scheme: earthy browns, ochres, aged parchment tones — warm and dark
- Claude has full creative latitude to make the design striking; "wow factor" is the explicit goal
- Perspective 3D view with a box/room wireframe and floor grid for spatial orientation
- Click-drag on empty view area orbits the camera (rotates perspective)
- Click-drag on the source node moves the source on the current view plane (projected onto whichever two axes face the camera)
- Three preset view buttons for orthographic snaps: XY (top-down), XZ (side), YZ (front)
- Camera rotation shows strong perspective cues (vanishing point lines, wireframe foreshortening)

### Source Node Appearance
- Node size = perspective distance from the camera (not just Z) — shrinks as source recedes in 3D
- Node opacity maps to acoustic distance gain: at minimum distance gain (furthest), opacity = ~10%; at closest, fully opaque
- Node brightens + cursor changes to grab icon on hover/drag
- Listener node centered, non-moveable

### R (Radius/Scale) Parameter
- New APVTS parameter: R multiplies XYZ before engine: effectiveX = X*R, effectiveY = Y*R, effectiveZ = Z*R
- R is DAW-automatable (PARAM-01 compliant)
- Visually: grid/room wireframe scales with R — the space appears bigger/smaller, source stays proportionally positioned
- R range: 0.0–2.0 (or similar), default 1.0

### Controls Layout
- Plugin window is resizable; OpenGL view fills the majority of the window
- Bottom strip below the OpenGL view: four knobs in a row — X, Y, Z, R — spaced apart
- LFO controls sit below each respective knob (X LFO under X knob, Y LFO under Y knob, Z LFO under Z knob; R has no LFO)
- LFO per axis: waveform display (clickable — cycles sine → triangle → saw → square), frequency knob, depth knob, phase knob
- Small SYNC button next to each LFO frequency knob; when active, knob switches to beat-division display (e.g., "1/4")
- Reverb controls (size, decay, damping, wet) accessible in the main UI — placement Claude's discretion

### Dev Panel
- Toggled open/closed from a button (side slide-out or overlay panel — Claude's discretion)
- Shows only DSP tuning constants — NOT X/Y/Z/R/LFO/reverb (those are in main UI)
- Grouped by DSP section: Binaural (head shadow, ILD, ITD, smoothing), Comb Filters (10x delay+FB + wet max), Elevation (pinna notch/Q/shelf, chest, floor), Distance (delay max, smooth, air abs), and any future sections
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

</decisions>

<specifics>
## Specific Ideas

- "I want you to wow me in the design department" — strong aesthetic is a stated goal, not a nice-to-have
- Alchemy/hermetic aesthetic: think aged manuscript paper, dark iron, gold leaf, alchemical symbols — earthy, warm, not clinical
- When rotating perspective, the room wireframe should visually communicate the twist clearly (foreshortening of walls is a primary cue)
- At default top-down view (XY plane), dragging source only changes X and Y; tilting the view adds Z influence — this is intentional and should feel natural
- LFO display shows the actual waveform shape as a small graphic (not just a label)

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `PluginEditor.h` — currently a stub placeholder, Phase 6 writes the full custom editor
- `PluginEditor.cpp` — currently just returns `GenericAudioProcessorEditor`; Phase 6 replaces entirely
- `XYZPanProcessor::apvts` — public APVTS; editor accesses it directly for SliderAttachments and parameter callbacks
- `getStateInformation`/`setStateInformation` — already wired via `apvts.copyState()`/`apvts.replaceState()`; PARAM-04 may just need testing
- `ParamIDs.h` — Phase 6 adds `R` parameter ID here
- `ParamLayout.cpp` — Phase 6 adds `R` parameter registration

### Established Patterns
- APVTS with `getRawParameterValue()` for audio thread; `SliderAttachment`/`ButtonAttachment` for UI thread
- Dev panel params have always been separate from main position params conceptually (established in Phase 1 context)
- All Hz parameters use NormalisableRange skew 0.3 (convention from Phases 2–4)
- Engine receives `EngineParams` struct per block — Phase 6 adds `r` field, processor applies X*R, Y*R, Z*R before setParams

### Integration Points
- `xyzpan_ui` CMake target planned in Phase 1 context but never created — Phase 6 creates it (OpenGL rendering, JUCE-dependent)
- `PluginProcessor::createEditor()` currently returns `GenericAudioProcessorEditor` — Phase 6 returns custom editor class
- `EngineParams` struct needs no new fields for R (processor multiplies before passing X/Y/Z); R is pure APVTS + UI concern
- OpenGL view needs a double-buffer mechanism for audio→GL data (UI-07): current position read by timer/repaint from APVTS atomics; no direct audio thread → GL writes

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 06-ui-and-parameter-system*
*Context gathered: 2026-03-13*
