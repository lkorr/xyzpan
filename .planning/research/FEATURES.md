# Feature Landscape

**Domain:** 3D Spatial Audio Panner VST Plugin (Binaural)
**Researched:** 2026-03-12

## Table Stakes

Features users expect from any spatial audio panner plugin. Missing any of these and the product feels broken or amateurish.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| XYZ position control with visual feedback | Every competitor (DearVR, Spat, Panagement) has a graphical panning interface. Users expect to see where the source is. | Medium | 2D pad with depth axis is the standard pattern. DearVR uses a sphere display; Panagement uses a top-down circle. |
| Smooth parameter automation | DAW automation of X/Y/Z/distance is the primary way users create spatial movement. Zipper noise or stepping artifacts kill the product. | High | Must use parameter smoothing (exponential ramps, not raw value jumps). Steam Audio documents zipper artifacts from rapid HRTF switching as a known problem. |
| Full DAW preset system | Users expect to save/load spatial positions and full plugin state. DAW session recall must work flawlessly. | Low | VST3/JUCE handle most of this via `getStateInformation`/`setStateInformation`. Ship 10-20 factory presets showing off capabilities. |
| Bypass with artifact-free switching | VST3 spec requires the plugin to handle bypass itself, including latency-compensated pass-through. Users toggle bypass constantly during mixing. | Medium | Must ramp gain to avoid clicks. If plugin has internal latency, bypass must delay-compensate the dry signal. |
| Latency reporting | DAWs rely on `getLatencySamples()` for plugin delay compensation. Wrong values break timing across the session. | Low | Report the maximum fixed latency from delay lines. Variable latency (doppler) complicates this -- report worst-case. |
| Distance attenuation (inverse-square + LPF) | Every spatial plugin models distance. Panagement 2, DearVR, Spat Revolution all couple volume rolloff with HF attenuation. Users expect "drag back = quieter + duller." | Medium | Panagement consolidates volume, LPF, and reverb wet/dry into a single distance gesture. This is the UX gold standard. |
| Azimuth panning (left/right) | The most basic spatial operation. Must be perceptually smooth and convincing on headphones. | Medium | ITD + ILD (interaural level difference) are the minimum. ITD alone sounds unconvincing. |
| Mono and stereo input support | Users will insert this on mono tracks (vocals, instruments) and stereo tracks (synths, buses). Must handle both. | Low | Stereo input needs width control to collapse to mono before spatializing, or spatialized as a pair with adjustable spread. |
| CPU efficiency | Spatial plugins go on every track in a session. DearVR is noted as "lighter on CPU" vs Spat Revolution. Users expect <5% single-core per instance. | High | Avoid per-sample convolution with large HRTF kernels. XYZPan's parametric approach (IIR filters, delay lines) is inherently CPU-friendly -- this is an advantage. |
| Headphone output mode | Binaural plugins are used on headphones. This is the primary output. Must sound convincing without crossfeed or speaker compensation. | Low | This is the default and only output mode for XYZPan. Not a multi-format tool. |

## Differentiators

