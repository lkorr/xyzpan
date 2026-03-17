#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "LFOWaveformButton.h"
#include "AlchemyLookAndFeel.h"

// ---------------------------------------------------------------------------
// LFOStrip — per-axis or per-prefix LFO controls group.
// Contains: waveform display button + Rate knob (or BeatDiv combo when synced)
//           + SYNC button + Depth/Phase knobs.
//
// Two constructor forms:
//   1. LFOStrip(char axis, apvts) — axis LFOs: "lfo_x_rate" etc., shared "lfo_tempo_sync"
//   2. LFOStrip(prefix, syncParamID, apvts) — orbit LFOs: prefix + "_rate" etc.
// ---------------------------------------------------------------------------
class LFOStrip : public juce::Component,
                 private juce::AudioProcessorValueTreeState::Listener {
public:
    LFOStrip(char axis, juce::AudioProcessorValueTreeState& apvts);
    LFOStrip(const juce::String& prefix, const juce::String& syncParamID,
             juce::AudioProcessorValueTreeState& apvts);
    ~LFOStrip() override;

    void resized() override;

private:
    void init(const juce::String& rateID, const juce::String& depthID,
              const juce::String& phaseID, const juce::String& waveformID,
              const juce::String& beatDivID, const juce::String& syncID,
              juce::AudioProcessorValueTreeState& apvts);

    // APVTS listener callback (called on audio thread)
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    void updateSyncVisibility();

    // Layout constants — fixed pixel sizes
    static constexpr int kKnobSize = 79;
    static constexpr int kLabelH   = 13;
    static constexpr int kWaveW    = 24;
    static constexpr int kSyncW    = 30;
    static constexpr int kSyncH    = 18;
    static constexpr int kWavePadL = 6;    // left inset for waveform button from divider

    LFOWaveformButton waveBtn_;

    juce::Slider rateKnob_, depthKnob_, phaseKnob_;
    juce::Label  rateLabel_, depthLabel_, phaseLabel_;
    juce::TextButton syncBtn_{"Sync"};

    // BeatDiv as discrete ComboBox (replaces old slider)
    juce::ComboBox beatDivCombo_;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SA> rateAtt_, depthAtt_, phaseAtt_;
    std::unique_ptr<CA> beatDivAtt_;
    std::unique_ptr<BA> syncAtt_;

    // Sync state tracking for visibility toggle
    bool syncOn_ = false;
    juce::AudioProcessorValueTreeState* apvts_ = nullptr;
    juce::String syncParamID_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFOStrip)
};
