#include "xyzpan/Engine.h"
#include "xyzpan/Coordinates.h"
#include "xyzpan/Constants.h"
#include "xyzpan/dsp/SineLUT.h"
#include "xyzpan/dsp/FastMath.h"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace xyzpan {

// Distance-difference azimuth: virtual ears at (±h, 0, 0) in listener-relative space.
// Returns signed factor: +1 = right, -1 = left, 0 = median plane.
static inline float computeAzimuthFactor(float x, float y, float z, float h) {
    if (h < 1e-7f) return 0.0f;
    const float yz2 = y * y + z * z;
    const float distLeft  = dsp::fastSqrt((x + h) * (x + h) + yz2);
    const float distRight = dsp::fastSqrt((x - h) * (x - h) + yz2);
    const float delta = distLeft - distRight;  // positive when source is right of center
    return std::clamp(delta / (2.0f * h), -1.0f, 1.0f);
}

// Distance-difference rear factor: virtual ears at (0, ±h, 0) in listener-relative space.
// Returns signed factor: +1 = rear, -1 = front, 0 = interaural plane.
static inline float computeRearFactor(float x, float y, float z, float h) {
    if (h < 1e-7f) return 0.0f;
    const float xz2 = x * x + z * z;
    const float distFront = dsp::fastSqrt(xz2 + (y - h) * (y - h));
    const float distBack  = dsp::fastSqrt(xz2 + (y + h) * (y + h));
    const float delta = distFront - distBack;  // positive when source is behind
    return std::clamp(delta / (2.0f * h), -1.0f, 1.0f);
}

