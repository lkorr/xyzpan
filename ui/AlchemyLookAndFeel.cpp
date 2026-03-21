#include "AlchemyLookAndFeel.h"

namespace xyzpan {

// ---------------------------------------------------------------------------
AlchemyLookAndFeel::AlchemyLookAndFeel()
{
    // Set JUCE standard colours to alchemy palette
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(kBackground));
    setColour(juce::Slider::rotarySliderFillColourId,    juce::Colour(kWarmGold));
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(kDarkIron));
    setColour(juce::Slider::thumbColourId,               juce::Colour(kParchment));
    setColour(juce::Label::textColourId,                 juce::Colour(kParchment));
    setColour(juce::TextButton::buttonColourId,          juce::Colour(kDarkIron));
    setColour(juce::TextButton::textColourOffId,         juce::Colour(kParchment));
    setColour(juce::TextButton::textColourOnId,          juce::Colour(kBrightGold));
    setColour(juce::ComboBox::backgroundColourId,        juce::Colour(kDarkIron));
    setColour(juce::ComboBox::textColourId,              juce::Colour(kParchment));
    setColour(juce::PopupMenu::backgroundColourId,       juce::Colour(kDarkIron));
    setColour(juce::PopupMenu::textColourId,             juce::Colour(kParchment));
}

// ---------------------------------------------------------------------------
void AlchemyLookAndFeel::drawRotarySlider(
    juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPosProportional,
    float rotaryStartAngle, float rotaryEndAngle,
    juce::Slider& slider)
{
    const float radius    = static_cast<float>(juce::jmin(width, height)) * 0.5f - 4.0f;
    const float centreX   = static_cast<float>(x) + static_cast<float>(width)  * 0.5f;
    const float centreY   = static_cast<float>(y) + static_cast<float>(height) * 0.5f;

    const float disabledAlpha = slider.isEnabled() ? 1.0f : 0.3f;

    // Background circle — dark iron
    g.setColour(juce::Colour(kDarkIron).withAlpha(disabledAlpha));
    g.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

    // Outer ring — bronze
    g.setColour(juce::Colour(kBronze).withAlpha(disabledAlpha));
    g.drawEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f, 1.5f);

    // Arc track — very dark background arc from start to end
    {
        const float arcRadius = radius * 0.78f;
        juce::Path trackArc;
        trackArc.addCentredArc(centreX, centreY, arcRadius, arcRadius,
                               0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(kBronze).withAlpha(0.3f * disabledAlpha));
        g.strokePath(trackArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    }

    // Value arc — use slider's fill colour (allows per-knob colouring)
    {
        const float arcRadius  = radius * 0.78f;
        const float angle      = rotaryStartAngle + sliderPosProportional
                                 * (rotaryEndAngle - rotaryStartAngle);
        juce::Path valueArc;
        valueArc.addCentredArc(centreX, centreY, arcRadius, arcRadius,
                               0.0f, rotaryStartAngle, angle, true);
        g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId).withAlpha(disabledAlpha));
        g.strokePath(valueArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    }

    // Thumb dot — parchment coloured circle at current value position
    {
        const float dotRadius  = 4.0f;
        const float angle      = rotaryStartAngle + sliderPosProportional
                                 * (rotaryEndAngle - rotaryStartAngle) - juce::MathConstants<float>::halfPi;
        const float dotR       = radius * 0.78f;
        const float dotX       = centreX + dotR * std::cos(angle);
        const float dotY       = centreY + dotR * std::sin(angle);
        g.setColour(juce::Colour(kParchment).withAlpha(disabledAlpha));
        g.fillEllipse(dotX - dotRadius, dotY - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
    }
}

