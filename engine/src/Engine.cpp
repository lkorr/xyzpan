#include "xyzpan/Engine.h"
#include "xyzpan/Coordinates.h"
#include "xyzpan/Constants.h"
#include "xyzpan/dsp/SineLUT.h"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace xyzpan {

// ============================================================================
// BinauralPipeline — per-source DSP state
// ============================================================================

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
}

// ============================================================================
// DistancePipeline — per-source distance DSP state
// ============================================================================

void DistancePipeline::prepare(float sr) {
    int distDelayCap = static_cast<int>(kDistDelayMaxMs * 0.001f * 192000.0f) + 8;
    distDelayL.prepare(distDelayCap);
    distDelayR.prepare(distDelayCap);
    distDelayL.reset();
    distDelayR.reset();
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
    lastDistDelaySamp = 2.0f;
}

void DistancePipeline::reset() {
    distDelayL.reset();
    distDelayR.reset();
    airLPF_L.reset();
    airLPF_R.reset();
    airLPF2_L.reset();
    airLPF2_R.reset();
    distDelaySmooth.reset(2.0f);
    distGainSmooth.reset(1.0f);
    lastDistDelaySamp = 2.0f;
}

// ============================================================================
// prepare()
// ============================================================================

void XYZPanEngine::prepare(double inSampleRate, int inMaxBlockSize) {
    sampleRate   = inSampleRate;
    maxBlockSize = inMaxBlockSize;

    // Pre-allocate mono mixing buffer.
    monoBuffer.resize(static_cast<size_t>(inMaxBlockSize), 0.0f);

    const float sr = static_cast<float>(inSampleRate);

    // Allocate delay lines for maximum ITD upper bound.
    // kMaxITDUpperBound_ms gives headroom for the dev panel creative exaggeration range.
    int delayCap = static_cast<int>(kMaxITDUpperBound_ms * 0.001f * sr) + 8;
    delayL_.prepare(delayCap);
    delayR_.prepare(delayCap);

    // Set initial SVF coefficients — all wide open (inaudible) at start.
    shadowL_.setCoefficients(kHeadShadowFullOpenHz, sr);
    shadowR_.setCoefficients(kHeadShadowFullOpenHz, sr);
    rearSvfL_.setCoefficients(kRearShadowFullOpenHz, sr);
    rearSvfR_.setCoefficients(kRearShadowFullOpenHz, sr);

    // Prepare smoothers with default time constants.
    itdSmooth_.prepare(kDefaultSmoothMs_ITD, sr);
    shadowCutoffSmooth_.prepare(kDefaultSmoothMs_Filter, sr);
    ildGainSmooth_.prepare(kDefaultSmoothMs_Gain, sr);
    rearCutoffSmooth_.prepare(kDefaultSmoothMs_Filter, sr);

    // Zero all delay and filter state.
    delayL_.reset();
    delayR_.reset();
    shadowL_.reset();
    shadowR_.reset();
    rearSvfL_.reset();
    rearSvfR_.reset();

    // Initialize smoothers to neutral values:
    //   ITD = 0 (no delay), cutoffs = wide open, gain = unity
    itdSmooth_.reset(0.0f);
    shadowCutoffSmooth_.reset(kHeadShadowFullOpenHz);
    ildGainSmooth_.reset(1.0f);
    rearCutoffSmooth_.reset(kRearShadowFullOpenHz);

    // Sync tracking members.
    lastSmoothMs_ITD_    = kDefaultSmoothMs_ITD;
    lastSmoothMs_Filter_ = kDefaultSmoothMs_Filter;
    lastSmoothMs_Gain_   = kDefaultSmoothMs_Gain;

    // -------------------------------------------------------------------------
    // Phase 3: Comb bank
    // -------------------------------------------------------------------------
    for (int i = 0; i < kMaxCombFilters; ++i) {
        int combCap = static_cast<int>(kCombMaxDelay_ms * 0.001f * sr) + 4;
        combBank_[i].prepare(combCap);
        combBank_[i].setDelay(static_cast<int>(kCombDefaultDelays_ms[i] * 0.001f * sr));
        combBank_[i].setFeedback(kCombDefaultFeedback[i]);
    }
    combWetSmooth_.prepare(kDefaultSmoothMs_Gain, sr);
    combWetSmooth_.reset(0.0f);

    // Phase 3: Pinna notch + shelf (biquad — no prepare needed, just reset)
    pinnaNotch_.reset();
    pinnaNotch2_.reset();
    pinnaP1_.reset();
    presenceShelf_.reset();
    earCanalPeak_.reset();
    pinnaShelf_.reset();

    // -------------------------------------------------------------------------
    // Phase 3: Chest bounce
    // -------------------------------------------------------------------------
    for (auto& hp : chestHPF_) {
        hp.setType(dsp::SVFType::HP);
        hp.setCoefficients(700.0f, sr);
        hp.reset();
    }
    chestLP_.setCoefficients(1000.0f, sr);
    chestLP_.reset();
    {
        int chestCap = static_cast<int>(kChestDelayMaxMs * 0.001f * sr) + 8;
        chestDelay_.prepare(chestCap);
        chestDelay_.reset();
    }
    chestGainSmooth_.prepare(kDefaultSmoothMs_Gain, sr);
    chestGainSmooth_.reset(0.0f);

    // -------------------------------------------------------------------------
    // Phase 3: Floor bounce
    // -------------------------------------------------------------------------
    {
        int floorCap = static_cast<int>(kFloorDelayMaxMs * 0.001f * sr) + 8;
        floorDelayL_.prepare(floorCap);
        floorDelayR_.prepare(floorCap);
        floorDelayL_.reset();
        floorDelayR_.reset();
    }
    floorLPF_.setCoefficients(kFloorAbsHz, sr);
    floorLPF_.reset();
    floorGainSmooth_.prepare(kDefaultSmoothMs_Gain, sr);
    floorGainSmooth_.reset(0.0f);

    // -------------------------------------------------------------------------
    // Phase 4: Distance Processing
    // -------------------------------------------------------------------------
    {
        // Size delay lines for kDistDelayMaxMs at 192kHz (worst case sample rate).
        int distDelayCap = static_cast<int>(kDistDelayMaxMs * 0.001f * 192000.0f) + 8;
        distDelayL_.prepare(distDelayCap);
        distDelayR_.prepare(distDelayCap);
        distDelayL_.reset();
        distDelayR_.reset();
    }
    airLPF_L_.setCoefficients(kAirAbsMaxHz, sr);
    airLPF_R_.setCoefficients(kAirAbsMaxHz, sr);
    airLPF_L_.reset();
    airLPF_R_.reset();
    airLPF2_L_.setCoefficients(kAirAbs2MaxHz, sr);
    airLPF2_R_.setCoefficients(kAirAbs2MaxHz, sr);
    airLPF2_L_.reset();
    airLPF2_R_.reset();
    nearFieldLF_L_.reset();
    nearFieldLF_R_.reset();
    distDelaySmooth_.prepare(kDistSmoothMs, sr);
    distDelaySmooth_.reset(2.0f);
    distGainSmooth_.prepare(kDefaultSmoothMs_Gain, sr);
    distGainSmooth_.reset(1.0f);
    lastDistSmoothMs_    = kDistSmoothMs;
    lastDistDelaySamp_   = 2.0f;

    // -------------------------------------------------------------------------
    // Phase 5: Reverb
    // -------------------------------------------------------------------------
    reverb_.prepare(inSampleRate, inMaxBlockSize);
    reverb_.setSize(kVerbDefaultSize);
    reverb_.setDecay(kVerbDefaultDecay);
    reverb_.setDamping(kVerbDefaultDamping);
    reverb_.setWetDry(1.0f);
    reverb_.reset();
    verbWetSmooth_.prepare(kDefaultSmoothMs_Gain, sr);
    verbWetSmooth_.reset(kVerbDefaultWet);

    // Aux reverb send pre-delay lines
    {
        int auxCap = static_cast<int>(kVerbPreDelayMaxMs * 0.001f * 192000.0f) + 8;
        auxPreDelayL_.prepare(auxCap);
        auxPreDelayR_.prepare(auxCap);
        auxPreDelayL_.reset();
        auxPreDelayR_.reset();
    }
    auxGainSmooth_.prepare(kDefaultSmoothMs_Gain, sr);
    auxGainSmooth_.reset(1.0f);

    // Phase 5: LFO
    lfoX_.prepare(inSampleRate);
    lfoY_.prepare(inSampleRate);
    lfoZ_.prepare(inSampleRate);
    lfoDepthXSmooth_.prepare(kDefaultSmoothMs_Gain, sr);
    lfoDepthYSmooth_.prepare(kDefaultSmoothMs_Gain, sr);
    lfoDepthZSmooth_.prepare(kDefaultSmoothMs_Gain, sr);
    lfoDepthXSmooth_.reset(0.0f);
    lfoDepthYSmooth_.reset(0.0f);
    lfoDepthZSmooth_.reset(0.0f);

    // -------------------------------------------------------------------------
    // Stereo source node splitting — R channel pipeline + orbit LFOs
    // -------------------------------------------------------------------------
    srcR_.prepare(sr, delayCap, kCombMaxDelay_ms);
    distR_.prepare(sr);

    orbitLfoXY_.prepare(inSampleRate);
    orbitLfoXZ_.prepare(inSampleRate);
    orbitLfoYZ_.prepare(inSampleRate);
    orbitDepthXYSmooth_.prepare(kDefaultSmoothMs_Gain, sr);
    orbitDepthXZSmooth_.prepare(kDefaultSmoothMs_Gain, sr);
    orbitDepthYZSmooth_.prepare(kDefaultSmoothMs_Gain, sr);
    orbitDepthXYSmooth_.reset(0.0f);
    orbitDepthXZSmooth_.reset(0.0f);
    orbitDepthYZSmooth_.reset(0.0f);
    stereoWidthSmooth_.prepare(kDefaultSmoothMs_Gain, sr);
    stereoWidthSmooth_.reset(0.0f);

    // Angular smoothers for circular phase/offset knobs (5ms time constant)
    angularSmA_ = std::exp(-6.28318530f / (kDefaultSmoothMs_Gain * 0.001f * sr));
    phaseSmCos_ = 1.0f; phaseSmSin_ = 0.0f;   // angle = 0
    offsetSmCos_ = 1.0f; offsetSmSin_ = 0.0f;  // angle = 0

    // Dev tool: test tone pulse LFO + noise RNG
    pulseLFO_.prepare(inSampleRate);
    pulseLFO_.waveform = dsp::LFOWaveform::Square;
    pulseLFO_.reset(0.0f);
    noiseRng_.seed(12345u);
}

