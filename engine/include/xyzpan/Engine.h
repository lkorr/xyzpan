#pragma once
#include "xyzpan/Types.h"
#include <vector>

namespace xyzpan {

// XYZPanEngine — pure C++ audio processing engine with no JUCE dependency.
//
// Lifecycle:
//   1. prepare(sampleRate, maxBlockSize) — called before processing; pre-allocates buffers
//   2. setParams(params)                 — called once per processBlock with current parameter snapshot
//   3. process(...)                      — called each processBlock; fills outL and outR
//   4. reset()                           — called on transport restart; clears all state
//
// Audio contract:
//   - Accepts 1 or 2 input channels; stereo is summed to mono internally.
//   - Always produces 2-channel (stereo) output.
//   - No allocation inside process(); all buffers pre-allocated in prepare().
class XYZPanEngine {
public:
    XYZPanEngine() = default;
    ~XYZPanEngine() = default;

    // Non-copyable, non-movable (owns audio buffers).
    XYZPanEngine(const XYZPanEngine&) = delete;
    XYZPanEngine& operator=(const XYZPanEngine&) = delete;

    // Called before processing begins. Allocates monoBuffer to maxBlockSize.
    void prepare(double sampleRate, int maxBlockSize);

    // Set current parameters (snapshot from APVTS atomics, called once per block).
    void setParams(const EngineParams& params);

    // Process audio.
    //   inputs            — array of input channel pointers (1 or 2 channels)
    //   numInputChannels  — number of valid input channel pointers
    //   outL, outR        — output channel pointers (pre-allocated by caller)
    //   numSamples        — number of samples to process (must not exceed maxBlockSize)
    //
    // Phase 1: pass-through. Sums to mono if stereo input, copies to outL and outR.
    void process(const float* const* inputs, int numInputChannels,
                 float* outL, float* outR, int numSamples);

    // Reset all internal state (delay lines, filter states, etc.).
    // Phase 1: no-op.
    void reset();

private:
    EngineParams currentParams;
    std::vector<float> monoBuffer;  // pre-allocated in prepare() to maxBlockSize
    double sampleRate   = 44100.0;
    int    maxBlockSize = 512;
};

} // namespace xyzpan
