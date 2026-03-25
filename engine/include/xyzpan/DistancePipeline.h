#pragma once
#include "xyzpan/dsp/FractionalDelayLine.h"
#include "xyzpan/dsp/OnePoleLP.h"
#include "xyzpan/dsp/OnePoleSmooth.h"

namespace xyzpan {

// Per-source distance DSP state. When stereo width > 0, L and R input channels
// each get independent distance processing (gain attenuation, delay+doppler,
// air absorption) based on their own node positions.
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
};

} // namespace xyzpan
