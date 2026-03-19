#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <atomic>

// ---------------------------------------------------------------------------
// LFOWaveformDisplay — live oscilloscope trace of actual LFO output history.
// Samples the engine's final tick()*depth value at 30fps into a ring buffer
// and renders it as a scrolling waveform. All waveforms handled identically.
// ---------------------------------------------------------------------------
class LFOWaveformDisplay : public juce::Component,
                           public juce::Timer {
public:
    LFOWaveformDisplay();
    ~LFOWaveformDisplay() override = default;

    void setOutputSource(std::atomic<float>* src);

    void paint(juce::Graphics& g) override;
    void visibilityChanged() override;

    // Timer
    void timerCallback() override;

private:
    std::atomic<float>* outputSource_ = nullptr;

    static constexpr int kFps = 30;
    static constexpr int kHistorySize = 64;   // ~2.1s at 30fps
    std::array<float, kHistorySize> history_{};
    int historyWritePos_ = 0;
    int historyCount_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFOWaveformDisplay)
};
