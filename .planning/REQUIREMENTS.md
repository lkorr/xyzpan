# Requirements: XYZPan

**Defined:** 2026-03-12
**Core Value:** Accurate real-time binaural rendering of 3D spatial audio positioning

## v1 Requirements

### Project Setup (SETUP)

- [x] **SETUP-01**: CMake project builds JUCE plugin (VST3 + Standalone) on Windows with MSVC
- [x] **SETUP-02**: Pure C++ engine exists as separate static library with no JUCE headers
- [x] **SETUP-03**: JUCE PluginProcessor calls engine for all audio processing
- [x] **SETUP-04**: Unit test target builds and runs via CTest (Catch2)
- [x] **SETUP-05**: pluginval validation passes at strictness level 5

### Coordinate System (COORD)

- [x] **COORD-01**: Plugin accepts X, Y, Z position inputs normalized -1.0 to 1.0
- [x] **COORD-02**: XYZ converted to azimuth angle (from X and Y)
- [x] **COORD-03**: XYZ converted to elevation angle (from azimuth and Z)
- [x] **COORD-04**: Euclidean distance computed from X, Y, Z for distance processing
- [x] **COORD-05**: All coordinate conversions are sample-rate independent

### Binaural Panning (PAN)

- [x] **PAN-01**: Interaural time difference (ITD) applies up to 0.7ms delay to opposite ear based on azimuth X component
- [x] **PAN-02**: Head shadow filter applied to opposite ear based on azimuth X component
- [x] **PAN-03**: Mono input split to stereo L/R at the panning stage
- [x] **PAN-04**: Stereo input accepted and summed to mono before processing
- [x] **PAN-05**: Panning is smooth and click-free during parameter automation

### Depth Processing (DEPTH)

- [x] **DEPTH-01**: ~10 comb filters in series model front/back depth perception
- [x] **DEPTH-02**: Comb filter delays range 0ms to 1.5ms (arbitrary per filter)
- [x] **DEPTH-03**: Comb filter dry/wet scales from 0% (Y=0) to 30% max (Y=-1)
- [x] **DEPTH-04**: Comb filter feedback hard-clamped to prevent instability
- [x] **DEPTH-05**: All comb filter parameters (count, delays, wet amounts) tuneable via dev panel

### Elevation Processing (ELEV)

- [x] **ELEV-01**: Pinna notch filter: -15dB notch at 8kHz at elevation 0, smoothly to +5dB at elevation 1.0, with +3dB high shelf at 4kHz+ at elevation 1.0
- [x] **ELEV-02**: Pinna filter stays at elevation-0 values between -1 and 0; high shelf removes 3dB scaling from 0 to -1
- [x] **ELEV-03**: Chest bounce: parallel filtered delay (4x highpass at 700Hz, 1x 6dB/oct lowpass at 1kHz), delay 0ms at -1 to 2ms at 1, volume -8dB at -1 to -inf at 1
- [x] **ELEV-04**: Floor bounce: parallel delayed copy at -5dB at -1 to -inf at 1, delay 0ms at -1 to 20ms at 1
- [x] **ELEV-05**: All elevation filter parameters tuneable via dev panel

### Distance Processing (DIST)

- [x] **DIST-01**: Gain attenuation follows inverse-square law (every distance doubling removes 6dB)
- [x] **DIST-02**: Low-pass filter rolls off from 22kHz (closest) to 8kHz (furthest) at 6dB/octave
- [x] **DIST-03**: Distance delay ranges 0ms (closest) to 300ms (furthest) — not latency-compensated, applied as creative effect
- [x] **DIST-04**: Doppler shift occurs naturally from delay modulation over time when distance changes
- [x] **DIST-05**: Doppler shift is toggleable (off = no distance delay applied)
- [x] **DIST-06**: Delay line uses cubic (Hermite) interpolation to avoid artifacts during modulation
- [x] **DIST-07**: All distance parameters tuneable via dev panel

### Reverb (VERB)

