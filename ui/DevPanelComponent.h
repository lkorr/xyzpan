#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <memory>

// ---------------------------------------------------------------------------
// DevPanelComponent — scrollable overlay panel for all DSP tuning constants.
//
// Shows all 40 dev parameters in four groups:
//   - Binaural (7 params)
//   - Comb Filters (21 params: 10 delays + 10 feedback + wet max)
//   - Elevation (7 params)
//   - Distance (5 params, including DOPPLER_ENABLED as ToggleButton)
//
// Plain functional appearance (NOT alchemy theme) for readability.
// Always keep alive — use setVisible() to show/hide. Never destroy/recreate.
// SliderAttachment lifetime must match component lifetime.
// ---------------------------------------------------------------------------
class DevPanelComponent : public juce::Component {
public:
    explicit DevPanelComponent(juce::AudioProcessorValueTreeState& apvts);
    ~DevPanelComponent() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    juce::Viewport viewport_;
    juce::Component content_;

    // Owned controls — created once in constructor, kept alive permanently
    std::vector<std::unique_ptr<juce::Slider>>   sliders_;
    std::vector<std::unique_ptr<juce::Label>>    labels_;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments_;

    std::vector<std::unique_ptr<juce::ToggleButton>> toggles_;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> toggleAtts_;

    // Group headers (just labels with bold font)
    std::vector<std::unique_ptr<juce::Label>> groupHeaders_;

    // Layout constants
    static constexpr int kRowH    = 36;   // height per slider row
    static constexpr int kLabelW  = 150;  // parameter name label width
    static constexpr int kSliderW = 100;  // slider width
    static constexpr int kGroupH  = 24;   // group header height
    static constexpr int kPadding = 6;    // left/right padding

    // Build helpers called from constructor
    void addGroupHeader(const juce::String& title, int& yPos);
    void addDevSlider(const juce::String& paramID, int& yPos,
                      juce::AudioProcessorValueTreeState& apvts);
    void addDevToggle(const juce::String& paramID, int& yPos,
                      juce::AudioProcessorValueTreeState& apvts);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DevPanelComponent)
};
