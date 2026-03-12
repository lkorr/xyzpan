# Domain Pitfalls

**Domain:** 3D Spatial Audio Panner VST Plugin (Binaural, C++/JUCE)
**Researched:** 2026-03-12

---

## Critical Pitfalls

Mistakes that cause rewrites, shipped bugs, or unusable plugins.

---

### Pitfall 1: Memory Allocation on the Audio Thread

**What goes wrong:** Any call to `new`, `delete`, `malloc`, `free`, `std::string` construction, `std::vector::push_back` (when capacity is exceeded), or `std::shared_ptr` reference counting inside `processBlock()` causes non-deterministic timing. The OS memory allocator takes a lock internally, and if another thread holds it, the audio thread stalls. Result: clicks, pops, dropouts -- intermittent and nearly impossible to reproduce during development but guaranteed to surface in user sessions.

**Why it happens:** It is easy to hide allocations. `std::string` concatenation for debug logging, `std::vector` resizing when delay line length changes, even `std::function` captures can allocate. `std::shared_ptr` destructor runs atomic operations that contend across cores.

**Consequences:** Intermittent audio dropouts that worsen under CPU load. Users report "works fine on my machine" because the timing window for the glitch depends on system load, buffer size, and the specific allocator implementation.

**Prevention:**
- Pre-allocate ALL buffers in `prepareToPlay()`. Delay lines, scratch buffers, LFO state -- everything.
- Use `juce::ScopedNoDenormals` at the top of `processBlock()` (one instance, top level, is sufficient -- it sets CPU register flags for the thread).
- Use `std::atomic<float>` or `juce::SmoothedValue` for parameter communication, never `std::shared_ptr`.
- For passing variable-size data to the audio thread (e.g., new HRIR tables), use a lock-free FIFO (`juce::AbstractFifo` or single-producer/single-consumer queue) to transfer ownership without allocation.
- In debug builds, override the global `operator new` or use a tool like `RealtimeSanitizer` (RADSan) to assert-fail on any allocation during `processBlock()`.

**Detection:** Run pluginval at strictness level 5+. It specifically tests for memory allocations on the audio thread. Also test with Thread Sanitizer (TSan) enabled.

**Confidence:** HIGH -- Universally documented across JUCE forums, KVR, and official JUCE guidance.

