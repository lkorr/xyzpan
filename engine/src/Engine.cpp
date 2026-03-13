#include "xyzpan/Engine.h"
#include "xyzpan/Coordinates.h"
#include "xyzpan/Constants.h"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace xyzpan {

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
    distDelaySmooth_.prepare(kDistSmoothMs, sr);
    distDelaySmooth_.reset(2.0f);
    distGainSmooth_.prepare(kDefaultSmoothMs_Gain, sr);
    distGainSmooth_.reset(1.0f);
    lastDistSmoothMs_ = kDistSmoothMs;

    // -------------------------------------------------------------------------
    // Phase 5: Reverb
    // -------------------------------------------------------------------------
    reverb_.prepare(inSampleRate, inMaxBlockSize);
    reverb_.setSize(kVerbDefaultSize);
    reverb_.setDecay(kVerbDefaultDecay);
    reverb_.setDamping(kVerbDefaultDamping);
    // FDNReverb's internal wetGain_ is set to 1.0 so processSample() returns
    // the raw reverb signal. The Engine applies smoothed wet gain externally
    // (verbWetSmooth_) so wet/dry transitions are click-free.
    reverb_.setWetDry(1.0f);
    reverb_.reset();
    verbWetSmooth_.prepare(kDefaultSmoothMs_Gain, sr);
    verbWetSmooth_.reset(kVerbDefaultWet);

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
}

// ============================================================================
// setParams()
// ============================================================================

void XYZPanEngine::setParams(const EngineParams& params) {
    currentParams = params;
}

// ============================================================================
// process()
// ============================================================================