Features that set XYZPan apart from the competition. Not expected, but create real value.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Dev-tuneable DSP constants at runtime | No competitor exposes their internal DSP constants. DearVR/Spat are black boxes. XYZPan lets the user (in dev mode) tweak comb filter tunings, ITD curves, shadow filter slopes -- turning it into a spatial audio research tool and sound design instrument. | Medium | Runtime-editable constants with immediate audible feedback. This is genuinely novel. Academic researchers and sound designers will love this. |
| Simplified parametric HRTF (no database) | Competitors either use HRTF convolution (CPU-heavy, coloration complaints) or simplified ITD-only processing. XYZPan models HRTF perceptual effects through parametric DSP: ITD, head shadow filter, pinna notch, chest bounce, floor bounce. This avoids HRTF coloration artifacts that plague DearVR and Steam Audio. | High | The parametric approach means no "one size fits none" HRTF problem. Users who dislike DearVR's tonal coloring will find this appealing. Research confirms pinna notch filters are the primary elevation cue. |
| Comb filter array for front/back perception | ~10 tuneable comb filters modeling the acoustic effects that create front/back disambiguation. This is a creative and novel approach -- competitors rely on HRTF databases for this, which are generic and often fail for individual users. | High | Front/back confusion is the #1 complaint about binaural audio. A tuneable approach lets users dial in what works for their ears. |
| Per-axis LFO modulation | One LFO per axis (X, Y, Z) with sine/tri/saw/square waveforms. Most spatial plugins either have no built-in modulation (DearVR, Spat) or only basic auto-pan (Panagement). Per-axis LFOs enable orbital paths, figure-8s, and complex 3D trajectories from simple waveform combinations. | Medium | Pancake 2 popularized custom LFO curves for panning. XYZPan's per-axis approach is more systematic. Sound Particles Energy Panner uses audio-driven movement instead. Different creative tool. |
| Doppler shift | Physical doppler from distance changes. Only Waves Doppler and Lese Transfer model this in the plugin market. Most spatial panners ignore it entirely. Adds realism for fly-by effects and moving sources. | Medium | Doppler is a natural side-effect of interpolated delay lines when distance changes. Transpanner Pro also models this -- but it is rare. |
| Distance-scaled reverb pre-delay | Reverb pre-delay increases with distance, modeling the time gap between direct sound and first reflections. Most reverb plugins have static pre-delay. Coupling it to distance creates automatic depth staging. | Low | Simple to implement: pre-delay = f(distance). High perceptual impact for the complexity cost. |
| Elevation via pinna notch + chest/floor bounce | Multi-cue elevation modeling (pinna notch filter, chest bounce delay, floor bounce delay) rather than HRTF-only elevation. Research shows pinna notch frequency is the primary elevation cue, and chest/floor reflections add below-horizon cues that HRTF databases often miss. | High | Most plugins struggle with elevation. DearVR Pro gets mixed reviews on up/down perception. This multi-cue approach could be genuinely better for elevation. |

## Anti-Features

Features that seem good but should be explicitly avoided.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| Full HRTF database loading / SOFA file support | HRTF convolution is CPU-expensive (per-sample FFT), introduces coloration artifacts, and the "generic HRTF" problem means it sounds wrong for most listeners. APL Virtuoso and Spat Revolution handle this well because it is their entire focus. XYZPan's value proposition is the parametric approach. Adding HRTF support undermines the core differentiator and adds massive complexity. | Stick with parametric modeling. The tuneable constants in dev mode serve the same individualization purpose without the complexity. |
| Multi-format output (5.1, 7.1, Atmos, Ambisonics) | DearVR Pro 2 supports 35+ speaker formats. Spat Revolution supports everything. Competing on format breadth is a losing game for a new plugin. Multi-format output requires entirely different rendering pipelines and massively increases testing surface. | Binaural stereo output only. Do one thing well. If speaker output is ever needed, it is a separate product. |
| Complex visual 3D room with reflections | Spat Revolution has a full 3D room visualization. Building a real-time 3D renderer is a massive UI engineering effort that distracts from the DSP work. Users care about sound quality, not visual fidelity of the room model. | Simple 2D XY pad with elevation indicator and distance ring. Panagement's minimal UI proves this works. |
| Head tracking / OSC input | DearVR Pro 2 and Spat Revolution support head trackers. This requires OSC networking, calibration UI, and testing with specific hardware. Niche use case for a music production tool. | Omit entirely for V1. If needed later, it is an additive feature that does not affect core architecture. |
| Per-sample convolution reverb | Convolution reverb with room IRs sounds great but is CPU-heavy and latency-inducing. XYZPan already has an algorithmic reverb with distance-scaled pre-delay. Convolution would conflict with the "lightweight" positioning. | Algorithmic reverb only. Keep it sparse and mix-friendly like Panagement's approach -- "rather tame and sparse, in order to easily fit in the mix." |
| Stereo width processing beyond basic input control | Voxengo Spatifier, Panagement, and others offer elaborate stereo widening. This is a different product category (stereo imaging) bolted onto a panner. Scope creep. | Basic stereo-to-mono collapse knob for stereo inputs. Width processing is the user's job with their own tools. |
| Ambisonics encode/decode | Waves B360 exists for this. Ambisonics is a format conversion tool, not a creative spatial panner. Different audience, different architecture. | Not applicable to XYZPan's binaural-only mission. |

## Feature Dependencies