// ---------------------------------------------------------------------------
void AlchemyLookAndFeel::drawLinearSlider(
    juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
    const juce::Slider::SliderStyle style,
    juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal) {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height,
                                          sliderPos, 0.0f, 1.0f, style, slider);
        return;
    }

    const bool enabled = slider.isEnabled();
    const float disabledAlpha = enabled ? 1.0f : 0.3f;
    const bool hero = slider.findColour(juce::Slider::rotarySliderFillColourId)
                      == juce::Colour(kBrightGold);
    const float trackH  = hero ? 6.0f : 3.0f;
    const float thumbR   = hero ? 5.0f : 3.5f;
    const float cy       = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float left     = static_cast<float>(x) + thumbR;
    const float right    = static_cast<float>(x + width) - thumbR;
    const float trackTop = cy - trackH * 0.5f;

    if (hero && enabled) {
        // Outer glow — semi-transparent bright gold shadow behind track
        g.setColour(juce::Colour(kBrightGold).withAlpha(0.15f));
        g.fillRoundedRectangle(left - 2.0f, trackTop - 3.0f,
                               (right - left) + 4.0f, trackH + 6.0f, 4.0f);
    }

    // Background track — dark iron with bronze outline
    g.setColour(juce::Colour(kDarkIron).withAlpha(disabledAlpha));
    g.fillRoundedRectangle(left, trackTop, right - left, trackH, trackH * 0.5f);
    g.setColour(juce::Colour(kBronze).withAlpha(0.5f * disabledAlpha));
    g.drawRoundedRectangle(left, trackTop, right - left, trackH, trackH * 0.5f, 1.0f);

    // Value fill
    const float fillW = sliderPos - left;
    if (fillW > 0.5f) {
        if (hero) {
            // Warm-gold → bright-gold horizontal gradient
            g.setGradientFill(juce::ColourGradient(
                juce::Colour(kWarmGold).withAlpha(disabledAlpha), left, cy,
                juce::Colour(kBrightGold).withAlpha(disabledAlpha), sliderPos, cy, false));
        } else {
            g.setColour(juce::Colour(kWarmGold).withAlpha(disabledAlpha));
        }
        g.fillRoundedRectangle(left, trackTop, fillW, trackH, trackH * 0.5f);
    }

    // Thumb
    g.setColour(juce::Colour(kParchment).withAlpha(disabledAlpha));
    g.fillEllipse(sliderPos - thumbR, cy - thumbR, thumbR * 2.0f, thumbR * 2.0f);
}

// ---------------------------------------------------------------------------
void AlchemyLookAndFeel::drawButtonBackground(
    juce::Graphics& g,
    juce::Button& button,
    const juce::Colour& /*backgroundColour*/,
    bool shouldDrawButtonAsHighlighted,
    bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);

    // Fill: dark iron; slightly brighter when pressed or highlighted
    juce::Colour fill = juce::Colour(kDarkIron);
    if (shouldDrawButtonAsDown)
        fill = fill.brighter(0.2f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.brighter(0.1f);

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 3.0f);

    // Border: bronze
    const juce::Colour border = button.getToggleState()
        ? juce::Colour(kWarmGold)
        : juce::Colour(kBronze);
    g.setColour(border);
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
}

// ---------------------------------------------------------------------------
void AlchemyLookAndFeel::drawButtonText(
    juce::Graphics& g,
    juce::TextButton& button,
    bool /*shouldDrawButtonAsHighlighted*/,
    bool /*shouldDrawButtonAsDown*/)
{
    const auto fontSize = juce::jmin(12.0f, static_cast<float>(button.getHeight()) * 0.5f);
    g.setFont(juce::Font(juce::FontOptions(fontSize)));

    const auto textColour = button.getToggleState()
        ? button.findColour(juce::TextButton::textColourOnId)
        : button.findColour(juce::TextButton::textColourOffId);

    g.setColour(textColour.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f));
    g.drawText(button.getButtonText(), button.getLocalBounds(),
               juce::Justification::centred, true);
}

// ---------------------------------------------------------------------------
void AlchemyLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.fillAll(label.findColour(juce::Label::backgroundColourId));

    if (!label.isBeingEdited()) {
        const float alpha = label.isEnabled() ? 1.0f : 0.5f;
        const juce::Font font(juce::FontOptions(static_cast<float>(label.getHeight()) * 0.75f));
        g.setColour(juce::Colour(kParchment).withAlpha(alpha));
        g.setFont(font);

        auto textArea = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());
        g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                         juce::jmax(1, static_cast<int>(static_cast<float>(textArea.getHeight())
                                                         / font.getHeight())),
                         label.getMinimumHorizontalScale());
    }
    else if (label.isEnabled()) {
        g.setColour(juce::Colour(kParchment));
        g.drawRect(label.getLocalBounds());
    }
}

