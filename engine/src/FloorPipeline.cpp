#include "xyzpan/FloorPipeline.h"
#include "xyzpan/Constants.h"

namespace xyzpan {

void FloorPipeline::prepare(float sr) {
    int floorCap = static_cast<int>(kFloorDelayMaxMs * 0.001f * sr) + 8;
    delayL.prepare(floorCap);
    delayR.prepare(floorCap);
    delayL.reset();
    delayR.reset();
    lpfL.setCoefficients(kFloorAbsHz, sr);
    lpfR.setCoefficients(kFloorAbsHz, sr);
    lpfL.reset();
    lpfR.reset();
    gainSmooth.prepare(kDefaultSmoothMs_Gain, sr);
    gainSmooth.reset(0.0f);
    delaySmooth.prepare(kDefaultSmoothMs_Gain, sr);
    delaySmooth.reset(2.0f);
}

void FloorPipeline::reset() {
    delayL.reset();
    delayR.reset();
    lpfL.reset();
    lpfR.reset();
    gainSmooth.reset(0.0f);
    delaySmooth.reset(2.0f);
}

} // namespace xyzpan
