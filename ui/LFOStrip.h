#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "LFOWaveformButton.h"
#include "AlchemyLookAndFeel.h"

// ---------------------------------------------------------------------------
// LFOStrip — per-axis LFO controls group.
// Contains: waveform display button + Rate/Depth/Phase knobs + SYNC button.
// Axis must be 'X', 'Y', or 'Z'.
// ---------------------------------------------------------------------------
class LFOStrip : public juce::Component {
public:
    LFOStrip(char axis, juce::AudioProcessorValueTreeState& apvts);
    ~LFOStrip() override = default;

    void resized() override;

private:
    LFOWaveformButton waveBtn_;

    juce::Slider rateKnob_, depthKnob_, phaseKnob_;
    juce::Label  rateLabel_, depthLabel_, phaseLabel_;
    juce::TextButton syncBtn_{"SYNC"};

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SA> rateAtt_, depthAtt_, phaseAtt_;
    std::unique_ptr<BA> syncAtt_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFOStrip)
};