// ============================================================================
// setParams()
// ============================================================================

void XYZPanEngine::setParams(const EngineParams& params) {
    currentParams = params;
}

// ============================================================================
// processBinauralForSource() — binaural split for a single source node
// Coefficients must be pre-set per-block before calling this function.
// Only .process() calls are made here — no setCoefficients.
// ============================================================================

XYZPanEngine::BinauralResult XYZPanEngine::processBinauralForSource(
    float inputSample,
    float nodeX, float nodeY, float nodeZ,
    float sr,
    dsp::FractionalDelayLine& dl, dsp::FractionalDelayLine& dr,
    dsp::SVFLowPass& shL, dsp::SVFLowPass& shR,
    dsp::SVFLowPass& rSvfL, dsp::SVFLowPass& rSvfR,
    dsp::OnePoleSmooth& itdSm, dsp::OnePoleSmooth& shCutSm,
    dsp::OnePoleSmooth& ildSm, dsp::OnePoleSmooth& rearCutSm,
    dsp::BiquadFilter& nfL, dsp::BiquadFilter& nfR,
    std::array<dsp::FeedbackCombFilter, kMaxCombFilters>& combs,
    dsp::OnePoleSmooth& combWetSm,
    dsp::BiquadFilter& presShelf, dsp::BiquadFilter& earCanal,
    dsp::BiquadFilter& pP1, dsp::BiquadFilter& pN1,
    dsp::BiquadFilter& pN2, dsp::BiquadFilter& pShelf
) {
    // Per-node position-derived values
    const float nodeHorizMag = std::sqrt(nodeX * nodeX + nodeY * nodeY);
    const float nodeAzimuthFactor = (nodeHorizMag > 1e-7f)
        ? nodeX / nodeHorizMag : 0.0f;

    // Per-node mono cylinder
    const float cylRadius = currentParams.vertMonoCylinderRadius;
    const float nodeT = std::clamp(nodeHorizMag / (cylRadius + 1e-7f), 0.0f, 1.0f);
    const float nodeMonoBlend = 1.0f - nodeT * nodeT * (3.0f - 2.0f * nodeT);
    const float nodeEffAzimuth = nodeAzimuthFactor * (1.0f - nodeMonoBlend);

    // Per-node rear factor
    const float nodeRearFactor = (nodeHorizMag > 1e-7f)
        ? (-nodeY / nodeHorizMag) : nodeY;

    // Per-node binaural cue targets (signed azimuth: +right, -left)
    const float nodeAbsEffAzimuth = std::abs(nodeEffAzimuth);
    const float nodeItdTarget = currentParams.maxITD_ms * nodeEffAzimuth * sr / 1000.0f;
    const float nodeShadowCutTarget = kHeadShadowFullOpenHz
        + (currentParams.headShadowMinHz - kHeadShadowFullOpenHz) * nodeAbsEffAzimuth;
    // Pre-computed per-block: ildGainBase = std::pow(10.0f, -ildMaxDb/20.0f)
    const float nodeIldTarget = 1.0f - (1.0f - ildGainBase_)
        * nodeAbsEffAzimuth;
    const float nodeRearAmount = std::max(0.0f, nodeRearFactor);
    const float nodeRearCutTarget = kRearShadowFullOpenHz
        + (currentParams.rearShadowMinHz - kRearShadowFullOpenHz) * nodeRearAmount;
    const float nodeCombWetTarget = currentParams.combWetMax * std::max(0.0f, nodeRearFactor);

    // NOTE: setCoefficients on presShelf/earCanal/pP1/pN1/pN2/pShelf are called
    // ONCE per block in the per-block preamble, not here. This function only calls .process().

    // Comb bank
    const float combWet = combWetSm.process(nodeCombWetTarget);
    float combSig = inputSample;
    for (int c = 0; c < kMaxCombFilters; ++c)
        combSig = combs[c].process(combSig);
    const float depthOut = inputSample * (1.0f - combWet) + combSig * combWet;

    // Mono EQ chain
    float monoEQ = presShelf.process(depthOut);
    monoEQ = earCanal.process(monoEQ);
    monoEQ = pP1.process(monoEQ);
    monoEQ = pN1.process(monoEQ);
    monoEQ = pN2.process(monoEQ);
    monoEQ = pShelf.process(monoEQ);

    // Smooth binaural parameters
    const float itdSamples   = itdSm.process(nodeItdTarget);
    const float shadowCutoff = shCutSm.process(nodeShadowCutTarget);
    const float ildGain      = ildSm.process(nodeIldTarget);
    const float rearCutoff   = rearCutSm.process(nodeRearCutTarget);

    // Push into delay lines
    dl.push(monoEQ);
    dr.push(monoEQ);

    constexpr float kMinDelay = 2.0f;
    // Signed ITD: positive itdSamples → source right → delay left ear
    float dL = dl.read(kMinDelay + std::max(0.0f,  itdSamples));
    float dR = dr.read(kMinDelay + std::max(0.0f, -itdSamples));

    if (!std::isfinite(dL)) dL = 0.0f;
    if (!std::isfinite(dR)) dR = 0.0f;

    // ILD — attenuate far ear based on smoothed ITD sign
    if (itdSamples > 0.0f)       dL *= ildGain;
    else if (itdSamples < 0.0f)  dR *= ildGain;

    // Near-field ILD — coefficients pre-set per-block, only .process() here
    dL = nfL.process(dL);
    dR = nfR.process(dR);

    // Head shadow — coefficients pre-set per-block, only .process() here
    dL = shL.process(dL);
    dR = shR.process(dR);

    // Rear shadow — coefficients pre-set per-block, only .process() here
    dL = rSvfL.process(dL);
    dR = rSvfR.process(dR);

    return { dL, dR };
}

// ============================================================================
// processDistanceForNode() — distance processing for a single source node
// ============================================================================

XYZPanEngine::DistanceResult XYZPanEngine::processDistanceForNode(
    float dL, float dR,
    float nodeX, float nodeY, float nodeZ,
    float sr, bool dopplerOn,
    dsp::FractionalDelayLine& ddL, dsp::FractionalDelayLine& ddR,
    dsp::OnePoleLP& aL1, dsp::OnePoleLP& aR1,
    dsp::OnePoleLP& aL2, dsp::OnePoleLP& aR2,
    dsp::OnePoleSmooth& dgSmooth,
    dsp::OnePoleSmooth& ddSmooth,
    float& lastDelaySamp
) {
    const float rawDist = std::sqrt(nodeX * nodeX + nodeY * nodeY + nodeZ * nodeZ);
    const float nodeDist = std::max(rawDist, kMinDistance);
    const float maxRange = std::max(currentParams.sphereRadius - kMinDistance, 0.001f);
    const float nodeDistFrac = std::clamp((nodeDist - kMinDistance) / maxRange, 0.0f, 1.0f);
    const float rawNodeDistFrac = std::clamp(rawDist / kSqrt3, 0.0f, 1.0f);

    // Distance gain (inverse-square law)
    constexpr float kDistRefScale = 0.047546796f;
    const float distRef = currentParams.sphereRadius * kDistRefScale;
    const float distRatio = distRef / nodeDist;
    const float distGainTarget = std::clamp(distRatio * distRatio, 0.0f, kDistGainMax);
    const float distGain = dgSmooth.process(distGainTarget);
    dL *= distGain;
    dR *= distGain;

    // Distance delay + doppler
    const float delayTargetSamples = std::max(2.0f,
        rawNodeDistFrac * currentParams.distDelayMaxMs * 0.001f * sr);

    ddL.push(dL);
    ddR.push(dR);
    if (dopplerOn) {
        const float rawDelay = ddSmooth.process(delayTargetSamples);
        const float deltaDelay = rawDelay - lastDelaySamp;
        const float clampedDelta = std::clamp(deltaDelay, -kDopplerMaxDeltaSamp, kDopplerMaxDeltaSamp);
        lastDelaySamp += clampedDelta;
        const float delaySamp = std::max(2.0f, lastDelaySamp);
        dL = ddL.read(delaySamp);
        dR = ddR.read(delaySamp);
    } else {
        ddSmooth.process(2.0f);
        lastDelaySamp = 2.0f;
        dL = ddL.read(2.0f);
        dR = ddR.read(2.0f);
    }

    // Air absorption — coefficients pre-set per-block, only .process() here
    dL = aL1.process(dL);
    dR = aR1.process(dR);
    dL = aL2.process(dL);
    dR = aR2.process(dR);

    return { dL, dR, nodeDistFrac };
}

// ============================================================================
// process()
// ============================================================================

