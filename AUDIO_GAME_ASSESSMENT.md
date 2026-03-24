# Audio-First Walking Simulator — Feasibility Assessment

> XYZPan spatial audio engine → all-audio game where the player explores acoustic environments through sound alone.
> Written 2026-03-22 for future reference when actionable planning begins.

---

## 1. Concept

A walking simulator with no (or minimal) visuals. The player navigates architectural spaces — rooms, corridors, outdoor areas — populated with spatialized sound objects. The entire experience is conveyed through binaural audio over headphones. Acoustic realism is the core mechanic: reverb tells you you're in a cathedral, occlusion tells you there's a wall between you and the sound, doppler tells you something is moving past you.

---

## 2. Engine Portability

The XYZPan DSP engine (`xyzpan_engine`) is a **standalone pure C++20 static library** with zero JUCE dependency. Extraction is trivial.

**Interface:** `prepare(sr, blockSize)` → `setParams(EngineParams)` → `process(in, out, numSamples)` → `reset()`

**`EngineParams`** is a plain struct (~80 floats/bools). Raw float buffer I/O. No framework types cross the boundary.

### What transfers directly
- ITD/ILD binaural panning (azimuth-based delay + gain + head shadow)
- Pinna EQ chain (elevation-mapped notch sweep, 8-10 bands)
- Chest bounce + floor bounce (vertical localization cues)
- Distance attenuation (inverse-square) + air absorption (cascaded one-pole LP)
- Doppler delay (distance delay with Hermite/Sinc interpolation)
- Comb filter bank (front/back depth perception, 10 filters)
- Early reflections (image source method, 6 walls)
- FDN reverb (4-delay Householder matrix)
- All DSP primitives: FractionalDelayLine, SVF, Biquad, FeedbackCombFilter, OnePoleSmooth, LFO

### What gets stripped
- LFO tempo sync (no DAW host)
- Test tone oscillator
- Aux bus routing
- JUCE parameter bridges (replaced by game engine bindings)

---

## 3. Game Engine Choice

### Recommended: Godot 4.x with GDExtension

- **GDExtension C++ API** — compile `xyzpan_engine` as a native extension, bypass Godot's built-in spatial audio
- MIT licensed, no royalties
- Scene tree provides world graph: Area3D for acoustic zones, CharacterBody3D for player, colliders for walls
- GDScript for game logic, triggers, narrative
- Minimal renderer is fine — black screen + optional debug wireframe
- Cross-platform (Windows, Linux, macOS)
- Physics raycasting built in (needed for occlusion)

### Alternatives considered
- **Custom SDL2/SDL3 runtime** — maximum control but requires building scene graph, collision, level loading, serialization from scratch
- **Unreal/Unity** — overkill renderer overhead for zero visual output, licensing complexity

---

## 4. Acoustic Simulation: Steam Audio

**OpenAL is not the answer.** It has zero built-in ray tracing or acoustic propagation — it's purely an audio rendering API. Your engine already exceeds OpenAL's spatialization capabilities.

### Steam Audio (Valve, open source, C API)
The right tool for geometry-aware acoustic simulation:

- **Ray-traced propagation** — traces up to 4096+ rays against actual level geometry
- **Occlusion** — partial occlusion as a 0-1 fraction for non-point sources
- **Transmission** — 3-band frequency-dependent material EQ (wood vs concrete vs glass)
- **Reflection simulation** — produces Ambisonic impulse responses from actual room geometry
- **Pathing** — baked probe-based sound propagation around corners, through hallways, multiple openings
- **GPU acceleration** — Intel Embree (~20x CPU), AMD Radeon Rays (50-150x, GPU)
- **C API** — works with any engine, no middleware lock-in
- **Dynamic geometry support** (Embree backend)

### Recommended architecture: Steam Audio + XYZPan

Steam Audio computes the physics (ray hits, materials, propagation paths). It outputs raw simulation data: occlusion fractions, transmission EQ, reflection IRs, propagation directions.

XYZPan takes those results and applies the final binaural rendering with its parametric pipeline. Steam Audio says "60% occluded by concrete" — XYZPan applies appropriate filtering while keeping ITD/ILD/elevation/distance processing.

**Why not replace XYZPan with Steam Audio's renderer?** Steam Audio's HRTF is a black-box measured filter. XYZPan's parametric approach (explicit ITD/ILD/head shadow/pinna/chest/floor) gives finer control, is more CPU-efficient to instance, and allows creative manipulation of spatial perception. Best of both worlds: Steam Audio for simulation, XYZPan for rendering.

### Resonance Audio (Google)
Not recommended. Ambisonics-first, efficient for mobile/VR but uses a shoebox reverb model — not sophisticated enough for complex architectural acoustics. Google has stepped back from active development.

---

## 5. What the Engine Currently Lacks

### 5.1 Multi-Source Processing — Critical, Large Scope

**Current:** Processes exactly one source. One `EngineParams`, one position, one output.

**Needed:** Dozens to hundreds of simultaneous sound objects.

- Refactor to manage N source instances with independent DSP state
- Voice management: priority system, distance-based culling, max polyphony
- Sum all binaural outputs to single stereo master
- Memory: ~50KB per source instance → 100 sources ≈ 5MB (manageable)
- CPU: 100 sources at 48kHz = 4.8M samples/sec through full chain → needs LOD
  - Full chain for near/important sources
  - Lite path for distant sources (skip combs, skip ER, simplified reverb)