```
Core Parameter System
  |-> X/Y/Z position parameters
  |     |-> Azimuth calculation (derived from X/Y)
  |     |-> Elevation calculation (derived from Y/Z or explicit)
  |     |-> Distance calculation (derived from X/Y/Z magnitude)
  |     |-> LFO modulation (requires base X/Y/Z params to modulate)
  |
  |-> Binaural Core
  |     |-> ITD (interaural time delay) -- requires azimuth
  |     |-> Head shadow filter (ILD) -- requires azimuth
  |     |-> Pinna notch filter -- requires elevation
  |     |-> Chest bounce delay -- requires elevation
  |     |-> Floor bounce delay -- requires elevation
  |
  |-> Distance Processing
  |     |-> Inverse-square attenuation -- requires distance
  |     |-> LPF rolloff (air absorption) -- requires distance
  |     |-> Distance delay (0-300ms) -- requires distance
  |     |     |-> Doppler shift (side-effect of changing delay)
  |     |-> Reverb pre-delay scaling -- requires distance
  |
  |-> Depth Processing
  |     |-> Comb filter array (~10 filters) -- requires azimuth (front/back)
  |
  |-> Reverb
  |     |-> Algorithmic reverb engine (independent)
  |     |-> Pre-delay scaling -- requires distance processing
  |
  |-> LFO System
  |     |-> LFO engine (sine/tri/saw/square) -- independent
  |     |-> LFO-to-parameter routing -- requires core parameter system
  |     |-> Rate/depth/waveform controls per axis
  |
  |-> Dev Mode
        |-> Runtime constant editor -- requires all DSP modules to expose constants
        |-> All filter coefficients, delay times, gain curves editable
```

**Critical path:** Core Parameter System -> Binaural Core (ITD + head shadow) -> Distance Processing -> Comb Filters -> LFOs -> Reverb -> Dev Mode

**Parallelizable:** Reverb engine can be built independently. LFO engine can be built independently. UI can be built in parallel with DSP.

## MVP Recommendation

The minimum viable product to validate XYZPan's spatial audio concept:

### Must Have (MVP)

1. **XYZ position control** -- The fundamental interaction. A 2D pad (X/Z top-down) with a vertical slider (Y elevation). All parameters automatable.
2. **ITD + head shadow filtering** -- The binaural core. Without both, azimuth panning sounds flat and unconvincing. ITD alone is not enough (confirmed by research).
3. **Distance attenuation + LPF rolloff** -- Inverse-square gain + low-pass filter coupled to distance. This is table stakes for any spatial plugin.
4. **Pinna notch filter for elevation** -- Research confirms this is the primary elevation cue. A parametric notch filter whose frequency shifts with elevation angle. Without this, the "3D" claim is hollow.
5. **Comb filter array for front/back** -- This is XYZPan's core differentiator. Must be in MVP to validate the concept. Start with 4-6 filters, expand to 10.
6. **Basic preset system** -- Save/load via DAW state. No factory presets needed for MVP, but state recall must work.
7. **Bypass** -- Non-negotiable for any VST plugin.

### Should Have (Post-MVP, Pre-Release)

8. **Distance delay (0-300ms)** -- Adds depth realism. Required before doppler can work.
9. **Doppler shift** -- Falls out naturally from the distance delay implementation.
10. **LFO modulation (1 per axis)** -- Creative movement tool. Important for differentiation but not needed to validate spatial quality.
11. **Reverb with distance-scaled pre-delay** -- Adds space and depth. Can use a simple Schroeder reverb initially.
12. **Chest bounce + floor bounce delays** -- Completes the elevation model. Additive improvement over pinna notch alone.
13. **Dev mode** -- Runtime constant editing. Important differentiator but can be added once DSP is stable.

### Defer

- **Factory presets** -- Create after the DSP is finalized so presets do not need constant updating.
- **Polished UI** -- Functional controls first, visual polish later. A working XY pad with sliders is sufficient for validation.
- **Latency compensation for distance delay** -- Complex edge case. Report fixed latency initially, handle variable latency later.

### MVP Validation Criteria

The MVP succeeds if:
- A sound source can be convincingly placed at arbitrary positions in 3D space on headphones
- Front/back disambiguation works better than generic HRTF (test against DearVR MICRO, which is free)
- Elevation perception is noticeable (up vs down vs level)
- Distance creates natural depth staging (near = loud/bright, far = quiet/dull)
- CPU usage stays under 5% single core per instance at 44.1kHz

## Competitor Analysis

### DearVR Pro 2 (Dear Reality / Sennheiser) -- NOW FREE (discontinued)

**Price:** Free (as of March 2025; no longer developed or maintained)
**Strengths:**
- 35+ output formats (binaural, Ambisonics up to 3rd order, multi-channel up to 9.1.6)
- 46 virtual acoustic room presets with early reflections
- Stereo input with width control
- Strong DAW automation support
- Lightweight CPU usage relative to Spat Revolution
- HP/LP filters on reverb sections