void XYZPanEngine::process(const float* const* inputs, int numInputChannels,
                            float* outL, float* outR,
                            float* auxL, float* auxR,
                            int numSamples) {
    if (inputs == nullptr || inputs[0] == nullptr || outL == nullptr || outR == nullptr)
        return;

    // -------------------------------------------------------------------------
    // Input preparation — keep separate L/R pointers for stereo mode
    // -------------------------------------------------------------------------
    const float* inputL = inputs[0];
    const float* inputR = (numInputChannels >= 2 && inputs[1] != nullptr) ? inputs[1] : nullptr;

    // When mono (no second input), monoIn = inputL. When stereo, still prepare
    // mono buffer for mono fallback path (width=0).
    if (inputR != nullptr) {
        for (int i = 0; i < numSamples; ++i)
            monoBuffer[static_cast<size_t>(i)] = 0.5f * (inputL[i] + inputR[i]);
    }

    // -------------------------------------------------------------------------
    // Per-block preamble: re-prepare smoothers if time constants changed.
    // -------------------------------------------------------------------------
    const float sr = static_cast<float>(sampleRate);

    if (currentParams.smoothMs_ITD != lastSmoothMs_ITD_) {
        itdSmooth_.prepare(currentParams.smoothMs_ITD, sr);
        lastSmoothMs_ITD_ = currentParams.smoothMs_ITD;
    }
    if (currentParams.smoothMs_Filter != lastSmoothMs_Filter_) {
        shadowCutoffSmooth_.prepare(currentParams.smoothMs_Filter, sr);
        rearCutoffSmooth_.prepare(currentParams.smoothMs_Filter, sr);
        lastSmoothMs_Filter_ = currentParams.smoothMs_Filter;
    }
    if (currentParams.smoothMs_Gain != lastSmoothMs_Gain_) {
        ildGainSmooth_.prepare(currentParams.smoothMs_Gain, sr);
        lastSmoothMs_Gain_ = currentParams.smoothMs_Gain;
    }

    // -------------------------------------------------------------------------
    // Phase 3: per-block updates (position-independent only)
    // -------------------------------------------------------------------------
    for (int c = 0; c < kMaxCombFilters; ++c) {
        combBank_[c].setDelay(static_cast<int>(currentParams.combDelays_ms[c] * 0.001f * sr));
        combBank_[c].setFeedback(currentParams.combFeedback[c]);
        // Also update srcR_ comb bank with same coefficients
        srcR_.combBank[c].setDelay(static_cast<int>(currentParams.combDelays_ms[c] * 0.001f * sr));
        srcR_.combBank[c].setFeedback(currentParams.combFeedback[c]);
    }

    pinnaP1_.setCoefficients(dsp::BiquadType::PeakingEQ,
        kPinnaP1FreqHz, sr, kPinnaP1Q, kPinnaP1GainDb);

    // -------------------------------------------------------------------------
    // Phase 4: per-block distance processing
    // -------------------------------------------------------------------------
    if (currentParams.distSmoothMs != lastDistSmoothMs_) {
        distDelaySmooth_.prepare(currentParams.distSmoothMs, sr);
        distR_.distDelaySmooth.prepare(currentParams.distSmoothMs, sr);
        lastDistSmoothMs_ = currentParams.distSmoothMs;
    }

    const bool dopplerOn = currentParams.dopplerEnabled;

    // -------------------------------------------------------------------------
    // Phase 5: per-block reverb parameter updates
    // -------------------------------------------------------------------------
    reverb_.setDecay(currentParams.verbDecay);
    reverb_.setDamping(currentParams.verbDamping);

    // -------------------------------------------------------------------------
    // Phase 5: LFO — set rate and waveform per block
    // -------------------------------------------------------------------------
    auto lfoRate = [&](float freeHz, float beatDiv, bool tempoSync) -> float {
        if (tempoSync && currentParams.hostBpm > 0.0f)
            return (currentParams.hostBpm / 60.0f) * beatDiv;
        return freeHz;
    };
    lfoX_.waveform = static_cast<dsp::LFOWaveform>(currentParams.lfoXWaveform);
    lfoY_.waveform = static_cast<dsp::LFOWaveform>(currentParams.lfoYWaveform);
    lfoZ_.waveform = static_cast<dsp::LFOWaveform>(currentParams.lfoZWaveform);
    lfoX_.setRateHz(lfoRate(currentParams.lfoXRate, currentParams.lfoXBeatDiv, currentParams.lfoTempoSync) * currentParams.lfoSpeedMul);
    lfoY_.setRateHz(lfoRate(currentParams.lfoYRate, currentParams.lfoYBeatDiv, currentParams.lfoTempoSync) * currentParams.lfoSpeedMul);
    lfoZ_.setRateHz(lfoRate(currentParams.lfoZRate, currentParams.lfoZBeatDiv, currentParams.lfoTempoSync) * currentParams.lfoSpeedMul);
    lfoX_.setPhaseOffset(currentParams.lfoXPhase);
    lfoY_.setPhaseOffset(currentParams.lfoYPhase);
    lfoZ_.setPhaseOffset(currentParams.lfoZPhase);
    lfoX_.setSmoothMs(currentParams.lfoXSmooth * 100.0f);  // 0-1 → 0-100ms
    lfoY_.setSmoothMs(currentParams.lfoYSmooth * 100.0f);
    lfoZ_.setSmoothMs(currentParams.lfoZSmooth * 100.0f);
    if (currentParams.lfoXResetPhase) lfoX_.requestReset();
    if (currentParams.lfoYResetPhase) lfoY_.requestReset();
    if (currentParams.lfoZResetPhase) lfoZ_.requestReset();

    // -------------------------------------------------------------------------
    // Stereo orbit LFOs — per-block setup
    // -------------------------------------------------------------------------
    orbitLfoXY_.waveform = static_cast<dsp::LFOWaveform>(currentParams.stereoOrbitXYWaveform);
    orbitLfoXZ_.waveform = static_cast<dsp::LFOWaveform>(currentParams.stereoOrbitXZWaveform);
    orbitLfoYZ_.waveform = static_cast<dsp::LFOWaveform>(currentParams.stereoOrbitYZWaveform);
    orbitLfoXY_.setRateHz(lfoRate(currentParams.stereoOrbitXYRate, currentParams.stereoOrbitXYBeatDiv, currentParams.stereoOrbitTempoSync) * currentParams.stereoOrbitSpeedMul);
    orbitLfoXZ_.setRateHz(lfoRate(currentParams.stereoOrbitXZRate, currentParams.stereoOrbitXZBeatDiv, currentParams.stereoOrbitTempoSync) * currentParams.stereoOrbitSpeedMul);
    orbitLfoYZ_.setRateHz(lfoRate(currentParams.stereoOrbitYZRate, currentParams.stereoOrbitYZBeatDiv, currentParams.stereoOrbitTempoSync) * currentParams.stereoOrbitSpeedMul);
    orbitLfoXY_.setPhaseOffset(currentParams.stereoOrbitXYPhase);
    orbitLfoXZ_.setPhaseOffset(currentParams.stereoOrbitXZPhase);
    orbitLfoYZ_.setPhaseOffset(currentParams.stereoOrbitYZPhase);
    orbitLfoXY_.setSmoothMs(currentParams.stereoOrbitXYSmooth * 100.0f);
    orbitLfoXZ_.setSmoothMs(currentParams.stereoOrbitXZSmooth * 100.0f);
    orbitLfoYZ_.setSmoothMs(currentParams.stereoOrbitYZSmooth * 100.0f);
    if (currentParams.stereoOrbitXYResetPhase) orbitLfoXY_.requestReset();
    if (currentParams.stereoOrbitXZResetPhase) orbitLfoXZ_.requestReset();
    if (currentParams.stereoOrbitYZResetPhase) orbitLfoYZ_.requestReset();

    // -------------------------------------------------------------------------
    // Per-block orbit angular smoother update
    // The phase/offset knobs are per-block parameters — their IIR smoother
    // only needs one tick per block. Advancing once every 64-128 samples
    // (1.45ms) is valid: the 5ms smoothing constant is longer than the block.
    // This saves 4 cos/sin + 2 atan2 per sample when stereo is active.
    // -------------------------------------------------------------------------
    constexpr float kPI_early = 3.14159265358979323846f;
    constexpr float kTwoPI_early = 2.0f * kPI_early;
    float blkSmoothedPhase = 0.0f, blkSmoothedOffset = 0.0f, blkRPhaseOffset = 0.0f;
    {
        const float smoothedWidth_peek = stereoWidthSmooth_.current();
        if (smoothedWidth_peek > 0.001f && inputR != nullptr) {
            const float b = 1.0f - angularSmA_;
            {
                const float phaseRad = currentParams.stereoOrbitPhase * kTwoPI_early;
                phaseSmCos_ = std::cos(phaseRad) * b + phaseSmCos_ * angularSmA_;
                phaseSmSin_ = std::sin(phaseRad) * b + phaseSmSin_ * angularSmA_;
            }
            {
                const float offsetRad = currentParams.stereoOrbitOffset * kTwoPI_early;
                offsetSmCos_ = std::cos(offsetRad) * b + offsetSmCos_ * angularSmA_;
                offsetSmSin_ = std::sin(offsetRad) * b + offsetSmSin_ * angularSmA_;
            }
            blkSmoothedPhase  = std::atan2(phaseSmSin_,  phaseSmCos_);
            blkSmoothedOffset = std::atan2(offsetSmSin_, offsetSmCos_);
            blkRPhaseOffset   = kPI_early + blkSmoothedPhase;
        }
    }

    // -------------------------------------------------------------------------
    // Per-block coefficient pre-computation
    // All transcendental math (cos, sin, pow, sqrt, tan, exp) runs here ONCE
    // per block, not per sample. Filter .process() calls remain per-sample.
    // -------------------------------------------------------------------------
    constexpr float kPI = 3.14159265358979323846f;
    constexpr float kTwoPI = 2.0f * kPI;

    // Pre-compute dB-to-linear conversions (std::pow) once per block
    const float chestGainLin   = std::pow(10.0f, currentParams.chestGainDb / 20.0f);
    const float floorGainLin   = std::pow(10.0f, currentParams.floorGainDb / 20.0f);
    ildGainBase_               = std::pow(10.0f, -currentParams.ildMaxDb / 20.0f);
    const float auxMaxBoostLin = std::pow(10.0f, currentParams.auxSendGainMaxDb / 20.0f);

    // Block-start position (unmodulated — LFO adds per-sample delta which is
    // negligible for coefficient purposes within one 64-128 sample block)
    const float blkX = currentParams.x;
    const float blkY = currentParams.y;
    const float blkZ = currentParams.z;
    const float blkHorizMag  = std::sqrt(blkX * blkX + blkY * blkY);
    const float blkRearFactor = (blkHorizMag > 1e-7f) ? (-blkY / blkHorizMag) : blkY;

    // Block-start EQ targets (Z-driven pinna, Y-driven presence/earCanal)
    const float blkPresenceGainDb  = currentParams.presenceShelfMaxDb * (-blkRearFactor);
    const float blkEarCanalGainDb  = std::min(0.0f, currentParams.earCanalMaxDb * (-blkRearFactor));
    const float blkZ_clamped       = std::clamp(blkZ, 0.0f, 1.0f);
    const float blkPinnaGainDb     = -15.0f + 20.0f * blkZ_clamped;
    const float blkShelfGainDb     = 3.0f * std::clamp(blkZ + 1.0f, 0.0f, 1.0f);
    const float blkPinnaN1Freq     = std::clamp(kPinnaN1MinHz + (kPinnaN1MaxHz - kPinnaN1MinHz) * ((blkZ + 1.0f) * 0.5f), kPinnaN1MinHz, kPinnaN1MaxHz);
    const float blkPinnaN2Freq     = blkPinnaN1Freq + kPinnaN2OffsetHz;

    // Block-start binaural targets (mono path)
    const float blkAzimuthFactor   = (blkHorizMag > 1e-7f) ? blkX / blkHorizMag : 0.0f;
    const float blkCylR            = currentParams.vertMonoCylinderRadius;
    const float blkT               = std::clamp(blkHorizMag / (blkCylR + 1e-7f), 0.0f, 1.0f);
    const float blkMonoBlend       = 1.0f - blkT * blkT * (3.0f - 2.0f * blkT);
    const float blkEffAzimuth      = blkAzimuthFactor * (1.0f - blkMonoBlend);
    const float blkAbsEffAzimuth   = std::abs(blkEffAzimuth);
    const float blkShadowCutoffTgt = kHeadShadowFullOpenHz
        + (currentParams.headShadowMinHz - kHeadShadowFullOpenHz) * blkAbsEffAzimuth;
    const float blkRearAmount      = std::max(0.0f, blkRearFactor);
    const float blkRearCutoffTgt   = kRearShadowFullOpenHz
        + (currentParams.rearShadowMinHz - kRearShadowFullOpenHz) * blkRearAmount;
    const float blkNFGainDb        = kNearFieldLFMaxDb * (1.0f - std::clamp(
        (std::sqrt(blkX*blkX + blkY*blkY + blkZ*blkZ) - kMinDistance)
        / std::max(currentParams.sphereRadius - kMinDistance, 0.001f), 0.0f, 1.0f))
        * blkAbsEffAzimuth;

    // Block-start distance (for air LPF)
    const float blkDist     = computeDistance(blkX, blkY, blkZ);
    const float blkMaxRange = std::max(currentParams.sphereRadius - kMinDistance, 0.001f);
    const float blkDistFrac = std::clamp((blkDist - kMinDistance) / blkMaxRange, 0.0f, 1.0f);
    const float blkAirCutoff1 = currentParams.airAbsMaxHz
        + (currentParams.airAbsMinHz - currentParams.airAbsMaxHz) * blkDistFrac;
    const float blkAirCutoff2 = kAirAbs2MaxHz
        + (kAirAbs2MinHz - kAirAbs2MaxHz) * blkDistFrac;

    // --- Mono path per-block setCoefficients ---
    // Pinna EQ biquads (setCoefficients contains cos/sin/pow/sqrt)
    presenceShelf_.setCoefficients(dsp::BiquadType::HighShelf,
        currentParams.presenceShelfFreqHz, sr, 0.7071f, blkPresenceGainDb);
    earCanalPeak_.setCoefficients(dsp::BiquadType::PeakingEQ,
        currentParams.earCanalFreqHz, sr, currentParams.earCanalQ, blkEarCanalGainDb);
    pinnaNotch_.setCoefficients(dsp::BiquadType::PeakingEQ,
        blkPinnaN1Freq, sr, currentParams.pinnaNotchQ, blkPinnaGainDb);
    pinnaNotch2_.setCoefficients(dsp::BiquadType::PeakingEQ,
        blkPinnaN2Freq, sr, kPinnaN2Q, kPinnaN2GainDb);
    pinnaShelf_.setCoefficients(dsp::BiquadType::HighShelf,
        currentParams.pinnaShelfFreqHz, sr, 0.7071f, blkShelfGainDb);

    // Near-field ILD biquads (mono path)
    if (blkEffAzimuth > 0.0f) {
        nearFieldLF_R_.setCoefficients(dsp::BiquadType::LowShelf, kNearFieldLFHz, sr, 0.7071f, blkNFGainDb);
        nearFieldLF_L_.setCoefficients(dsp::BiquadType::LowShelf, kNearFieldLFHz, sr, 0.7071f, 0.0f);
    } else if (blkEffAzimuth < 0.0f) {
        nearFieldLF_L_.setCoefficients(dsp::BiquadType::LowShelf, kNearFieldLFHz, sr, 0.7071f, blkNFGainDb);
        nearFieldLF_R_.setCoefficients(dsp::BiquadType::LowShelf, kNearFieldLFHz, sr, 0.7071f, 0.0f);
    } else {
        nearFieldLF_L_.setCoefficients(dsp::BiquadType::LowShelf, kNearFieldLFHz, sr, 0.7071f, 0.0f);
        nearFieldLF_R_.setCoefficients(dsp::BiquadType::LowShelf, kNearFieldLFHz, sr, 0.7071f, 0.0f);
    }

    // Head shadow SVFs (mono path) — cutoff stepped once per block
    if (blkEffAzimuth >= 0.0f) {
        shadowL_.setCoefficients(blkShadowCutoffTgt, sr);
        shadowR_.setCoefficients(kHeadShadowFullOpenHz, sr);
    } else {
        shadowL_.setCoefficients(kHeadShadowFullOpenHz, sr);
        shadowR_.setCoefficients(blkShadowCutoffTgt, sr);
    }

    // Rear shadow SVFs (mono path)
    rearSvfL_.setCoefficients(blkRearCutoffTgt, sr);
    rearSvfR_.setCoefficients(blkRearCutoffTgt, sr);

    // Air absorption LPFs (mono path — OnePoleLP, setCoefficients contains std::exp)
    airLPF_L_.setCoefficients(blkAirCutoff1, sr);
    airLPF_R_.setCoefficients(blkAirCutoff1, sr);
    airLPF2_L_.setCoefficients(blkAirCutoff2, sr);
    airLPF2_R_.setCoefficients(blkAirCutoff2, sr);

    // --- Stereo path per-block setCoefficients ---
    // For the stereo path, compute L-node and R-node block-start positions using
    // unmodulated params. The orbit LFO modulation changes slowly; per-block
    // coefficient update is sufficient (LFO phase delta per block is negligible for EQ).
    {
        const float blkHalfSpread = currentParams.stereoWidth * kStereoMaxSpreadRadius;
        float blkSpreadX = 1.0f, blkSpreadY = 0.0f;
        if (currentParams.stereoFaceListener && blkHorizMag > 1e-5f) {
            blkSpreadX =  blkY / blkHorizMag;
            blkSpreadY = -blkX / blkHorizMag;
        }

        // Peek orbit LFOs to get block-start orbit state (before per-sample ticks)
        const float blkOrbitRawXY = orbitLfoXY_.peek();
        const float blkOrbitRawXZ = orbitLfoXZ_.peek();
        const float blkOrbitRawYZ = orbitLfoYZ_.peek();
        const float blkOrbitDepXY = orbitDepthXYSmooth_.current();
        const float blkOrbitDepXZ = orbitDepthXZSmooth_.current();
        const float blkOrbitDepYZ = orbitDepthYZSmooth_.current();

        const float blkOrbitAngleXY = blkOrbitRawXY * blkOrbitDepXY * kPI;
        const float blkLAngle = blkOrbitAngleXY + blkSmoothedOffset;
        const float blkRAngle = blkOrbitAngleXY + blkSmoothedOffset + blkRPhaseOffset;

        // L offset in XY plane
        float blkLOffX = blkHalfSpread * (blkSpreadX * dsp::SineLUT::cosLookupAngle(blkLAngle) - blkSpreadY * dsp::SineLUT::lookupAngle(blkLAngle));
        float blkLOffY = blkHalfSpread * (blkSpreadX * dsp::SineLUT::lookupAngle(blkLAngle) + blkSpreadY * dsp::SineLUT::cosLookupAngle(blkLAngle));
        float blkLOffZ = 0.0f;

        // R offset in XY plane
        float blkROffX = blkHalfSpread * (blkSpreadX * dsp::SineLUT::cosLookupAngle(blkRAngle) - blkSpreadY * dsp::SineLUT::lookupAngle(blkRAngle));
        float blkROffY = blkHalfSpread * (blkSpreadX * dsp::SineLUT::lookupAngle(blkRAngle) + blkSpreadY * dsp::SineLUT::cosLookupAngle(blkRAngle));
        float blkROffZ = 0.0f;

        // XZ orbit rotation
        if (std::abs(blkOrbitDepXZ) > 1e-7f) {
            const float angXZ = blkOrbitRawXZ * blkOrbitDepXZ * kPI;
            const float cosXZ = dsp::SineLUT::cosLookupAngle(angXZ);
            const float sinXZ = dsp::SineLUT::lookupAngle(angXZ);

            float tmpX, tmpZ;
            tmpX = blkLOffX * cosXZ - blkLOffZ * sinXZ;
            tmpZ = blkLOffX * sinXZ + blkLOffZ * cosXZ;
            blkLOffX = tmpX; blkLOffZ = tmpZ;

            tmpX = blkROffX * cosXZ - blkROffZ * sinXZ;
            tmpZ = blkROffX * sinXZ + blkROffZ * cosXZ;
            blkROffX = tmpX; blkROffZ = tmpZ;
        }

        // YZ orbit rotation
        if (std::abs(blkOrbitDepYZ) > 1e-7f) {
            const float angYZ = blkOrbitRawYZ * blkOrbitDepYZ * kPI;
            const float cosYZ = dsp::SineLUT::cosLookupAngle(angYZ);
            const float sinYZ = dsp::SineLUT::lookupAngle(angYZ);

            float tmpY, tmpZ;
            tmpY = blkLOffY * cosYZ - blkLOffZ * sinYZ;
            tmpZ = blkLOffY * sinYZ + blkLOffZ * cosYZ;
            blkLOffY = tmpY; blkLOffZ = tmpZ;

            tmpY = blkROffY * cosYZ - blkROffZ * sinYZ;
            tmpZ = blkROffY * sinYZ + blkROffZ * cosYZ;
            blkROffY = tmpY; blkROffZ = tmpZ;
        }

        const float blkLNodeX = blkX + blkLOffX;
        const float blkLNodeY = blkY + blkLOffY;
        const float blkLNodeZ = blkZ + blkLOffZ;
        const float blkRNodeX = blkX + blkROffX;
        const float blkRNodeY = blkY + blkROffY;
        const float blkRNodeZ = blkZ + blkROffZ;

        // L node EQ targets
        const float lHorizMag   = std::sqrt(blkLNodeX*blkLNodeX + blkLNodeY*blkLNodeY);
        const float lRearFactor = (lHorizMag > 1e-7f) ? (-blkLNodeY / lHorizMag) : blkLNodeY;
        const float lPresGainDb = currentParams.presenceShelfMaxDb * (-lRearFactor);
        const float lEarGainDb  = std::min(0.0f, currentParams.earCanalMaxDb * (-lRearFactor));
        const float lZ_cl       = std::clamp(blkLNodeZ, 0.0f, 1.0f);
        const float lPinnaGain  = -15.0f + 20.0f * lZ_cl;
        const float lShelfGain  = 3.0f * std::clamp(blkLNodeZ + 1.0f, 0.0f, 1.0f);
        const float lN1Freq     = std::clamp(kPinnaN1MinHz + (kPinnaN1MaxHz - kPinnaN1MinHz) * ((blkLNodeZ + 1.0f) * 0.5f), kPinnaN1MinHz, kPinnaN1MaxHz);
        const float lN2Freq     = lN1Freq + kPinnaN2OffsetHz;
        const float lAzFactor   = (lHorizMag > 1e-7f) ? blkLNodeX / lHorizMag : 0.0f;
        const float lCylT       = std::clamp(lHorizMag / (currentParams.vertMonoCylinderRadius + 1e-7f), 0.0f, 1.0f);
        const float lMonoBlend  = 1.0f - lCylT * lCylT * (3.0f - 2.0f * lCylT);
        const float lEffAz      = lAzFactor * (1.0f - lMonoBlend);
        const float lAbsEffAz   = std::abs(lEffAz);
        const float lShadowCut  = kHeadShadowFullOpenHz + (currentParams.headShadowMinHz - kHeadShadowFullOpenHz) * lAbsEffAz;
        const float lRearAmt    = std::max(0.0f, lRearFactor);
        const float lRearCut    = kRearShadowFullOpenHz + (currentParams.rearShadowMinHz - kRearShadowFullOpenHz) * lRearAmt;
        const float lProx       = 1.0f - std::clamp((std::sqrt(blkLNodeX*blkLNodeX+blkLNodeY*blkLNodeY+blkLNodeZ*blkLNodeZ)-kMinDistance)/blkMaxRange, 0.0f, 1.0f);
        const float lNFGainDb   = kNearFieldLFMaxDb * lProx * lAbsEffAz;
        const float lLNodeDistFrac = std::clamp(
            (std::sqrt(blkLNodeX*blkLNodeX+blkLNodeY*blkLNodeY+blkLNodeZ*blkLNodeZ) - kMinDistance) / blkMaxRange, 0.0f, 1.0f);
        const float lAirCut1    = currentParams.airAbsMaxHz + (currentParams.airAbsMinHz - currentParams.airAbsMaxHz) * lLNodeDistFrac;
        const float lAirCut2    = kAirAbs2MaxHz + (kAirAbs2MinHz - kAirAbs2MaxHz) * lLNodeDistFrac;

        // R node EQ targets
        const float rHorizMag   = std::sqrt(blkRNodeX*blkRNodeX + blkRNodeY*blkRNodeY);
        const float rRearFactor = (rHorizMag > 1e-7f) ? (-blkRNodeY / rHorizMag) : blkRNodeY;
        const float rPresGainDb = currentParams.presenceShelfMaxDb * (-rRearFactor);
        const float rEarGainDb  = std::min(0.0f, currentParams.earCanalMaxDb * (-rRearFactor));
        const float rZ_cl       = std::clamp(blkRNodeZ, 0.0f, 1.0f);
        const float rPinnaGain  = -15.0f + 20.0f * rZ_cl;
        const float rShelfGain  = 3.0f * std::clamp(blkRNodeZ + 1.0f, 0.0f, 1.0f);
        const float rN1Freq     = std::clamp(kPinnaN1MinHz + (kPinnaN1MaxHz - kPinnaN1MinHz) * ((blkRNodeZ + 1.0f) * 0.5f), kPinnaN1MinHz, kPinnaN1MaxHz);
        const float rN2Freq     = rN1Freq + kPinnaN2OffsetHz;
        const float rAzFactor   = (rHorizMag > 1e-7f) ? blkRNodeX / rHorizMag : 0.0f;
        const float rCylT       = std::clamp(rHorizMag / (currentParams.vertMonoCylinderRadius + 1e-7f), 0.0f, 1.0f);
        const float rMonoBlend  = 1.0f - rCylT * rCylT * (3.0f - 2.0f * rCylT);
        const float rEffAz      = rAzFactor * (1.0f - rMonoBlend);
        const float rAbsEffAz   = std::abs(rEffAz);
        const float rShadowCut  = kHeadShadowFullOpenHz + (currentParams.headShadowMinHz - kHeadShadowFullOpenHz) * rAbsEffAz;
        const float rRearAmt    = std::max(0.0f, rRearFactor);
        const float rRearCut    = kRearShadowFullOpenHz + (currentParams.rearShadowMinHz - kRearShadowFullOpenHz) * rRearAmt;
        const float rProx       = 1.0f - std::clamp((std::sqrt(blkRNodeX*blkRNodeX+blkRNodeY*blkRNodeY+blkRNodeZ*blkRNodeZ)-kMinDistance)/blkMaxRange, 0.0f, 1.0f);
        const float rNFGainDb   = kNearFieldLFMaxDb * rProx * rAbsEffAz;
        const float rRNodeDistFrac = std::clamp(
            (std::sqrt(blkRNodeX*blkRNodeX+blkRNodeY*blkRNodeY+blkRNodeZ*blkRNodeZ) - kMinDistance) / blkMaxRange, 0.0f, 1.0f);
        const float rAirCut1    = currentParams.airAbsMaxHz + (currentParams.airAbsMinHz - currentParams.airAbsMaxHz) * rRNodeDistFrac;
        const float rAirCut2    = kAirAbs2MaxHz + (kAirAbs2MinHz - kAirAbs2MaxHz) * rRNodeDistFrac;

        // L pipeline EQ setCoefficients (stereo path)
        presenceShelf_.setCoefficients(dsp::BiquadType::HighShelf,
            currentParams.presenceShelfFreqHz, sr, 0.7071f, lPresGainDb);
        earCanalPeak_.setCoefficients(dsp::BiquadType::PeakingEQ,
            currentParams.earCanalFreqHz, sr, currentParams.earCanalQ, lEarGainDb);
        pinnaNotch_.setCoefficients(dsp::BiquadType::PeakingEQ,
            lN1Freq, sr, currentParams.pinnaNotchQ, lPinnaGain);
        pinnaNotch2_.setCoefficients(dsp::BiquadType::PeakingEQ,
            lN2Freq, sr, kPinnaN2Q, kPinnaN2GainDb);
        pinnaShelf_.setCoefficients(dsp::BiquadType::HighShelf,
            currentParams.pinnaShelfFreqHz, sr, 0.7071f, lShelfGain);
        if (lEffAz > 0.0f) {
            nearFieldLF_R_.setCoefficients(dsp::BiquadType::LowShelf, kNearFieldLFHz, sr, 0.7071f, lNFGainDb);
            nearFieldLF_L_.setCoefficients(dsp::BiquadType::LowShelf, kNearFieldLFHz, sr, 0.7071f, 0.0f);
        } else if (lEffAz < 0.0f) {
            nearFieldLF_L_.setCoefficients(dsp::BiquadType::LowShelf, kNearFieldLFHz, sr, 0.7071f, lNFGainDb);
            nearFieldLF_R_.setCoefficients(dsp::BiquadType::LowShelf, kNearFieldLFHz, sr, 0.7071f, 0.0f);
        } else {
            nearFieldLF_L_.setCoefficients(dsp::BiquadType::LowShelf, kNearFieldLFHz, sr, 0.7071f, 0.0f);
            nearFieldLF_R_.setCoefficients(dsp::BiquadType::LowShelf, kNearFieldLFHz, sr, 0.7071f, 0.0f);
        }
        if (lEffAz >= 0.0f) {
            shadowL_.setCoefficients(lShadowCut, sr);
            shadowR_.setCoefficients(kHeadShadowFullOpenHz, sr);
        } else {
            shadowL_.setCoefficients(kHeadShadowFullOpenHz, sr);
            shadowR_.setCoefficients(lShadowCut, sr);
        }
        rearSvfL_.setCoefficients(lRearCut, sr);
        rearSvfR_.setCoefficients(lRearCut, sr);
        airLPF_L_.setCoefficients(lAirCut1, sr);
        airLPF_R_.setCoefficients(lAirCut1, sr);
        airLPF2_L_.setCoefficients(lAirCut2, sr);
        airLPF2_R_.setCoefficients(lAirCut2, sr);

        // R pipeline (srcR_) EQ setCoefficients
        srcR_.presenceShelf.setCoefficients(dsp::BiquadType::HighShelf,
            currentParams.presenceShelfFreqHz, sr, 0.7071f, rPresGainDb);
        srcR_.earCanalPeak.setCoefficients(dsp::BiquadType::PeakingEQ,
            currentParams.earCanalFreqHz, sr, currentParams.earCanalQ, rEarGainDb);
        srcR_.pinnaP1.setCoefficients(dsp::BiquadType::PeakingEQ,
            kPinnaP1FreqHz, sr, kPinnaP1Q, kPinnaP1GainDb);
        srcR_.pinnaNotch.setCoefficients(dsp::BiquadType::PeakingEQ,
            rN1Freq, sr, currentParams.pinnaNotchQ, rPinnaGain);
        srcR_.pinnaNotch2.setCoefficients(dsp::BiquadType::PeakingEQ,
            rN2Freq, sr, kPinnaN2Q, kPinnaN2GainDb);
        srcR_.pinnaShelf.setCoefficients(dsp::BiquadType::HighShelf,
            currentParams.pinnaShelfFreqHz, sr, 0.7071f, rShelfGain);
        if (rEffAz > 0.0f) {
            srcR_.nearFieldLF_R.setCoefficients(dsp::BiquadType::LowShelf, kNearFieldLFHz, sr, 0.7071f, rNFGainDb);
            srcR_.nearFieldLF_L.setCoefficients(dsp::BiquadType::LowShelf, kNearFieldLFHz, sr, 0.7071f, 0.0f);
        } else if (rEffAz < 0.0f) {
            srcR_.nearFieldLF_L.setCoefficients(dsp::BiquadType::LowShelf, kNearFieldLFHz, sr, 0.7071f, rNFGainDb);
            srcR_.nearFieldLF_R.setCoefficients(dsp::BiquadType::LowShelf, kNearFieldLFHz, sr, 0.7071f, 0.0f);
        } else {
            srcR_.nearFieldLF_L.setCoefficients(dsp::BiquadType::LowShelf, kNearFieldLFHz, sr, 0.7071f, 0.0f);
            srcR_.nearFieldLF_R.setCoefficients(dsp::BiquadType::LowShelf, kNearFieldLFHz, sr, 0.7071f, 0.0f);
        }
        if (rEffAz >= 0.0f) {
            srcR_.shadowL.setCoefficients(rShadowCut, sr);
            srcR_.shadowR.setCoefficients(kHeadShadowFullOpenHz, sr);
        } else {
            srcR_.shadowL.setCoefficients(kHeadShadowFullOpenHz, sr);
            srcR_.shadowR.setCoefficients(rShadowCut, sr);
        }
        srcR_.rearSvfL.setCoefficients(rRearCut, sr);
        srcR_.rearSvfR.setCoefficients(rRearCut, sr);
        distR_.airLPF_L.setCoefficients(rAirCut1, sr);
        distR_.airLPF_R.setCoefficients(rAirCut1, sr);
        distR_.airLPF2_L.setCoefficients(rAirCut2, sr);
        distR_.airLPF2_R.setCoefficients(rAirCut2, sr);

    }

    // -------------------------------------------------------------------------
    // Per-sample loop
    // -------------------------------------------------------------------------
    const float testGainLin = currentParams.testToneEnabled
        ? std::pow(10.0f, currentParams.testToneGainDb / 20.0f)
        : 0.0f;
    const float sawIncrement = currentParams.testTonePitchHz / sr;
    pulseLFO_.setRateHz(currentParams.testTonePulseHz);
    std::uniform_real_distribution<float> noiseDist(-1.0f, 1.0f);

    // Pre-compute position-derived values for zero-LFO fast path
    const float blkRawModDist = std::sqrt(currentParams.x * currentParams.x
                                        + currentParams.y * currentParams.y
                                        + currentParams.z * currentParams.z);
    // blkHorizMag is already computed above (sqrt(blkX^2+blkY^2)) -- reused in zero-LFO path

    for (int i = 0; i < numSamples; ++i) {
        // ----------------------------------------------------------------
        // Test tone generation
        // ----------------------------------------------------------------
        float testSig = 0.0f;
        float testSigL = 0.0f;
        float testSigR = 0.0f;
        bool  testStereo = false;
        if (currentParams.testToneEnabled) {
            switch (currentParams.testToneWaveform) {
                case xyzpan::TestToneWaveform::WhiteNoise:
                    testSig = noiseDist(noiseRng_);
                    break;
                case xyzpan::TestToneWaveform::PulsingWhiteNoise:
                    testSig = noiseDist(noiseRng_) * (pulseLFO_.tick() > 0.0f ? 1.0f : 0.0f);
                    break;
                case xyzpan::TestToneWaveform::PulsingSaw:
                    testSig = (2.0f * sawPhase_ - 1.0f) * (pulseLFO_.tick() > 0.0f ? 1.0f : 0.0f);
                    sawPhase_ += sawIncrement;
                    if (sawPhase_ >= 1.0f) sawPhase_ -= 1.0f;
                    break;
                case xyzpan::TestToneWaveform::PulsingSquare: {
                    const float gate = pulseLFO_.tick() > 0.0f ? 1.0f : 0.0f;
                    testSig = (sawPhase_ < 0.5f ? 1.0f : -1.0f) * gate;
                    sawPhase_ += sawIncrement;
                    if (sawPhase_ >= 1.0f) sawPhase_ -= 1.0f;
                    break;
                }
                case xyzpan::TestToneWaveform::Square:
                    testSig = sawPhase_ < 0.5f ? 1.0f : -1.0f;
                    sawPhase_ += sawIncrement;
                    if (sawPhase_ >= 1.0f) sawPhase_ -= 1.0f;
                    break;
                case xyzpan::TestToneWaveform::StereoNoiseSaw: {
                    testStereo = true;
                    const float gate = pulseLFO_.tick() > 0.0f ? 1.0f : 0.0f;
                    testSigL = noiseDist(noiseRng_) * gate * testGainLin;
                    testSigR = (2.0f * sawPhase_ - 1.0f) * gate * testGainLin;
                    sawPhase_ += sawIncrement;
                    if (sawPhase_ >= 1.0f) sawPhase_ -= 1.0f;
                    break;
                }
                case xyzpan::TestToneWaveform::Saw: default:
                    testSig = 2.0f * sawPhase_ - 1.0f;
                    sawPhase_ += sawIncrement;
                    if (sawPhase_ >= 1.0f) sawPhase_ -= 1.0f;
                    break;
            }
            if (!testStereo) testSig *= testGainLin;
        }

        // ----------------------------------------------------------------
        // Position LFOs
        // ----------------------------------------------------------------
        const float depthX = lfoDepthXSmooth_.process(currentParams.lfoXDepth);
        const float depthY = lfoDepthYSmooth_.process(currentParams.lfoYDepth);
        const float depthZ = lfoDepthZSmooth_.process(currentParams.lfoZDepth);

        // Detect if any LFO depth is active -- when all are zero, position is block-constant
        const bool lfoActive = depthX > 1e-7f || depthY > 1e-7f || depthZ > 1e-7f;

        float modX, modY, modZ;
        if (lfoActive) {
            modX = currentParams.x + lfoX_.tick() * depthX;
            modY = currentParams.y + lfoY_.tick() * depthY;
            modZ = currentParams.z + lfoZ_.tick() * depthZ;
        } else {
            // Still tick LFOs to keep phase accumulation consistent (no jump when depth goes non-zero)
            lfoX_.tick(); lfoY_.tick(); lfoZ_.tick();
            // Use block-start values -- position is constant when LFO depths are all zero
            modX = currentParams.x;
            modY = currentParams.y;
            modZ = currentParams.z;
        }

        // Position-dependent targets from object center position
        // Skip sqrt when position is block-constant (zero-LFO fast path)
        float rawModDist, horizontalMag;
        if (lfoActive) {
            rawModDist    = std::sqrt(modX * modX + modY * modY + modZ * modZ);
            horizontalMag = std::sqrt(modX * modX + modY * modY);
        } else {
            rawModDist    = blkRawModDist;
            horizontalMag = blkHorizMag;
        }
        const float modDist     = std::max(rawModDist, kMinDistance);
        const float maxRange    = std::max(currentParams.sphereRadius - kMinDistance, 0.001f);
        const float modDistFrac = std::clamp((modDist - kMinDistance) / maxRange, 0.0f, 1.0f);

        constexpr float kDistRefScale = 0.047546796f;
        const float distRef = currentParams.sphereRadius * kDistRefScale;

        const float rawDistFrac = std::clamp(rawModDist / kSqrt3, 0.0f, 1.0f);

        // Shared EQ targets from object position
        const float rearFactor = (horizontalMag > 1e-7f)
            ? (-modY / horizontalMag) : modY;
        const float combWetTarget = currentParams.combWetMax * std::max(0.0f, rearFactor);

        // Distance targets
        const float distRatio = distRef / modDist;
        const float distGainTarget = std::clamp(distRatio * distRatio, 0.0f, kDistGainMax);
        const float delayTargetSamples = std::max(2.0f,
            rawDistFrac * currentParams.distDelayMaxMs * 0.001f * sr);

        // Air absorption coefficients — mono path only (stereo sets per-node in processDistanceForNode)
        const float airCutoffMod = currentParams.airAbsMaxHz
            + (currentParams.airAbsMinHz - currentParams.airAbsMaxHz) * modDistFrac;

        // Chest bounce (Z-driven) — use pre-computed chestGainLin (no std::pow per sample)
        const float chestElevNorm     = std::clamp((-modZ + 1.0f) * 0.5f, 0.0f, 1.0f);
        const float chestDelaySamp    = std::clamp((modZ + 1.0f) * 0.5f, 0.0f, 1.0f)
                                        * currentParams.chestDelayMaxMs * 0.001f * sr;
        const float chestLinearTarget = chestGainLin * chestElevNorm;

        // Floor bounce (Z-driven) — use pre-computed floorGainLin (no std::pow per sample)
        const float floorDelaySamp    = std::clamp((modZ + 1.0f) * 0.5f, 0.0f, 1.0f)
                                        * currentParams.floorDelayMaxMs * 0.001f * sr;
        const float floorElevNorm     = std::clamp((-modZ + 1.0f) * 0.5f, 0.0f, 1.0f);
        const float floorLinearTarget = floorGainLin * floorElevNorm;

        // Capture modulated position
        lastModulated_ = {modX, modY, modZ};

        // ----------------------------------------------------------------
        // Stereo width smooth + orbit LFOs
        // ----------------------------------------------------------------
        const float smoothedWidth = stereoWidthSmooth_.process(currentParams.stereoWidth);
        const bool stereoActive = smoothedWidth > 0.001f && inputR != nullptr;

        // Orbit LFO ticks — always tick to keep phase accumulation consistent
        const float orbitDepXY = orbitDepthXYSmooth_.process(currentParams.stereoOrbitXYDepth);
        const float orbitDepXZ = orbitDepthXZSmooth_.process(currentParams.stereoOrbitXZDepth);
        const float orbitDepYZ = orbitDepthYZSmooth_.process(currentParams.stereoOrbitYZDepth);
        const float orbitRawXY = orbitLfoXY_.tick();
        const float orbitRawXZ = orbitLfoXZ_.tick();
        const float orbitRawYZ = orbitLfoYZ_.tick();

        float dL, dR;
        float effectiveDistGain = 1.0f;
        float blendedDistFrac = modDistFrac;  // overwritten in stereo path

        if (stereoActive) {
            // Compute L/R node positions
            const float halfSpread = smoothedWidth * kStereoMaxSpreadRadius;

            // Spread direction: perpendicular to listener→object in XY plane
            float spreadX, spreadY;
            if (currentParams.stereoFaceListener && horizontalMag > 1e-5f) {
                spreadX =  modY / horizontalMag;
                spreadY = -modX / horizontalMag;
            } else {
                spreadX = 1.0f;
                spreadY = 0.0f;
            }

            // Orbit angle accumulation
            const float orbitAngleXY = orbitRawXY * orbitDepXY * kPI;

            // Use per-block orbit angular smoother (phase/offset smoothed once per block
            // in the preamble above — blkSmoothedPhase/blkSmoothedOffset/blkRPhaseOffset).
            // XZ/YZ orbit LFO cos/sin remain per-sample because orbitRawXZ/YZ change per sample.

            // L node: angle = orbitAngle + offset, R node: angle + offset + PI + phase
            const float lAngle = orbitAngleXY + blkSmoothedOffset;
            const float rAngle = orbitAngleXY + blkSmoothedOffset + blkRPhaseOffset;

            // Compute L offset in XY plane
            float lOffX = halfSpread * (spreadX * dsp::SineLUT::cosLookupAngle(lAngle) - spreadY * dsp::SineLUT::lookupAngle(lAngle));
            float lOffY = halfSpread * (spreadX * dsp::SineLUT::lookupAngle(lAngle) + spreadY * dsp::SineLUT::cosLookupAngle(lAngle));
            float lOffZ = 0.0f;

            // Compute R offset in XY plane
            float rOffX = halfSpread * (spreadX * dsp::SineLUT::cosLookupAngle(rAngle) - spreadY * dsp::SineLUT::lookupAngle(rAngle));
            float rOffY = halfSpread * (spreadX * dsp::SineLUT::lookupAngle(rAngle) + spreadY * dsp::SineLUT::cosLookupAngle(rAngle));
            float rOffZ = 0.0f;

            // Apply XZ orbit rotation to both offsets
            if (std::abs(orbitDepXZ) > 1e-7f) {
                const float angXZ = orbitRawXZ * orbitDepXZ * kPI;
                const float cosXZ = dsp::SineLUT::cosLookupAngle(angXZ);
                const float sinXZ = dsp::SineLUT::lookupAngle(angXZ);

                float tmpX, tmpZ;
                tmpX = lOffX * cosXZ - lOffZ * sinXZ;
                tmpZ = lOffX * sinXZ + lOffZ * cosXZ;
                lOffX = tmpX; lOffZ = tmpZ;

                tmpX = rOffX * cosXZ - rOffZ * sinXZ;
                tmpZ = rOffX * sinXZ + rOffZ * cosXZ;
                rOffX = tmpX; rOffZ = tmpZ;
            }

            // Apply YZ orbit rotation to both offsets
            if (std::abs(orbitDepYZ) > 1e-7f) {
                const float angYZ = orbitRawYZ * orbitDepYZ * kPI;
                const float cosYZ = dsp::SineLUT::cosLookupAngle(angYZ);
                const float sinYZ = dsp::SineLUT::lookupAngle(angYZ);

                float tmpY, tmpZ;
                tmpY = lOffY * cosYZ - lOffZ * sinYZ;
                tmpZ = lOffY * sinYZ + lOffZ * cosYZ;
                lOffY = tmpY; lOffZ = tmpZ;

                tmpY = rOffY * cosYZ - rOffZ * sinYZ;
                tmpZ = rOffY * sinYZ + rOffZ * cosYZ;
                rOffY = tmpY; rOffZ = tmpZ;
            }

            // Final node positions
            const float lNodeX = modX + lOffX;
            const float lNodeY = modY + lOffY;
            const float lNodeZ = modZ + lOffZ;
            const float rNodeX = modX + rOffX;
            const float rNodeY = modY + rOffY;
            const float rNodeZ = modZ + rOffZ;

            // Store for position bridge
            lastStereoNodes_ = { lNodeX, lNodeY, lNodeZ, rNodeX, rNodeY, rNodeZ, smoothedWidth };

            // Get input samples — test tone overrides both channels
            float sampleL = currentParams.testToneEnabled ? (testStereo ? testSigL : testSig) : inputL[i];
            float sampleR = currentParams.testToneEnabled ? (testStereo ? testSigR : testSig) : inputR[i];

            // Process L channel through L pipeline (existing flat members)
            auto [dL_L, dR_L] = processBinauralForSource(
                sampleL, lNodeX, lNodeY, lNodeZ, sr,
                delayL_, delayR_, shadowL_, shadowR_, rearSvfL_, rearSvfR_,
                itdSmooth_, shadowCutoffSmooth_, ildGainSmooth_, rearCutoffSmooth_,
                nearFieldLF_L_, nearFieldLF_R_,
                combBank_, combWetSmooth_,
                presenceShelf_, earCanalPeak_, pinnaP1_, pinnaNotch_, pinnaNotch2_, pinnaShelf_
            );

            // Process R channel through R pipeline (srcR_)
            auto [dL_R, dR_R] = processBinauralForSource(
                sampleR, rNodeX, rNodeY, rNodeZ, sr,
                srcR_.delayL, srcR_.delayR, srcR_.shadowL, srcR_.shadowR,
                srcR_.rearSvfL, srcR_.rearSvfR,
                srcR_.itdSmooth, srcR_.shadowCutoffSmooth, srcR_.ildGainSmooth, srcR_.rearCutoffSmooth,
                srcR_.nearFieldLF_L, srcR_.nearFieldLF_R,
                srcR_.combBank, srcR_.combWetSmooth,
                srcR_.presenceShelf, srcR_.earCanalPeak, srcR_.pinnaP1,
                srcR_.pinnaNotch, srcR_.pinnaNotch2, srcR_.pinnaShelf
            );

            // Per-node distance processing BEFORE summing
            auto distL_result = processDistanceForNode(dL_L, dR_L, lNodeX, lNodeY, lNodeZ, sr, dopplerOn,
                distDelayL_, distDelayR_, airLPF_L_, airLPF_R_, airLPF2_L_, airLPF2_R_,
                distGainSmooth_, distDelaySmooth_, lastDistDelaySamp_);

            auto distR_result = processDistanceForNode(dL_R, dR_R, rNodeX, rNodeY, rNodeZ, sr, dopplerOn,
                distR_.distDelayL, distR_.distDelayR, distR_.airLPF_L, distR_.airLPF_R,
                distR_.airLPF2_L, distR_.airLPF2_R, distR_.distGainSmooth, distR_.distDelaySmooth,
                distR_.lastDistDelaySamp);

            dL = distL_result.left + distR_result.left;
            dR = distL_result.right + distR_result.right;
            blendedDistFrac = 0.5f * (distL_result.distFrac + distR_result.distFrac);
            effectiveDistGain = 0.5f * (distGainSmooth_.current() + distR_.distGainSmooth.current());
        } else {
            // Mono path — sum to mono, use existing pipeline
            float mono;
            if (currentParams.testToneEnabled) {
                mono = testStereo ? 0.5f * (testSigL + testSigR) : testSig;
            } else if (inputR != nullptr) {
                mono = monoBuffer[static_cast<size_t>(i)];
            } else {
                mono = inputL[i];
            }

            lastStereoNodes_ = { modX, modY, modZ, modX, modY, modZ, 0.0f };

            // Binaural targets from LFO-modulated position (per-sample modulation
            // for ITD/ILD smoothers — these drive cheap smooth per-sample; only
            // setCoefficients calls were expensive and are now per-block above)
            const float rearAmount = std::max(0.0f, rearFactor);
            const float rearCutoffTarget = kRearShadowFullOpenHz
                + (currentParams.rearShadowMinHz - kRearShadowFullOpenHz) * rearAmount;

            const float azimuthFactor = (horizontalMag > 1e-7f)
                ? modX / horizontalMag : 0.0f;
            const float cylR = currentParams.vertMonoCylinderRadius;
            const float t = std::clamp(horizontalMag / (cylR + 1e-7f), 0.0f, 1.0f);
            const float monoBlend = 1.0f - t * t * (3.0f - 2.0f * t);
            const float effectiveAzimuth = azimuthFactor * (1.0f - monoBlend);

            const float absEffAzimuth = std::abs(effectiveAzimuth);
            const float itdTargetMod = currentParams.maxITD_ms * effectiveAzimuth * sr / 1000.0f;
            const float shadowCutoffTargetMod = kHeadShadowFullOpenHz
                + (currentParams.headShadowMinHz - kHeadShadowFullOpenHz) * absEffAzimuth;
            // Use pre-computed ildGainBase_ (no std::pow per sample)
            const float ildTargetMod = 1.0f - (1.0f - ildGainBase_) * absEffAzimuth;

            // NOTE: setCoefficients on presenceShelf_/earCanalPeak_/pinnaNotch_/
            // pinnaNotch2_/pinnaShelf_ are called ONCE per block in the preamble above.

            // Comb bank
            const float combWet = combWetSmooth_.process(combWetTarget);
            float combSig = mono;
            for (int c = 0; c < kMaxCombFilters; ++c)
                combSig = combBank_[c].process(combSig);
            const float depthOut = mono * (1.0f - combWet) + combSig * combWet;

            // Mono EQ chain
            float monoEQ = presenceShelf_.process(depthOut);
            monoEQ = earCanalPeak_.process(monoEQ);
            monoEQ = pinnaP1_.process(monoEQ);
            monoEQ = pinnaNotch_.process(monoEQ);
            monoEQ = pinnaNotch2_.process(monoEQ);
            monoEQ = pinnaShelf_.process(monoEQ);

            // Smooth binaural parameters
            const float itdSamples   = itdSmooth_.process(itdTargetMod);
            const float shadowCutoff = shadowCutoffSmooth_.process(shadowCutoffTargetMod);
            const float ildGain      = ildGainSmooth_.process(ildTargetMod);
            const float rearCutoff   = rearCutoffSmooth_.process(rearCutoffTarget);

            delayL_.push(monoEQ);
            delayR_.push(monoEQ);

            constexpr float kMinDelay = 2.0f;
            // Signed ITD: positive itdSamples → source right → delay left ear
            dL = delayL_.read(kMinDelay + std::max(0.0f,  itdSamples));
            dR = delayR_.read(kMinDelay + std::max(0.0f, -itdSamples));

            if (!std::isfinite(dL)) dL = 0.0f;
            if (!std::isfinite(dR)) dR = 0.0f;

            // ILD — attenuate far ear based on smoothed ITD sign
            if (itdSamples > 0.0f)       dL *= ildGain;
            else if (itdSamples < 0.0f)  dR *= ildGain;

            // Near-field ILD — coefficients pre-set per-block, only .process() here
            dL = nearFieldLF_L_.process(dL);
            dR = nearFieldLF_R_.process(dR);

            // Head shadow — coefficients pre-set per-block, only .process() here
            dL = shadowL_.process(dL);
            dR = shadowR_.process(dR);

            // Rear shadow — coefficients pre-set per-block, only .process() here
            dL = rearSvfL_.process(dL);
            dR = rearSvfR_.process(dR);

            // DSP state capture for mono path
            lastDSPState_.itdSamples     = itdSamples;
            lastDSPState_.shadowCutoffHz = shadowCutoff;
            lastDSPState_.ildGainLinear  = ildGain;
            lastDSPState_.rearCutoffHz   = rearCutoff;
            lastDSPState_.combWet        = combWet;
            lastDSPState_.monoBlend      = monoBlend;
        }

        // ----------------------------------------------------------------
        // Shared pipeline: chest bounce → floor bounce → distance → reverb
        // ----------------------------------------------------------------

        // Chest bounce
        {
            float chestSig = stereoActive
                ? (currentParams.testToneEnabled ? (testStereo ? 0.5f * (testSigL + testSigR) : testSig) : 0.5f * (inputL[i] + (inputR ? inputR[i] : inputL[i])))
                : (currentParams.testToneEnabled ? (testStereo ? 0.5f * (testSigL + testSigR) : testSig) : (inputR ? monoBuffer[static_cast<size_t>(i)] : inputL[i]));
            for (auto& hp : chestHPF_)
                chestSig = hp.process(chestSig);
            chestSig = chestLP_.process(chestSig);
            chestDelay_.push(chestSig);

            const float chestGain = chestGainSmooth_.process(chestLinearTarget);
            const float chestReadSamp = std::max(2.0f, chestDelaySamp);
            if (chestGain > 1e-6f) {
                float chestOut = chestDelay_.read(chestReadSamp) * chestGain * effectiveDistGain;
                dL += chestOut;
                dR += chestOut;
            }
        }

        // Floor bounce
        {
            floorDelayL_.push(dL);
            floorDelayR_.push(dR);

            const float floorGain = floorGainSmooth_.process(floorLinearTarget);
            const float floorReadSamp = std::max(2.0f, floorDelaySamp);
            if (floorGain > 1e-6f) {
                float floorL = floorDelayL_.read(floorReadSamp);
                float floorR = floorDelayR_.read(floorReadSamp);
                floorL = floorLPF_.process(floorL);
                floorR = floorLPF_.process(floorR);
                dL += floorL * floorGain;
                dR += floorR * floorGain;
            }
        }

        // Distance processing — mono path only (stereo path handled per-node above)
        if (!stereoActive) {
            // Air absorption LPF coefficients pre-set per-block (no setCoefficients here)

            const float distGain = distGainSmooth_.process(distGainTarget);
            dL *= distGain;
            dR *= distGain;
            effectiveDistGain = distGainSmooth_.current();

            distDelayL_.push(dL);
            distDelayR_.push(dR);
            if (dopplerOn) {
                const float rawDelay = distDelaySmooth_.process(delayTargetSamples);
                const float deltaDelay = rawDelay - lastDistDelaySamp_;
                const float clampedDelta = std::clamp(deltaDelay, -kDopplerMaxDeltaSamp, kDopplerMaxDeltaSamp);
                lastDistDelaySamp_ += clampedDelta;
                const float delaySamp = std::max(2.0f, lastDistDelaySamp_);
                dL = distDelayL_.read(delaySamp);
                dR = distDelayR_.read(delaySamp);
            } else {
                distDelaySmooth_.process(2.0f);
                lastDistDelaySamp_ = 2.0f;
                dL = distDelayL_.read(2.0f);
                dR = distDelayR_.read(2.0f);
            }

            // Air absorption
            dL = airLPF_L_.process(dL);
            dR = airLPF_R_.process(dR);
            dL = airLPF2_L_.process(dL);
            dR = airLPF2_R_.process(dR);
        }

        // Use blendedDistFrac (averaged from both nodes) in stereo, modDistFrac in mono
        const float effectiveDistFrac = blendedDistFrac;

        // Aux reverb send — auxMaxBoostLin pre-computed per-block (no std::pow per sample)
        if (auxL != nullptr) {
            auxPreDelayL_.push(dL);
            auxPreDelayR_.push(dR);
            const float auxDelaySamp = std::max(2.0f,
                effectiveDistFrac * currentParams.verbPreDelayMax * sr / 1000.0f);
            const float auxGainTarget = 1.0f + effectiveDistFrac * (auxMaxBoostLin - 1.0f);
            const float auxGain = auxGainSmooth_.process(auxGainTarget);
            auxL[i] = std::clamp(auxPreDelayL_.read(auxDelaySamp) * auxGain, -2.0f, 2.0f);
            auxR[i] = std::clamp(auxPreDelayR_.read(auxDelaySamp) * auxGain, -2.0f, 2.0f);
        } else {
            auxPreDelayL_.push(0.0f);
            auxPreDelayR_.push(0.0f);
            auxGainSmooth_.process(1.0f);
        }

        // Reverb
        {
            const float preDelaySamp = effectiveDistFrac
                * (currentParams.verbPreDelayMax * static_cast<float>(sampleRate) / 1000.0f);
            float wetL, wetR;
            reverb_.processSample(dL, dR, preDelaySamp, wetL, wetR);
            const float wetGain = verbWetSmooth_.process(currentParams.verbWet);
            dL += wetGain * wetL;
            dR += wetGain * wetR;
        }

        // Output clamp
        outL[i] = std::clamp(dL, -2.0f, 2.0f);
        outR[i] = std::clamp(dR, -2.0f, 2.0f);

        // DSP state capture (shared fields)
        lastDSPState_.sampleRate     = static_cast<float>(sampleRate);
        lastDSPState_.distDelaySamp  = lastDistDelaySamp_;
        lastDSPState_.distGainLinear = stereoActive ? 0.0f : distGainSmooth_.current();
        lastDSPState_.airCutoffHz    = stereoActive ? 0.0f : airCutoffMod;
        lastDSPState_.modX           = modX;
    }
}

