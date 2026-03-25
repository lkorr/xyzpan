#include "xyzpan/ERPipeline.h"
#include "xyzpan/Constants.h"

namespace xyzpan {

void ERPipeline::prepare(float sr) {
    int erDelayCap = static_cast<int>(kERMaxDelayMs * 0.001f * 192000.0f) + 8;
    sharedDelay.prepare(erDelayCap);
    sharedDelay.reset();

    int itdCap = static_cast<int>(kMaxITDUpperBound_ms * 0.001f * sr) + 8;
    for (auto& er : reflections) {
        er.wallAbsorption.setCoefficients(kERDampingLPMaxHz, sr);
        er.wallAbsorption.reset();
        er.delaySmooth.prepare(kDefaultSmoothMs_ITD, sr);
        er.delaySmooth.reset(2.0f);
        er.gainSmooth.prepare(kDefaultSmoothMs_Gain, sr);
        er.gainSmooth.reset(0.0f);
        er.itdDelayL.prepare(itdCap);
        er.itdDelayR.prepare(itdCap);
        er.itdDelayL.reset();
        er.itdDelayR.reset();
        er.shadowL.setCoefficients(kHeadShadowFullOpenHz, sr);
        er.shadowR.setCoefficients(kHeadShadowFullOpenHz, sr);
        er.shadowL.reset();
        er.shadowR.reset();
        er.itdSmooth.prepare(kDefaultSmoothMs_ITD, sr);
        er.itdSmooth.reset(0.0f);
        er.ildSmooth.prepare(kDefaultSmoothMs_Gain, sr);
        er.ildSmooth.reset(1.0f);
    }
}

void ERPipeline::reset() {
    sharedDelay.reset();
    for (auto& er : reflections) {
        er.wallAbsorption.reset();
        er.delaySmooth.reset(2.0f);
        er.gainSmooth.reset(0.0f);
        er.itdDelayL.reset();
        er.itdDelayR.reset();
        er.shadowL.reset();
        er.shadowR.reset();
        er.itdSmooth.reset(0.0f);
        er.ildSmooth.reset(1.0f);
    }
}

} // namespace xyzpan