### 5.2 Acoustic Propagation — Critical, Very Large Scope

**Current:** Fixed 6-wall cube image source + algorithmic FDN reverb.

**Needed (progressive complexity):**

| Stage | What | Complexity |
|-------|------|-----------|
| 1 | Zone-based reverb presets with crossfading at boundaries | Low-Medium |
| 2 | Direct-path occlusion via physics raycasting | Medium |
| 3 | Steam Audio integration for geometry-aware reflections + transmission | Large |
| 4 | Baked pathing for multi-room propagation (sound around corners) | Large |
| 5 | Full stochastic ray tracing (optional, may be covered by Steam Audio) | Very Large |

Key sub-problems:
- **Occlusion:** Wall between source and listener → attenuate direct path
- **Diffraction:** Sound bending around corners/doorways (hardest problem)
- **Material absorption:** Frequency-dependent per surface type
- **Dynamic ER:** Replace fixed cube with geometry-derived reflections
- **Late reverb:** Derive FDN params from room volume/surface area (Sabine/Eyring)
- **Portaling:** Sound filtered by doorway geometry when crossing rooms

### 5.3 Reverb Expansion — Medium Scope

- Per-room reverb parameters (geometry-derived or hand-authored)
- Reverb crossfading between zones during movement
- Possibly larger FDN (8-16 delays) for richer late reverb
- Convolution reverb option for pre-captured spaces (IR loading)
- Separate ER engine (geometry-derived) feeding late reverb (FDN)

### 5.4 Listener Movement & Orientation — Medium Scope

**Current:** Listener fixed at origin facing +Y.

- Player has world-space position and orientation
- Transform all source positions to listener-relative coordinates each frame
- Head rotation (yaw + pitch) changes azimuth/elevation of every source
- Optional: head tracking hardware (headphone IMUs) to decouple head from body rotation
- Already have `Coordinates.h` for spherical ↔ Cartesian transforms

### 5.5 Audio Asset Pipeline — Medium Scope

- Loading and streaming audio files (WAV, OGG, FLAC) — or leverage Godot's AudioStream
- Looping, one-shot, triggered playback modes
- Crossfading and transition logic for ambient layers
- Procedural audio for environmental sounds (wind, rain, footsteps per surface)

### 5.6 Game Systems — Large Scope (Not DSP)

- World representation (rooms, corridors, outdoor spaces)
- Collision for player movement
- Trigger zones for narrative events
- Audio object placement and behavior scripting
- Save/load
- Footstep system with surface detection

---

## 6. Scope Summary

| Component | Effort | Notes |
|-----------|--------|-------|
| Engine extraction + Godot GDExtension binding | Small | Architecture already supports this |
| Multi-source voice management + LOD | Medium-Large | Core refactor, CPU budgeting |
| Listener-relative coordinate transform | Small | Matrix math, Coordinates.h exists |
| Zone-based room acoustics | Medium | Presets, crossfading, portal filtering |
| Direct-path occlusion (raycast) | Medium | Godot physics raycasting |
| Steam Audio integration | Large | C API binding, geometry export, parameter mapping |
| Reverb expansion (per-room, crossfade) | Medium | FDN param derivation, smooth transitions |
| Audio asset loading/streaming | Medium | Or leverage Godot's system |
| Level design tools & content creation | Very Large | The game itself |
| Head tracking | Small-Medium | Optional, headphone IMU input |
| Procedural audio (wind, rain, footsteps) | Medium-Large | Major immersion factor |

---

## 7. Strengths & Risks

### What makes this feasible
1. **Hardest DSP is done** — binaural, elevation, distance, reverb. Months of tuning, working now.
2. **Engine is already JUCE-free** — clean static lib, no decontamination.
3. **Parametric HRTF scales** — lightweight enough to instance N times (unlike convolution HRTF).
4. **Godot provides non-audio infrastructure** — collision, scene graph, editor, cross-platform.
5. **Steam Audio solves the propagation problem** — battle-tested, GPU-accelerable, C API.

### What makes this hard
1. **Multi-source CPU budget** — 100+ sources through full chain at 48kHz is demanding.
2. **Acoustic propagation is an open research problem** — even AAA simplifies heavily.
3. **Content creation** — every sound must be spatially precise and narratively meaningful. Sound design replaces the art pipeline.
4. **Playtesting** — can't screenshot an audio bug. Every test requires listening.
5. **Steam Audio integration complexity** — geometry export, baking pipeline, parameter mapping.

---

## 8. Proof of Concept — First Step

Before committing to full scope, build a minimal POC:

1. Extract `xyzpan_engine` into Godot via GDExtension
2. Place 5-10 looping sound sources in a simple room (Godot scene)
3. Walk around with WASD + mouse look
4. Validate binaural spatialization works in real-time game context
5. Add basic occlusion (single raycast per source)
6. Measure CPU with 10/25/50 simultaneous sources

This proves the core loop before investing in Steam Audio integration or content creation.

---

## 9. Reference Links

- [Steam Audio C API](https://valvesoftware.github.io/steam-audio/doc/capi/guide.html)
- [Steam Audio GitHub](https://github.com/ValveSoftware/steam-audio)
- [Godot GDExtension Docs](https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/)
- [OpenAL Soft](https://openal-soft.org/) — not recommended but useful reference for EFX reverb presets
- [Resonance Audio](https://resonance-audio.github.io/resonance-audio/) — not recommended for this use case
