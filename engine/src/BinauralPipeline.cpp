#include "xyzpan/BinauralPipeline.h"
#include "xyzpan/Types.h"
#include <algorithm>
#include <cmath>

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

BinauralPipeline::BinauralResult BinauralPipeline::processSample(
    float inputSample, float nodeX, float nodeY, float nodeZ,
    float sr, float binBlend,
    float ildGainBase, float hardpanGainBase,
    const EngineParams& params) {
    // Per-node position-derived values
    const float nodeHorizMag = std::sqrt(nodeX * nodeX + nodeY * nodeY);
    const float nodeAzimuthFactor = (nodeHorizMag > 1e-7f)
        ? nodeX / nodeHorizMag : 0.0f;

    // Per-node mono cylinder
    const float cylRadius = params.vertMonoCylinderRadius;
    const float nodeT = std::clamp(nodeHorizMag / (cylRadius + 1e-7f), 0.0f, 1.0f);
    const float nodeMonoBlend = 1.0f - nodeT * nodeT * (3.0f - 2.0f * nodeT);
    const float nodeEffAzimuth = nodeAzimuthFactor * (1.0f - nodeMonoBlend);

    // Per-node rear factor
    const float nodeRearFactor = (nodeHorizMag > 1e-7f)
        ? (-nodeY / nodeHorizMag) : nodeY;

    // Per-node binaural cue targets (signed azimuth: +right, -left)
    const float nodeAbsEffAzimuth = std::abs(nodeEffAzimuth);
    const float nodeItdTarget = params.maxITD_ms * nodeEffAzimuth * sr / 1000.0f;
    const float nodeShadowRange = params.headShadowMinHz - params.headShadowFullOpenHz;
    const float nodeShadowCutTargetL = params.headShadowFullOpenHz + nodeShadowRange * std::max(0.0f,  nodeEffAzimuth);
    const float nodeShadowCutTargetR = params.headShadowFullOpenHz + nodeShadowRange * std::max(0.0f, -nodeEffAzimuth);
    const float nodeIldTarget = 1.0f - (1.0f - ildGainBase) * nodeAbsEffAzimuth;
    const float nodeRearAmount = std::max(0.0f, nodeRearFactor);
    const float nodeRearCutTarget = kRearShadowFullOpenHz
        + (params.rearShadowMinHz - kRearShadowFullOpenHz) * nodeRearAmount;
    const float nodeCombWetTarget = params.combWetMax * std::max(0.0f, nodeRearFactor);

    // Comb bank — smoother always runs; bypass skips audible effect
    const float combWet = combWetSmooth.process(nodeCombWetTarget);
    float combSig = inputSample;
    for (int c = 0; c < kMaxCombFilters; ++c)
        combSig = combBank[c].process(combSig);
    const float depthOut = params.bypassComb
        ? inputSample
        : inputSample * (1.0f - combWet) + combSig * combWet;

    // Mono EQ chain — bypass skips biquad .process() calls
    float monoEQ;
    if (params.bypassPinnaEQ) {
        monoEQ = depthOut;
    } else {
        monoEQ = presenceShelf.process(depthOut);
        monoEQ = earCanalPeak.process(monoEQ);
        if (!params.bypassExpandedPinna) {
            monoEQ = shoulderPeak.process(monoEQ);
            monoEQ = conchaNotch.process(monoEQ);
        }
        monoEQ = pinnaP1.process(monoEQ);
        monoEQ = pinnaNotch.process(monoEQ);
        monoEQ = pinnaNotch2.process(monoEQ);
        if (!params.bypassExpandedPinna) {
            monoEQ = upperPinna.process(monoEQ);
            monoEQ = tragusNotch.process(monoEQ);
        }
        monoEQ = pinnaShelf.process(monoEQ);
    }

    // Smooth binaural parameters — smoothers always run
    const float itdSamples   = itdSmooth.process(nodeItdTarget);
    shadowCutoffSmooth.process(nodeShadowCutTargetL);
    const float ildGain      = ildGainSmooth.process(nodeIldTarget);
    rearCutoffSmooth.process(nodeRearCutTarget);

    // Update SVF coefficients per-sample (TPT topology is modulation-safe)
    shadowL.setCoefficients(nodeShadowCutTargetL, sr);
    shadowR.setCoefficients(nodeShadowCutTargetR, sr);
    rearSvfL.setCoefficients(nodeRearCutTarget, sr);
    rearSvfR.setCoefficients(nodeRearCutTarget, sr);

    // Push into delay lines — always push to keep delay state consistent
    delayL.push(monoEQ);
    delayR.push(monoEQ);

    constexpr float kMinDelay = 2.0f;
    float dL, dR;
    if (params.bypassITD) {
        dL = delayL.read(kMinDelay);
        dR = delayR.read(kMinDelay);
    } else {
        const float itdL = std::max(0.0f,  itdSamples) * binBlend;
        const float itdR = std::max(0.0f, -itdSamples) * binBlend;
        dL = delayL.read(kMinDelay + itdL);
        dR = delayR.read(kMinDelay + itdR);
    }

    if (!std::isfinite(dL)) dL = 0.0f;
    if (!std::isfinite(dR)) dR = 0.0f;

    // Hardpan — opposite-ear attenuation when binaural is OFF
    if (binBlend < 0.999f) {
        const float hpAmount = 1.0f - binBlend;
        const float hpGain = 1.0f - (1.0f - hardpanGainBase) * nodeAbsEffAzimuth;
        const float appliedGain = 1.0f - hpAmount * (1.0f - hpGain);
        if (nodeEffAzimuth > 0.0f)      dL *= appliedGain;
        else if (nodeEffAzimuth < 0.0f) dR *= appliedGain;
    }

    // ILD — smooth crossfade around ITD zero to avoid gain discontinuity
    if (!params.bypassILD) {
        const float ildAtten = 1.0f - ildGain;
        const float blend = std::clamp(itdSamples / kILDCrossfadeWidth, -1.0f, 1.0f);
        dL *= 1.0f - ildAtten * std::max(0.0f,  blend);
        dR *= 1.0f - ildAtten * std::max(0.0f, -blend);
    }

    // Near-field ILD — coefficients pre-set per-block, only .process() here
    if (!params.bypassNearField) {
        dL = nearFieldLF_L.process(dL);
        dR = nearFieldLF_R.process(dR);
    }

    // Head shadow — coefficients updated per-sample above
    if (!params.bypassHeadShadow) {
        dL = shadowL.process(dL);
        dR = shadowR.process(dR);
    }

    // Rear shadow — coefficients updated per-sample above
    if (!params.bypassRearShadow) {
        dL = rearSvfL.process(dL);
        dR = rearSvfR.process(dR);
    }

    return { dL, dR };
}

} // namespace xyzpan
