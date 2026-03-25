#pragma once
#include "xyzpan/dsp/FractionalDelayLine.h"
#include "xyzpan/dsp/OnePoleLP.h"
#include "xyzpan/dsp/OnePoleSmooth.h"

namespace xyzpan {

// Per-node floor bounce DSP state. When stereo width > 0, L and R nodes each
// get independent floor bounce with separate L/R absorption LPFs.
struct FloorPipeline {
    dsp::FractionalDelayLine delayL, delayR;
    dsp::OnePoleLP lpfL, lpfR;               // separate L/R absorption
    dsp::OnePoleSmooth gainSmooth, delaySmooth;
    void prepare(float sr);
    void reset();
};

} // namespace xyzpan
