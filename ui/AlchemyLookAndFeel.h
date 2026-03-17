#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace xyzpan {

// ---------------------------------------------------------------------------
// AlchemyLookAndFeel — hermetic alchemy-inspired theme for XYZPan.
//
// Palette:
//   Background:   #1a1108  (very dark brown-black)
//   Dark iron:    #2A1A08  (knob/button background)
//   Bronze:       #8B5E2E  (room wireframe, borders)
//   Warm gold:    #C8A86B  (listener node, knob arc)
//   Parchment:    #D4B483  (text / labels)
//   Bright gold:  #E8C46A  (source node, highlights)
// ---------------------------------------------------------------------------
class AlchemyLookAndFeel : public juce::LookAndFeel_V4 {
public:
    AlchemyLookAndFeel();
    ~AlchemyLookAndFeel() override = default;

    // Rotary slider (knobs): alchemy gold arc on dark iron background
    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override;

    // Button background: dark iron fill with thin bronze border
    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    // Button text: parchment with size scaled to button height
    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    // Label text: parchment colour on transparent background
    void drawLabel(juce::Graphics& g, juce::Label& label) override;

    // Alchemy color constants — accessible for OpenGL renderer to match
    static constexpr uint32_t kBackground  = 0xFF1a1108u;
    static constexpr uint32_t kDarkIron    = 0xFF2A1A08u;
    static constexpr uint32_t kBronze      = 0xFF8B5E2Eu;
    static constexpr uint32_t kWarmGold    = 0xFFC8A86Bu;
    static constexpr uint32_t kParchment   = 0xFFD4B483u;
    static constexpr uint32_t kBrightGold  = 0xFFE8C46Au;
    static constexpr uint32_t kHoverGold   = 0xFFFFD580u;
    static constexpr uint32_t kStereoLeft  = 0xFFFF6B9Du;  // pink
    static constexpr uint32_t kStereoRight = 0xFF6B9DFFu;  // blue

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlchemyLookAndFeel)
};

} // namespace xyzpan