// Elevation factor from distance-difference between top/bottom virtual ear nodes.
// Returns 0.0 (nadir) to 1.0 (zenith), matching the old atan2-based range.
static inline float computeElevFactor(float x, float y, float z, float earOffset) {
    const float h = earOffset;
    if (h < 1e-7f) return 0.5f;
    const float xy2 = x * x + y * y;
    const float distTop    = dsp::fastSqrt(xy2 + (z - h) * (z - h));
    const float distBottom = dsp::fastSqrt(xy2 + (z + h) * (z + h));
    const float delta = distBottom - distTop;
    const float maxDelta = 2.0f * h;
    return std::clamp(delta / maxDelta * 0.5f + 0.5f, 0.0f, 1.0f);
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

    int delayCap = static_cast<int>(kMaxITDUpperBound_ms * 0.001f * sr) + 8;
    src_.prepare(sr, delayCap, kCombMaxDelay_ms);

    // Sync tracking members.
    lastSmoothMs_ITD_    = kDefaultSmoothMs_ITD;
    lastSmoothMs_Filter_ = kDefaultSmoothMs_Filter;
    lastSmoothMs_Gain_   = kDefaultSmoothMs_Gain;

    // -------------------------------------------------------------------------
    // Phase 3: Chest bounce
    // -------------------------------------------------------------------------
    chest_.prepare(sr);

    // -------------------------------------------------------------------------
    // Phase 3: Floor bounce
    // -------------------------------------------------------------------------
    floor_.prepare(sr);

    // -------------------------------------------------------------------------
    // Phase 4: Distance Processing
    // -------------------------------------------------------------------------
    dist_.prepare(sr);
    lastDistSmoothMs_ = kDistSmoothMs;

    // -------------------------------------------------------------------------
    // Phase 5: Reverb
    // -------------------------------------------------------------------------
    reverb_.prepare(inSampleRate, inMaxBlockSize);
    reverb_.setSize(kVerbDefaultSize);
    reverb_.setDecay(kVerbDefaultDecay);
    reverb_.setDamping(kVerbDefaultDamping);
    reverb_.setDiffusion(kVerbDefaultDiffusion);
    reverb_.setModDepth(kVerbDefaultModDepth);
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
    auxDelaySmooth_.prepare(kDefaultSmoothMs_Gain, sr);
    auxDelaySmooth_.reset(2.0f);

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
    chestR_.prepare(sr);
    floorR_.prepare(sr);
    erR_.prepare(sr);

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

    // Block-rate IIR coefficients for listener rotation smoothing.
    {
        const float blockPeriod = static_cast<float>(inMaxBlockSize) / sr;
        // During movement: 5ms time constant (tracks rapidly but still click-free).
        constexpr float kTrackMs = 5.0f;
        listenerMovSmA_ = std::exp(-blockPeriod / (kTrackMs * 0.001f));
        // After movement stops: 20ms time constant → converges within ~100ms.
        constexpr float kConvergeMs = 20.0f;
        listenerBlkSmA_ = std::exp(-blockPeriod / (kConvergeMs * 0.001f));
    }

    // Angular smoothers for listener yaw/pitch/roll (continuous knobs, 5ms)
    yawSmCos_ = 1.0f; yawSmSin_ = 0.0f;
    pitchSmCos_ = 1.0f; pitchSmSin_ = 0.0f;
    rollSmCos_ = 1.0f; rollSmSin_ = 0.0f;

    // Previous block trig for per-sample interpolation (identity = no rotation)
    prevCosY_ = 1.f; prevSinY_ = 0.f;
    prevCosP_ = 1.f; prevSinP_ = 0.f;
    prevCosR_ = 1.f; prevSinR_ = 0.f;

    // -------------------------------------------------------------------------
    // Early Reflections (Image Source Method)
    // -------------------------------------------------------------------------
    {
        er_.prepare(sr);
        erLevelSmooth_.prepare(kDefaultSmoothMs_Gain, sr);
        erLevelSmooth_.reset(0.0f);
        erReverbSendSmooth_.prepare(kDefaultSmoothMs_Gain, sr);
        erReverbSendSmooth_.reset(kERReverbSendDefault);
    }

    // Binaural toggle smoother — 5ms for click-free transition
    binauralBlendSmooth_.prepare(5.0f, sr);
    binauralBlendSmooth_.reset(1.0f);

    // Dev tool: test tone pulse LFO + noise RNG
    pulseLFO_.prepare(inSampleRate);
    pulseLFO_.waveform = dsp::LFOWaveform::Square;
    pulseLFO_.reset(0.0f);
    noiseRng_.seed(12345u);

    // Test tone gain — start muted so first block ramps up smoothly if enabled
    testToneGainSmooth_.prepare(kDefaultSmoothMs_Gain, sr);
    testToneGainSmooth_.reset(0.0f);
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
                            float* outL, float* outR,
                            float* auxL, float* auxR,
                            int numSamples) {
    if (inputs == nullptr || inputs[0] == nullptr || outL == nullptr || outR == nullptr)
        return;

    if (numSamples > maxBlockSize || numSamples <= 0) {
        std::memset(outL, 0, sizeof(float) * static_cast<size_t>(numSamples > 0 ? numSamples : 0));
        std::memset(outR, 0, sizeof(float) * static_cast<size_t>(numSamples > 0 ? numSamples : 0));
        if (auxL) std::memset(auxL, 0, sizeof(float) * static_cast<size_t>(numSamples > 0 ? numSamples : 0));
        if (auxR) std::memset(auxR, 0, sizeof(float) * static_cast<size_t>(numSamples > 0 ? numSamples : 0));
        return;
    }

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
        src_.itdSmooth.prepare(currentParams.smoothMs_ITD, sr);
        lastSmoothMs_ITD_ = currentParams.smoothMs_ITD;
    }
    if (currentParams.smoothMs_Filter != lastSmoothMs_Filter_) {
        src_.shadowCutoffSmooth.prepare(currentParams.smoothMs_Filter, sr);
        src_.rearCutoffSmooth.prepare(currentParams.smoothMs_Filter, sr);
        lastSmoothMs_Filter_ = currentParams.smoothMs_Filter;
    }
    if (currentParams.smoothMs_Gain != lastSmoothMs_Gain_) {
        src_.ildGainSmooth.prepare(currentParams.smoothMs_Gain, sr);
        lastSmoothMs_Gain_ = currentParams.smoothMs_Gain;
    }

    // -------------------------------------------------------------------------
    // Phase 3: per-block updates (position-independent only)
    // -------------------------------------------------------------------------
    for (int c = 0; c < kMaxCombFilters; ++c) {
        src_.combBank[c].setDelay(static_cast<int>(currentParams.combDelays_ms[c] * 0.001f * sr));
        src_.combBank[c].setFeedback(currentParams.combFeedback[c]);
        // Also update srcR_ comb bank with same coefficients
        srcR_.combBank[c].setDelay(static_cast<int>(currentParams.combDelays_ms[c] * 0.001f * sr));
        srcR_.combBank[c].setFeedback(currentParams.combFeedback[c]);
    }

    src_.pinnaP1.setCoefficients(dsp::BiquadType::PeakingEQ,
        currentParams.pinnaP1FreqHz, sr, currentParams.pinnaP1Q, currentParams.pinnaP1GainDb);

    // -------------------------------------------------------------------------
    // Phase 4: per-block distance processing
    // -------------------------------------------------------------------------
    if (currentParams.distSmoothMs != lastDistSmoothMs_) {
        dist_.distDelaySmooth.prepare(currentParams.distSmoothMs, sr);
        distR_.distDelaySmooth.prepare(currentParams.distSmoothMs, sr);
        lastDistSmoothMs_ = currentParams.distSmoothMs;
    }

    const bool dopplerOn = currentParams.dopplerEnabled;

    // -------------------------------------------------------------------------
    // Phase 5: per-block reverb parameter updates
    // -------------------------------------------------------------------------
    reverb_.setDecay(currentParams.verbDecay);
    reverb_.setDamping(currentParams.verbDamping);
    reverb_.setDiffusion(currentParams.verbDiffusion);
    reverb_.setModDepth(currentParams.verbModDepth);

    // -------------------------------------------------------------------------
    // Phase 5: LFO — set rate and waveform per block
    // -------------------------------------------------------------------------
    // beatDiv is in BARS. One bar = hostTimeSigNum beats (quarter-note beats when
    // denominator is 4). Seconds per bar = hostTimeSigNum * (60 / BPM) * (4 / den).
    // LFO freq = 1 / (beatDiv * secondsPerBar).
    auto lfoRate = [&](float freeHz, float beatDiv, bool tempoSync) -> float {
        if (tempoSync && currentParams.hostBpm > 0.0f && beatDiv > 0.0f) {
            const int num = currentParams.hostTimeSigNum > 0 ? currentParams.hostTimeSigNum : 4;
            const int den = currentParams.hostTimeSigDen > 0 ? currentParams.hostTimeSigDen : 4;
            // secondsPerBar = num * (4.0 / den) * (60.0 / bpm)
            const float secondsPerBar = static_cast<float>(num) * (4.0f / static_cast<float>(den))
                                      * (60.0f / currentParams.hostBpm);
            return 1.0f / (beatDiv * secondsPerBar);
        }
        return freeHz;
    };
    lfoX_.waveform = static_cast<dsp::LFOWaveform>(currentParams.lfoXWaveform);
    lfoY_.waveform = static_cast<dsp::LFOWaveform>(currentParams.lfoYWaveform);
    lfoZ_.waveform = static_cast<dsp::LFOWaveform>(currentParams.lfoZWaveform);
    lfoX_.setRateHz(lfoRate(currentParams.lfoXRate, currentParams.lfoXBeatDiv, currentParams.lfoXTempoSync) * (currentParams.lfoXTempoSync ? 1.0f : currentParams.lfoSpeedMul));
    lfoY_.setRateHz(lfoRate(currentParams.lfoYRate, currentParams.lfoYBeatDiv, currentParams.lfoYTempoSync) * (currentParams.lfoYTempoSync ? 1.0f : currentParams.lfoSpeedMul));
    lfoZ_.setRateHz(lfoRate(currentParams.lfoZRate, currentParams.lfoZBeatDiv, currentParams.lfoZTempoSync) * (currentParams.lfoZTempoSync ? 1.0f : currentParams.lfoSpeedMul));
    lfoX_.setPhaseOffset(currentParams.lfoXPhase);
    lfoY_.setPhaseOffset(currentParams.lfoYPhase);
    lfoZ_.setPhaseOffset(currentParams.lfoZPhase);
    lfoX_.setSmoothMs(currentParams.lfoXSmooth * 300.0f);  // 0-1 → 0-300ms
    lfoY_.setSmoothMs(currentParams.lfoYSmooth * 300.0f);
    lfoZ_.setSmoothMs(currentParams.lfoZSmooth * 300.0f);
    if (currentParams.lfoXResetPhase) lfoX_.requestReset();
    if (currentParams.lfoYResetPhase) lfoY_.requestReset();
    if (currentParams.lfoZResetPhase) lfoZ_.requestReset();

    // -------------------------------------------------------------------------
    // Stereo orbit LFOs — per-block setup
    // -------------------------------------------------------------------------
    orbitLfoXY_.waveform = static_cast<dsp::LFOWaveform>(currentParams.stereoOrbitXYWaveform);
    orbitLfoXZ_.waveform = static_cast<dsp::LFOWaveform>(currentParams.stereoOrbitXZWaveform);
    orbitLfoYZ_.waveform = static_cast<dsp::LFOWaveform>(currentParams.stereoOrbitYZWaveform);
    orbitLfoXY_.setRateHz(lfoRate(currentParams.stereoOrbitXYRate, currentParams.stereoOrbitXYBeatDiv, currentParams.stereoOrbitXYTempoSync) * (currentParams.stereoOrbitXYTempoSync ? 1.0f : currentParams.stereoOrbitSpeedMul));
    orbitLfoXZ_.setRateHz(lfoRate(currentParams.stereoOrbitXZRate, currentParams.stereoOrbitXZBeatDiv, currentParams.stereoOrbitXZTempoSync) * (currentParams.stereoOrbitXZTempoSync ? 1.0f : currentParams.stereoOrbitSpeedMul));
    orbitLfoYZ_.setRateHz(lfoRate(currentParams.stereoOrbitYZRate, currentParams.stereoOrbitYZBeatDiv, currentParams.stereoOrbitYZTempoSync) * (currentParams.stereoOrbitYZTempoSync ? 1.0f : currentParams.stereoOrbitSpeedMul));
    orbitLfoXY_.setPhaseOffset(currentParams.stereoOrbitXYPhase);
    orbitLfoXZ_.setPhaseOffset(currentParams.stereoOrbitXZPhase);
    orbitLfoYZ_.setPhaseOffset(currentParams.stereoOrbitYZPhase);
    orbitLfoXY_.setSmoothMs(currentParams.stereoOrbitXYSmooth * 300.0f);
    orbitLfoXZ_.setSmoothMs(currentParams.stereoOrbitXZSmooth * 300.0f);
    orbitLfoYZ_.setSmoothMs(currentParams.stereoOrbitYZSmooth * 300.0f);
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
    // kHardpanMaxDb is constexpr — computed once, not every block
    static const float kHardpanGainLin = std::pow(10.0f, kHardpanMaxDb / 20.0f);
    hardpanGainBase_           = kHardpanGainLin;
    const float auxMaxBoostLin = std::pow(10.0f, currentParams.auxSendGainMaxDb / 20.0f);
    blkDistGainMaxDb_          = 20.0f * std::log10(currentParams.distGainMax);

    // Per-block chest filter coefficient update (cheap: only recalc when params change)
    {
        const float chestHPF = currentParams.chestHPFHz;
        const float chestLP  = currentParams.chestLPHz;
        for (auto& hp : chest_.hpf)
            hp.setCoefficients(chestHPF, sr);
        chest_.lp.setCoefficients(chestLP, sr);
        // R-channel chest pipeline (stereo node splitting)
        for (auto& hp : chestR_.hpf)
            hp.setCoefficients(chestHPF, sr);
        chestR_.lp.setCoefficients(chestLP, sr);
    }

    // Per-block scaling factor pre-computation (avoids per-sample multiply)
    chest_.setBlockConstants(sr, currentParams.chestDelayMaxMs);
    chestR_.setBlockConstants(sr, currentParams.chestDelayMaxMs);
    floor_.setBlockConstants(sr, currentParams.floorDelayMaxMs);
    floorR_.setBlockConstants(sr, currentParams.floorDelayMaxMs);
    dist_.setBlockConstants(sr, currentParams.distDelayMaxMs);
    distR_.setBlockConstants(sr, currentParams.distDelayMaxMs);

    // Block-start position: peek LFO at block start (no phase advance) so
    // per-block filter coefficients track the modulated position.  LFO delta
    // within one 64-128 sample block is negligible for coefficient purposes.
    float blkX = currentParams.x + lfoX_.peek() * lfoDepthXSmooth_.current();
    float blkY = currentParams.y + lfoY_.peek() * lfoDepthYSmooth_.current();
    float blkZ = currentParams.z + lfoZ_.peek() * lfoDepthZSmooth_.current();

    // Walker mode: subtract listener position before head rotation so all
    // downstream DSP (pinna, presence, distance, air absorption) operates
    // on listener-relative coordinates.
    blkX -= currentParams.listenerX;
    blkY -= currentParams.listenerY;
    blkZ -= currentParams.listenerZ;

    // Listener head rotation — angular-smooth yaw/pitch/roll to handle 360°↔0° wrap.
    // While the parameter is actively changing, the IIR tracks the target using the
    // per-sample coefficient (for circle-wrap handling).  Once movement stops, the
    // IIR continues running with a block-rate coefficient (~100ms convergence) so
    // the transition is smooth.  Once converged, snap to exact target for
    // deterministic steady-state.
    float cosY, sinY, cosP, sinP, cosR, sinR;
    bool listenerFlipSnap = false;
    {
        const bool listenerParamsChanged =
            (currentParams.listenerYaw   != prevListenerYaw_) ||
            (currentParams.listenerPitch != prevListenerPitch_) ||
            (currentParams.listenerRoll  != prevListenerRoll_);

        if (listenerParamsChanged) {
            // Snap-on-flip: when pitch crosses ±90° via mouse drag, quatToRPY
            // (ui/QuatMath.h) jumps yaw and roll by ~π simultaneously. The physical
            // orientation is continuous, but its Euler representation isn't — so
            // IIR-ramping the cos/sin pairs through the flip would collapse the
            // rotation matrix mid-block (click). Detect and snap instead.
            auto wrapPi = [](float a) {
                constexpr float kTwoPi = 6.28318530717958647692f;
                constexpr float kPi    = 3.14159265358979323846f;
                a = std::fmod(a + kPi, kTwoPi);
                if (a < 0.0f) a += kTwoPi;
                return a - kPi;
            };
            constexpr float kHalfPi   = 1.57079632679489661923f;
            constexpr float kPi       = 3.14159265358979323846f;
            constexpr float kFlipTol  = 0.35f;   // ~20° around the ideal ±π jump
            constexpr float kPitchTol = 0.30f;   // only near the pole
            const float dYaw  = wrapPi(currentParams.listenerYaw  - prevListenerYaw_);
            const float dRoll = wrapPi(currentParams.listenerRoll - prevListenerRoll_);
            listenerFlipSnap =
                std::abs(std::abs(dYaw)  - kPi) < kFlipTol &&
                std::abs(std::abs(dRoll) - kPi) < kFlipTol &&
                std::abs(std::abs(currentParams.listenerPitch) - kHalfPi) < kPitchTol;

            prevListenerYaw_   = currentParams.listenerYaw;
            prevListenerPitch_ = currentParams.listenerPitch;
            prevListenerRoll_  = currentParams.listenerRoll;
            listenerSettled_ = false;

            if (listenerFlipSnap) {
                // Bypass IIR: write targets directly so rotation matrix stays unit.
                yawSmCos_   = dsp::SineLUT::cosLookupAngle(currentParams.listenerYaw);
                yawSmSin_   = dsp::SineLUT::lookupAngle   (currentParams.listenerYaw);
                pitchSmCos_ = dsp::SineLUT::cosLookupAngle(currentParams.listenerPitch);
                pitchSmSin_ = dsp::SineLUT::lookupAngle   (currentParams.listenerPitch);
                rollSmCos_  = dsp::SineLUT::cosLookupAngle(currentParams.listenerRoll);
                rollSmSin_  = dsp::SineLUT::lookupAngle   (currentParams.listenerRoll);
            } else {
                // While knob is moving: block-rate IIR (5ms time constant, tracks rapidly)
                const float aM = listenerMovSmA_;
                const float bM = 1.0f - aM;
                yawSmCos_   = dsp::SineLUT::cosLookupAngle(currentParams.listenerYaw)   * bM + yawSmCos_   * aM;
                yawSmSin_   = dsp::SineLUT::lookupAngle(currentParams.listenerYaw)       * bM + yawSmSin_   * aM;
                pitchSmCos_ = dsp::SineLUT::cosLookupAngle(currentParams.listenerPitch) * bM + pitchSmCos_ * aM;
                pitchSmSin_ = dsp::SineLUT::lookupAngle(currentParams.listenerPitch)     * bM + pitchSmSin_ * aM;
                rollSmCos_  = dsp::SineLUT::cosLookupAngle(currentParams.listenerRoll)  * bM + rollSmCos_  * aM;
                rollSmSin_  = dsp::SineLUT::lookupAngle(currentParams.listenerRoll)      * bM + rollSmSin_  * aM;
            }

            // Normalize IIR vectors to extract cos/sin directly
            auto normCS = [](float c, float s, float& outCos, float& outSin) {
                const float mag = std::sqrt(c * c + s * s);
                const float inv = (mag > 1e-12f) ? 1.0f / mag : 1.0f;
                outCos = c * inv;
                outSin = s * inv;
            };
            normCS(yawSmCos_,   yawSmSin_,   cosY, sinY);
            normCS(pitchSmCos_, pitchSmSin_, cosP, sinP);
            normCS(rollSmCos_,  rollSmSin_,  cosR, sinR);

            cachedCosY_ = cosY; cachedSinY_ = sinY;
            cachedCosP_ = cosP; cachedSinP_ = sinP;
            cachedCosR_ = cosR; cachedSinR_ = sinR;
        } else if (!listenerSettled_) {
            // Knob stopped — continue IIR toward target with block-rate coefficient
            // for smooth convergence (~100ms).
            const float aBlk = listenerBlkSmA_;
            const float bBlk = 1.0f - aBlk;
            const float tYC = dsp::SineLUT::cosLookupAngle(currentParams.listenerYaw);
            const float tYS = dsp::SineLUT::lookupAngle(currentParams.listenerYaw);
            const float tPC = dsp::SineLUT::cosLookupAngle(currentParams.listenerPitch);
            const float tPS = dsp::SineLUT::lookupAngle(currentParams.listenerPitch);
            const float tRC = dsp::SineLUT::cosLookupAngle(currentParams.listenerRoll);
            const float tRS = dsp::SineLUT::lookupAngle(currentParams.listenerRoll);

            yawSmCos_   = tYC * bBlk + yawSmCos_   * aBlk;
            yawSmSin_   = tYS * bBlk + yawSmSin_   * aBlk;
            pitchSmCos_ = tPC * bBlk + pitchSmCos_ * aBlk;
            pitchSmSin_ = tPS * bBlk + pitchSmSin_ * aBlk;
            rollSmCos_  = tRC * bBlk + rollSmCos_  * aBlk;
            rollSmSin_  = tRS * bBlk + rollSmSin_  * aBlk;

            auto normCS = [](float c, float s, float& outCos, float& outSin) {
                const float mag = std::sqrt(c * c + s * s);
                const float inv = (mag > 1e-12f) ? 1.0f / mag : 1.0f;
                outCos = c * inv;
                outSin = s * inv;
            };
            normCS(yawSmCos_,   yawSmSin_,   cosY, sinY);
            normCS(pitchSmCos_, pitchSmSin_, cosP, sinP);
            normCS(rollSmCos_,  rollSmSin_,  cosR, sinR);

            // Snap to exact when close enough (avoid infinite asymptotic tail)
            constexpr float kSnapThresh = 1e-6f;
            if (std::abs(yawSmCos_   - tYC) < kSnapThresh && std::abs(yawSmSin_   - tYS) < kSnapThresh &&
                std::abs(pitchSmCos_ - tPC) < kSnapThresh && std::abs(pitchSmSin_ - tPS) < kSnapThresh &&
                std::abs(rollSmCos_  - tRC) < kSnapThresh && std::abs(rollSmSin_  - tRS) < kSnapThresh) {
                cosY = tYC; sinY = tYS;
                cosP = tPC; sinP = tPS;
                cosR = tRC; sinR = tRS;
                yawSmCos_ = tYC;   yawSmSin_ = tYS;
                pitchSmCos_ = tPC; pitchSmSin_ = tPS;
                rollSmCos_ = tRC;  rollSmSin_ = tRS;
                listenerSettled_ = true;
            }

            cachedCosY_ = cosY; cachedSinY_ = sinY;
            cachedCosP_ = cosP; cachedSinP_ = sinP;
            cachedCosR_ = cosR; cachedSinR_ = sinR;
        } else {
            cosY = cachedCosY_; sinY = cachedSinY_;
            cosP = cachedCosP_; sinP = cachedSinP_;
            cosR = cachedCosR_; sinR = cachedSinR_;
        }
    }

    constexpr float kRotEps = 1e-7f;
    const bool listenerRotated = (std::abs(sinY) > kRotEps || std::abs(cosY - 1.0f) > kRotEps
                               || std::abs(sinP) > kRotEps || std::abs(sinR) > kRotEps);

    // If an Euler-representation flip was detected, align the per-sample ramp
    // start with this block's target so no interpolation occurs — otherwise the
    // cos/sin pair would still lerp through (0,0) over the block (click).
    if (listenerFlipSnap) {
        prevCosY_ = cosY; prevSinY_ = sinY;
        prevCosP_ = cosP; prevSinP_ = sinP;
        prevCosR_ = cosR; prevSinR_ = sinR;
    }

    // Per-sample trig interpolation: linearly ramp cos/sin from previous block's
    // values to this block's values, eliminating block-boundary discontinuities
    // during fast head rotation (especially roll near close sources).
    const bool interpRotation = listenerRotated
        && (prevCosY_ != cosY || prevSinY_ != sinY
         || prevCosP_ != cosP || prevSinP_ != sinP
         || prevCosR_ != cosR || prevSinR_ != sinR);
    const float invN = 1.0f / static_cast<float>(numSamples);
    const float dCosY = (cosY - prevCosY_) * invN;
    const float dSinY = (sinY - prevSinY_) * invN;
    const float dCosP = (cosP - prevCosP_) * invN;
    const float dSinP = (sinP - prevSinP_) * invN;
    const float dCosR = (cosR - prevCosR_) * invN;
    const float dSinR = (sinR - prevSinR_) * invN;
    float rCosY = prevCosY_, rSinY = prevSinY_;
    float rCosP = prevCosP_, rSinP = prevSinP_;
    float rCosR = prevCosR_, rSinR = prevSinR_;
    // Store current values for next block
    prevCosY_ = cosY; prevSinY_ = sinY;
    prevCosP_ = cosP; prevSinP_ = sinP;
    prevCosR_ = cosR; prevSinR_ = sinR;

    // Save listener-relative (pre-head-rotation) coords for face-observer spread + ER
    const float blkRelX = blkX;
    const float blkRelY = blkY;
    const float blkRelZ = blkZ;

    // Rotate block-start position into listener-relative frame for EQ targets.
    // Inverse yaw around Z, then inverse pitch around X, then roll around Y-forward.
    if (listenerRotated) {
        const float rx = blkX * cosY + blkY * sinY;
        const float ry = -blkX * sinY + blkY * cosY;
        blkX = rx;
        blkY = ry * cosP + blkZ * sinP;
        blkZ = -ry * sinP + blkZ * cosP;
        // Roll around forward axis (Y in engine coords)
        const float rrx =  blkX * cosR - blkZ * sinR;
        const float rrz =  blkX * sinR + blkZ * cosR;
        blkX = rrx;
        blkZ = rrz;
    }

    // Distance-difference virtual ear model for block-start position
    const float blkRearFactor = computeRearFactor(blkX, blkY, blkZ, currentParams.rearEarOffset);
    const float blkEffAzimuth = computeAzimuthFactor(blkX, blkY, blkZ, currentParams.azimuthEarOffset);

    // Block-start EQ targets (elevation-driven pinna, Y-driven presence/earCanal)
    const float blkPresenceGainDb  = currentParams.presenceShelfMaxDb * (-blkRearFactor);
    const float blkEarCanalGainDb  = std::min(0.0f, currentParams.earCanalMaxDb * (-blkRearFactor));
    const float blkElevFactor      = computeElevFactor(blkX, blkY, blkZ, currentParams.elevEarOffset);
    const float blkElevAbove       = std::max(0.0f, blkElevFactor * 2.0f - 1.0f);           // 0 at horizon, 1 at zenith
    const float blkPinnaGainDb     = -15.0f + 20.0f * blkElevAbove;
    const float blkShelfGainDb     = 3.0f * std::min(1.0f, blkElevFactor * 2.0f);           // 0 at nadir, 3 at horizon+
    const float blkPinnaN1Freq     = std::clamp(currentParams.pinnaN1MinHz + (currentParams.pinnaN1MaxHz - currentParams.pinnaN1MinHz) * blkElevFactor, currentParams.pinnaN1MinHz, currentParams.pinnaN1MaxHz);
    const float blkPinnaN2Freq     = blkPinnaN1Freq + currentParams.pinnaN2OffsetHz;

    // Block-start binaural near-field targets
    const float blkNFBaseDb        = currentParams.nearFieldLFMaxDb * (1.0f - std::clamp(
        (std::sqrt(blkX*blkX + blkY*blkY + blkZ*blkZ) - kMinDistance)
        / std::max(currentParams.sphereRadius - kMinDistance, 0.001f), 0.0f, 1.0f));
    const float blkNFGainR         = blkNFBaseDb * std::max(0.0f,  blkEffAzimuth);
    const float blkNFGainL         = blkNFBaseDb * std::max(0.0f, -blkEffAzimuth);

    // Block-start distance (for air LPF)
    const float blkDist     = computeDistance(blkX, blkY, blkZ);
    const float blkMaxRange = std::max(currentParams.sphereRadius - kMinDistance, 0.001f);
    const float blkDistFrac = std::clamp((blkDist - kMinDistance) / blkMaxRange, 0.0f, 1.0f);
    const float blkAirCutoff1 = currentParams.airAbsMaxHz
        + (currentParams.airAbsMinHz - currentParams.airAbsMaxHz) * blkDistFrac;
    const float blkAirCutoff2 = currentParams.airAbs2MaxHz
        + (currentParams.airAbs2MinHz - currentParams.airAbs2MaxHz) * blkDistFrac;

    // --- Mono path per-block setCoefficients ---
    // Skip when stereo is active — the stereo path below overwrites src_.* coefficients
    const bool blkStereoLikely = stereoWidthSmooth_.current() > 0.001f && inputR != nullptr;
    if (!blkStereoLikely) {
        // Pinna EQ biquads — smoothed to avoid clicks from coefficient jumps at block boundaries
        src_.presenceShelf.setCoefficientsSmoothed(dsp::BiquadType::HighShelf,
            currentParams.presenceShelfFreqHz, sr, 0.7071f, blkPresenceGainDb, numSamples);
        src_.earCanalPeak.setCoefficientsSmoothed(dsp::BiquadType::PeakingEQ,
            currentParams.earCanalFreqHz, sr, currentParams.earCanalQ, blkEarCanalGainDb, numSamples);
        src_.pinnaNotch.setCoefficientsSmoothed(dsp::BiquadType::PeakingEQ,
            blkPinnaN1Freq, sr, currentParams.pinnaNotchQ, blkPinnaGainDb, numSamples);
        src_.pinnaNotch2.setCoefficientsSmoothed(dsp::BiquadType::PeakingEQ,
            blkPinnaN2Freq, sr, currentParams.pinnaN2Q, currentParams.pinnaN2GainDb, numSamples);
        src_.pinnaShelf.setCoefficientsSmoothed(dsp::BiquadType::HighShelf,
            currentParams.pinnaShelfFreqHz, sr, 0.7071f, blkShelfGainDb, numSamples);

        // Expanded pinna EQ (P5) — 4 additional bands, mono path
        {
            const float belowFactor = 1.0f - blkElevFactor;
            const float aboveFactor = blkElevFactor;
            const float blkShoulderGainDb = currentParams.shoulderPeakMaxDb * belowFactor;
            const float blkConchaGainDb   = currentParams.conchaNotchMaxDb * belowFactor;
            const float blkUpperPinnaGainDb = currentParams.upperPinnaMinDb
                + (currentParams.upperPinnaMaxDb - currentParams.upperPinnaMinDb) * aboveFactor;
            const float blkTragusRear  = std::max(0.0f, blkRearFactor);
            const float blkTragusGainDb = currentParams.tragusNotchMaxDb * blkTragusRear * belowFactor;

            src_.shoulderPeak.setCoefficientsSmoothed(dsp::BiquadType::PeakingEQ,
                currentParams.shoulderPeakFreqHz, sr, currentParams.shoulderPeakQ, blkShoulderGainDb, numSamples);
            src_.conchaNotch.setCoefficientsSmoothed(dsp::BiquadType::PeakingEQ,
                currentParams.conchaNotchFreqHz, sr, currentParams.conchaNotchQ, blkConchaGainDb, numSamples);
            src_.upperPinna.setCoefficientsSmoothed(dsp::BiquadType::PeakingEQ,
                currentParams.upperPinnaFreqHz, sr, currentParams.upperPinnaQ, blkUpperPinnaGainDb, numSamples);
            src_.tragusNotch.setCoefficientsSmoothed(dsp::BiquadType::PeakingEQ,
                currentParams.tragusNotchFreqHz, sr, currentParams.tragusNotchQ, blkTragusGainDb, numSamples);
        }

        // Near-field ILD biquads (mono path) — smoothed, continuous blend through azimuth zero
        src_.nearFieldLF_R.setCoefficientsSmoothed(dsp::BiquadType::LowShelf, currentParams.nearFieldLFHz, sr, 0.7071f, blkNFGainR, numSamples);
        src_.nearFieldLF_L.setCoefficientsSmoothed(dsp::BiquadType::LowShelf, currentParams.nearFieldLFHz, sr, 0.7071f, blkNFGainL, numSamples);
    }

    // Head shadow + rear shadow SVFs: coefficients updated per-sample in inner loop

    // Air absorption LPFs (mono path — OnePoleLP, smoothed to avoid block-boundary clicks)
    dist_.airLPF_L.setCoefficientsSmoothed(blkAirCutoff1, sr, numSamples);
    dist_.airLPF_R.setCoefficientsSmoothed(blkAirCutoff1, sr, numSamples);
    dist_.airLPF2_L.setCoefficientsSmoothed(blkAirCutoff2, sr, numSamples);
    dist_.airLPF2_R.setCoefficientsSmoothed(blkAirCutoff2, sr, numSamples);

    // Floor bounce HF absorption LPF — smoothed to avoid block-boundary clicks
    floor_.lpfL.setCoefficientsSmoothed(currentParams.floorAbsHz, sr, numSamples);
    floor_.lpfR.setCoefficientsSmoothed(currentParams.floorAbsHz, sr, numSamples);
    floorR_.lpfL.setCoefficientsSmoothed(currentParams.floorAbsHz, sr, numSamples);
    floorR_.lpfR.setCoefficientsSmoothed(currentParams.floorAbsHz, sr, numSamples);

    // ER wall absorption LPF — per-block smoothed (was per-sample setCoefficients)
    const float erDampCutoff = kERDampingLPMaxHz
        + (kERDampingLPMinHz - kERDampingLPMaxHz) * currentParams.erDamping;
    er_.updateWallAbsorption(erDampCutoff, sr, numSamples);
    erR_.updateWallAbsorption(erDampCutoff, sr, numSamples);

    // ER directional cues — per-block pinna EQ + near-field coefficients (mono path).
    // Skip when stereo is active — the stereo path below sets per-node ER coefficients.
    if (!blkStereoLikely) {
        er_.updateTapDirectionalCoeffs(
            blkRelX, blkRelY, blkRelZ,
            currentParams.listenerX, currentParams.listenerY, currentParams.listenerZ,
            currentParams.erRoomSize, sr, numSamples,
            listenerRotated, cosY, sinY, cosP, sinP, cosR, sinR,
            currentParams.sphereRadius, currentParams);
    }

    // --- Stereo path per-block setCoefficients ---
    // For the stereo path, compute L-node and R-node block-start positions using
    // unmodulated params. The orbit LFO modulation changes slowly; per-block
    // coefficient update is sufficient (LFO phase delta per block is negligible for EQ).
    // Pre-cache orbit XY trig for inner loop when orbit XY depth is zero
    // (angle is block-constant, saves 8 LUT lookups per sample)
    bool orbitXYBlockConstant = false;
    float blkLCosXY = 1.0f, blkLSinXY = 0.0f;
    float blkRCosXY = 1.0f, blkRSinXY = 0.0f;
    {
        const float blkHalfSpread = currentParams.stereoWidth * kStereoMaxSpreadRadius;
        float blkSpreadX = 1.0f, blkSpreadY = 0.0f;
        const float blkRelHorizMag = std::sqrt(blkRelX * blkRelX + blkRelY * blkRelY);
        if (currentParams.stereoFaceListener && blkRelHorizMag > 1e-5f) {
            blkSpreadX =  blkRelY / blkRelHorizMag;
            blkSpreadY = -blkRelX / blkRelHorizMag;
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

        orbitXYBlockConstant = blkOrbitDepXY < 1e-7f;
        blkLCosXY = dsp::SineLUT::cosLookupAngle(blkSmoothedOffset);
        blkLSinXY = dsp::SineLUT::lookupAngle(blkSmoothedOffset);
        blkRCosXY = dsp::SineLUT::cosLookupAngle(blkSmoothedOffset + blkRPhaseOffset);
        blkRSinXY = dsp::SineLUT::lookupAngle(blkSmoothedOffset + blkRPhaseOffset);

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
        const float lRearFactor = computeRearFactor(blkLNodeX, blkLNodeY, blkLNodeZ, currentParams.rearEarOffset);
        const float lPresGainDb = currentParams.presenceShelfMaxDb * (-lRearFactor);
        const float lEarGainDb  = std::min(0.0f, currentParams.earCanalMaxDb * (-lRearFactor));
        const float lElevFactor = computeElevFactor(blkLNodeX, blkLNodeY, blkLNodeZ, currentParams.elevEarOffset);
        const float lElevAbove  = std::max(0.0f, lElevFactor * 2.0f - 1.0f);
        const float lPinnaGain  = -15.0f + 20.0f * lElevAbove;
        const float lShelfGain  = 3.0f * std::min(1.0f, lElevFactor * 2.0f);
        const float lN1Freq     = std::clamp(currentParams.pinnaN1MinHz + (currentParams.pinnaN1MaxHz - currentParams.pinnaN1MinHz) * lElevFactor, currentParams.pinnaN1MinHz, currentParams.pinnaN1MaxHz);
        const float lN2Freq     = lN1Freq + currentParams.pinnaN2OffsetHz;
        const float lEffAz      = computeAzimuthFactor(blkLNodeX, blkLNodeY, blkLNodeZ, currentParams.azimuthEarOffset);
        const float lFullDist   = std::sqrt(blkLNodeX*blkLNodeX+blkLNodeY*blkLNodeY+blkLNodeZ*blkLNodeZ);
        const float lLNodeDistFrac = std::clamp((lFullDist - kMinDistance) / blkMaxRange, 0.0f, 1.0f);
        const float lProx       = 1.0f - lLNodeDistFrac;
        const float lNFBaseDb   = currentParams.nearFieldLFMaxDb * lProx;
        const float lNFGainR    = lNFBaseDb * std::max(0.0f,  lEffAz);
        const float lNFGainL    = lNFBaseDb * std::max(0.0f, -lEffAz);
        const float lAirCut1    = currentParams.airAbsMaxHz + (currentParams.airAbsMinHz - currentParams.airAbsMaxHz) * lLNodeDistFrac;
        const float lAirCut2    = currentParams.airAbs2MaxHz + (currentParams.airAbs2MinHz - currentParams.airAbs2MaxHz) * lLNodeDistFrac;

        // R node EQ targets
        const float rRearFactor = computeRearFactor(blkRNodeX, blkRNodeY, blkRNodeZ, currentParams.rearEarOffset);
        const float rPresGainDb = currentParams.presenceShelfMaxDb * (-rRearFactor);
        const float rEarGainDb  = std::min(0.0f, currentParams.earCanalMaxDb * (-rRearFactor));
        const float rElevFactor = computeElevFactor(blkRNodeX, blkRNodeY, blkRNodeZ, currentParams.elevEarOffset);
        const float rElevAbove  = std::max(0.0f, rElevFactor * 2.0f - 1.0f);
        const float rPinnaGain  = -15.0f + 20.0f * rElevAbove;
        const float rShelfGain  = 3.0f * std::min(1.0f, rElevFactor * 2.0f);
        const float rN1Freq     = std::clamp(currentParams.pinnaN1MinHz + (currentParams.pinnaN1MaxHz - currentParams.pinnaN1MinHz) * rElevFactor, currentParams.pinnaN1MinHz, currentParams.pinnaN1MaxHz);
        const float rN2Freq     = rN1Freq + currentParams.pinnaN2OffsetHz;
        const float rEffAz      = computeAzimuthFactor(blkRNodeX, blkRNodeY, blkRNodeZ, currentParams.azimuthEarOffset);
        const float rFullDist   = std::sqrt(blkRNodeX*blkRNodeX+blkRNodeY*blkRNodeY+blkRNodeZ*blkRNodeZ);
        const float rRNodeDistFrac = std::clamp((rFullDist - kMinDistance) / blkMaxRange, 0.0f, 1.0f);
        const float rProx       = 1.0f - rRNodeDistFrac;
        const float rNFBaseDb   = currentParams.nearFieldLFMaxDb * rProx;
        const float rNFGainR    = rNFBaseDb * std::max(0.0f,  rEffAz);
        const float rNFGainL    = rNFBaseDb * std::max(0.0f, -rEffAz);
        const float rAirCut1    = currentParams.airAbsMaxHz + (currentParams.airAbsMinHz - currentParams.airAbsMaxHz) * rRNodeDistFrac;
        const float rAirCut2    = currentParams.airAbs2MaxHz + (currentParams.airAbs2MinHz - currentParams.airAbs2MaxHz) * rRNodeDistFrac;

        // L pipeline EQ setCoefficients (stereo path) — smoothed
        src_.presenceShelf.setCoefficientsSmoothed(dsp::BiquadType::HighShelf,
            currentParams.presenceShelfFreqHz, sr, 0.7071f, lPresGainDb, numSamples);
        src_.earCanalPeak.setCoefficientsSmoothed(dsp::BiquadType::PeakingEQ,
            currentParams.earCanalFreqHz, sr, currentParams.earCanalQ, lEarGainDb, numSamples);
        src_.pinnaNotch.setCoefficientsSmoothed(dsp::BiquadType::PeakingEQ,
            lN1Freq, sr, currentParams.pinnaNotchQ, lPinnaGain, numSamples);
        src_.pinnaNotch2.setCoefficientsSmoothed(dsp::BiquadType::PeakingEQ,
            lN2Freq, sr, currentParams.pinnaN2Q, currentParams.pinnaN2GainDb, numSamples);
        src_.pinnaShelf.setCoefficientsSmoothed(dsp::BiquadType::HighShelf,
            currentParams.pinnaShelfFreqHz, sr, 0.7071f, lShelfGain, numSamples);

        // L pipeline expanded pinna EQ (P5)
        {
            const float lBelow = 1.0f - lElevFactor;
            const float lAbove = lElevFactor;
            const float lShoulderDb = currentParams.shoulderPeakMaxDb * lBelow;
            const float lConchaDb   = currentParams.conchaNotchMaxDb * lBelow;
            const float lUpperDb    = currentParams.upperPinnaMinDb
                + (currentParams.upperPinnaMaxDb - currentParams.upperPinnaMinDb) * lAbove;
            const float lTragusDb   = currentParams.tragusNotchMaxDb
                * std::max(0.0f, lRearFactor) * lBelow;
            src_.shoulderPeak.setCoefficientsSmoothed(dsp::BiquadType::PeakingEQ,
                currentParams.shoulderPeakFreqHz, sr, currentParams.shoulderPeakQ, lShoulderDb, numSamples);
            src_.conchaNotch.setCoefficientsSmoothed(dsp::BiquadType::PeakingEQ,
                currentParams.conchaNotchFreqHz, sr, currentParams.conchaNotchQ, lConchaDb, numSamples);
            src_.upperPinna.setCoefficientsSmoothed(dsp::BiquadType::PeakingEQ,
                currentParams.upperPinnaFreqHz, sr, currentParams.upperPinnaQ, lUpperDb, numSamples);
            src_.tragusNotch.setCoefficientsSmoothed(dsp::BiquadType::PeakingEQ,
                currentParams.tragusNotchFreqHz, sr, currentParams.tragusNotchQ, lTragusDb, numSamples);
        }

        src_.nearFieldLF_R.setCoefficientsSmoothed(dsp::BiquadType::LowShelf, currentParams.nearFieldLFHz, sr, 0.7071f, lNFGainR, numSamples);
        src_.nearFieldLF_L.setCoefficientsSmoothed(dsp::BiquadType::LowShelf, currentParams.nearFieldLFHz, sr, 0.7071f, lNFGainL, numSamples);
        // Shadow + rear SVFs: coefficients updated per-sample in processBinauralForSource()
        dist_.airLPF_L.setCoefficientsSmoothed(lAirCut1, sr, numSamples);
        dist_.airLPF_R.setCoefficientsSmoothed(lAirCut1, sr, numSamples);
        dist_.airLPF2_L.setCoefficientsSmoothed(lAirCut2, sr, numSamples);
        dist_.airLPF2_R.setCoefficientsSmoothed(lAirCut2, sr, numSamples);

        // R pipeline (srcR_) EQ setCoefficients — smoothed
        srcR_.presenceShelf.setCoefficientsSmoothed(dsp::BiquadType::HighShelf,
            currentParams.presenceShelfFreqHz, sr, 0.7071f, rPresGainDb, numSamples);
        srcR_.earCanalPeak.setCoefficientsSmoothed(dsp::BiquadType::PeakingEQ,
            currentParams.earCanalFreqHz, sr, currentParams.earCanalQ, rEarGainDb, numSamples);
        srcR_.pinnaP1.setCoefficientsSmoothed(dsp::BiquadType::PeakingEQ,
            currentParams.pinnaP1FreqHz, sr, currentParams.pinnaP1Q, currentParams.pinnaP1GainDb, numSamples);
        srcR_.pinnaNotch.setCoefficientsSmoothed(dsp::BiquadType::PeakingEQ,
            rN1Freq, sr, currentParams.pinnaNotchQ, rPinnaGain, numSamples);
        srcR_.pinnaNotch2.setCoefficientsSmoothed(dsp::BiquadType::PeakingEQ,
            rN2Freq, sr, currentParams.pinnaN2Q, currentParams.pinnaN2GainDb, numSamples);
        srcR_.pinnaShelf.setCoefficientsSmoothed(dsp::BiquadType::HighShelf,
            currentParams.pinnaShelfFreqHz, sr, 0.7071f, rShelfGain, numSamples);

        // R pipeline expanded pinna EQ (P5)
        {
            const float rBelow = 1.0f - rElevFactor;
            const float rAbove = rElevFactor;
            const float rShoulderDb = currentParams.shoulderPeakMaxDb * rBelow;
            const float rConchaDb   = currentParams.conchaNotchMaxDb * rBelow;
            const float rUpperDb    = currentParams.upperPinnaMinDb
                + (currentParams.upperPinnaMaxDb - currentParams.upperPinnaMinDb) * rAbove;
            const float rTragusDb   = currentParams.tragusNotchMaxDb
                * std::max(0.0f, rRearFactor) * rBelow;
            srcR_.shoulderPeak.setCoefficientsSmoothed(dsp::BiquadType::PeakingEQ,
                currentParams.shoulderPeakFreqHz, sr, currentParams.shoulderPeakQ, rShoulderDb, numSamples);
            srcR_.conchaNotch.setCoefficientsSmoothed(dsp::BiquadType::PeakingEQ,
                currentParams.conchaNotchFreqHz, sr, currentParams.conchaNotchQ, rConchaDb, numSamples);
            srcR_.upperPinna.setCoefficientsSmoothed(dsp::BiquadType::PeakingEQ,
                currentParams.upperPinnaFreqHz, sr, currentParams.upperPinnaQ, rUpperDb, numSamples);
            srcR_.tragusNotch.setCoefficientsSmoothed(dsp::BiquadType::PeakingEQ,
                currentParams.tragusNotchFreqHz, sr, currentParams.tragusNotchQ, rTragusDb, numSamples);
        }

        srcR_.nearFieldLF_R.setCoefficientsSmoothed(dsp::BiquadType::LowShelf, currentParams.nearFieldLFHz, sr, 0.7071f, rNFGainR, numSamples);
        srcR_.nearFieldLF_L.setCoefficientsSmoothed(dsp::BiquadType::LowShelf, currentParams.nearFieldLFHz, sr, 0.7071f, rNFGainL, numSamples);
        // Shadow + rear SVFs: coefficients updated per-sample in processBinauralForSource()
        distR_.airLPF_L.setCoefficientsSmoothed(rAirCut1, sr, numSamples);
        distR_.airLPF_R.setCoefficientsSmoothed(rAirCut1, sr, numSamples);
        distR_.airLPF2_L.setCoefficientsSmoothed(rAirCut2, sr, numSamples);
        distR_.airLPF2_R.setCoefficientsSmoothed(rAirCut2, sr, numSamples);

        // ER directional cues (stereo path) — pre-rotation listener-relative coords
        er_.updateTapDirectionalCoeffs(
            blkRelX + blkLOffX, blkRelY + blkLOffY, blkRelZ + blkLOffZ,
            currentParams.listenerX, currentParams.listenerY, currentParams.listenerZ,
            currentParams.erRoomSize, sr, numSamples,
            listenerRotated, cosY, sinY, cosP, sinP, cosR, sinR,
            currentParams.sphereRadius, currentParams);
        erR_.updateTapDirectionalCoeffs(
            blkRelX + blkROffX, blkRelY + blkROffY, blkRelZ + blkROffZ,
            currentParams.listenerX, currentParams.listenerY, currentParams.listenerZ,
            currentParams.erRoomSize, sr, numSamples,
            listenerRotated, cosY, sinY, cosP, sinP, cosR, sinR,
            currentParams.sphereRadius, currentParams);

    }

    // -------------------------------------------------------------------------
    // Per-sample loop
    // -------------------------------------------------------------------------
    const float testGainTargetBlock = currentParams.testToneEnabled
        ? std::pow(10.0f, currentParams.testToneGainDb / 20.0f)
        : 0.0f;
    const float sawIncrement = currentParams.testTonePitchHz / sr;
    pulseLFO_.setRateHz(currentParams.testTonePulseHz);
    std::uniform_real_distribution<float> noiseDist(-1.0f, 1.0f);
    const float verbPreDelayMaxSamp = currentParams.verbPreDelayMax * sr / 1000.0f;

    // Pre-compute position-derived values for zero-LFO fast path
    // Use listener-relative position for distance (walker mode)
    const float relPosX = currentParams.x - currentParams.listenerX;
    const float relPosY = currentParams.y - currentParams.listenerY;
    const float relPosZ = currentParams.z - currentParams.listenerZ;
    const float blkRawModDist = std::sqrt(relPosX * relPosX
                                        + relPosY * relPosY
                                        + relPosZ * relPosZ);
    // blkRawModDist is reused in the zero-LFO path below

    // Pre-converge block-constant smoothers analytically (O(1) instead of O(numSamples))
    lfoDepthXSmooth_.converge(currentParams.lfoXDepth, numSamples);
    lfoDepthYSmooth_.converge(currentParams.lfoYDepth, numSamples);
    lfoDepthZSmooth_.converge(currentParams.lfoZDepth, numSamples);
    orbitDepthXYSmooth_.converge(currentParams.stereoOrbitXYDepth, numSamples);
    orbitDepthXZSmooth_.converge(currentParams.stereoOrbitXZDepth, numSamples);
    orbitDepthYZSmooth_.converge(currentParams.stereoOrbitYZDepth, numSamples);
    stereoWidthSmooth_.converge(currentParams.stereoWidth, numSamples);
    binauralBlendSmooth_.converge(currentParams.binauralEnabled ? 1.0f : 0.0f, numSamples);

    for (int i = 0; i < numSamples; ++i) {
        // ----------------------------------------------------------------
        // Test tone generation
        // ----------------------------------------------------------------
        float testSig = 0.0f;
        float testSigL = 0.0f;
        float testSigR = 0.0f;
        bool  testStereo = false;
        const float smoothedTestGain = testToneGainSmooth_.process(testGainTargetBlock);
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
                    testSigL = noiseDist(noiseRng_) * gate;
                    testSigR = (2.0f * sawPhase_ - 1.0f) * gate;
                    sawPhase_ += sawIncrement;
                    if (sawPhase_ >= 1.0f) sawPhase_ -= 1.0f;
                    break;
                }
                case xyzpan::TestToneWaveform::Sine:
                    testSig = std::sin(6.283185307f * sawPhase_);
                    sawPhase_ += sawIncrement;
                    if (sawPhase_ >= 1.0f) sawPhase_ -= 1.0f;
                    break;
                case xyzpan::TestToneWaveform::Click: {
                    const float gate = pulseLFO_.tick() > 0.0f ? 1.0f : 0.0f;
                    const bool gateOn = gate > 0.5f;
                    if (gateOn && !prevPulseGate_)
                        clickSamplesLeft_ = static_cast<float>(sampleRate) * 0.001f;
                    prevPulseGate_ = gateOn;
                    testSig = (clickSamplesLeft_ > 0.0f) ? 1.0f : 0.0f;
                    clickSamplesLeft_ = std::max(0.0f, clickSamplesLeft_ - 1.0f);
                    break;
                }
                case xyzpan::TestToneWaveform::Triangle:
                    testSig = 4.0f * (sawPhase_ < 0.5f ? sawPhase_ : (1.0f - sawPhase_)) - 1.0f;
                    sawPhase_ += sawIncrement;
                    if (sawPhase_ >= 1.0f) sawPhase_ -= 1.0f;
                    break;
                case xyzpan::TestToneWaveform::Saw: default:
                    testSig = 2.0f * sawPhase_ - 1.0f;
                    sawPhase_ += sawIncrement;
                    if (sawPhase_ >= 1.0f) sawPhase_ -= 1.0f;
                    break;
            }
            if (testStereo) {
                testSigL *= smoothedTestGain;
                testSigR *= smoothedTestGain;
            } else {
                testSig *= smoothedTestGain;
            }
        }

        // ----------------------------------------------------------------
        // Position LFOs
        // ----------------------------------------------------------------
        const float depthX = lfoDepthXSmooth_.current();
        const float depthY = lfoDepthYSmooth_.current();
        const float depthZ = lfoDepthZSmooth_.current();

        // Detect if any LFO depth is active -- when all are zero, position is block-constant
        const bool lfoActive = depthX > 1e-7f || depthY > 1e-7f || depthZ > 1e-7f;

        float modX, modY, modZ;
        float lfoValX, lfoValY, lfoValZ;
        if (lfoActive) {
            lfoValX = lfoX_.tick() * depthX;
            lfoValY = lfoY_.tick() * depthY;
            lfoValZ = lfoZ_.tick() * depthZ;
            modX = currentParams.x + lfoValX;
            modY = currentParams.y + lfoValY;
            modZ = currentParams.z + lfoValZ;
        } else {
            // Still tick LFOs to keep phase accumulation consistent (no jump when depth goes non-zero)
            lfoX_.tick(); lfoY_.tick(); lfoZ_.tick();
            lfoValX = 0.f; lfoValY = 0.f; lfoValZ = 0.f;
            // Use block-start values -- position is constant when LFO depths are all zero
            modX = currentParams.x;
            modY = currentParams.y;
            modZ = currentParams.z;
        }

        // Save world-space position for bridge (source sphere stays at world position)
        const float worldModX = modX, worldModY = modY, worldModZ = modZ;

        // Walker mode: subtract listener position (listener-relative for DSP)
        modX -= currentParams.listenerX;
        modY -= currentParams.listenerY;
        modZ -= currentParams.listenerZ;

        // Rotate into listener-relative frame for DSP (binaural cues, EQ targets).
        // When interpolating, use per-sample ramped trig values to avoid block-boundary jumps.
        if (listenerRotated) {
            const float cY = interpRotation ? rCosY : cosY;
            const float sY = interpRotation ? rSinY : sinY;
            const float cP = interpRotation ? rCosP : cosP;
            const float sP = interpRotation ? rSinP : sinP;
            const float cR = interpRotation ? rCosR : cosR;
            const float sR = interpRotation ? rSinR : sinR;
            const float rx = modX * cY + modY * sY;
            const float ry = -modX * sY + modY * cY;
            modX = rx;
            modY = ry * cP + modZ * sP;
            modZ = -ry * sP + modZ * cP;
            // Roll around forward axis (Y in engine coords)
            const float rrx =  modX * cR - modZ * sR;
            const float rrz =  modX * sR + modZ * cR;
            modX = rrx;
            modZ = rrz;
        }

        // Position-dependent targets from listener-relative center position
        // rawModDist uses listener-relative (rotation-invariant) distance
        float rawModDist;
        if (lfoActive) {
            rawModDist    = dsp::fastSqrt(modX * modX + modY * modY + modZ * modZ);
        } else {
            rawModDist    = blkRawModDist;
        }
        const float modDist     = std::max(rawModDist, kMinDistance);
        const float maxRange    = std::max(currentParams.sphereRadius - kMinDistance, 0.001f);
        const float modDistFrac = std::clamp((modDist - kMinDistance) / maxRange, 0.0f, 1.0f);

        const float distGainMaxDb = blkDistGainMaxDb_;

        const float rawDistFrac = std::clamp(rawModDist / kSqrt3, 0.0f, 1.0f);

        // Distance targets — compressed cubic Hermite curve
        const float distGainTarget = compressedDistGain(
            modDistFrac, distGainMaxDb, currentParams.distGainFloorDb,
            currentParams.distCurveSteep, currentParams.distGainMax);

        // Air absorption coefficients — mono path only (stereo sets per-node in processDistance)
        const float airCutoffMod = currentParams.airAbsMaxHz
            + (currentParams.airAbsMinHz - currentParams.airAbsMaxHz) * modDistFrac;

        // Capture modulated position (world-space for GL bridge)
        lastModulated_ = {worldModX, worldModY, worldModZ};

        // ----------------------------------------------------------------
        // Stereo width smooth + orbit LFOs
        // ----------------------------------------------------------------
        const float smoothedWidth = stereoWidthSmooth_.current();
        const bool stereoActive = smoothedWidth > 0.001f && inputR != nullptr;

        // Orbit LFO ticks — always tick to keep phase accumulation consistent
        const float orbitDepXY = orbitDepthXYSmooth_.current();
        const float orbitDepXZ = orbitDepthXZSmooth_.current();
        const float orbitDepYZ = orbitDepthYZSmooth_.current();
        const float orbitRawXY = orbitLfoXY_.tick();
        const float orbitRawXZ = orbitLfoXZ_.tick();
        const float orbitRawYZ = orbitLfoYZ_.tick();
        const float orbitValXY = orbitRawXY * orbitDepXY;
        const float orbitValXZ = orbitRawXZ * orbitDepXZ;
        const float orbitValYZ = orbitRawYZ * orbitDepYZ;

        // Binaural blend — pre-converged above, read cached value
        const float binBlend = binauralBlendSmooth_.current();

        float dL, dR;
        float effectiveDistGain = 1.0f;
        float blendedDistFrac = modDistFrac;  // overwritten in stereo path
        float dopplerInputMono = 0.0f;        // doppler'd input for chest/ER
        float erReverbAccumL = 0.0f, erReverbAccumR = 0.0f;

        if (stereoActive) {
            // Compute L/R node positions in world space, then rotate for DSP
            const float halfSpread = smoothedWidth * kStereoMaxSpreadRadius;

            // Spread direction: perpendicular to listener→source in XY plane (world-relative,
            // NOT head-rotated — offsets are added to world-space positions)
            const float relX = worldModX - currentParams.listenerX;
            const float relY = worldModY - currentParams.listenerY;
            const float relHorizMag = dsp::fastSqrt(relX * relX + relY * relY);
            float spreadX, spreadY;
            if (currentParams.stereoFaceListener && relHorizMag > 1e-5f) {
                spreadX =  relY / relHorizMag;
                spreadY = -relX / relHorizMag;
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
            // Use block-cached trig when orbit XY depth is zero (angle is block-constant)
            float lCos, lSin, rCos, rSin;
            if (orbitXYBlockConstant) {
                lCos = blkLCosXY; lSin = blkLSinXY;
                rCos = blkRCosXY; rSin = blkRSinXY;
            } else {
                const float lAngle = orbitAngleXY + blkSmoothedOffset;
                const float rAngle = orbitAngleXY + blkSmoothedOffset + blkRPhaseOffset;
                lCos = dsp::SineLUT::cosLookupAngle(lAngle);
                lSin = dsp::SineLUT::lookupAngle(lAngle);
                rCos = dsp::SineLUT::cosLookupAngle(rAngle);
                rSin = dsp::SineLUT::lookupAngle(rAngle);
            }

            // Compute L offset in XY plane
            float lOffX = halfSpread * (spreadX * lCos - spreadY * lSin);
            float lOffY = halfSpread * (spreadX * lSin + spreadY * lCos);
            float lOffZ = 0.0f;

            // Compute R offset in XY plane
            float rOffX = halfSpread * (spreadX * rCos - spreadY * rSin);
            float rOffY = halfSpread * (spreadX * rSin + spreadY * rCos);
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

            // Final node positions (world space — for bridge and distance)
            const float lNodeX = worldModX + lOffX;
            const float lNodeY = worldModY + lOffY;
            const float lNodeZ = worldModZ + lOffZ;
            const float rNodeX = worldModX + rOffX;
            const float rNodeY = worldModY + rOffY;
            const float rNodeZ = worldModZ + rOffZ;

            // Store world-space for position bridge
            lastStereoNodes_ = { lNodeX, lNodeY, lNodeZ, rNodeX, rNodeY, rNodeZ, smoothedWidth };

            // Listener-relative node positions for DSP (distance, doppler, ER)
            const float lRelX = lNodeX - currentParams.listenerX;
            const float lRelY = lNodeY - currentParams.listenerY;
            const float lRelZ = lNodeZ - currentParams.listenerZ;
            const float rRelX = rNodeX - currentParams.listenerX;
            const float rRelY = rNodeY - currentParams.listenerY;
            const float rRelZ = rNodeZ - currentParams.listenerZ;

            // Rotate listener-relative positions into head frame for binaural DSP
            float dspLX = lRelX, dspLY = lRelY, dspLZ = lRelZ;
            float dspRX = rRelX, dspRY = rRelY, dspRZ = rRelZ;
            if (listenerRotated) {
                const float cY = interpRotation ? rCosY : cosY;
                const float sY = interpRotation ? rSinY : sinY;
                const float cP = interpRotation ? rCosP : cosP;
                const float sP = interpRotation ? rSinP : sinP;
                const float cR = interpRotation ? rCosR : cosR;
                const float sR = interpRotation ? rSinR : sinR;
                float rx, ry;
                rx = lRelX * cY + lRelY * sY;
                ry = -lRelX * sY + lRelY * cY;
                dspLX = rx;
                dspLY = ry * cP + lRelZ * sP;
                dspLZ = -ry * sP + lRelZ * cP;
                // Roll around forward axis (Y in engine coords)
                float rrx =  dspLX * cR - dspLZ * sR;
                float rrz =  dspLX * sR + dspLZ * cR;
                dspLX = rrx;
                dspLZ = rrz;

                rx = rRelX * cY + rRelY * sY;
                ry = -rRelX * sY + rRelY * cY;
                dspRX = rx;
                dspRY = ry * cP + rRelZ * sP;
                dspRZ = -ry * sP + rRelZ * cP;
                // Roll around forward axis (Y in engine coords)
                rrx =  dspRX * cR - dspRZ * sR;
                rrz =  dspRX * sR + dspRZ * cR;
                dspRX = rrx;
                dspRZ = rrz;
            }

            // Get input samples — test tone overrides both channels
            float sampleL = currentParams.testToneEnabled ? (testStereo ? testSigL : testSig) : inputL[i];
            float sampleR = currentParams.testToneEnabled ? (testStereo ? testSigR : testSig) : inputR[i];

            // Doppler FIRST — per-node mono doppler before comb/pinna/binaural
            // Distance is rotation-invariant, use listener-relative node positions
            const bool effectiveDoppler = dopplerOn && !currentParams.bypassDoppler;
            const float lNodeRawDist = dsp::fastSqrt(lRelX*lRelX + lRelY*lRelY + lRelZ*lRelZ);
            const float rNodeRawDist = dsp::fastSqrt(rRelX*rRelX + rRelY*rRelY + rRelZ*rRelZ);
            const float lRawDistFrac = std::clamp(lNodeRawDist / kSqrt3, 0.0f, 1.0f);
            const float rRawDistFrac = std::clamp(rNodeRawDist / kSqrt3, 0.0f, 1.0f);

            sampleL = dist_.processDoppler(sampleL, lRawDistFrac, sr, effectiveDoppler, currentParams);
            sampleR = distR_.processDoppler(sampleR, rRawDistFrac, sr, effectiveDoppler, currentParams);

            // Per-node chest bounce — driven by T/B virtual ear elevation factor
            const float lElevF = computeElevFactor(dspLX, dspLY, dspLZ, currentParams.elevEarOffset);
            const float rElevF = computeElevFactor(dspRX, dspRY, dspRZ, currentParams.elevEarOffset);
            float chestL = chest_.processSample(sampleL, lElevF, sr, chestGainLin, currentParams);
            float chestROut = chestR_.processSample(sampleR, rElevF, sr, chestGainLin, currentParams);

            // Process L channel through L pipeline — listener-relative positions
            auto [dL_L, dR_L] = src_.processSample(
                sampleL, dspLX, dspLY, dspLZ, sr, binBlend,
                ildGainBase_, hardpanGainBase_, currentParams);

            // Process R channel through R pipeline — listener-relative positions
            auto [dL_R, dR_R] = srcR_.processSample(
                sampleR, dspRX, dspRY, dspRZ, sr, binBlend,
                ildGainBase_, hardpanGainBase_, currentParams);

            // Add chest to per-node binaural (both ears, before distance)
            dL_L += chestL; dR_L += chestL;
            dL_R += chestROut; dR_R += chestROut;

            // Per-node distance processing (gain + air absorption only, no doppler)
            auto distL_result = dist_.processDistance(dL_L, dR_L, lRelX, lRelY, lRelZ, sr,
                blkDistGainMaxDb_, currentParams);

            auto distR_result = distR_.processDistance(dL_R, dR_R, rRelX, rRelY, rRelZ, sr,
                blkDistGainMaxDb_, currentParams);

            // Per-node floor bounce (on per-node distance output, before combine)
            float fL_L = distL_result.left, fR_L = distL_result.right;
            float fL_R = distR_result.left,  fR_R = distR_result.right;
            floor_.processSample(fL_L, fR_L, lElevF, sr, floorGainLin, currentParams);
            floorR_.processSample(fL_R, fR_R, rElevF, sr, floorGainLin, currentParams);

            // Combine with -3dB per node to maintain energy parity with mono path
            dL = (fL_L + fL_R) * kStereoNodeGain;
            dR = (fR_L + fR_R) * kStereoNodeGain;
            blendedDistFrac = 0.5f * (distL_result.distFrac + distR_result.distFrac);
            effectiveDistGain = 0.5f * (dist_.distGainSmooth.current() + distR_.distGainSmooth.current());

            // Per-node early reflections (6 reflections per node = 12 total)
            {
                const float erLevelSm = erLevelSmooth_.process(
                    (currentParams.erEnabled && !currentParams.bypassER) ? currentParams.erLevel : 0.0f);
                const float erSendSm = erReverbSendSmooth_.process(currentParams.erReverbSend);

                if (erLevelSm > 1e-6f) {
                    const float roomHalf = currentParams.erRoomSize;

                    // Per-node distGainTarget for ER gain scaling (reuse cached distances)
                    const float lNodeDist = std::max(lNodeRawDist, kMinDistance);
                    const float lNodeDistFrac = std::clamp((lNodeDist - kMinDistance) / maxRange, 0.0f, 1.0f);
                    const float lDistGainTarget = compressedDistGain(
                        lNodeDistFrac, distGainMaxDb, currentParams.distGainFloorDb,
                        currentParams.distCurveSteep, currentParams.distGainMax);

                    const float rNodeDist = std::max(rNodeRawDist, kMinDistance);
                    const float rNodeDistFrac = std::clamp((rNodeDist - kMinDistance) / maxRange, 0.0f, 1.0f);
                    const float rDistGainTarget = compressedDistGain(
                        rNodeDistFrac, distGainMaxDb, currentParams.distGainFloorDb,
                        currentParams.distCurveSteep, currentParams.distGainMax);

                    const float eCY = interpRotation ? rCosY : cosY;
                    const float eSY = interpRotation ? rSinY : sinY;
                    const float eCP = interpRotation ? rCosP : cosP;
                    const float eSP = interpRotation ? rSinP : sinP;
                    const float eCR = interpRotation ? rCosR : cosR;
                    const float eSR = interpRotation ? rSinR : sinR;
                    auto erLResult = er_.processSample(sampleL, lRelX, lRelY, lRelZ,
                        currentParams.listenerX, currentParams.listenerY, currentParams.listenerZ,
                        lDistGainTarget, sr, roomHalf,
                        ildGainBase_, listenerRotated, eCY, eSY, eCP, eSP, eCR, eSR,
                        currentParams);
                    auto erRResult = erR_.processSample(sampleR, rRelX, rRelY, rRelZ,
                        currentParams.listenerX, currentParams.listenerY, currentParams.listenerZ,
                        rDistGainTarget, sr, roomHalf,
                        ildGainBase_, listenerRotated, eCY, eSY, eCP, eSP, eCR, eSR,
                        currentParams);

                    dL += (erLResult.directL + erRResult.directL) * erLevelSm;
                    dR += (erLResult.directR + erRResult.directR) * erLevelSm;
                    erReverbAccumL = (erLResult.reverbL + erRResult.reverbL) * erLevelSm * erSendSm;
                    erReverbAccumR = (erLResult.reverbR + erRResult.reverbR) * erLevelSm * erSendSm;
                } else {
                    er_.sharedDelay.push(sampleL);
                    erR_.sharedDelay.push(sampleR);
                }
            }
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

            lastStereoNodes_ = { worldModX, worldModY, worldModZ, worldModX, worldModY, worldModZ, 0.0f };

            // Doppler FIRST — mono doppler before comb/pinna/binaural
            const bool effectiveDoppler = dopplerOn && !currentParams.bypassDoppler;
            mono = dist_.processDoppler(mono, rawDistFrac, sr, effectiveDoppler, currentParams);
            dopplerInputMono = mono;

            // Binaural processing — comb bank + pinna EQ + ITD/ILD/shadow
            {
                auto [binL, binR] = src_.processSample(
                    mono, modX, modY, modZ, sr, binBlend,
                    ildGainBase_, hardpanGainBase_, currentParams);
                dL = binL;
                dR = binR;
            }

            // DSP state capture for mono path — read back from pipeline smoothers
            {
                lastDSPState_.itdSamples     = src_.itdSmooth.current();
                lastDSPState_.shadowCutoffHz = src_.shadowCutoffSmooth.current();
                lastDSPState_.ildGainLinear  = src_.ildGainSmooth.current();
                lastDSPState_.rearCutoffHz   = src_.rearCutoffSmooth.current();
                lastDSPState_.combWet        = src_.combWetSmooth.current();
                lastDSPState_.monoBlend      = 0.0f;  // deprecated — cylinder blend removed
            }
        }

        // ----------------------------------------------------------------
        // Mono-only pipeline: chest bounce → floor bounce → distance → ER
        // (Stereo path handles these per-node inside the stereoActive block above)
        // ----------------------------------------------------------------
        if (!stereoActive) {

            // Chest bounce — driven by T/B virtual ear elevation factor
            {
                const float monoElevF = computeElevFactor(modX, modY, modZ, currentParams.elevEarOffset);
                float chestOut = chest_.processSample(dopplerInputMono, monoElevF, sr,
                                                       chestGainLin, currentParams);
                dL += chestOut;
                dR += chestOut;

                // Floor bounce — same elevation factor
                floor_.processSample(dL, dR, monoElevF, sr, floorGainLin, currentParams);
            }

            // Distance processing — mono path only (gain + air absorption; doppler already applied)
            {
                auto distResult = dist_.processDistance(dL, dR, modX, modY, modZ, sr,
                    blkDistGainMaxDb_, currentParams);
                dL = distResult.left;
                dR = distResult.right;
                effectiveDistGain = dist_.distGainSmooth.current();
            }

            // Early Reflections (Image Source Method) — mono path
            // ER input = doppler'd mono input (carries pitch shift into reflections)
            {
                const float erLevelSm = erLevelSmooth_.process(
                    (currentParams.erEnabled && !currentParams.bypassER) ? currentParams.erLevel : 0.0f);
                const float erSendSm = erReverbSendSmooth_.process(currentParams.erReverbSend);

                if (erLevelSm > 1e-6f) {
                    const float roomHalf = currentParams.erRoomSize;

                    const float eCY = interpRotation ? rCosY : cosY;
                    const float eSY = interpRotation ? rSinY : sinY;
                    const float eCP = interpRotation ? rCosP : cosP;
                    const float eSP = interpRotation ? rSinP : sinP;
                    const float eCR = interpRotation ? rCosR : cosR;
                    const float eSR = interpRotation ? rSinR : sinR;
                    // Listener-relative (pre-rotation) node coords — ER applies head rotation
                    // internally for binaural panning.
                    const float mRelX = worldModX - currentParams.listenerX;
                    const float mRelY = worldModY - currentParams.listenerY;
                    const float mRelZ = worldModZ - currentParams.listenerZ;
                    auto erResult = er_.processSample(dopplerInputMono,
                        mRelX, mRelY, mRelZ,
                        currentParams.listenerX, currentParams.listenerY, currentParams.listenerZ,
                        distGainTarget, sr, roomHalf,
                        ildGainBase_, listenerRotated, eCY, eSY, eCP, eSP, eCR, eSR,
                        currentParams);

                    dL += erResult.directL * erLevelSm;
                    dR += erResult.directR * erLevelSm;
                    erReverbAccumL = erResult.reverbL * erLevelSm * erSendSm;
                    erReverbAccumR = erResult.reverbR * erLevelSm * erSendSm;
                } else {
                    er_.sharedDelay.push(dopplerInputMono);
                }
            }
        } // end !stereoActive

        // Use blendedDistFrac (averaged from both nodes) in stereo, modDistFrac in mono
        const float effectiveDistFrac = blendedDistFrac;

        // Aux reverb send — auxMaxBoostLin pre-computed per-block (no std::pow per sample)
        if (auxL != nullptr) {
            auxPreDelayL_.push(dL);
            auxPreDelayR_.push(dR);
            const float auxDelaySamp = std::max(2.0f, auxDelaySmooth_.process(
                effectiveDistFrac * verbPreDelayMaxSamp));
            const float auxGainTarget = 1.0f + effectiveDistFrac * (auxMaxBoostLin - 1.0f);
            const float auxGain = auxGainSmooth_.process(auxGainTarget);
            auxL[i] = std::clamp(auxPreDelayL_.read(auxDelaySamp) * auxGain, -2.0f, 2.0f);
            auxR[i] = std::clamp(auxPreDelayR_.read(auxDelaySamp) * auxGain, -2.0f, 2.0f);
        } else {
            auxPreDelayL_.push(0.0f);
            auxPreDelayR_.push(0.0f);
            auxGainSmooth_.process(1.0f);
        }

        // Reverb — gate FDN when wet is zero (both target and smoothed) to save CPU.
        // FDN will build up naturally when reverb is re-enabled.
        {
            const float wetGain = verbWetSmooth_.process(currentParams.verbWet);
            if (wetGain > 1e-6f || currentParams.verbWet > 1e-6f) {
                const float preDelaySamp = effectiveDistFrac * verbPreDelayMaxSamp;
                float wetL, wetR;
                reverb_.processSample(dL + erReverbAccumL, dR + erReverbAccumR, preDelaySamp, wetL, wetR);
                if (!currentParams.bypassReverb) {
                    dL += wetGain * wetL;
                    dR += wetGain * wetR;
                }
            }
        }

        // Output clamp
        outL[i] = std::clamp(dL, -2.0f, 2.0f);
        outR[i] = std::clamp(dR, -2.0f, 2.0f);

        // DSP state capture (shared fields)
        lastDSPState_.sampleRate     = static_cast<float>(sampleRate);
        lastDSPState_.distDelaySamp  = dist_.distDelaySmooth.current();
        lastDSPState_.distGainLinear = stereoActive ? 0.0f : dist_.distGainSmooth.current();
        lastDSPState_.airCutoffHz    = stereoActive ? 0.0f : airCutoffMod;
        lastDSPState_.modX           = modX;

        // Capture last-sample LFO output values for UI display
        // Advance per-sample trig interpolation
        if (interpRotation) {
            rCosY += dCosY; rSinY += dSinY;
            rCosP += dCosP; rSinP += dSinP;
            rCosR += dCosR; rSinR += dSinR;
        }

        lastLfoOutX_ = lfoValX;
        lastLfoOutY_ = lfoValY;
        lastLfoOutZ_ = lfoValZ;
        lastLfoOutOrbitXY_ = orbitValXY;
        lastLfoOutOrbitXZ_ = orbitValXZ;
        lastLfoOutOrbitYZ_ = orbitValYZ;
    }
}

