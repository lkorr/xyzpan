#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace xyzpan {

// ---------------------------------------------------------------------------
// AlchemyLookAndFeel — hermetic alchemy-inspired theme for XYZPan.
//
// Palette:
//   Background:   #110B1A  (deep purple-black)
//   Dark iron:    #1C1228  (dark plum — knob/button background)
//   Bronze:       #7B5EA7  (muted purple — wireframe, borders)
//   Warm gold:    #D4A843  (warm gold — knob arc, slider fills)
//   Parchment:    #C8BCD8  (lavender-grey — text/labels)
//   Bright gold:  #FFD700  (bright gold — source node, highlights)
//   Hover gold:   #FFE566  (light gold-yellow — hover state)
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

    // Linear slider: standard (dark iron) or hero (glowing gold gradient) mode
    // Hero mode activated when slider's rotarySliderFillColourId == kBrightGold
    void drawLinearSlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override;

    // Label text: parchment colour on transparent background
    void drawLabel(juce::Graphics& g, juce::Label& label) override;

    // Toggle button: text-only button, gold border when ON, bronze when OFF
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

    // Alchemy color constants — accessible for OpenGL renderer to match
    static constexpr uint32_t kBackground  = 0xFF110B1Au;
    static constexpr uint32_t kDarkIron    = 0xFF1C1228u;
    static constexpr uint32_t kBronze      = 0xFF7B5EA7u;
    static constexpr uint32_t kWarmGold    = 0xFFD4A843u;
    static constexpr uint32_t kParchment   = 0xFFC8BCD8u;
    static constexpr uint32_t kBrightGold  = 0xFFFFD700u;
    static constexpr uint32_t kHoverGold   = 0xFFFFE566u;
    static constexpr uint32_t kStereoLeft  = 0xFFFF6B9Du;  // pink
    static constexpr uint32_t kStereoRight = 0xFF6B9DFFu;  // blue

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlchemyLookAndFeel)
};

} // namespace xyzpan
