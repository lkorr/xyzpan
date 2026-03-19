#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "LFOShapeSelector.h"
#include "LFOWaveformDisplay.h"
#include "AlchemyLookAndFeel.h"
#include <atomic>

// ---------------------------------------------------------------------------
// LFOStrip — per-axis or per-prefix LFO controls group.
// Contains: shape selector row + Depth/Smooth knobs + SYNC button
//           + Rate knob (or BeatDiv combo when synced) + Phase knob.
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
    void setOutputSource(std::atomic<float>* src);

private:
    void init(const juce::String& rateID, const juce::String& depthID,
              const juce::String& phaseID, const juce::String& waveformID,
              const juce::String& smoothID, const juce::String& beatDivID,
              const juce::String& syncID,
              juce::AudioProcessorValueTreeState& apvts);

    // APVTS listener callback (called on audio thread)
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    void updateSyncVisibility();

    // Layout constants — fixed pixel sizes
    static constexpr int kKnobSz       = 54;  // uniform knob size for all 4 knobs (50% larger)
    static constexpr int kLabelH        = 13;
    static constexpr int kSyncW         = 30;
    static constexpr int kSyncH         = 18;
    static constexpr int kShapeRowH     = 18;
    static constexpr int kShapeTopMargin = 4;
    static constexpr int kShapeLRMargin = 6;   // left/right padding for shape selector row
    static constexpr int kDisplayMinH  = 30;
    static constexpr int kDisplayMaxH  = 80;
    static constexpr int kDisplayLRMargin = 4;

    LFOShapeSelector shapeSelector_;
    LFOWaveformDisplay waveformDisplay_;

    juce::Slider rateKnob_, depthKnob_, phaseKnob_, smoothKnob_;
    juce::Slider beatDivKnob_;   // discrete rotary knob for synced beat division
    juce::Label  rateLabel_, depthLabel_, phaseLabel_, smoothLabel_;
    juce::TextButton syncBtn_{"Sync"};

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SA> rateAtt_, depthAtt_, phaseAtt_, smoothAtt_;
    std::unique_ptr<SA> beatDivAtt_;
    std::unique_ptr<BA> syncAtt_;

    // Sync state tracking for visibility toggle
    bool syncOn_ = false;
    juce::AudioProcessorValueTreeState* apvts_ = nullptr;
    juce::String syncParamID_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFOStrip)
};
