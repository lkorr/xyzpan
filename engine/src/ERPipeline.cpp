#include "xyzpan/ERPipeline.h"
#include "xyzpan/Types.h"
#include "xyzpan/Constants.h"
#include <algorithm>
#include <cmath>

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

ERPipeline::ERResult ERPipeline::processSample(
    float input, float nodeX, float nodeY, float nodeZ,
    float distGainTarget, float sr,
    float dampCutoff, float roomHalf,
    float ildGainBase, bool rotated,
    float cY, float sY, float cP, float sP,
    float cR, float sR,
    const EngineParams& params) {
    sharedDelay.push(input);

    float erDirectL = 0.0f, erDirectR = 0.0f;
    float erRevL = 0.0f, erRevR = 0.0f;

    constexpr int wallAxis[6]  = { 0, 0, 1, 1, 2, 2 };
    constexpr float wallSign[6] = { 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f };

    const float rawDist = std::sqrt(nodeX * nodeX + nodeY * nodeY + nodeZ * nodeZ);
    const float directDist = std::max(rawDist, kMinDistance);

    for (int w = 0; w < kNumER; ++w) {
        auto& er = reflections[w];

        float imgX = nodeX, imgY = nodeY, imgZ = nodeZ;
        const int axis = wallAxis[w];
        const float sign = wallSign[w];
        if (axis == 0)      imgX = 2.0f * sign - nodeX;
        else if (axis == 1) imgY = 2.0f * sign - nodeY;
        else                imgZ = 2.0f * sign - nodeZ;

        const float pathNorm = std::sqrt(imgX * imgX + imgY * imgY + imgZ * imgZ);
        const float pathMeters = pathNorm * roomHalf;
        const float delaySamp = std::max(2.0f, pathMeters / kSpeedOfSound * sr);

        er.wallAbsorption.setCoefficients(dampCutoff, sr);

        const float reflDist = std::max(pathNorm, kMinDistance);
        const float ratioPenalty = directDist / reflDist;
        const float gainTarget = std::clamp(distGainTarget * ratioPenalty, 0.0f, params.distGainMax);

        const float smoothDelay = std::max(2.0f, er.delaySmooth.process(delaySamp));
        const float smoothGain = er.gainSmooth.process(gainTarget);

        float reflected = sharedDelay.read(smoothDelay);
        reflected = er.wallAbsorption.process(reflected);
        reflected *= smoothGain;

        // Simplified binaural from image source azimuth (listener-relative)
        float lrImgX = imgX, lrImgY = imgY;
        if (rotated) {
            const float rx = imgX * cY + imgY * sY;
            const float ry = -imgX * sY + imgY * cY;
            lrImgX = rx;
            lrImgY = ry * cP + imgZ * sP;
            const float rrz = -ry * sP + imgZ * cP;  // Z after yaw+pitch
            // Roll around forward axis (Y in engine coords)
            lrImgX = lrImgX * cR + rrz * sR;
            // lrImgY unchanged by roll (forward axis)
        }
        const float imgHorizMag = std::sqrt(lrImgX * lrImgX + lrImgY * lrImgY);
        const float imgAzFactor = (imgHorizMag > 1e-7f) ? lrImgX / imgHorizMag : 0.0f;

        const float erItdTarget = params.maxITD_ms * imgAzFactor * sr / 1000.0f;
        const float erItdSamples = er.itdSmooth.process(erItdTarget);

        const float absImgAz = std::abs(imgAzFactor);
        const float erIldTarget = 1.0f - (1.0f - ildGainBase) * absImgAz;
        const float erIldGain = er.ildSmooth.process(erIldTarget);

        const float erShadowRange = params.headShadowMinHz - params.headShadowFullOpenHz;
        er.shadowL.setCoefficients(
            params.headShadowFullOpenHz + erShadowRange * std::max(0.0f,  imgAzFactor), sr);
        er.shadowR.setCoefficients(
            params.headShadowFullOpenHz + erShadowRange * std::max(0.0f, -imgAzFactor), sr);

        er.itdDelayL.push(reflected);
        er.itdDelayR.push(reflected);

        constexpr float kERMinDelay = 2.0f;
        float erL = er.itdDelayL.read(kERMinDelay + std::max(0.0f,  erItdSamples));
        float erR = er.itdDelayR.read(kERMinDelay + std::max(0.0f, -erItdSamples));

        const float erIldAtten = 1.0f - erIldGain;
        const float erBlend = std::clamp(erItdSamples / kILDCrossfadeWidth, -1.0f, 1.0f);
        erL *= 1.0f - erIldAtten * std::max(0.0f,  erBlend);
        erR *= 1.0f - erIldAtten * std::max(0.0f, -erBlend);

        erL = er.shadowL.process(erL);
        erR = er.shadowR.process(erR);

        erDirectL += erL;
        erDirectR += erR;
        erRevL += erL;
        erRevR += erR;
    }

    return { erDirectL, erDirectR, erRevL, erRevR };
}

} // namespace xyzpan
