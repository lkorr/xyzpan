# XYZPan

3D spatial audio panner plugin (VST3) with real-time binaural rendering.

## Features

- Binaural panning with ITD, ILD, head shadow, and pinna filtering
- Early reflections via image-source method (6-wall shoebox room)
- FDN reverb with modulation and pre-delay
- Doppler effect with distance delay
- Distance-based air absorption, gain curves, and presence EQ
- Per-axis LFO modulation with multiple waveforms
- Stereo source node splitting with orbit LFOs
- Listener position and head orientation control
- Multi-instance listener linking
- Custom OpenGL 3D visualization
- 7 factory presets + user preset save/load

## Installation

### Windows

Copy `XYZPan.vst3` to:
```
C:\Program Files\Common Files\VST3\
```
Rescan plugins in your DAW.

### macOS — AU (Logic Pro)

1. Copy `XYZPan.component` to `/Library/Audio/Plug-Ins/Components/`
2. In Terminal, run: `sudo xattr -r -d com.apple.quarantine /Library/Audio/Plug-Ins/Components/XYZPan.component`
3. Restart your Mac
4. In Logic: click **Audio FX** slot → **Audio Units** → **pailiaq** → **XYZPan**

### macOS — VST3

1. Copy `XYZPan.vst3` to `/Library/Audio/Plug-Ins/VST3/`
2. In Terminal, run: `sudo xattr -r -d com.apple.quarantine /Library/Audio/Plug-Ins/VST3/XYZPan.vst3`
3. Rescan plugins in your DAW

## System Requirements

- Windows 10+ or macOS 11+
- OpenGL 3.2 compatible GPU
- VST3-compatible DAW

## Factory Presets

| Preset | Description |
|---|---|
| Default | Front-center, no modulation |
| Orbit XY | Circular orbit in the horizontal plane |
| Slow Drift | Gentle wandering movement across all axes |
| Behind You | Source positioned behind the listener |
| Fly Around | High-energy 3D motion with full LFO modulation |
| Overhead | Source above the listener's head |
| Near Whisper | Very close, intimate positioning |

## Building from Source

Requires CMake 3.25+, C++20 compiler, and JUCE 8.0.12 (fetched automatically via CPM).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Output: `build/plugin/XYZPan_artefacts/Release/VST3/XYZPan.vst3`
