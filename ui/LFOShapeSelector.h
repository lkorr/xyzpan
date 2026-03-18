#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "xyzpan/dsp/LFO.h"

// ---------------------------------------------------------------------------
// LFOShapeSelector — horizontal row of 6 mini waveform shape buttons.
// Replaces LFOWaveformButton. Each button shows a waveform preview;
// the selected shape is highlighted with warm gold.
// ---------------------------------------------------------------------------
class LFOShapeSelector : public juce::Component,
                         private juce::AudioProcessorParameter::Listener {
public:
    LFOShapeSelector();
    ~LFOShapeSelector() override;

    // Bind to an APVTS waveform parameter (range [0, 5], step 1).
    void setParam(juce::AudioProcessorValueTreeState& apvts,
                  const juce::String& waveformParamID);

    void resized() override;

    // Returns waveform Y value in [-1, 1] for normalized phase t in [0, 1].
    static float computeWaveformY(float t, int waveform);

private:
    // Per-button inner component
    class ShapeButton : public juce::Component {
    public:
        ShapeButton(LFOShapeSelector& owner, int index);
        void paint(juce::Graphics& g) override;
        void mouseUp(const juce::MouseEvent&) override;
        void mouseEnter(const juce::MouseEvent&) override;
        void mouseExit(const juce::MouseEvent&) override;
    private:
        LFOShapeSelector& owner_;
        int index_;
        bool hovered_ = false;
    };

    void selectShape(int index);

    // AudioProcessorParameter::Listener
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int, bool) override {}

    int selected_ = 0;
    juce::AudioProcessorParameter* param_ = nullptr;
    ShapeButton buttons_[xyzpan::dsp::kLFOWaveformCount] = {
        { *this, 0 }, { *this, 1 }, { *this, 2 },
        { *this, 3 }, { *this, 4 }, { *this, 5 }
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFOShapeSelector)
};