// ---------------------------------------------------------------------------
void AlchemyLookAndFeel::drawComboBox(
    juce::Graphics& g, int width, int height,
    bool isButtonDown,
    int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
    juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float>(0.0f, 0.0f,
        static_cast<float>(width), static_cast<float>(height));

    g.setColour(juce::Colour(kDarkIron));
    g.fillRoundedRectangle(bounds, 3.0f);

    g.setColour(isButtonDown ? juce::Colour(kWarmGold) : juce::Colour(kBronze));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.0f);

    {
        const float arrowH = static_cast<float>(height) * 0.3f;
        const float arrowW = arrowH * 1.2f;
        const float arrowX = static_cast<float>(width) - arrowW - 8.0f;
        const float arrowY = (static_cast<float>(height) - arrowH) * 0.5f;

        juce::Path arrow;
        arrow.addTriangle(arrowX, arrowY,
                          arrowX + arrowW, arrowY,
                          arrowX + arrowW * 0.5f, arrowY + arrowH);
        g.setColour(juce::Colour(kWarmGold).withAlpha(box.isEnabled() ? 1.0f : 0.4f));
        g.fillPath(arrow);
    }
}

// ---------------------------------------------------------------------------
void AlchemyLookAndFeel::drawPopupMenuItem(
    juce::Graphics& g,
    const juce::Rectangle<int>& area,
    bool isSeparator, bool isActive,
    bool isHighlighted, bool isTicked, bool /*hasSubMenu*/,
    const juce::String& text,
    const juce::String& /*shortcutKeyText*/,
    const juce::Drawable* /*icon*/,
    const juce::Colour* /*textColour*/)
{
    if (isSeparator)
    {
        g.setColour(juce::Colour(kBronze).withAlpha(0.4f));
        g.fillRect(area.reduced(5, 0).removeFromTop(1));
        return;
    }

    if (isHighlighted && isActive)
    {
        g.setColour(juce::Colour(kBronze).withAlpha(0.25f));
        g.fillRect(area);
    }

    g.setFont(juce::Font(juce::FontOptions(
        juce::jmin(14.0f, static_cast<float>(area.getHeight()) * 0.7f))));

    juce::Colour textCol = isActive
        ? juce::Colour(kParchment) : juce::Colour(kParchment).withAlpha(0.4f);
    if (isHighlighted && isActive) textCol = juce::Colour(kBrightGold);
    if (isTicked) textCol = juce::Colour(kWarmGold);

    g.setColour(textCol);
    g.drawText(text, area.reduced(12, 0), juce::Justification::centredLeft, true);

    if (isTicked)
    {
        const float dotR = 3.0f;
        const float dotX = static_cast<float>(area.getX()) + 5.0f;
        const float dotY = static_cast<float>(area.getCentreY());
        g.setColour(juce::Colour(kWarmGold));
        g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);
    }
}

// ---------------------------------------------------------------------------
void AlchemyLookAndFeel::drawToggleButton(
    juce::Graphics& g, juce::ToggleButton& button,
    bool shouldDrawButtonAsHighlighted,
    bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);

    juce::Colour fill = juce::Colour(kDarkIron);
    if (shouldDrawButtonAsDown)
        fill = fill.brighter(0.2f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.brighter(0.1f);
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 3.0f);

    const juce::Colour border = button.getToggleState()
        ? juce::Colour(kWarmGold)
        : juce::Colour(kBronze);
    g.setColour(border);
    g.drawRoundedRectangle(bounds, 3.0f, 1.5f);

    const auto fontSize = juce::jmin(12.0f,
        static_cast<float>(button.getHeight()) * 0.5f);
    g.setFont(juce::Font(juce::FontOptions(fontSize)));
    g.setColour(button.getToggleState()
        ? juce::Colour(kBrightGold)
        : juce::Colour(kParchment).withMultipliedAlpha(
            button.isEnabled() ? 1.0f : 0.5f));
    g.drawText(button.getButtonText(), button.getLocalBounds(),
               juce::Justification::centred, true);
}

} // namespace xyzpan
