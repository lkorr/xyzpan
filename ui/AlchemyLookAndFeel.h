#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace xyzpan {

// ---------------------------------------------------------------------------
// AlchemyLookAndFeel — alchemical materials-inspired theme for XYZPan.
//
// Palette (6 base colors + shade ramps):
//   1. Obsidian       #0F0D0A   Background dark
//   2. Dark Parchment #1E1A14   Background mid (panels)
//   3. Gold Leaf      #C9A84C   Primary accent
//   4. Burnt Stone    #554A37   Secondary accent
//   5. Cinnabar       #8B3A3A   Highlight (sparingly)
//   6. Aged Papyrus   #C8B88A   Text / labels
// ---------------------------------------------------------------------------
class AlchemyLookAndFeel : public juce::LookAndFeel_V4 {
public:
    AlchemyLookAndFeel();
    ~AlchemyLookAndFeel() override = default;

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    void drawLinearSlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override;

    void drawLabel(juce::Graphics& g, juce::Label& label) override;

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

    void drawComboBox(juce::Graphics& g, int width, int height,
                      bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    void drawPopupMenuItem(juce::Graphics& g,
                           const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive,
                           bool isHighlighted, bool isTicked, bool hasSubMenu,
                           const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon,
                           const juce::Colour* textColour) override;

    // ===== Base palette colors =====
    // 1. Obsidian — background dark
    static constexpr uint32_t kObsidian         = 0xFF0F0D0Au;
    static constexpr uint32_t kObsidianLight    = 0xFF1A1611u;
    static constexpr uint32_t kObsidianMid      = 0xFF252017u;
    static constexpr uint32_t kObsidianBright   = 0xFF302A1Fu;

    // 2. Dark Parchment — background mid (panels)
    static constexpr uint32_t kDarkParchment       = 0xFF1E1A14u;
    static constexpr uint32_t kDarkParchmentLight  = 0xFF2A2419u;
    static constexpr uint32_t kDarkParchmentMid    = 0xFF362E21u;
    static constexpr uint32_t kDarkParchmentBright = 0xFF43392Au;

    // 3. Gold Leaf — primary accent
    static constexpr uint32_t kGoldLeafDark  = 0xFFA68B3Au;
    static constexpr uint32_t kGoldLeaf      = 0xFFC9A84Cu;
    static constexpr uint32_t kGoldLeafLight = 0xFFD9BE6Eu;
    static constexpr uint32_t kGoldLeafPale  = 0xFFE8D49Au;

    // 4. Burnt Stone — secondary accent
    static constexpr uint32_t kVerdigrisDark  = 0xFF43392Au;
    static constexpr uint32_t kVerdigris      = 0xFF554A37u;
    static constexpr uint32_t kVerdigrisLight = 0xFF685C45u;
    static constexpr uint32_t kVerdigrisPale  = 0xFF7B6E54u;

    // 5. Cinnabar — highlight (use sparingly)
    static constexpr uint32_t kCinnabarDark  = 0xFF6B2A2Au;
    static constexpr uint32_t kCinnabar      = 0xFF8B3A3Au;
    static constexpr uint32_t kCinnabarLight = 0xFFA85454u;
    static constexpr uint32_t kCinnabarPale  = 0xFFC47878u;

    // 6. Aged Papyrus — text / labels
    static constexpr uint32_t kAgedPapyrusDark  = 0xFFA89A70u;
    static constexpr uint32_t kAgedPapyrus      = 0xFFC8B88Au;
    static constexpr uint32_t kAgedPapyrusLight = 0xFFD8CCAau;
    static constexpr uint32_t kAgedPapyrusPale  = 0xFFE8DFCAu;

    // ===== Derived functional colors =====
    static constexpr uint32_t kStereoLeft   = kGoldLeafLight;   // warm gold tint
    static constexpr uint32_t kStereoRight  = kVerdigris;       // burnt stone tint

    // ===== Legacy aliases (for code that referenced old names) =====
    static constexpr uint32_t kBackground  = kObsidian;
    static constexpr uint32_t kDarkIron    = kDarkParchment;
    static constexpr uint32_t kBronze      = kVerdigris;
    static constexpr uint32_t kWarmGold    = kGoldLeaf;
    static constexpr uint32_t kParchment   = kAgedPapyrus;
    static constexpr uint32_t kBrightGold  = kGoldLeafLight;
    static constexpr uint32_t kHoverGold   = kGoldLeafPale;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlchemyLookAndFeel)
};

} // namespace xyzpan
