#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// ---------------------------------------------------------------------------
// LFOWaveformButton — clickable mini waveform display.
// Cycles 0=sine, 1=triangle, 2=saw, 3=square on each click.
// Reads/writes the LFO waveform parameter via APVTS.
// ---------------------------------------------------------------------------
class LFOWaveformButton : public juce::Component {
public:
    LFOWaveformButton();

    // Bind to an APVTS waveform parameter (must be called before paint/click).
    void setParam(juce::AudioProcessorValueTreeState& apvts,
                  const juce::String& waveformParamID);

    // Sync display from the current APVTS value (call after setParam).
    void parameterChanged();

    void paint(juce::Graphics& g) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    int waveform_ = 0; // 0=sine, 1=triangle, 2=saw, 3=square
    bool hovered_ = false;
    juce::AudioProcessorParameter* param_ = nullptr;

    // t in [0,1] → [-1,1]; matches engine LFO waveform formulas exactly.
    static float computeWaveformY(float t, int waveform);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFOWaveformButton)
};