**Weaknesses:**
- HRTF coloration complaints (sounds "phasey" on some material)
- Discontinued -- no future updates, no bug fixes
- Automation has noted limits; complex to move listening position
- Elevation perception gets mixed reviews
- No built-in modulation/LFO system

**XYZPan advantage:** Parametric HRTF avoids coloration. Tuneable comb filters for front/back. LFO system for creative movement. Actively developed.

**XYZPan risk:** DearVR Pro 2 is now free. Hard to compete on price with "free and established." Must compete on sound quality and creative features.

---

### IRCAM Spat Revolution (FLUX:: Immersive) -- $1,790

**Price:** $1,790 (Ultimate)
**Strengths:**
- Most comprehensive spatial audio engine available
- Multiple spatialization algorithms (VBAP, DBAP, HOA up to 7th order, binaural)
- Unlimited source objects
- Convolution and algorithmic reverb
- Modular architecture with rooms, sources, masters
- OSC control, head tracking, DAW integration via send/return plugins
- Wave Field Synthesis option

**Weaknesses:**
- Extremely expensive ($1,790)
- Complex setup (standalone app + DAW plugins, not a simple insert)
- Heavy CPU usage
- Steep learning curve
- Overkill for "I just want to place a sound in 3D space"

**XYZPan advantage:** Simple insert plugin (no standalone app). Radically cheaper. Faster workflow for single-source positioning. Dev mode for customization that Spat does not offer.

**XYZPan risk:** Spat is the gold standard for professionals. XYZPan should not try to match its feature breadth.

---

### Waves B360 Ambisonics Encoder

**Price:** ~$29-149 (Waves pricing varies with sales)
**Strengths:**
- Up to 7 independent image sources
- Width, rotation, elevation controls
- Sound sphere visualization
- Integration with Waves Nx head tracking
- Simple workflow for 360/VR content

**Weaknesses:**
- Ambisonics-only output (B-format)
- Requires Ambisonics decode chain for monitoring
- Limited to 1st-order Ambisonics (4 channels)
- No distance modeling
- No reverb/room simulation

**XYZPan advantage:** Direct binaural output (no decode chain needed). Full distance model with doppler. Reverb built in. Not an Ambisonics tool at all -- different product category.

**XYZPan risk:** Minimal. These serve different audiences.

---

### Sennheiser AMBEO Orbit / DearVR MICRO (Free)

**Price:** Free
**Strengths:**
- Dead simple: azimuth, elevation, distance, width
- Patented "clarity" / "focus" control (blend between stereo and binaural processing)
- Built-in room reflections
- Lightweight, zero learning curve
- Based on Neumann KU100 binaural reference

**Weaknesses:**
- Very limited feature set (no LFO, no doppler, no comb filters)
- No automation of movement paths
- Basic distance model
- Discontinued (like all Dear Reality products)

**XYZPan advantage:** Far more capable: LFOs, doppler, comb filters, dev mode, reverb. The "focus" knob concept (blend binaural vs stereo) is worth studying -- XYZPan could adopt a similar "binaural intensity" control.

**XYZPan risk:** DearVR MICRO is free and "good enough" for many users. XYZPan must clearly demonstrate superior spatialization to justify adoption.

---

### Panagement 2 (Auburn Sounds) -- Free / 25 EUR

**Price:** Free (limited) / 25 EUR (full)
**Strengths:**
- Elegant distance-as-single-gesture UX (drag back = quieter + duller + wetter)
- Built-in binaural delay
- LFO modulation of pan/distance/gain/tilt
- 5 room models
- Phasescope for mono compatibility monitoring
- Mix-friendly sparse reverb
- Excellent value at 25 EUR

**Weaknesses:**
- 2D only (no elevation)
- Simple ITD + power difference binaural (no pinna modeling)
- No front/back disambiguation beyond HRTF
- No doppler
- Limited to binaural stereo output

**XYZPan advantage:** Full 3D (elevation via pinna notch + chest/floor bounce). Front/back comb filters. Doppler. Dev-tuneable DSP. Panagement is 2D; XYZPan is genuinely 3D.

**XYZPan risk:** Panagement's UX is best-in-class for simplicity. XYZPan must not be so complex that users reach for Panagement instead. Study Panagement's single-gesture distance control.

---

### Summary: Competitive Position

