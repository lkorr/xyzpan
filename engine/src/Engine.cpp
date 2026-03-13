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
    const float dist = computeDistance(x, y, currentParams.z);

    // ITD: sinusoidal scaling from |x| (Woodworth-inspired; only far ear delayed).
    // At X=0: 0 delay; at X=±1: maxITD_ms delay on the far ear.
    // Note: using X directly as sin-proxy since X = sin(azimuth) when Y=0.
    // When Y≠0, this underestimates lateral angle slightly — intentional creative choice.
    const float itdTarget = currentParams.maxITD_ms
                            * std::sin(std::abs(x) * (3.14159265f / 2.0f))
                            * sr / 1000.0f;

    // Head shadow: linear interpolation from fully open to minimum cutoff.
    // At X=0: kHeadShadowFullOpenHz (inaudible). At X=±1: headShadowMinHz.
    const float shadowCutoffTarget = kHeadShadowFullOpenHz
                                   + (currentParams.headShadowMinHz - kHeadShadowFullOpenHz)
                                   * std::abs(x);

    // ILD: proximity-weighted far-ear attenuation.
    // proximity=1 at kMinDistance (closest), proximity=0 at kSqrt3 (max distance).
    // Both azimuth and proximity must be non-zero for ILD to apply.
    const float proximity  = std::clamp(1.0f - (dist - kMinDistance) / (kSqrt3 - kMinDistance),
                                        0.0f, 1.0f);
    const float ildLinear  = std::pow(10.0f, -currentParams.ildMaxDb / 20.0f);
    const float ildTarget  = 1.0f - (1.0f - ildLinear) * std::abs(x) * proximity;

    // Rear shadow: linear ramp when Y < 0 (source behind listener).
    // At Y=0 to Y=1: no rear shadow. At Y=-1: full rear shadow.
    const float rearAmount        = std::max(0.0f, -y);
    const float rearCutoffTarget  = kRearShadowFullOpenHz
                                  + (currentParams.rearShadowMinHz - kRearShadowFullOpenHz)
                                  * rearAmount;

    // -------------------------------------------------------------------------
    // Per-sample loop: smooth parameters and apply binaural pipeline
    // -------------------------------------------------------------------------
    for (int i = 0; i < numSamples; ++i) {
        const float mono = monoIn[i];

        // Smooth parameters (one-pole IIR, audio-rate)
        const float itdSamples   = itdSmooth_.process(itdTarget);
        const float shadowCutoff = shadowCutoffSmooth_.process(shadowCutoffTarget);
        const float ildGain      = ildGainSmooth_.process(ildTarget);
        const float rearCutoff   = rearCutoffSmooth_.process(rearCutoffTarget);

        // Push mono into both delay lines
        delayL_.push(mono);
        delayR_.push(mono);

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
        // x > 0 (source right): right ear is near, left ear is far → delay left.
        // x < 0 (source left):  left ear is near, right ear is far → delay right.
        // x == 0: both at kMinDelay (no ITD).
        float dL, dR;
        if (x > 0.0f) {
            dL = delayL_.read(kMinDelay + itdSamples);
            dR = delayR_.read(kMinDelay);
        } else if (x < 0.0f) {
            dL = delayL_.read(kMinDelay);
            dR = delayR_.read(kMinDelay + itdSamples);
        } else {
            dL = delayL_.read(kMinDelay);
            dR = delayR_.read(kMinDelay);
        }

        // ILD: apply smoothed gain attenuation to far ear only.
        // Near ear remains at unity.
        if (x > 0.0f)       dL *= ildGain;
        else if (x < 0.0f)  dR *= ildGain;

        // Head shadow: update SVF coefficients and filter.
        // Far ear gets shadow cutoff; near ear stays wide open.
        // Per-sample coefficient update allows smooth cutoff modulation at audio rate.
        // (std::tan() is called here per sample — correct for smooth modulation;
        //  profile and optimize to per-N-samples if CPU budget is tight.)
        if (x >= 0.0f) {
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

        outL[i] = dL;
        outR[i] = dR;
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
}

} // namespace xyzpan