**Sources:**
- [JUCE Forum: Locks and memory allocations in the processing thread](https://forum.juce.com/t/locks-and-memory-allocations-in-the-processing-thread/39964)
- [JUCE Forum: Understanding Lock in Audio Thread](https://forum.juce.com/t/understanding-lock-in-audio-thread/60007)
- [JUCE Forum: APVTS Updates & Thread/Realtime Safety](https://forum.juce.com/t/apvts-updates-thread-realtime-safety/36928)

---

### Pitfall 2: Delay Line Interpolation Artifacts (Doppler)

**What goes wrong:** When modulating delay time for doppler shift, the read pointer moves to a fractional sample position. Without proper interpolation, this creates discontinuities (clicks). With naive linear interpolation, sweeping the delay time produces audible high-frequency modulation artifacts (a "phasey" quality). With allpass interpolation, random-access reads (needed when delay time jumps) produce transient garbage because the allpass filter is recursive and needs settling time.

**Why it happens:** A delay line stores discrete samples. Reading between samples requires interpolation. The choice of interpolation method has direct audible consequences, and different methods suit different use cases. For doppler (continuous smooth modulation), you need an interpolation method that sounds musical when swept. For preset-recall jumps (discontinuous changes), you need crossfading.

**Consequences:** Clicks and pops during automation of distance parameter. Metallic/phasey sound during continuous source movement. Potentially destabilizing if the delay modulation rate exceeds safe bounds.

**Prevention:**
- Use **cubic (Hermite) interpolation** for fractional delay reads during continuous modulation. It is the sweet spot between linear (too much HF modulation) and sinc (too CPU expensive). At 48kHz and above, cubic interpolation is nearly artifact-free for audio-rate modulation.
- Apply a **one-pole lowpass filter** on the delay time control signal (not the audio). This prevents the delay time from changing faster than ~20Hz equivalent, which limits the maximum pitch shift and prevents discontinuities. Set the smoothing time constant to 5-20ms.
- For **large discontinuous jumps** (preset recall, parameter reset), use a **crossfade between two delay taps**: fade out old tap, fade in new tap over 5-10ms. Do NOT try to "ramp" a delay line from 10ms to 500ms -- the resulting pitch shift sweep sounds terrible.
- Clamp the maximum rate of delay time change per sample to prevent the read pointer from overtaking the write pointer (which produces silence or garbage).

**Detection:** Automate the distance parameter rapidly in your DAW. Listen for clicks at the transition points. Also automate in a sine wave pattern and listen for metallic artifacts.

**Confidence:** HIGH -- Extensively discussed on KVR DSP forums with mathematical backing.

**Sources:**
- [KVR: Delay Line Interpolation](https://www.kvraudio.com/forum/viewtopic.php?t=251962)
- [JUCE Forum: Pitch Shifting Delay, Smoothing Artifacts, and the Doppler Effect](https://forum.juce.com/t/pitch-shifting-delay-smoothing-artifacts-and-the-doppler-effect/41488)
- [KVR: Parameter smoothing for delay line](https://www.kvraudio.com/forum/viewtopic.php?t=412600)

---

### Pitfall 3: Comb Filter Feedback Instability

**What goes wrong:** The binaural processing chain includes comb filters for pinna reflections and possibly room simulation. If the feedback coefficient ever reaches or exceeds 1.0 (even momentarily due to parameter modulation or floating-point accumulation), the filter self-oscillates and the output grows exponentially to infinity, producing a loud burst that can damage speakers/hearing.

**Why it happens:** Feedback comb filters are IIR structures. Stability requires |feedback gain| < 1.0 strictly. When feedback is a tuneable parameter, users (or LFO modulation) can push it to extremes. Additionally, inserting a lowpass filter inside the feedback loop (for frequency-dependent damping) introduces phase delay that shifts the comb filter's resonant frequencies and can interact with the feedback gain in unexpected ways. Denormalized floats in the feedback path cause CPU spikes (10-100x) rather than instability per se, but NaN propagation from a single bad sample will kill the entire signal chain.

**Consequences:** Ear-damaging volume spikes. NaN propagation that silences the plugin until the DAW session is reloaded. CPU spikes from denormals in feedback tails.

**Prevention:**
- Hard-clamp feedback coefficient to `[-0.999f, 0.999f]` AFTER parameter smoothing and LFO modulation are applied. Never trust the parameter range alone -- modulation can push it out of bounds.
- Add a **soft clipper** (e.g., `tanh`) inside the feedback path to prevent runaway gain, even at the cost of slight nonlinear coloration.
- Check for NaN/Inf every N samples (e.g., every block) and reset filter state to zero if detected: `if (std::isnan(state) || std::isinf(state)) state = 0.0f;`
- Use `juce::ScopedNoDenormals` at the top of `processBlock()` to flush denormals to zero CPU-wide. This is the standard solution and has zero performance overhead.
- Consider using `double` precision for filter state variables in tight feedback loops -- it extends the dynamic range before denormals kick in and improves numerical stability.

**Detection:** Set feedback to max, send an impulse, and verify the tail decays to silence (not to infinity or NaN). Profile CPU with feedback at max and input at silence -- watch for denormal-induced spikes.

**Confidence:** HIGH -- Well-established DSP theory, confirmed by multiple KVR and JUCE forum discussions.

**Sources:**
- [CCRMA: Feedback Comb Filters](https://ccrma.stanford.edu/~jos/pasp/Feedback_Comb_Filters.html)
- [DSPRelated: Comb Filters](https://www.dsprelated.com/freebooks/pasp/Comb_Filters.html)
- [JUCE Forum: IIR Filter denormalisation](https://forum.juce.com/t/iir-filter-denormalisation/14302)

---

### Pitfall 4: Parameter Smoothing (Zipper Noise)

**What goes wrong:** When a VST parameter changes (automation, user interaction, preset recall), the new value is applied immediately at the start of the next audio block. For parameters that directly affect gain, filter coefficients, or delay times, this discontinuity produces audible "zipper noise" -- a buzzing or clicking artifact.

**Why it happens:** DAWs update parameters at the block boundary, not per-sample. A block of 512 samples at 44.1kHz is ~11.6ms. Jumping from one value to another over that boundary creates a step function in the control signal.

**Consequences:** Audible clicks on every parameter change. Especially bad for distance (affects gain and delay simultaneously), azimuth/elevation (affects ITD and ILD), and any parameter the LFO modulates.

**Prevention:**
- Use `juce::SmoothedValue<float>` for ALL parameters that feed into the DSP path. Set the ramp length to 10-20ms (441-882 samples at 44.1kHz).
- Call `smoothedValue.setTargetValue(newParam)` at the start of each block, then call `smoothedValue.getNextValue()` per sample inside the processing loop.
- For parameters that change filter coefficients (head shadow, pinna EQ), recalculate coefficients at a decimated "control rate" (e.g., every 32 samples) rather than every sample. This balances smoothness against CPU cost.
- **Critical for ITD:** The interaural time delay is a delay time in samples. Smoothing this parameter IS the doppler effect. If you want smooth panning without pitch artifacts, you need the crossfade approach (two taps) rather than simple delay modulation.

**Detection:** Automate any parameter in a staircase pattern (jump between values) and listen for clicks at each step. Pluginval's parameter fuzz test will also surface unsmoothed parameters.

**Confidence:** HIGH -- Standard JUCE practice, enforced by pluginval.

**Sources:**
- [JUCE Forum: AudioParameter thread safety](https://forum.juce.com/t/audioparameter-thread-safety/21097)
- [DeepWiki: JUCE Audio Plugin Development](https://deepwiki.com/cline/prompts/4.3-juce-audio-plugin-development)

---

### Pitfall 5: Sample Rate Dependency

**What goes wrong:** Delay times, filter coefficients, smoothing rates, and LFO frequencies are hardcoded in samples rather than computed relative to the sample rate. The plugin works at 44.1kHz but sounds completely wrong at 48kHz, 88.2kHz, or 96kHz. ITD delay for maximum azimuth is ~0.66ms, which is 29 samples at 44.1kHz but 63 samples at 96kHz.

**Why it happens:** During development, you test at one sample rate and all the magic numbers work. Constants like "29 samples of delay" get baked in. Filter coefficient formulas that depend on sample rate get copy-pasted without the `sampleRate` variable.

**Consequences:** At higher sample rates: ITDs are too short (sounds come from inside the head), filter frequencies are wrong (head shadow doesn't match), LFO speeds are halved. At lower sample rates: ITDs are too long (unnatural), potential buffer overruns if delay lines are sized in fixed sample counts.

**Prevention:**
- Store ALL timing constants in milliseconds or seconds, converting to samples via `timeInSamples = timeInMs * sampleRate / 1000.0` in `prepareToPlay()`.
- Recalculate all filter coefficients in `prepareToPlay()` when sample rate changes.
- Size delay line buffers based on `maxDelayTimeMs * maxSampleRate / 1000 + guardSamples`. Allocate for the max sample rate you will support (192kHz) so that `prepareToPlay()` never needs to reallocate.
- LFO phase increment must be `frequency / sampleRate`, not a hardcoded constant.
- Test at 44100, 48000, 88200, 96000, and 192000 Hz. Audition the binaural effect at each rate.

**Detection:** Run the plugin at 96kHz and compare the sound to 44.1kHz. If they differ significantly, you have sample-rate-dependent constants.

**Confidence:** HIGH -- Fundamental DSP principle.

---

### Pitfall 6: OpenGL Context Management in Plugin UIs

**What goes wrong:** The custom OpenGL 3D panner UI works with one instance but crashes or freezes the DAW when multiple instances are open. On macOS, OpenGL is deprecated (since 10.14) and the rendering path is increasingly unreliable. On Windows with JUCE 8+, OpenGL conflicts with Direct2D rendering, causing GUI sluggishness and DAW freezes. Different graphics drivers (Intel, AMD, NVIDIA) behave inconsistently.

**Why it happens:** OpenGL contexts are per-thread and per-window. Multiple plugin instances share the same process but need separate contexts. Some DAWs (Logic, Ableton) sandbox plugins or run them in separate processes, changing context ownership semantics. JUCE's OpenGL renderer has had longstanding issues with multi-instance scenarios across versions 6, 7, and 8. Context creation/destruction during editor open/close can race with the DAW's own rendering.

**Consequences:** DAW freezes requiring force-quit. Sporadic crashes when opening/closing projects with multiple instances. UI stops painting (blank white rectangle) while still responding to mouse events. Performance degradation that scales with instance count (3-4 instances enough to lag DAW meters).

**Prevention:**
- **Minimize OpenGL surface area.** Use OpenGL ONLY for the 3D panner visualization component, not for the entire plugin UI. Render the 3D view to a framebuffer object (FBO), then blit the result to a JUCE `Image` which is composited normally. This isolates OpenGL from the DAW's rendering pipeline.
- **Handle context loss gracefully.** The OpenGL context can be destroyed at any time (editor close, DAW reparenting the window). All GPU resources (VBOs, textures, shaders) must be re-creatable. Never cache raw OpenGL handles across editor lifetime.
- **Throttle the render rate.** Do not render at 60fps continuously. Use a timer (15-30fps) and only re-render when state changes. This dramatically reduces GPU contention with multiple instances.
- **Test multi-instance aggressively.** Open 8+ instances in every target DAW. Open and close editors while audio plays. Resize windows. Switch between plugin windows rapidly.
- **Have a software fallback.** If OpenGL context creation fails, degrade to a 2D top-down view rendered with JUCE's software renderer. Users with broken GPU drivers will thank you.
- **Consider the long game.** Apple deprecated OpenGL. JUCE 8+ has Direct2D issues on Windows. You may want to evaluate rendering to an offscreen buffer with a minimal custom renderer rather than depending on JUCE's OpenGL integration.

**Detection:** Open 4+ instances in Reaper, Ableton, and FL Studio simultaneously. Monitor for frame drops, freezes, and crashes. Test on both integrated (Intel) and discrete (NVIDIA/AMD) GPUs.

**Confidence:** HIGH -- Multiple confirmed bug reports across JUCE versions 6-8.

**Sources:**
- [JUCE Forum: Host freezing when multiple plugin UI instances are open since JUCE 8.0.8](https://forum.juce.com/t/host-freezing-when-multiple-plugin-ui-instances-are-open-since-juce-8-0-8/67441)
- [JUCE Forum: Rendering multiple instances of a plugin with OpenGL](https://forum.juce.com/t/rendering-multiple-instances-of-a-plugin-with-opengl/15639)
- [JUCE Forum: Crash with multiple plugin instances using OpenGL](https://forum.juce.com/t/crash-with-multiple-plugin-instances-using-opengl/11276)
- [JUCE Forum: Significant GUI Sluggishness & DAW Freezes on Windows/Nvidia (JUCE 8 / OpenGL / D2D Conflict)](https://forum.juce.com/t/significant-gui-sluggishness-daw-freezes-on-windows-nvidia-suspected-juce-8-opengl-d2d-conflict/67863)

---

### Pitfall 7: State Save/Restore (Preset and Session Recall)

**What goes wrong:** The plugin loses settings when the DAW project is saved and reopened. Or worse: it recalls incorrect settings, overwriting the saved state with a default preset. Different DAWs call `getStateInformation()`/`setStateInformation()` in different orders and at different times, and the plugin's initialization logic conflicts with the recall sequence.

**Why it happens:** DAW-specific behaviors create a minefield:
- **Ableton Live** calls `setCurrentProgram()` AFTER `setStateInformation()`, so if your preset code resets parameters, it overwrites the recalled state.
- **FL Studio** (VST3) calls `getStateInformation()` BEFORE `setStateInformation()` during plugin creation, so an early save can capture the default state instead of the restored state.
- **Pro Tools (AAX)** has its own parameter save/recall mechanism that operates independently of `getStateInformation()`/`setStateInformation()`, meaning parameters are recalled even if those functions are empty.
- **VST3 generally:** The wrapper may call `setStateInformation()` before `prepareToPlay()`, meaning sample-rate-dependent state is restored before the sample rate is known.

**Consequences:** User loses hours of work when a session doesn't recall correctly. Trust is destroyed -- users stop using the plugin.

**Prevention:**
- Use `juce::AudioProcessorValueTreeState` (APVTS) for ALL parameters. It handles state serialization, thread safety, and format-specific quirks.
- Store a **version number** in the saved state XML/binary. This enables forward-compatible state migration when you add, remove, or rename parameters in future versions.
- Do NOT implement `setCurrentProgram()`/`getCurrentProgram()` with actual preset logic unless you fully understand the DAW interaction. If you have factory presets, load them through a separate mechanism, not through the program change API.
- Test state recall in EVERY target DAW. Save a session with non-default values, close, reopen, and verify every parameter. Automate this test if possible.
- Handle the case where `setStateInformation()` receives data from a newer version of the plugin gracefully (ignore unknown parameters, use defaults for missing ones).

**Detection:** Pluginval's state restoration test (strictness 5+) catches most issues. Also manually test: set all parameters to non-default, save, close, reopen, verify.

**Confidence:** HIGH -- Dozens of confirmed DAW-specific bugs on JUCE forums.

**Sources:**
- [JUCE Forum: Problems recalling plugin state versus current preset](https://forum.juce.com/t/problems-recalling-plugin-state-versus-current-preset/5895)
- [JUCE Forum: FL Studio 20 VST3 getStateInformation called when plugin is being created](https://forum.juce.com/t/fl-studio-20-vst-3-getstateinformation-called-when-plugin-is-being-created/32381)
- [JUCE Forum: AAX unexpected state save/recall](https://forum.juce.com/t/aax-unexpected-state-save-recall/32372)
- [JUCE Forum: Basic APVTS plugin won't pass PluginVal](https://forum.juce.com/t/basic-audioprocessorvaluetreestate-plugin-wont-pass-pluginval/39543)

---

### Pitfall 8: HRIR/HRTF Interpolation Comb Filtering

**What goes wrong:** When the source moves between HRTF measurement positions, interpolating between adjacent HRIRs produces comb filtering because the HRIRs have different embedded ITDs (onset delays). Adding two signals of similar amplitude but different phase creates destructive interference at specific frequencies, producing a hollow, flanged sound.

**Why it happens:** HRTF datasets (CIPIC, LISTEN, SOFA) store HRIRs that include the ITD as a time offset within the impulse response. If you naively interpolate between two HRIRs where one has a 5-sample onset delay and another has a 12-sample onset delay, the interpolated result is a sum of time-shifted versions of similar spectra -- a textbook comb filter.

**Consequences:** Audible "phasey" artifacts during continuous source movement. The effect is most pronounced for lateral positions (large ITD differences) and at high frequencies.

**Prevention:**
- **Separate ITD from HRIR.** Strip the onset delay from each HRIR before interpolation. Compute the interpolated ITD separately. Apply the interpolated ITD as a fractional delay, and convolve with the time-aligned (ITD-removed) interpolated HRIR. This is the approach used by the 3D Tune-In Toolkit and eliminates the primary source of comb artifacts.
- Use **barycentric interpolation** between the three nearest HRIRs (triangulated on the sphere), not just linear interpolation between two. This produces smoother transitions.
- If you are NOT using HRTF convolution (your design uses analytic models for ITD, ILD, and head shadow), this pitfall does not apply directly. But if you later add HRTF support, remember this.

**Detection:** Pan a source slowly in azimuth while playing white noise. Listen for comb filtering / flanging artifacts, especially at lateral positions (90/270 degrees).

**Confidence:** HIGH -- Published research (PLOS One, CCRMA).

**Sources:**
- [3D Tune-In Toolkit: An open-source library for real-time binaural spatialisation (PLOS One)](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0211899)
- [Anaglyph Binaural Engine](https://anaglyph.dalembert.upmc.fr/)

---

## Moderate Pitfalls

Issues that cause bugs or significant rework but are recoverable.

---

### Pitfall 9: Latency Reporting to the DAW

**What goes wrong:** The plugin introduces latency (from delay lines, lookahead, or internal buffering) but doesn't report it correctly via `setLatencySamples()`. The DAW can't compensate, so the plugin's output is misaligned with other tracks. Worse: calling `setLatencySamples()` inside `prepareToPlay()` in VST3 format triggers a restart loop because the wrapper calls `prepareToPlay()` again when latency changes.

**Prevention:**
- Set initial latency in the constructor.
- If latency depends on sample rate, update it in `prepareToPlay()` but be aware of the VST3 restart issue. Test specifically in Cubase (known to only update latency on enable/disable) and Pro Tools (handles dynamic latency well).
- If your plugin has zero algorithmic latency (processes sample-by-sample with no lookahead), report 0. Do NOT report the ITD delay as latency -- it is part of the effect, not processing delay.

**Confidence:** HIGH

**Sources:**
- [JUCE Forum: Calling setLatencySamples in prepareToPlay](https://forum.juce.com/t/calling-setlatencysamples-in-preparetoplay/48131)
- [JUCE Forum: Plugin Latency Compensation Puzzle](https://forum.juce.com/t/plugin-latency-compensation-puzzle/50594)

---

### Pitfall 10: `parameterChanged` Callback Thread Ambiguity

**What goes wrong:** The `AudioProcessorValueTreeState::Listener::parameterChanged` callback is called from whichever thread triggered the change. When automation drives it, that's the audio thread. When the GUI drives it, that's the message thread. Code that assumes one thread (e.g., updating a GUI label from `parameterChanged`) crashes or causes deadlocks.

**Prevention:**
- Treat `parameterChanged` as if it ALWAYS runs on the audio thread. Never call GUI code from it.
- For GUI updates in response to parameter changes, use `juce::AsyncUpdater` or a timer on the message thread that polls the parameter values.
- Pluginval specifically tests this scenario with its automation parameter fuzz test.

**Confidence:** HIGH

**Sources:**
- [JUCE Forum: Simple Audio/GUI Thread Safety Question](https://forum.juce.com/t/simple-audio-gui-thread-safety-question/40383)

---

### Pitfall 11: Azimuth Wraparound in Circular Coordinates

**What goes wrong:** When interpolating azimuth (horizontal angle), the value wraps from 359 degrees to 0 degrees. Naive linear interpolation between 359 and 1 produces 180 (going the wrong way around the circle), causing the sound to sweep through the entire azimuth range in one sample.

**Prevention:**
- Always compute the shortest angular distance when interpolating circular values. Use `atan2(sin(target - current), cos(target - current))` or equivalent shortest-arc logic.
- Apply this to LFO modulation of azimuth as well -- a sine LFO sweeping 350-370 degrees must not wrap through 0.

**Confidence:** HIGH -- Classic numerical bug, confirmed by RealSpace3D bug reports.

---

### Pitfall 12: Bus Layout and Channel Configuration

**What goes wrong:** The plugin is mono-in/stereo-out but doesn't correctly declare its bus layout. Some DAWs refuse to load it, others route audio incorrectly (stereo in forced to mono, or mono duplicated to stereo before the plugin sees it).

**Prevention:**
- Override `isBusesLayoutSupported()` to explicitly accept ONLY mono input and stereo output. Reject all other configurations.
- Test in Pro Tools (strictest about bus layouts), FL Studio (has "fixed size buffers" and output connection quirks), and Logic (AU format requires explicit channel layout declaration).

**Confidence:** HIGH

---

## Technical Debt Patterns

Common shortcuts in audio plugins that cause problems later.

---

### Debt 1: God-Class Processor

**What goes wrong:** All DSP code lives in `PluginProcessor::processBlock()` -- ITD, ILD, head shadow, pinna, distance, LFO, doppler, all in one 500-line function. It works initially but becomes impossible to test, debug, or modify individual components.

**Prevention:** Extract each DSP stage into its own class with a `process(AudioBuffer&)` method: `ITDProcessor`, `HeadShadowFilter`, `PinnaReflectionComb`, `DistanceModel`, `LFOModulator`. Chain them in `processBlock()`. Each class is independently testable with unit tests.

---

### Debt 2: Magic Number Constants

**What goes wrong:** DSP code is full of `0.00058f` (speed of sound coefficient), `17.5f` (head radius cm), `0.3f` (damping factor). Changing any constant requires hunting through the codebase. Worse, the same physical constant appears in multiple places with slightly different values.

**Prevention:** Define all physical constants in a single header. Use named constants: `constexpr float kHeadRadiusCm = 8.75f;` Use functions to derive sample-dependent values: `int itdSamples(float azimuthRad, float sampleRate)`.

---

### Debt 3: Mixing Control Logic and DSP

**What goes wrong:** Parameter smoothing, LFO modulation, and range mapping are interleaved with the DSP math. When you need to change how a parameter is smoothed or add modulation to a new target, you have to modify DSP code.

**Prevention:** Implement a clear separation: Parameters -> Smoothing -> Modulation -> DSP Input Values. The DSP code should receive final, ready-to-use values (delay in samples, gain in linear, filter cutoff in Hz) and never touch raw parameter values or smoothing state.

---

### Debt 4: No Automated Testing

**What goes wrong:** The plugin "works" because you listen to it in your DAW. But there are no tests that verify: filter stability across parameter ranges, output is finite for all inputs, state recall produces identical output, sample-rate-independent behavior, bypass produces clean pass-through.

**Prevention:** Write audio unit tests from day one. Feed test signals (impulse, sine, white noise) through each DSP module, verify output properties (finite, within expected amplitude range, correct frequency response). Run these in CI alongside pluginval.

---

## Performance Traps

---

### Trap 1: Per-Sample Function Call Overhead

**What goes wrong:** Each sample passes through 6+ DSP stages, each implemented as a virtual function call. At 192kHz, that is 192,000 * 6 = 1.15 million virtual calls per second. The branch predictor and instruction cache thrash.

**Prevention:**
- Process in blocks per-stage, not per-sample across all stages. Run the ITD delay on the entire block, then head shadow on the entire block, etc. This keeps each algorithm's code hot in the instruction cache.
- Avoid virtual dispatch in the inner loop. Use templates or direct function calls. If you need runtime dispatch (e.g., switching interpolation methods), dispatch once per block, not per sample.
- Consider a fixed internal block size (e.g., 64 samples) with FIFO buffering if you need fine-grained control rate updates.

**Confidence:** HIGH

**Sources:**
- [Medium: Fixed vs. Variable Buffer Processing in Real-Time Audio DSP](https://medium.com/@12264447666.williamashley/fixed-vs-variable-buffer-processing-in-real-time-audio-dsp-performance-determinism-and-66da78390b0f)

---

### Trap 2: Cache-Hostile Memory Layout

**What goes wrong:** The binaural processor has left and right delay lines, left and right filter states, left and right output buffers stored in separate objects scattered across the heap. Processing alternates between left and right channels, causing constant cache misses.

**Prevention:**
- Interleave processing: process the entire left channel for a block, then the entire right channel. This keeps each channel's data hot.
- Alternatively, store left and right channel data contiguously in a struct-of-arrays layout rather than an array-of-structs.
- A 512-sample `float` buffer is 2KB -- fits comfortably in L1 cache (32KB typical). Keep your working set under this threshold per processing stage.

---

### Trap 3: Branching in the Inner Loop

**What goes wrong:** The processing loop has conditionals: `if (enableDoppler)`, `if (lfoActive)`, `if (distance > farThreshold)`. Modern CPUs predict branches well for consistent patterns, but when these conditionals change mid-block (due to parameter modulation), prediction fails and the pipeline stalls.

**Prevention:**
- Move conditionals OUTSIDE the sample loop. Check once per block and call different processing functions (or use different code paths). For example, have `processWithDoppler()` and `processWithoutDoppler()`.
- For parameters that can change mid-block smoothly (like LFO amount), always run the LFO code but multiply its output by a 0-or-1 enable value. The multiply is cheaper than the branch mispredict.

---

### Trap 4: Unnecessary Trigonometry Per Sample

**What goes wrong:** Computing `sin()`, `cos()`, `atan2()` for azimuth/elevation on every sample. These functions take 20-100 CPU cycles each. At 192kHz stereo with 6+ trig calls per sample, this dominates the CPU budget.

**Prevention:**
- Compute trig functions at the control rate (once per block or every N samples) and interpolate linearly between.
- Use lookup tables with linear interpolation for `sin`/`cos` -- 1-2 cycles vs 20-100.
- For head shadow and pinna models that depend on angle, precompute the filter coefficients at the control rate, not the sample rate.

---

## DAW Compatibility Issues

---

### Issue 1: DAW-Specific Plugin Format Quirks

| DAW | Format | Key Quirks |
|-----|--------|------------|
| **Logic Pro** | AU | Strict AU validation (`auval`). Sandboxes plugins on Apple Silicon (separate process per instance -- singletons break). Plugin may pass `auval` but still not appear in Logic. |
| **Ableton Live** | VST3/AU | Sensitive to code signing/notarization on macOS. JUCE version upgrades can break plugin detection. Parameter sync between plugin GUI and DAW automation can desync. |
| **FL Studio** | VST3 | Calls `getStateInformation()` before `setStateInformation()` on creation. Plugin scanning shows errors even when plugin loads fine. Needs "fixed size buffers" setting in some cases. |
| **Pro Tools** | AAX | Requires PACE/iLok signing and Avid developer account. Independent parameter recall mechanism. Strictest bus layout enforcement. |
| **Reaper** | VST3/AU/LV2 | Unique threading model exposes GUI bugs that other DAWs hide. Most flexible but also most permissive -- passing Reaper doesn't mean passing others. |

**Prevention:** Test in ALL target DAWs. Run pluginval at strictness 5+. Do not assume that working in one DAW means working in all.

**Confidence:** HIGH

**Sources:**
- [JUCE Forum: What DAWs have a unique way of doing things and deserve special scrutiny](https://forum.juce.com/t/what-daws-have-a-unique-way-of-doing-things-and-deserve-special-scrutiny-when-testing-before-plugin-release/57443)

---

### Issue 2: Buffer Size Assumptions

**What goes wrong:** The plugin assumes a minimum buffer size (e.g., 64 samples) or a power-of-two size. Some DAWs send buffers of 1 sample (Logic Pro in certain configurations), odd sizes (e.g., 100, 300), or sizes that change between callbacks.

**Prevention:**
- Never assume buffer size. Your `processBlock()` must handle any size from 1 to 8192+.
- If you need a fixed internal block size, implement FIFO buffering in `processBlock()` that accumulates input samples and processes in fixed chunks.
- Test with buffer sizes: 1, 32, 64, 128, 256, 512, 1024, 2048, and non-power-of-two values.

---

### Issue 3: Multi-Instance Shared State

**What goes wrong:** Static variables or singletons are shared across plugin instances in DAWs that run plugins in-process. But Logic Pro on Apple Silicon sandboxes each instance into its own process, creating separate copies of statics. Code that depends on shared state breaks in one environment or the other.

**Prevention:**
- Do NOT use static variables or singletons for any mutable state. Each plugin instance must be fully self-contained.
- If you need shared resources (e.g., HRTF data loaded once), use reference-counted shared ownership with proper synchronization, and handle the case where each instance loads its own copy (sandbox mode).

---

## UX Pitfalls

---

### UX 1: 3D Space Represented Poorly in 2D

**What goes wrong:** The UI shows an XY plane (top-down view) but the user can't visualize or control elevation (Z axis). Or the UI shows a 3D perspective view that looks impressive but is imprecise for fine positioning -- users can't tell if the source is at 30 or 45 degrees elevation.

**Prevention:**
- Provide BOTH a 3D perspective view (for intuitive spatial understanding) and precise numeric readouts/sliders for each axis (for fine control).
- Show at least two 2D projections: top-down (XY) and front-facing (XZ or YZ), each with a draggable point.
- The 3D view is for overview; the 2D projections and sliders are for precision. Do not force users to do precise work in the 3D view.

---

### UX 2: No Visual Feedback of Binaural Effect

**What goes wrong:** The user moves the panner dot but has no visual indication of what the binaural processing is doing -- no ITD display, no ILD indication, no frequency response visualization. They must rely entirely on listening, which is fatiguing and imprecise.

**Prevention:**
- Show the effective left/right delay difference (ITD) as a visual indicator.
- Show a simplified frequency response curve indicating how the head shadow is filtering each ear.
- Display the current distance attenuation as a gain meter or numeric value.

---

### UX 3: LFO Controls That Don't Show Motion

**What goes wrong:** LFO modulation is active but the UI doesn't reflect it. The XYZ position dot sits still while the sound orbits. The user can't see what the LFO is doing without listening.

**Prevention:**
- Animate the source position in the 3D/2D view to reflect the actual modulated position (base position + LFO offset).
- Show LFO waveform shapes on each axis and the current phase position.

---

### UX 4: Azimuth/Elevation Confusion with XYZ

**What goes wrong:** The plugin exposes raw XYZ Cartesian coordinates but users think in terms of azimuth (left/right angle), elevation (up/down angle), and distance. Or vice versa: the UI shows spherical coordinates but the DSP needs Cartesian. Conversion errors cause the sound to end up in the wrong position.

**Prevention:**
- Expose BOTH coordinate systems in the UI. Let users type azimuth/elevation/distance OR drag in XYZ space.
- Internally, pick ONE canonical representation (Cartesian is usually better for the DSP) and convert at the parameter boundary.
- Be explicit about coordinate conventions: is 0 azimuth front or right? Is positive Y up or forward? Document it and be consistent.

---

## "Looks Done But Isn't" Checklist

Things commonly missed in spatial audio plugins that users notice immediately.

---

### 1. Bypass Behavior

- [ ] Bypass produces clean pass-through with NO processing artifacts (no click on engage/disengage)
- [ ] Bypassed signal has correct latency compensation (DAW-reported latency still applies)
- [ ] Bypass crossfades wet/dry over 5-10ms to avoid discontinuity
- [ ] Mono input passed through as mono when bypassed, not duplicated to stereo with a channel offset

### 2. Tail Handling

- [ ] When input goes silent, reverb/comb filter tails ring out naturally (not cut off)
- [ ] `getTailLengthSeconds()` returns the actual maximum tail length so DAWs don't truncate during offline bounce
- [ ] Tail length accounts for the longest feedback comb filter decay time at maximum feedback setting

### 3. Silence Detection / CPU Optimization

- [ ] When input is silent AND all tails have decayed, processing skips expensive DSP to save CPU
- [ ] "Silent" threshold accounts for denormalized floats (use a proper RMS threshold, not just == 0)
- [ ] Plugin wakes back up instantly when input resumes (no fade-in delay)

### 4. Mono Compatibility Check

- [ ] The stereo binaural output collapses to mono without catastrophic cancellation (some cancellation is expected but verify it doesn't null completely)
- [ ] Document to users that this is a binaural plugin and mono monitoring will lose the 3D effect

### 5. Offline Bounce / Non-Realtime Rendering

- [ ] Plugin produces identical output when bounced offline vs played in real time
- [ ] No timing-dependent bugs (e.g., using `Time::getMillisecondCounter()` instead of sample position)
- [ ] LFO phase is deterministic from the start of playback, not from when the plugin was instantiated

### 6. Preset Management

- [ ] Factory presets demonstrate the plugin's capabilities (front, behind, above, orbiting, approaching/receding)
- [ ] User presets save and recall correctly, including LFO state
- [ ] Preset switching crossfades to avoid clicks

### 7. Automation

- [ ] ALL parameters are automatable and smoothed
- [ ] Automation recording from GUI interaction works (DAW records the movements)
- [ ] Automation playback drives both the DSP and the GUI (visual feedback matches automation)
- [ ] Sample-accurate automation is handled (or gracefully degraded) per JUCE capabilities

### 8. Edge Cases in Position

- [ ] Source at exact listener position (distance = 0) doesn't produce division by zero, infinite gain, or NaN
- [ ] Source directly above/below listener (elevation = +/-90) doesn't produce singularities in the azimuth calculation
- [ ] Maximum distance doesn't produce silence (just very quiet) unless explicitly designed that way

### 9. Head Model Assumptions

- [ ] The head radius, ear spacing, and shoulder height are reasonable defaults (ITD max ~0.66ms, head radius ~8.75cm)
- [ ] If these are exposed as parameters, they have sane ranges (prevent negative head radius, zero ear spacing, etc.)

### 10. Plugin Metadata

- [ ] Plugin name, manufacturer, unique ID are all set correctly (FL Studio displays "Your Company" if not set)
- [ ] Plugin category is set to "Fx" -> "Spatial" or equivalent so DAWs categorize it correctly
- [ ] Version number is embedded and incremented on each release

---

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| Core DSP engine | Comb filter instability, denormal CPU spikes | Hard-clamp feedback, `ScopedNoDenormals`, NaN guards from day one |
| Delay modulation (doppler) | Interpolation clicks, read-pointer overrun | Cubic interpolation, smoothed control signal, crossfade for jumps |
| Parameter system | Zipper noise, thread safety violations | `SmoothedValue` on ALL params, APVTS for all parameter management |
| OpenGL UI | Multi-instance crashes, DAW freezes | FBO-to-image approach, throttled rendering, software fallback |
| State management | DAW-specific save/recall bugs | APVTS, version-tagged state, test every DAW |
| DAW compatibility | Bus layout rejection, validation failures | `isBusesLayoutSupported()`, pluginval at strictness 5+, test all targets |
| LFO modulation | Modulated values exceeding safe ranges | Post-modulation clamping on all parameters, especially feedback and delay |
| Performance optimization | Cache misses, branch mispredicts | Block-based per-stage processing, branchless inner loops |

---

## Sources

### JUCE Forums (HIGH confidence)
- [AudioParameter thread safety](https://forum.juce.com/t/audioparameter-thread-safety/21097)
- [APVTS Updates & Thread/Realtime Safety](https://forum.juce.com/t/apvts-updates-thread-realtime-safety/36928)
- [Understanding Lock in Audio Thread](https://forum.juce.com/t/understanding-lock-in-audio-thread/60007)
- [Host freezing with multiple plugin UIs (JUCE 8.0.8)](https://forum.juce.com/t/host-freezing-when-multiple-plugin-ui-instances-are-open-since-juce-8-0-8/67441)
- [OpenGL multi-instance crashes](https://forum.juce.com/t/crash-with-multiple-plugin-instances-using-opengl/11276)
- [GUI Sluggishness / OpenGL / D2D Conflict](https://forum.juce.com/t/significant-gui-sluggishness-daw-freezes-on-windows-nvidia-suspected-juce-8-opengl-d2d-conflict/67863)
- [setLatencySamples in prepareToPlay](https://forum.juce.com/t/calling-setlatencysamples-in-preparetoplay/48131)
- [FL Studio VST3 getStateInformation timing](https://forum.juce.com/t/fl-studio-20-vst-3-getstateinformation-called-when-plugin-is-being-created/32381)
- [DAW-specific testing scrutiny](https://forum.juce.com/t/what-daws-have-a-unique-way-of-doing-things-and-deserve-special-scrutiny-when-testing-before-plugin-release/57443)
- [ScopedNoDenormals usage](https://forum.juce.com/t/when-to-use-scopednodenormals-and-when-to-not/37112)
- [Locks and memory allocations in processing thread](https://forum.juce.com/t/locks-and-memory-allocations-in-the-processing-thread/39964)

### KVR Audio Forums (HIGH confidence)
- [Delay Line Interpolation](https://www.kvraudio.com/forum/viewtopic.php?t=251962)
- [Parameter smoothing for delay line](https://www.kvraudio.com/forum/viewtopic.php?t=412600)
- [Delay and smooth time changes](https://www.kvraudio.com/forum/viewtopic.php?p=6415633)

### Academic / Reference (HIGH confidence)
- [CCRMA: Feedback Comb Filters](https://ccrma.stanford.edu/~jos/pasp/Feedback_Comb_Filters.html)
- [3D Tune-In Toolkit (PLOS One)](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0211899)

### Tools
- [pluginval (Tracktion)](https://github.com/Tracktion/pluginval)
- [Melatonin: pluginval is a plugin dev's best friend](https://melatonin.dev/blog/pluginval-is-a-plugin-devs-best-friend/)
