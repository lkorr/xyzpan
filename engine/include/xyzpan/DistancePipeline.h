#pragma once
#include "xyzpan/dsp/FractionalDelayLine.h"
#include "xyzpan/dsp/OnePoleLP.h"
#include "xyzpan/dsp/OnePoleSmooth.h"

namespace xyzpan {

// Per-source distance DSP state. When stereo width > 0, L and R input channels
// each get independent distance processing (gain attenuation, delay+doppler,
// air absorption) based on their own node positions.
struct EngineParams;

struct DistancePipeline {
    dsp::FractionalDelayLine dopplerDelay;        // mono doppler delay line
    dsp::OnePoleLP airLPF_L, airLPF_R;            // air absorption stage 1 (stereo, post-binaural)
    dsp::OnePoleLP airLPF2_L, airLPF2_R;          // air absorption stage 2 (stereo, post-binaural)
    dsp::OnePoleLP dopplerPostAA;                  // post-delay anti-alias LP (mono)
    dsp::OnePoleLP dopplerPreAA;                   // pre-delay anti-alias LP (mono)
    dsp::OnePoleSmooth distGainSmooth;
    dsp::OnePoleSmooth distDelaySmooth;
    float prevDelaySamp = 2.0f;  // rate limiter state for doppler
    void prepare(float sr);
    void reset();

    // Mono doppler delay (applied before comb/binaural).
    float processDoppler(float input, float rawNodeDistFrac, float sr,
                         bool effectiveDoppler, const EngineParams& params);

    // Distance processing for a single source node (gain + air absorption).
    // distGainMaxDb = 20*log10(distGainMax), pre-computed per block.
    struct DistResult { float left; float right; float distFrac; };
    DistResult processDistance(float dL, float dR, float nodeX, float nodeY, float nodeZ,
                               float sr, float distGainMaxDb, const EngineParams& params);
};

} // namespace xyzpan