// ============================================================================
// reset()
// ============================================================================

void XYZPanEngine::reset() {
    // Clear all delay line state — prevents ringing on transport restart.
    delayL_.reset();
    delayR_.reset();

    // Clear all filter state.
    shadowL_.reset();
    shadowR_.reset();
    rearSvfL_.reset();
    rearSvfR_.reset();

    // Reset smoothers to neutral values (no sudden parameter jumps after reset).
    itdSmooth_.reset(0.0f);
    shadowCutoffSmooth_.reset(kHeadShadowFullOpenHz);
    ildGainSmooth_.reset(1.0f);
    rearCutoffSmooth_.reset(kRearShadowFullOpenHz);

    // Reset tracking members so the next process() block re-evaluates them.
    lastSmoothMs_ITD_    = kDefaultSmoothMs_ITD;
    lastSmoothMs_Filter_ = kDefaultSmoothMs_Filter;
    lastSmoothMs_Gain_   = kDefaultSmoothMs_Gain;

    // Phase 3: comb bank
    for (auto& c : combBank_) c.reset();
    combWetSmooth_.reset(0.0f);

    // Phase 3: pinna EQ
    pinnaNotch_.reset();
    pinnaNotch2_.reset();
    pinnaP1_.reset();
    presenceShelf_.reset();
    earCanalPeak_.reset();
    pinnaShelf_.reset();

    // Phase 3: chest bounce
    for (auto& hp : chestHPF_) hp.reset();
    chestLP_.reset();
    chestDelay_.reset();
    chestGainSmooth_.reset(0.0f);

    // Phase 3: floor bounce
    floorDelayL_.reset();
    floorDelayR_.reset();
    floorLPF_.reset();
    floorGainSmooth_.reset(0.0f);

    // Phase 4: distance processing
    distDelayL_.reset();
    distDelayR_.reset();
    airLPF_L_.reset();
    airLPF_R_.reset();
    airLPF2_L_.reset();
    airLPF2_R_.reset();
    nearFieldLF_L_.reset();
    nearFieldLF_R_.reset();
    distDelaySmooth_.reset(2.0f);
    distGainSmooth_.reset(1.0f);
    lastDistDelaySamp_ = 2.0f;

    // Aux reverb send
    auxPreDelayL_.reset();
    auxPreDelayR_.reset();
    auxGainSmooth_.reset(1.0f);

    // Phase 5: reverb
    reverb_.reset();
    verbWetSmooth_.reset(kVerbDefaultWet);

    // Phase 5: LFO
    lfoX_.reset(currentParams.lfoXPhase);
    lfoY_.reset(currentParams.lfoYPhase);
    lfoZ_.reset(currentParams.lfoZPhase);
    lfoDepthXSmooth_.reset(0.0f);
    lfoDepthYSmooth_.reset(0.0f);
    lfoDepthZSmooth_.reset(0.0f);

    // Stereo source node splitting
    srcR_.reset();
    distR_.reset();
    orbitLfoXY_.reset(currentParams.stereoOrbitXYPhase);
    orbitLfoXZ_.reset(currentParams.stereoOrbitXZPhase);
    orbitLfoYZ_.reset(currentParams.stereoOrbitYZPhase);
    orbitDepthXYSmooth_.reset(0.0f);
    orbitDepthXZSmooth_.reset(0.0f);
    orbitDepthYZSmooth_.reset(0.0f);
    stereoWidthSmooth_.reset(0.0f);

    // Angular smoothers for circular phase/offset
    phaseSmCos_ = 1.0f; phaseSmSin_ = 0.0f;
    offsetSmCos_ = 1.0f; offsetSmSin_ = 0.0f;

    // Dev tool: test tone oscillator
    sawPhase_ = 0.0f;
    pulseLFO_.reset(0.0f);
}

} // namespace xyzpan