| Capability | XYZPan | DearVR Pro 2 | Spat Revolution | B360 | AMBEO Orbit | Panagement 2 |
|------------|--------|-------------|----------------|------|-------------|-------------|
| Binaural output | Yes | Yes | Yes | No (Ambisonics) | Yes | Yes |
| Multi-format output | No | 35+ formats | Unlimited | Ambisonics only | No | No |
| Elevation | Pinna notch + bounce | HRTF-based | HRTF-based | Yes | Yes | No |
| Front/back | Comb filters (tuneable) | HRTF-based | HRTF-based | Yes | HRTF-based | No |
| Distance model | Full (attenuation + LPF + delay + doppler) | Yes | Yes | No | Basic | Yes (no doppler) |
| Doppler | Yes | No | No | No | No | No |
| Built-in LFO | Per-axis (X/Y/Z) | No | No | No | No | Single LFO |
| Reverb | Algorithmic + distance pre-delay | 46 room presets | Algorithmic + convolution | No | Basic reflections | 5 room models |
| Dev-tuneable DSP | Yes | No | No | No | No | No |
| HRTF approach | Parametric (no database) | HRTF database | HRTF database | N/A | HRTF database | ITD + ILD |
| CPU weight | Light (parametric) | Light | Heavy | Light | Light | Light |
| Price | TBD | Free (discontinued) | $1,790 | ~$29-149 | Free (discontinued) | Free/25 EUR |
| Active development | Yes | No (discontinued) | Yes | Yes | No (discontinued) | Unknown |

**XYZPan's niche:** The tuneable, parametric, 3D binaural panner for people who want full spatial control without HRTF coloration, with creative movement tools (LFOs, doppler) and the ability to customize every DSP parameter. Positioned between Panagement's simplicity and Spat Revolution's complexity.

## Sources

- [DearVR Pro 2 - KVR Audio](https://www.kvraudio.com/product/dearvr-pro-2-by-dear-reality)
- [DearVR Pro 2 - Dear Reality](https://www.dear-reality.com/products/dearvr-pro-2)
- [Dear Reality plugins now free - MusicRadar](https://www.musicradar.com/music-tech/dear-reality-is-giving-away-11-immersive-audio-plugins-for-free)
- [Spat Revolution - FLUX:: Immersive](https://www.flux.audio/project/spat-revolution/)
- [Spat Revolution - IRCAM Forum](https://forum.ircam.fr/projects/detail/spat-revolution/)
- [Waves B360 Ambisonics Encoder](https://www.waves.com/plugins/b360-ambisonics-encoder)
- [AMBEO Orbit - SoundGym](https://www.soundgym.co/blog/item?id=sennheiser-ambeo-orbit)
- [Panagement 2 - Auburn Sounds](https://www.auburnsounds.com/products/Panagement.html)
- [Best Spatial Audio Plugins 2025 - Audiocube](https://www.audiocube.app/blog/spatial-audio-plugin)
- [Best Binaural Plugins - Audiocube](https://www.audiocube.app/blog/best-binaural-binaural-plugin)
- [9 Best 3D Spatial Audio Plugins - Artists in DSP](https://artistsindsp.com/the-9-best-3d-spatial-audio-vst-plugins-for-immersive-mixing/)
- [3D Audio Spatialization Plugin Test - Designing Sound](https://designingsound.org/2018/03/29/lets-test-3d-audio-spatialization-plugins/)
- [3D Audio Weighing The Options - Designing Sound](https://designingsound.org/2015/04/17/3d-audio-weighing-the-options/)
- [Pinna notch frequency estimation - DAFx-15 / NTNU](https://www.ntnu.edu/documents/1001201110/1266017954/DAFx-15_submission_61.pdf)
- [Parametric elevation control for binaural - ScienceDirect](https://www.sciencedirect.com/science/article/abs/pii/S0003682X18305991)
- [Sound Particles Energy Panner](https://soundparticles.com/products/energypanner)
- [Lese Transfer Doppler-Spatializer](https://lese.io/plugin/transfer/)
- [THX Spatial Creator - Plugin Alliance](https://www.plugin-alliance.com/products/spatial-creator)
- [Halo 3D Pan](https://nthn.gumroad.com/l/halo)
- [Transpanner 2 Free 3D Audio Panning - BPB](https://bedroomproducersblog.com/2024/09/19/transpanner-2/)
- [VST3 Processing FAQ - Steinberg](https://steinbergmedia.github.io/vst3_dev_portal/pages/FAQ/Processing.html)
- [Goodhertz CanOpener Studio](https://goodhertz.com/canopener-studio/)
- [Steam Audio - Valve](https://valvesoftware.github.io/steam-audio/)
