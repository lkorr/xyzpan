#include "xyzpan/BinauralPipeline.h"

namespace xyzpan {

void BinauralPipeline::prepare(float sr, int delayCap, float combMaxMs) {
    delayL.prepare(delayCap);
    delayR.prepare(delayCap);
    shadowL.setCoefficients(kHeadShadowFullOpenHz, sr);
    shadowR.setCoefficients(kHeadShadowFullOpenHz, sr);
    rearSvfL.setCoefficients(kRearShadowFullOpenHz, sr);
    rearSvfR.setCoefficients(kRearShadowFullOpenHz, sr);
    itdSmooth.prepare(kDefaultSmoothMs_ITD, sr);
    shadowCutoffSmooth.prepare(kDefaultSmoothMs_Filter, sr);
    ildGainSmooth.prepare(kDefaultSmoothMs_Gain, sr);
    rearCutoffSmooth.prepare(kDefaultSmoothMs_Filter, sr);

    int combCap = static_cast<int>(combMaxMs * 0.001f * sr) + 4;
    for (int i = 0; i < kMaxCombFilters; ++i) {
        combBank[i].prepare(combCap);
        combBank[i].setDelay(static_cast<int>(kCombDefaultDelays_ms[i] * 0.001f * sr));
        combBank[i].setFeedback(kCombDefaultFeedback[i]);
    }
    combWetSmooth.prepare(kDefaultSmoothMs_Gain, sr);

    reset();
}

void BinauralPipeline::reset() {
    delayL.reset();
    delayR.reset();
    shadowL.reset();
    shadowR.reset();
    rearSvfL.reset();
    rearSvfR.reset();
    itdSmooth.reset(0.0f);
    shadowCutoffSmooth.reset(kHeadShadowFullOpenHz);
    ildGainSmooth.reset(1.0f);
    rearCutoffSmooth.reset(kRearShadowFullOpenHz);
    nearFieldLF_L.reset();
    nearFieldLF_R.reset();
    for (auto& c : combBank) c.reset();
    combWetSmooth.reset(0.0f);
    presenceShelf.reset();
    earCanalPeak.reset();
    pinnaP1.reset();
    pinnaNotch.reset();
    pinnaNotch2.reset();
    pinnaShelf.reset();
    shoulderPeak.reset();
    conchaNotch.reset();
    upperPinna.reset();
    tragusNotch.reset();
}

} // namespace xyzpan