- [x] **VERB-01**: Algorithmic reverb applied at end of signal chain
- [x] **VERB-02**: Pre-delay scales with distance: 0ms (closest) to 50ms (furthest)
- [ ] **VERB-03**: Reverb parameters (size, decay, damping, wet/dry) exposed as plugin parameters
- [x] **VERB-04**: Reverb is sparse and mix-friendly (not convolution)

### LFO Modulation (LFO)

- [ ] **LFO-01**: One LFO per axis (X, Y, Z) — 3 total
- [ ] **LFO-02**: Each LFO has selectable waveform: sine, triangle, saw, square
- [ ] **LFO-03**: Each LFO has rate, depth, and phase controls
- [ ] **LFO-04**: LFOs modulate position offset (add/subtract around fixed position)
- [ ] **LFO-05**: LFO rate syncs to host tempo (optional) or free-running Hz

### Parameter System (PARAM)

- [ ] **PARAM-01**: All DSP parameters exposed as VST automation parameters
- [ ] **PARAM-02**: Dev panel exposes all internal constants (filter frequencies, delay ranges, dB values, comb tunings)
- [ ] **PARAM-03**: Parameter changes are smoothed to prevent zipper noise (no clicks on automation)
- [ ] **PARAM-04**: Plugin state saves and restores correctly across DAW sessions
- [ ] **PARAM-05**: Factory presets demonstrating spatial positions and LFO patterns

### User Interface (UI)

- [ ] **UI-01**: Custom OpenGL renderer showing 2D projection of 3D space
- [ ] **UI-02**: Listener node centered in the view
- [ ] **UI-03**: Object node draggable in X/Y, with Z shown as size change (depth perspective)
- [ ] **UI-04**: LFO controls visible per axis (waveform, rate, depth, phase)
- [ ] **UI-05**: Dev panel toggleable showing all tuneable DSP constants
- [ ] **UI-06**: All parameter controls also accessible as standard UI elements (not just the 3D view)
- [ ] **UI-07**: UI updates at display rate, not audio rate (double-buffer pattern for audio→GL)

### Plugin Infrastructure (INFRA)

- [ ] **INFRA-01**: Builds as VST3 on Windows
- [ ] **INFRA-02**: Builds as Standalone on Windows
- [ ] **INFRA-03**: Adapts to DAW session sample rate (recalculates all coefficients on prepareToPlay)
- [ ] **INFRA-04**: Zero memory allocations on audio thread (all pre-allocated in prepareToPlay)
- [ ] **INFRA-05**: Passes pluginval at strictness level 5
- [ ] **INFRA-06**: Works correctly in Ableton, FL Studio, and Reaper
- [ ] **INFRA-07**: Multiple instances can run simultaneously without interference

## v2 Requirements

### Cross-Platform (XPLAT)

- **XPLAT-01**: Builds on macOS (AU + VST3)
- **XPLAT-02**: Metal rendering fallback for macOS (OpenGL deprecated)

### Extended Features (EXT)

- **EXT-01**: AAX format support (Pro Tools)
- **EXT-02**: CLAP format support (when JUCE 9 ships)
- **EXT-03**: Head tracking via OSC input
- **EXT-04**: Preset browser with categories
- **EXT-05**: Undo/redo for parameter changes

## Out of Scope

