#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "xyzpan/DSPStateBridge.h"
#include <vector>
#include <memory>
#include <unordered_map>

// ---------------------------------------------------------------------------
// DevPanelComponent — scrollable overlay panel for all DSP tuning constants.
//
// Shows all dev parameters in collapsible groups + live DSP readout section.
// Plain functional appearance (NOT alchemy theme) for readability.
// Always keep alive — use setVisible() to show/hide. Never destroy/recreate.
// SliderAttachment lifetime must match component lifetime.
// ---------------------------------------------------------------------------
class DevPanelComponent : public juce::Component,
                          public juce::Timer {
public:
    explicit DevPanelComponent(juce::AudioProcessorValueTreeState& apvts,
                               xyzpan::DSPStateBridge* dspBridge = nullptr);
    ~DevPanelComponent() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;
    void timerCallback() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& e,
                        const juce::MouseWheelDetails& wheel) override;

    // Returns the custom width set by drag, or 0 if default
    int getCustomWidth() const { return customWidth_; }

    // Collapsible section — holds header + child component pointers
    struct CollapsibleSection {
        juce::Label* header = nullptr;
        std::vector<juce::Component*> children;
        bool collapsed = false;
    };

private:
    juce::Viewport viewport_;
    juce::Component content_;

    xyzpan::DSPStateBridge* dspBridge_ = nullptr;

    // Owned controls — created once in constructor, kept alive permanently
    std::vector<std::unique_ptr<juce::Slider>>   sliders_;
    std::vector<std::unique_ptr<juce::Label>>    labels_;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments_;

    std::vector<std::unique_ptr<juce::ToggleButton>> toggles_;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> toggleAtts_;

    // Group headers (just labels with bold font)
    std::vector<std::unique_ptr<juce::Label>> groupHeaders_;

    // DSP readout value labels (updated by timer)
    std::vector<std::unique_ptr<juce::Label>> readoutNameLabels_;
    std::vector<std::unique_ptr<juce::Label>> readoutValueLabels_;

    // Collapsible sections
    std::vector<CollapsibleSection> sections_;
    int currentSectionIdx_ = -1;

    // Layout constants
    static constexpr int kRowH    = 36;   // height per slider row
    static constexpr int kLabelW  = 150;  // parameter name label width
    static constexpr int kSliderW = 100;  // slider width
    static constexpr int kGroupH  = 24;   // group header height
    static constexpr int kPadding = 6;    // left/right padding
    static constexpr int kInfoBoxH = 80;  // hover description box height

    // Hover info box
    std::unordered_map<juce::Component*, juce::String> componentToDescKey_;
    juce::String currentInfoText_;
    static const std::unordered_map<juce::String, juce::String>& getDescriptions();

    // Build helpers called from constructor
    void beginSection(const juce::String& title);
    void addDevSlider(const juce::String& paramID,
                      juce::AudioProcessorValueTreeState& apvts);
    void addDevToggle(const juce::String& paramID,
                      juce::AudioProcessorValueTreeState& apvts);
    void addReadonlyLabel(const juce::String& name);

    // Recalculates all component positions based on collapsed state
    void relayout();

    // Drag-to-resize: thin overlay component on the left edge
    static constexpr int kDragHandleW = 6;
    int customWidth_ = 0;       // 0 = use parent default; >0 = user-dragged width
    bool dragging_ = false;
    int dragStartX_ = 0;
    int dragStartW_ = 0;

    struct DragHandle : public juce::Component {
        DevPanelComponent& owner;
        explicit DragHandle(DevPanelComponent& o) : owner(o) { setMouseCursor(juce::MouseCursor::LeftRightResizeCursor); }
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent&) override;
    };
    DragHandle dragHandle_ { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DevPanelComponent)
};