// ============================================================================
// reset()
// ============================================================================

void XYZPanEngine::reset() {
    // Binaural pipeline (ITD/ILD/shadow + comb + pinna EQ)
    src_.reset();

    // Reset tracking members so the next process() block re-evaluates them.
    lastSmoothMs_ITD_    = kDefaultSmoothMs_ITD;
    lastSmoothMs_Filter_ = kDefaultSmoothMs_Filter;
    lastSmoothMs_Gain_   = kDefaultSmoothMs_Gain;

    // Phase 3: chest bounce
    chest_.reset();

    // Phase 3: floor bounce
    floor_.reset();

    // Phase 4: distance processing
    dist_.reset();

    // Aux reverb send
    auxPreDelayL_.reset();
    auxPreDelayR_.reset();
    auxGainSmooth_.reset(1.0f);
    auxDelaySmooth_.reset(2.0f);

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
    chestR_.reset();
    floorR_.reset();
    erR_.reset();
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

    // Previous block trig for per-sample interpolation
    prevCosY_ = 1.f; prevSinY_ = 0.f;
    prevCosP_ = 1.f; prevSinP_ = 0.f;
    prevCosR_ = 1.f; prevSinR_ = 0.f;

    // Early Reflections
    er_.reset();
    erLevelSmooth_.reset(0.0f);
    erReverbSendSmooth_.reset(kERReverbSendDefault);

    // Binaural toggle smoother
    binauralBlendSmooth_.reset(1.0f);

    // LFO output values for UI display
    lastLfoOutX_ = 0.f; lastLfoOutY_ = 0.f; lastLfoOutZ_ = 0.f;
    lastLfoOutOrbitXY_ = 0.f; lastLfoOutOrbitXZ_ = 0.f; lastLfoOutOrbitYZ_ = 0.f;

    // Dev tool: test tone oscillator
    sawPhase_ = 0.0f;
    clickSamplesLeft_ = 0.0f;
    prevPulseGate_ = false;
    pulseLFO_.reset(0.0f);
    testToneGainSmooth_.reset(0.0f);
}

XYZPanEngine::LFOOutputs XYZPanEngine::getLastLFOOutputs() const noexcept {
    return { lastLfoOutX_, lastLfoOutY_, lastLfoOutZ_,
             lastLfoOutOrbitXY_, lastLfoOutOrbitXZ_, lastLfoOutOrbitYZ_ };
}

} // namespace xyzpan