void XYZPanEngine::process(const float* const* inputs, int numInputChannels,
                            float* outL, float* outR, int numSamples) {
    if (inputs == nullptr || inputs[0] == nullptr || outL == nullptr || outR == nullptr)
        return;

    // -------------------------------------------------------------------------
    // Stereo-to-mono sum (unchanged from Phase 1)
    // -------------------------------------------------------------------------
    const float* monoIn = inputs[0];

    if (numInputChannels >= 2 && inputs[1] != nullptr) {
        for (int i = 0; i < numSamples; ++i)
            monoBuffer[static_cast<size_t>(i)] = 0.5f * (inputs[0][i] + inputs[1][i]);
        monoIn = monoBuffer.data();
    }

    // -------------------------------------------------------------------------
    // Per-block preamble: re-prepare smoothers if time constants changed.
    //
    // OnePoleSmooth::prepare() only recomputes a_ and b_ from the new time
    // constant (one exp() call) — it does NOT reset z_ (no audible click).
    // Without this check, the dev panel smoothing controls would be non-functional
    // (smoothers stuck at initial prepare() values from the last hardware change).
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
    // Per-block target computation
    //
    // All targets are computed once per block from currentParams.
    // Per-sample smoothers then ramp toward these targets each sample.
    // -------------------------------------------------------------------------
    const float x    = currentParams.x;
    const float y    = currentParams.y;
    const float z    = currentParams.z;
    const float dist = computeDistance(x, y, z);

    // NOTE: ITD, head shadow, and ILD targets are now computed per-sample inside the
    // per-sample loop using the LFO-modulated position (modX). This enables smooth,
    // audio-rate LFO oscillation of all binaural cues.
    // The block-level `dist` and `proximity` are still used by downstream stages
    // (distance processing, reverb pre-delay) which are based on the base position.

    // Rear shadow: linear ramp when Y < 0 (source behind listener).
    // At Y=0 to Y=1: no rear shadow. At Y=-1: full rear shadow.
    const float rearAmount        = std::max(0.0f, -y);
    const float rearCutoffTarget  = kRearShadowFullOpenHz
                                  + (currentParams.rearShadowMinHz - kRearShadowFullOpenHz)
                                  * rearAmount;

    // -------------------------------------------------------------------------
    // Phase 3: per-block updates
    // -------------------------------------------------------------------------

    // Comb bank: update delays and feedback from currentParams.
    // Per-block only — delay/feedback changes are not audio-rate.
    for (int c = 0; c < kMaxCombFilters; ++c) {
        combBank_[c].setDelay(static_cast<int>(currentParams.combDelays_ms[c] * 0.001f * sr));
        combBank_[c].setFeedback(currentParams.combFeedback[c]);
    }

    // Comb wet target: scales linearly from 0 at Y=0 to combWetMax at Y=-1.
    // std::max(0, -y) gives 0 at front/center and 1 at full back.
    const float combWetTarget = currentParams.combWetMax * std::max(0.0f, -y);

    // Pinna notch + shelf: update biquad coefficients per block.
    // For Z < 0: freeze pinna at Z=0 values (z_clamped = 0).
    // For Z >= 0: lerp from -15 dB (Z=0) to +5 dB (Z=1).
    const float z_clamped    = std::max(0.0f, z);
    const float pinnaGainDb  = -15.0f + 20.0f * z_clamped;  // lerp(-15, +5, z_clamped)
    // High shelf: scales from 0 dB (Z=-1) to +3 dB (Z=0 and above), then held at +3dB.
    // std::clamp(z+1, 0, 1) gives 0 at Z=-1, 1 at Z=0+, so shelf ramps in only below horizon.
    const float shelfGainDb  = 3.0f * std::clamp(z + 1.0f, 0.0f, 1.0f);
    pinnaNotch_.setCoefficients(dsp::BiquadType::PeakingEQ,
        currentParams.pinnaNotchFreqHz, sr, currentParams.pinnaNotchQ, pinnaGainDb);
    pinnaShelf_.setCoefficients(dsp::BiquadType::HighShelf,
        currentParams.pinnaShelfFreqHz, sr, 0.7071f, shelfGainDb);

    // Chest bounce: delay and gain computed per block.
    // Delay: 0 ms at Z=1 (above), chestDelayMaxMs at Z=-1 (below horizon).
    // Gain: attenuated at Z=-1 and zero at Z=1.
    const float chestElevNorm   = std::clamp((-z + 1.0f) * 0.5f, 0.0f, 1.0f);
    const float chestDelayMs    = std::clamp((z + 1.0f) * 0.5f, 0.0f, 1.0f)
                                  * currentParams.chestDelayMaxMs;
    const float chestDelaySamp  = chestDelayMs * 0.001f * sr;
    const float chestLinearTarget = std::pow(10.0f, currentParams.chestGainDb / 20.0f) * chestElevNorm;

    // Floor bounce: delay and gain computed per block.
    const float floorElevNorm   = std::clamp((-z + 1.0f) * 0.5f, 0.0f, 1.0f);
    const float floorDelayMs    = std::clamp((z + 1.0f) * 0.5f, 0.0f, 1.0f)
                                  * currentParams.floorDelayMaxMs;
    const float floorDelaySamp  = floorDelayMs * 0.001f * sr;
    const float floorLinearTarget = std::pow(10.0f, currentParams.floorGainDb / 20.0f) * floorElevNorm;

    // -------------------------------------------------------------------------
    // Phase 4: per-block distance processing targets
    // -------------------------------------------------------------------------

    // Re-prepare distDelaySmooth_ if distSmoothMs param changed from dev panel.
    if (currentParams.distSmoothMs != lastDistSmoothMs_) {
        distDelaySmooth_.prepare(currentParams.distSmoothMs, sr);
        lastDistSmoothMs_ = currentParams.distSmoothMs;
    }

    // distFrac: 0 at kMinDistance (closest), 1 at kSqrt3 (furthest corner).
    const float distFrac = (dist - kMinDistance) / (kSqrt3 - kMinDistance);

    // Inverse-square gain: kDistGainRef/dist gives unity at dist=1.0 (Y=1 default position).
    // kDistGainMax caps the boost at +6dB for very close sources (prevents explosion).
    const float distGainTarget = std::clamp(kDistGainRef / dist, 0.0f, kDistGainMax);

    // Propagation delay: linearly maps distFrac to [0, distDelayMaxMs] (DIST-03).
    const float delayTargetMs = distFrac * currentParams.distDelayMaxMs;
    const float delayTargetSamples = std::max(2.0f,
        delayTargetMs * 0.001f * static_cast<float>(sampleRate));

    // Air absorption LPF cutoff: lerps from airAbsMaxHz at min to airAbsMinHz at max (DIST-02).
    const float airCutoffTarget = currentParams.airAbsMaxHz
        + (currentParams.airAbsMinHz - currentParams.airAbsMaxHz) * distFrac;
    airLPF_L_.setCoefficients(airCutoffTarget, static_cast<float>(sampleRate));
    airLPF_R_.setCoefficients(airCutoffTarget, static_cast<float>(sampleRate));

    const bool dopplerOn = currentParams.dopplerEnabled;

    // -------------------------------------------------------------------------
    // Phase 5: per-block reverb parameter updates
    // Size is fixed at prepare() time (changing delay lengths causes pitch artifacts).
    // Only decay and damping vary live at block rate.
    // -------------------------------------------------------------------------
    reverb_.setDecay(currentParams.verbDecay);
    reverb_.setDamping(currentParams.verbDamping);
    // wetGain is smoothed per-sample via verbWetSmooth_ (see process loop below)

    // -------------------------------------------------------------------------
    // Phase 5: LFO — set rate and waveform per block (not per sample).
    // When tempo sync is on, compute rate from hostBpm; else use free-running rate.
    // -------------------------------------------------------------------------
    auto lfoRate = [&](float freeHz, float beatDiv) -> float {
        if (currentParams.lfoTempoSync && currentParams.hostBpm > 0.0f)
            return (currentParams.hostBpm / 60.0f) * beatDiv;
        return freeHz;
    };
    lfoX_.waveform = static_cast<dsp::LFOWaveform>(currentParams.lfoXWaveform);
    lfoY_.waveform = static_cast<dsp::LFOWaveform>(currentParams.lfoYWaveform);
    lfoZ_.waveform = static_cast<dsp::LFOWaveform>(currentParams.lfoZWaveform);
    lfoX_.setRateHz(lfoRate(currentParams.lfoXRate, currentParams.lfoXBeatDiv));
    lfoY_.setRateHz(lfoRate(currentParams.lfoYRate, currentParams.lfoYBeatDiv));
    lfoZ_.setRateHz(lfoRate(currentParams.lfoZRate, currentParams.lfoZBeatDiv));

    // -------------------------------------------------------------------------
    // Per-sample loop: smooth parameters and apply binaural pipeline
    // -------------------------------------------------------------------------
    for (int i = 0; i < numSamples; ++i) {
        const float mono = monoIn[i];

        // ----------------------------------------------------------------
        // Phase 5: LFO — tick once per sample, modulate position BEFORE
        // coordinate conversion (binaural target recomputation).
        // ----------------------------------------------------------------
        const float depthX = lfoDepthXSmooth_.process(currentParams.lfoXDepth);
        const float depthY = lfoDepthYSmooth_.process(currentParams.lfoYDepth);
        const float depthZ = lfoDepthZSmooth_.process(currentParams.lfoZDepth);
        const float modX = std::clamp(currentParams.x + lfoX_.tick() * depthX, -1.0f, 1.0f);
        const float modY = std::clamp(currentParams.y + lfoY_.tick() * depthY, -1.0f, 1.0f);
        const float modZ = std::clamp(currentParams.z + lfoZ_.tick() * depthZ, -1.0f, 1.0f);

        // Recompute proximity-scaled binaural targets per-sample from LFO-modulated position.
        // This ensures all binaural cues (ITD, ILD, head shadow) track the LFO oscillation.
        const float modDist      = computeDistance(modX, modY, modZ);
        const float modProximity = std::clamp(
            1.0f - (modDist - kMinDistance) / (kSqrt3 - kMinDistance), 0.0f, 1.0f);
        const float itdTargetMod = currentParams.maxITD_ms
                                   * std::sin(std::abs(modX) * (3.14159265f / 2.0f))
                                   * modProximity * sr / 1000.0f;
        const float shadowCutoffTargetMod = kHeadShadowFullOpenHz
                                          + (currentParams.headShadowMinHz - kHeadShadowFullOpenHz)
                                          * std::abs(modX) * modProximity;
        const float ildTargetMod = 1.0f - (1.0f - std::pow(10.0f, -currentParams.ildMaxDb / 20.0f))
                                   * std::abs(modX) * modProximity;
        (void)modY; // modY/modZ available for future per-sample use if needed
        (void)modZ;

        // Phase 6: capture last-sample modulated position for PositionBridge (UI-07).
        // Store every sample; the final value after the loop is what the GL thread reads.
        lastModulated_ = {modX, modY, modZ};

        // ----------------------------------------------------------------
        // Phase 3: Comb bank (DEPTH) — Y-driven dry/wet blend
        // ----------------------------------------------------------------
        const float combWet = combWetSmooth_.process(combWetTarget);
        float combSig = mono;
        for (int c = 0; c < kMaxCombFilters; ++c)
            combSig = combBank_[c].process(combSig);
        const float depthOut = mono * (1.0f - combWet) + combSig * combWet;

        // ----------------------------------------------------------------
        // Phase 3: Pinna notch + high shelf (ELEV-01, ELEV-02)
        // ----------------------------------------------------------------
        float monoEQ = pinnaNotch_.process(depthOut);
        monoEQ       = pinnaShelf_.process(monoEQ);

        // Smooth parameters (one-pole IIR, audio-rate) using LFO-modulated targets.
        const float itdSamples   = itdSmooth_.process(itdTargetMod);
        const float shadowCutoff = shadowCutoffSmooth_.process(shadowCutoffTargetMod);
        const float ildGain      = ildGainSmooth_.process(ildTargetMod);
        const float rearCutoff   = rearCutoffSmooth_.process(rearCutoffTarget);

        // Push pinna-EQ'd mono into both binaural delay lines
        delayL_.push(monoEQ);
        delayR_.push(monoEQ);

        // Minimum read delay of 2.0 samples ensures the Hermite interpolation
        // never reads from "future" positions in the ring buffer (the C and D
        // Catmull-Rom points would be at writePos_ and writePos_+1 for delay<2,
        // which contain stale data from the previous ring buffer cycle).
        // Both ears share the same base delay of 2.0 so the ITD difference is
        // preserved exactly:  far_ear_delay = 2.0 + itdSamples (extra ITD from far ear)
        //                     near_ear_delay = 2.0             (identical base)
        // The 2-sample absolute offset is equal for both ears and introduces no ITD.
        constexpr float kMinDelay = 2.0f;

        // Read delay lines: far ear gets smoothed ITD delay, near ear reads at kMinDelay.
        // modX > 0 (source right): right ear is near, left ear is far → delay left.
        // modX < 0 (source left):  left ear is near, right ear is far → delay right.
        // modX == 0: both at kMinDelay (no ITD).
        float dL, dR;
        if (modX > 0.0f) {
            dL = delayL_.read(kMinDelay + itdSamples);
            dR = delayR_.read(kMinDelay);
        } else if (modX < 0.0f) {
            dL = delayL_.read(kMinDelay);
            dR = delayR_.read(kMinDelay + itdSamples);
        } else {
            dL = delayL_.read(kMinDelay);
            dR = delayR_.read(kMinDelay);
        }

        // Safety: NaN/Inf guard — if binaural delay output is non-finite, zero it.
        // Prevents feedback spirals from propagating through all downstream stages.
        if (!std::isfinite(dL)) dL = 0.0f;
        if (!std::isfinite(dR)) dR = 0.0f;

        // ILD: apply smoothed gain attenuation to far ear only.
        // Near ear remains at unity.
        if (modX > 0.0f)       dL *= ildGain;
        else if (modX < 0.0f)  dR *= ildGain;

        // Head shadow: update SVF coefficients and filter.
        // Far ear gets shadow cutoff; near ear stays wide open.
        // Per-sample coefficient update allows smooth cutoff modulation at audio rate.
        // (std::tan() is called here per sample — correct for smooth modulation;
        //  profile and optimize to per-N-samples if CPU budget is tight.)
        if (modX >= 0.0f) {
            // Source right or center: left is far
            shadowL_.setCoefficients(shadowCutoff, sr);
            shadowR_.setCoefficients(kHeadShadowFullOpenHz, sr);
        } else {
            // Source left: right is far
            shadowL_.setCoefficients(kHeadShadowFullOpenHz, sr);
            shadowR_.setCoefficients(shadowCutoff, sr);
        }
        dL = shadowL_.process(dL);
        dR = shadowR_.process(dR);

        // Rear shadow: applied equally to both ears.
        rearSvfL_.setCoefficients(rearCutoff, sr);
        rearSvfR_.setCoefficients(rearCutoff, sr);
        dL = rearSvfL_.process(dL);
        dR = rearSvfR_.process(dR);

        // ----------------------------------------------------------------
        // Phase 3: Chest bounce (ELEV-03)
        // Processes the ORIGINAL mono input (not pinna-EQ'd) — chest bounce
        // is a physical reflection before the pinna path.
        // 4x HP cascade (700 Hz) + 1x LP (1 kHz) + delay + gain
        // ----------------------------------------------------------------
        {
            float chestSig = mono;
            for (auto& hp : chestHPF_)
                chestSig = hp.process(chestSig);
            chestSig = chestLP_.process(chestSig);
            chestDelay_.push(chestSig);

            const float chestGain = chestGainSmooth_.process(chestLinearTarget);
            // Use at least 2 samples of delay to ensure valid Hermite read positions.
            // When the computed delay is near zero (Z near -1), clamp to minimum
            // so the bounce is still audible at its maximum gain position.
            const float chestReadSamp = std::max(2.0f, chestDelaySamp);
            if (chestGain > 1e-6f) {
                float chestOut = chestDelay_.read(chestReadSamp) * chestGain;
                dL += chestOut;
                dR += chestOut;
            }
        }

        // ----------------------------------------------------------------
        // Phase 3: Floor bounce (ELEV-04)
        // Per-ear delayed copy of the post-binaural stereo signal.
        // ----------------------------------------------------------------
        {
            floorDelayL_.push(dL);
            floorDelayR_.push(dR);

            const float floorGain = floorGainSmooth_.process(floorLinearTarget);
            // Use at least 2 samples of delay to ensure valid Hermite read positions.
            // When the computed delay is near zero (Z near -1), clamp to minimum
            // so the bounce is still audible at its maximum gain position.
            const float floorReadSamp = std::max(2.0f, floorDelaySamp);
            if (floorGain > 1e-6f) {
                dL += floorDelayL_.read(floorReadSamp) * floorGain;
                dR += floorDelayR_.read(floorReadSamp) * floorGain;
            }
        }

        // ----------------------------------------------------------------
        // Phase 4: Distance Processing (DIST-01 through DIST-06)
        // Signal chain order: gain -> delay+doppler -> air absorption LPF
        // Gain first (quieter signal into delay line), then delay, then LPF
        // (air absorption filters the arriving signal after doppler shift).
        // ----------------------------------------------------------------

        // DIST-01: Inverse-square gain attenuation
        const float distGain = distGainSmooth_.process(distGainTarget);
        dL *= distGain;
        dR *= distGain;

        // DIST-03, DIST-04, DIST-05, DIST-06: Propagation delay + doppler
        distDelayL_.push(dL);
        distDelayR_.push(dR);
        if (dopplerOn) {
            // Smooth delay target — ramping delay creates doppler pitch shift (DIST-04)
            const float delaySamp = std::max(2.0f, distDelaySmooth_.process(delayTargetSamples));
            dL = distDelayL_.read(delaySamp);
            dR = distDelayR_.read(delaySamp);
        } else {
            // DIST-05: Doppler off — keep smoother state valid, read at minimum delay
            distDelaySmooth_.process(2.0f);
            dL = distDelayL_.read(2.0f);
            dR = distDelayR_.read(2.0f);
        }

        // DIST-02: Air absorption LPF (after doppler so shifted signal gets filtered)
        dL = airLPF_L_.process(dL);
        dR = airLPF_R_.process(dR);

        // ----------------------------------------------------------------
        // Phase 5: FDN Reverb (VERB-01, VERB-02) — final stereo stage.
        // Pre-delay in samples scales with distance: distFrac * verbPreDelayMax.
        // distFrac = (distance - kMinDistance) / (kSqrt3 - kMinDistance) clamped [0,1].
        // ----------------------------------------------------------------
        {
            const float reverbDistFrac = std::clamp(
                (dist - kMinDistance) / (kSqrt3 - kMinDistance), 0.0f, 1.0f);
            const float preDelaySamp = reverbDistFrac
                * (currentParams.verbPreDelayMax * static_cast<float>(sampleRate) / 1000.0f);
            float wetL, wetR;
            reverb_.processSample(dL, dR, preDelaySamp, wetL, wetR);
            const float wetGain = verbWetSmooth_.process(currentParams.verbWet);
            dL += wetGain * wetL;
            dR += wetGain * wetR;
        }

        // Hard output clamp: prevents NaN/Inf or runaway gain from reaching the DAW.
        // ±2.0 headroom allows legitimate +6dB boost without hard-clipping normal material.
        outL[i] = std::clamp(dL, -2.0f, 2.0f);
        outR[i] = std::clamp(dR, -2.0f, 2.0f);
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
    pinnaShelf_.reset();

    // Phase 3: chest bounce
    for (auto& hp : chestHPF_) hp.reset();
    chestLP_.reset();
    chestDelay_.reset();
    chestGainSmooth_.reset(0.0f);

    // Phase 3: floor bounce
    floorDelayL_.reset();
    floorDelayR_.reset();
    floorGainSmooth_.reset(0.0f);

    // Phase 4: distance processing
    distDelayL_.reset();
    distDelayR_.reset();
    airLPF_L_.reset();
    airLPF_R_.reset();
    distDelaySmooth_.reset(2.0f);
    distGainSmooth_.reset(1.0f);

    // Phase 5: reverb
    reverb_.reset();
    verbWetSmooth_.reset(kVerbDefaultWet);

    // Phase 5: LFO
    lfoX_.reset(0.0f);
    lfoY_.reset(0.0f);
    lfoZ_.reset(0.0f);
    lfoDepthXSmooth_.reset(0.0f);
    lfoDepthYSmooth_.reset(0.0f);
    lfoDepthZSmooth_.reset(0.0f);
}

} // namespace xyzpan
