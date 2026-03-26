#pragma once
#include "xyzpan/Constants.h"
#include "xyzpan/dsp/FractionalDelayLine.h"
#include "xyzpan/dsp/OnePoleLP.h"
#include "xyzpan/dsp/OnePoleSmooth.h"
#include "xyzpan/dsp/SVFLowPass.h"
#include <array>

namespace xyzpan {

// Per-reflection DSP state for the early reflections image source method.
// 6 instances (one per wall face of the virtual cube: ±X, ±Y, ±Z).
struct EarlyReflection {
    dsp::OnePoleLP wallAbsorption;          // Wall absorption LPF
    dsp::OnePoleSmooth delaySmooth;         // Smooth delay transitions
    dsp::OnePoleSmooth gainSmooth;          // Smooth gain transitions
    // Simplified binaural (ITD + ILD + head shadow per ear)
    dsp::FractionalDelayLine itdDelayL, itdDelayR;
    dsp::SVFLowPass shadowL, shadowR;
    dsp::OnePoleSmooth itdSmooth;
    dsp::OnePoleSmooth ildSmooth;
};

// Per-node early reflections DSP state. When stereo width > 0, each node
// gets its own set of 6 reflections with independent image source positions.
struct EngineParams;

struct ERPipeline {
    std::array<EarlyReflection, kNumER> reflections;
    dsp::FractionalDelayLine sharedDelay;
    void prepare(float sr);
    void reset();

    // Process early reflections for a single source node.
    struct ERResult { float directL, directR, reverbL, reverbR; };
    ERResult processSample(float input, float nodeX, float nodeY, float nodeZ,
                           float distGainTarget, float sr,
                           float dampCutoff, float roomHalf,
                           float ildGainBase, bool rotated,
                           float cosY, float sinY, float cosP, float sinP,
                           float cosR, float sinR,
                           const EngineParams& params);
};

} // namespace xyzpan