| Feature | Reason |
|---------|--------|
| HRTF database / SOFA file loading | Undermines parametric approach; adds CPU cost and coloration |
| Multi-format output (5.1, 7.1, Ambisonics) | Binaural stereo only — do one thing well |
| Multiple sound objects per instance | Use multiple instances; keeps DSP pipeline simple |
| Convolution reverb | CPU-heavy, latency-inducing; algorithmic is sufficient |
| Complex 3D room visualization | Scope creep; simple 2D projection with depth indicator suffices |
| Stereo width processing | Different product category; user can apply their own tools |
| Mobile/embedded targets | Desktop DAW plugin only |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| SETUP-01 | Phase 1: Project Scaffolding | Complete |
| SETUP-02 | Phase 1: Project Scaffolding | Complete |
| SETUP-03 | Phase 1: Project Scaffolding | Complete |
| SETUP-04 | Phase 1: Project Scaffolding | Complete |
| SETUP-05 | Phase 1: Project Scaffolding | Complete |
| COORD-01 | Phase 1: Project Scaffolding | Complete |
| COORD-02 | Phase 1: Project Scaffolding | Complete |
| COORD-03 | Phase 1: Project Scaffolding | Complete |
| COORD-04 | Phase 1: Project Scaffolding | Complete |
| COORD-05 | Phase 1: Project Scaffolding | Complete |
| PAN-01 | Phase 2: Binaural Panning Core | Complete |
| PAN-02 | Phase 2: Binaural Panning Core | Complete |
| PAN-03 | Phase 2: Binaural Panning Core | Complete |
| PAN-04 | Phase 2: Binaural Panning Core | Complete |
| PAN-05 | Phase 2: Binaural Panning Core | Complete |
| DEPTH-01 | Phase 3: Depth and Elevation | Complete |
| DEPTH-02 | Phase 3: Depth and Elevation | Complete |
| DEPTH-03 | Phase 3: Depth and Elevation | Complete |
| DEPTH-04 | Phase 3: Depth and Elevation | Complete |
| DEPTH-05 | Phase 3: Depth and Elevation | Complete |
| ELEV-01 | Phase 3: Depth and Elevation | Complete |
| ELEV-02 | Phase 3: Depth and Elevation | Complete |
| ELEV-03 | Phase 3: Depth and Elevation | Complete |
| ELEV-04 | Phase 3: Depth and Elevation | Complete |
| ELEV-05 | Phase 3: Depth and Elevation | Complete |
| DIST-01 | Phase 4: Distance Processing | Complete |
| DIST-02 | Phase 4: Distance Processing | Complete |
| DIST-03 | Phase 4: Distance Processing | Complete |
| DIST-04 | Phase 4: Distance Processing | Complete |
| DIST-05 | Phase 4: Distance Processing | Complete |
| DIST-06 | Phase 4: Distance Processing | Complete |
| DIST-07 | Phase 4: Distance Processing | Complete |
| VERB-01 | Phase 5: Creative Tools | Complete |
| VERB-02 | Phase 5: Creative Tools | Complete |
| VERB-03 | Phase 5: Creative Tools | Pending |
| VERB-04 | Phase 5: Creative Tools | Complete |
| LFO-01 | Phase 5: Creative Tools | Pending |
| LFO-02 | Phase 5: Creative Tools | Pending |
| LFO-03 | Phase 5: Creative Tools | Pending |
| LFO-04 | Phase 5: Creative Tools | Pending |
| LFO-05 | Phase 5: Creative Tools | Pending |
| PARAM-01 | Phase 6: UI and Parameter System | Pending |
| PARAM-02 | Phase 6: UI and Parameter System | Pending |
| PARAM-03 | Phase 6: UI and Parameter System | Pending |
| PARAM-04 | Phase 6: UI and Parameter System | Pending |
| PARAM-05 | Phase 6: UI and Parameter System | Pending |
| UI-01 | Phase 6: UI and Parameter System | Pending |
| UI-02 | Phase 6: UI and Parameter System | Pending |
| UI-03 | Phase 6: UI and Parameter System | Pending |
| UI-04 | Phase 6: UI and Parameter System | Pending |
| UI-05 | Phase 6: UI and Parameter System | Pending |
| UI-06 | Phase 6: UI and Parameter System | Pending |
| UI-07 | Phase 6: UI and Parameter System | Pending |
| INFRA-01 | Phase 7: Integration and Validation | Pending |
| INFRA-02 | Phase 7: Integration and Validation | Pending |
| INFRA-03 | Phase 7: Integration and Validation | Pending |
| INFRA-04 | Phase 7: Integration and Validation | Pending |
| INFRA-05 | Phase 7: Integration and Validation | Pending |
| INFRA-06 | Phase 7: Integration and Validation | Pending |
| INFRA-07 | Phase 7: Integration and Validation | Pending |

**Coverage:**
- v1 requirements: 60 total
- Mapped to phases: 60
- Unmapped: 0

---
*Requirements defined: 2026-03-12*
*Last updated: 2026-03-13 after plan 03-02 completion*
