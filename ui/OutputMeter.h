#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

namespace xyzpan {

// ---------------------------------------------------------------------------
// OutputMeter — vertical L/R RMS meter with peak hold.
// Reads two atomic<float> RMS values (linear 0..1+) from the audio thread.
// Repaints on a timer at ~30Hz.
// ---------------------------------------------------------------------------
class OutputMeter : public juce::Component, private juce::Timer {
public:
    OutputMeter();

    void setRMSSources(std::atomic<float>* left, std::atomic<float>* right);

    void paint(juce::Graphics& g) override;
    void resized() override {}

private:
    void timerCallback() override;

    static float linearToDb(float linear) {
        return linear > 1e-6f ? 20.0f * std::log10(linear) : -100.0f;
    }

    // Map dB to 0..1 for meter display (-60dB floor, 0dB top)
    static float dbToNorm(float db) {
        constexpr float floor = -60.0f;
        if (db <= floor) return 0.0f;
        if (db >= 0.0f) return 1.0f;
        return (db - floor) / (0.0f - floor);
    }

    std::atomic<float>* rmsL_ = nullptr;
    std::atomic<float>* rmsR_ = nullptr;

    float smoothL_ = 0.f, smoothR_ = 0.f;         // smoothed RMS (linear)
    float peakL_ = 0.f, peakR_ = 0.f;              // peak hold (linear)
    float peakDecayL_ = 0.f, peakDecayR_ = 0.f;    // peak decay timer

    static constexpr float kSmoothCoeff = 0.15f;   // EMA rise/fall
    static constexpr float kPeakHoldSec = 1.0f;
    static constexpr float kPeakDecayRate = 0.02f;  // per tick fall

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputMeter)
};

} // namespace xyzpan
