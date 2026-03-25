#include "xyzpan/DistancePipeline.h"
#include "xyzpan/Constants.h"
#include <algorithm>
#include <cmath>

namespace xyzpan {

void DistancePipeline::prepare(float sr) {
    int distDelayCap = static_cast<int>(kDistDelayMaxMs * 0.001f * 192000.0f) + 8;
    dopplerDelay.prepare(distDelayCap);
    dopplerDelay.reset();
    const float aaCutoff = std::min(kDopplerAAMaxHz, sr * 0.45f);
    dopplerPostAA.setCoefficients(aaCutoff, sr);
    dopplerPostAA.reset();
    dopplerPreAA.setCoefficients(aaCutoff, sr);
    dopplerPreAA.reset();
    airLPF_L.setCoefficients(kAirAbsMaxHz, sr);
    airLPF_R.setCoefficients(kAirAbsMaxHz, sr);
    airLPF_L.reset();
    airLPF_R.reset();
    airLPF2_L.setCoefficients(kAirAbs2MaxHz, sr);
    airLPF2_R.setCoefficients(kAirAbs2MaxHz, sr);
    airLPF2_L.reset();
    airLPF2_R.reset();
    distDelaySmooth.prepare(kDistSmoothMs, sr);
    distDelaySmooth.reset(2.0f);
    distGainSmooth.prepare(kDefaultSmoothMs_Gain, sr);
    distGainSmooth.reset(1.0f);
}

void DistancePipeline::reset() {
    dopplerDelay.reset();
    dopplerPostAA.reset();
    dopplerPreAA.reset();
    airLPF_L.reset();
    airLPF_R.reset();
    airLPF2_L.reset();
    airLPF2_R.reset();
    distDelaySmooth.reset(2.0f);
    distGainSmooth.reset(1.0f);
    prevDelaySamp = 2.0f;
}

} // namespace xyzpan
