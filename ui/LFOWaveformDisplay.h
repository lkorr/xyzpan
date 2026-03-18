#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <atomic>

// ---------------------------------------------------------------------------
// LFOWaveformDisplay — live scrolling waveform showing ~2 cycles of the
// current LFO shape.  Reads waveform/phase/smooth/depth from APVTS.
// Phase position read from audio-thread atomic (drift-free sync with DSP).
// ---------------------------------------------------------------------------
class LFOWaveformDisplay : public juce::Component,
                           public juce::Timer,
                           private juce::AudioProcessorParameter::Listener {
public:
    LFOWaveformDisplay();
    ~LFOWaveformDisplay() override;

    void setParams(juce::AudioProcessorValueTreeState& apvts,
                   const juce::String& waveformID,
                   const juce::String& phaseID,
                   const juce::String& smoothID,
                   const juce::String& depthID);

    void setPhaseSource(std::atomic<float>* src);
    void setSHSource(std::atomic<float>* src);

    void paint(juce::Graphics& g) override;
    void visibilityChanged() override;

    // Timer
    void timerCallback() override;

private:
    // AudioProcessorParameter::Listener
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int, bool) override {}

    void readParams();

    juce::AudioProcessorParameter* waveformParam_ = nullptr;
    juce::AudioProcessorParameter* phaseParam_    = nullptr;
    juce::AudioProcessorParameter* smoothParam_   = nullptr;
    juce::AudioProcessorParameter* depthParam_    = nullptr;

    // Cached denormalized values
    int   waveform_ = 0;
    float phase_    = 0.0f;
    float smooth_   = 0.0f;
    float depth_    = 1.0f;

    // Real LFO phase from audio thread (replaces free-running accumulator)
    std::atomic<float>* phaseSource_ = nullptr;
    // S&H held value from audio thread (for accurate S&H display)
    std::atomic<float>* shSource_ = nullptr;

    static constexpr int kFps = 30;

    // S&H history ring buffer (message-thread only)
    static constexpr int kSHHistorySize = 32;
    std::array<float, kSHHistorySize> shHistory_{};
    int   shWritePos_  = 0;
    int   shCount_     = 0;     // valid entries (0..kSHHistorySize)
    float shLastValue_ = 0.0f;  // for change detection
    float shDisplayPhase_   = 0.0f;   // monotonic local phase for S&H scrolling
    float shPrevAnimPhase_  = -1.0f;  // previous animPhase for delta computation

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFOWaveformDisplay)
};
