#pragma once

namespace xyzpan::dsp {

enum class LFOWaveform { Sine = 0, Triangle, Saw, Square };

// Phase accumulator LFO with 4 waveforms.
// Output range: [-1, 1]. Use setRateHz() + tick() pattern.
// Phase offset via reset(phaseOffsetNorm) before first tick.
class LFO {
public:
    LFOWaveform waveform = LFOWaveform::Sine;

    // Call once before processing; sets internal sample rate.
    // Does NOT reset accumulator (allows live sample rate changes without phase jump).
    void prepare(double sampleRate);

    // Set accumulator to phaseOffsetNorm, wrapped to [0, 1).
    // Call before first tick() to set initial phase.
    void reset(float phaseOffsetNorm = 0.0f);

    // Set the LFO rate in Hz. Must be called after prepare().
    void setRateHz(float hz);

    // Advance accumulator and return output in [-1, 1].
    float tick();

private:
    float  accumulator_ = 0.0f;
    float  increment_   = 0.0f;
    double sampleRate_  = 44100.0;
};

} // namespace xyzpan::dsp
