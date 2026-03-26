#include "AlchemyLookAndFeel.h"

namespace xyzpan {

// ---------------------------------------------------------------------------
AlchemyLookAndFeel::AlchemyLookAndFeel()
{
    applyTheme(ColorTheme{}); // default = Alchemy Gold
}

// ---------------------------------------------------------------------------
void AlchemyLookAndFeel::applyTheme(const ColorTheme& theme)
{
    activeTheme_ = theme;

    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(theme.background));
    setColour(juce::Slider::rotarySliderFillColourId,    juce::Colour(theme.lfoAccent));
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(theme.darkIron));
    setColour(juce::Slider::thumbColourId,               juce::Colour(theme.parchment));
    setColour(juce::Label::textColourId,                 juce::Colour(theme.parchment));
    setColour(juce::TextButton::buttonColourId,          juce::Colour(theme.darkIron));
    setColour(juce::TextButton::textColourOffId,         juce::Colour(theme.parchment));
    setColour(juce::TextButton::textColourOnId,          juce::Colour(theme.brightGold));
    setColour(juce::ComboBox::backgroundColourId,        juce::Colour(theme.darkIron));
    setColour(juce::ComboBox::textColourId,              juce::Colour(theme.parchment));
    setColour(juce::PopupMenu::backgroundColourId,       juce::Colour(theme.darkIron));
    setColour(juce::PopupMenu::textColourId,             juce::Colour(theme.parchment));
}

// ---------------------------------------------------------------------------
void AlchemyLookAndFeel::drawRotarySlider(
    juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPosProportional,
    float rotaryStartAngle, float rotaryEndAngle,
    juce::Slider& slider)
{
    const auto& t = activeTheme_;

    const float radius    = static_cast<float>(juce::jmin(width, height)) * 0.5f - 4.0f;
    const float centreX   = static_cast<float>(x) + static_cast<float>(width)  * 0.5f;
    const float centreY   = static_cast<float>(y) + static_cast<float>(height) * 0.5f;

    const bool  enabled       = slider.isEnabled();
    const float disabledAlpha = enabled ? 1.0f : 0.3f;
    const bool  hovering      = slider.isMouseOverOrDragging() && enabled;
    const bool  dragging      = slider.isMouseButtonDown() && enabled;
    const auto  arcColour     = slider.findColour(juce::Slider::rotarySliderFillColourId);
    const float valueAngle    = rotaryStartAngle + sliderPosProportional
                                * (rotaryEndAngle - rotaryStartAngle);

    // --- Layer 1: Hover glow (behind everything) ---
    if (hovering)
    {
        const float glowR = radius * 1.3f;
        const float glowAlpha = dragging ? 0.12f : 0.08f;
        juce::ColourGradient glow(arcColour.withAlpha(glowAlpha * disabledAlpha),
                                  centreX, centreY,
                                  arcColour.withAlpha(0.0f),
                                  centreX + glowR, centreY, true);
        g.setGradientFill(glow);
        g.fillEllipse(centreX - glowR, centreY - glowR, glowR * 2.0f, glowR * 2.0f);
    }

    // --- Layer 2: Drop shadow ---
    {
        const float shadowAlpha = hovering ? 0.8f : 0.6f;
        const float shadowBlur  = radius * (hovering ? 0.2f : 0.15f);
        const float shadowOfsY  = radius * 0.06f;
        juce::DropShadow shadow(juce::Colour(t.obsidian).withAlpha(shadowAlpha * disabledAlpha),
                                static_cast<int>(shadowBlur),
                                juce::Point<int>(0, static_cast<int>(shadowOfsY)));
        juce::Path shadowEllipse;
        shadowEllipse.addEllipse(centreX - radius, centreY - radius,
                                 radius * 2.0f, radius * 2.0f);
        shadow.drawForPath(g, shadowEllipse);
    }

    // --- Layer 3: Outer bevel ring ---
    {
        const float ringWidth = juce::jmax(1.5f, radius * 0.08f);
        g.setColour(juce::Colour(t.bronze).withAlpha(disabledAlpha));
        g.drawEllipse(centreX - radius, centreY - radius,
                      radius * 2.0f, radius * 2.0f, ringWidth);
    }

    // --- Layer 4: Knob body (radial gradient) ---
    const float bodyRadius = radius * 0.88f;
    {
        const float lightOfsX = -bodyRadius * 0.3f;
        const float lightOfsY = -bodyRadius * 0.3f;
        juce::ColourGradient bodyGrad(
            juce::Colour(t.darkParchmentMid).withAlpha(disabledAlpha),
            centreX + lightOfsX, centreY + lightOfsY,
            juce::Colour(t.obsidianLight).withAlpha(disabledAlpha),
            centreX + bodyRadius, centreY + bodyRadius, true);
        bodyGrad.addColour(0.55, juce::Colour(t.darkParchment).withAlpha(disabledAlpha));
        g.setGradientFill(bodyGrad);
        g.fillEllipse(centreX - bodyRadius, centreY - bodyRadius,
                      bodyRadius * 2.0f, bodyRadius * 2.0f);
    }

    // --- Layer 5: Inner edge shadow ---
    {
        const float edgeWidth = juce::jmax(1.0f, radius * 0.04f);
        g.setColour(juce::Colour(t.obsidian).withAlpha(0.4f * disabledAlpha));
        g.drawEllipse(centreX - bodyRadius, centreY - bodyRadius,
                      bodyRadius * 2.0f, bodyRadius * 2.0f, edgeWidth);
    }

    // --- Layer 6: Specular highlight ---
    {
        const float specR   = bodyRadius * 0.35f;
        const float specOfsX = -bodyRadius * 0.25f;
        const float specOfsY = -bodyRadius * 0.3f;
        juce::ColourGradient specGrad(
            juce::Colour(t.agedPapyrus).withAlpha(0.12f * disabledAlpha),
            centreX + specOfsX, centreY + specOfsY,
            juce::Colour(t.agedPapyrus).withAlpha(0.0f),
            centreX + specOfsX + specR, centreY + specOfsY + specR, true);
        g.setGradientFill(specGrad);
        g.fillEllipse(centreX + specOfsX - specR, centreY + specOfsY - specR,
                      specR * 2.0f, specR * 2.0f);
    }

    // --- Layer 7: Metallic sheen ---
    {
        juce::Graphics::ScopedSaveState saveState(g);
        juce::Path bodyClip;
        bodyClip.addEllipse(centreX - bodyRadius, centreY - bodyRadius,
                            bodyRadius * 2.0f, bodyRadius * 2.0f);
        g.reduceClipRegion(bodyClip);

        juce::ColourGradient sheenGrad(
            juce::Colour(t.agedPapyrusDark).withAlpha(0.0f),
            centreX - bodyRadius, centreY - bodyRadius * 0.5f,
            juce::Colour(t.agedPapyrusDark).withAlpha(0.0f),
            centreX + bodyRadius, centreY + bodyRadius * 0.5f, false);
        sheenGrad.addColour(0.45, juce::Colour(t.agedPapyrusDark).withAlpha(0.07f * disabledAlpha));
        sheenGrad.addColour(0.55, juce::Colour(t.agedPapyrusDark).withAlpha(0.07f * disabledAlpha));
        g.setGradientFill(sheenGrad);
        g.fillRect(centreX - bodyRadius, centreY - bodyRadius,
                   bodyRadius * 2.0f, bodyRadius * 2.0f);
    }

    // --- Layer 8: Arc track (recessed groove) ---
    const float arcRadius = radius * 0.68f;
    const float arcStroke = juce::jlimit(2.5f, 6.0f, radius * 0.1f);
    {
        juce::Path trackArc;
        trackArc.addCentredArc(centreX, centreY, arcRadius, arcRadius,
                               0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(t.obsidian).withAlpha(0.6f * disabledAlpha));
        g.strokePath(trackArc, juce::PathStrokeType(arcStroke,
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // --- Layer 9: Value arc + glow ---
    {
        juce::Path valueArc;
        valueArc.addCentredArc(centreX, centreY, arcRadius, arcRadius,
                               0.0f, rotaryStartAngle, valueAngle, true);

        // Glow behind arc
        g.setColour(arcColour.withAlpha(0.15f * disabledAlpha));
        g.strokePath(valueArc, juce::PathStrokeType(arcStroke * 3.0f,
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Solid arc
        g.setColour(arcColour.withAlpha(disabledAlpha));
        g.strokePath(valueArc, juce::PathStrokeType(arcStroke,
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // --- Layer 10: Indicator line (replaces thumb dot) ---
    {
        const float lineAngle = valueAngle - juce::MathConstants<float>::halfPi;
        const float innerR    = radius * 0.25f;
        const float outerR    = radius * 0.62f;
        const float lineWidth = juce::jlimit(1.5f, 3.0f, radius * 0.04f);
        const float cosA = std::cos(lineAngle);
        const float sinA = std::sin(lineAngle);

        juce::Path indicator;
        indicator.startNewSubPath(centreX + innerR * cosA, centreY + innerR * sinA);
        indicator.lineTo(centreX + outerR * cosA, centreY + outerR * sinA);

        // Glow behind line
        g.setColour(juce::Colour(t.agedPapyrus).withAlpha(0.15f * disabledAlpha));
        g.strokePath(indicator, juce::PathStrokeType(lineWidth * 3.0f,
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Solid indicator
        g.setColour(juce::Colour(t.agedPapyrus).withAlpha(0.9f * disabledAlpha));
        g.strokePath(indicator, juce::PathStrokeType(lineWidth,
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
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

    const auto& t = activeTheme_;

    const bool enabled = slider.isEnabled();
    const float disabledAlpha = enabled ? 1.0f : 0.3f;
    const bool hero = slider.findColour(juce::Slider::rotarySliderFillColourId)
                      == juce::Colour(t.brightGold);
    const float trackH  = hero ? 6.0f : 3.0f;
    const float thumbR   = hero ? 5.0f : 3.5f;
    const float cy       = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float left     = static_cast<float>(x) + thumbR;
    const float right    = static_cast<float>(x + width) - thumbR;
    const float trackTop = cy - trackH * 0.5f;

    if (hero && enabled) {
        // Outer glow
        g.setColour(juce::Colour(t.brightGold).withAlpha(0.15f));
        g.fillRoundedRectangle(left - 2.0f, trackTop - 3.0f,
                               (right - left) + 4.0f, trackH + 6.0f, 4.0f);
    }

    // Background track
    g.setColour(juce::Colour(t.darkIron).withAlpha(disabledAlpha));
    g.fillRoundedRectangle(left, trackTop, right - left, trackH, trackH * 0.5f);
    g.setColour(juce::Colour(t.bronze).withAlpha(0.5f * disabledAlpha));
    g.drawRoundedRectangle(left, trackTop, right - left, trackH, trackH * 0.5f, 1.0f);

    // Value fill
    const float fillW = sliderPos - left;
    if (fillW > 0.5f) {
        if (hero) {
            g.setGradientFill(juce::ColourGradient(
                juce::Colour(t.warmGold).withAlpha(disabledAlpha), left, cy,
                juce::Colour(t.brightGold).withAlpha(disabledAlpha), sliderPos, cy, false));
        } else {
            g.setColour(juce::Colour(t.warmGold).withAlpha(disabledAlpha));
        }
        g.fillRoundedRectangle(left, trackTop, fillW, trackH, trackH * 0.5f);
    }

    // Thumb
    g.setColour(juce::Colour(t.parchment).withAlpha(disabledAlpha));
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
    const auto& t = activeTheme_;
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);

    juce::Colour fill = juce::Colour(t.darkIron);
    if (shouldDrawButtonAsDown)
        fill = fill.brighter(0.2f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.brighter(0.1f);

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 3.0f);

    const juce::Colour border = button.getToggleState()
        ? juce::Colour(t.warmGold)
        : juce::Colour(t.bronze);
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
    const auto& t = activeTheme_;

    g.fillAll(label.findColour(juce::Label::backgroundColourId));

    if (!label.isBeingEdited()) {
        const float alpha = label.isEnabled() ? 1.0f : 0.5f;
        const auto textColour = label.findColour(juce::Label::textColourId);
        const juce::Font font(juce::FontOptions(static_cast<float>(label.getHeight()) * 0.75f));
        g.setFont(font);

        auto textArea = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());
        const int maxLines = juce::jmax(1, static_cast<int>(
            static_cast<float>(textArea.getHeight()) / font.getHeight()));

        // Glow pass for XYZ hero labels (identified by goldLeafPale text colour)
        if (textColour == juce::Colour(t.goldLeafPale)) {
            g.setColour(textColour.withAlpha(0.20f * alpha));
            g.drawFittedText(label.getText(), textArea.translated(0, 1),
                             label.getJustificationType(), maxLines,
                             label.getMinimumHorizontalScale());
        }

        g.setColour(textColour.withAlpha(alpha));
        g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                         maxLines, label.getMinimumHorizontalScale());
    }
    else if (label.isEnabled()) {
        g.setColour(juce::Colour(t.parchment));
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
    const auto& t = activeTheme_;
    const auto bounds = juce::Rectangle<float>(0.0f, 0.0f,
        static_cast<float>(width), static_cast<float>(height));

    g.setColour(juce::Colour(t.darkIron));
    g.fillRoundedRectangle(bounds, 3.0f);

    g.setColour(isButtonDown ? juce::Colour(t.warmGold) : juce::Colour(t.bronze));
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
        g.setColour(juce::Colour(t.warmGold).withAlpha(box.isEnabled() ? 1.0f : 0.4f));
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
    const auto& t = activeTheme_;

    if (isSeparator)
    {
        g.setColour(juce::Colour(t.bronze).withAlpha(0.4f));
        g.fillRect(area.reduced(5, 0).removeFromTop(1));
        return;
    }

    if (isHighlighted && isActive)
    {
        g.setColour(juce::Colour(t.bronze).withAlpha(0.25f));
        g.fillRect(area);
    }

    g.setFont(juce::Font(juce::FontOptions(
        juce::jmin(14.0f, static_cast<float>(area.getHeight()) * 0.7f))));

    juce::Colour textCol = isActive
        ? juce::Colour(t.parchment) : juce::Colour(t.parchment).withAlpha(0.4f);
    if (isHighlighted && isActive) textCol = juce::Colour(t.brightGold);
    if (isTicked) textCol = juce::Colour(t.warmGold);

    g.setColour(textCol);
    g.drawText(text, area.reduced(12, 0), juce::Justification::centredLeft, true);

    if (isTicked)
    {
        const float dotR = 3.0f;
        const float dotX = static_cast<float>(area.getX()) + 5.0f;
        const float dotY = static_cast<float>(area.getCentreY());
        g.setColour(juce::Colour(t.warmGold));
        g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);
    }
}

// ---------------------------------------------------------------------------
void AlchemyLookAndFeel::drawToggleButton(
    juce::Graphics& g, juce::ToggleButton& button,
    bool shouldDrawButtonAsHighlighted,
    bool shouldDrawButtonAsDown)
{
    const auto& t = activeTheme_;

    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);

    juce::Colour fill = juce::Colour(t.darkIron);
    if (shouldDrawButtonAsDown)
        fill = fill.brighter(0.2f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.brighter(0.1f);
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 3.0f);

    const juce::Colour border = button.getToggleState()
        ? juce::Colour(t.warmGold)
        : juce::Colour(t.bronze);
    g.setColour(border);
    g.drawRoundedRectangle(bounds, 3.0f, 1.5f);

    if (button.getButtonText().isEmpty()) {
        // Checkbox style — subtle inner glow when toggled on
        if (button.getToggleState()) {
            auto inner = bounds.reduced(4.0f);

            auto cx = inner.getCentreX();
            auto cy = inner.getCentreY();
            auto radius = juce::jmax(inner.getWidth(), inner.getHeight()) * 0.55f;
            juce::ColourGradient glow(
                juce::Colour(t.warmGold).withAlpha(0.25f), cx, cy,
                juce::Colour(t.warmGold).withAlpha(0.0f), cx + radius, cy + radius,
                true);
            g.setGradientFill(glow);
            g.fillRoundedRectangle(inner, 2.0f);

            g.setColour(juce::Colour(t.warmGold).withAlpha(0.06f));
            g.fillRoundedRectangle(inner, 2.0f);
        }
    } else {
        const auto fontSize = juce::jmin(12.0f,
            static_cast<float>(button.getHeight()) * 0.5f);
        g.setFont(juce::Font(juce::FontOptions(fontSize)));
        g.setColour(button.getToggleState()
            ? juce::Colour(t.brightGold)
            : juce::Colour(t.parchment).withMultipliedAlpha(
                button.isEnabled() ? 1.0f : 0.5f));
        g.drawText(button.getButtonText(), button.getLocalBounds(),
                   juce::Justification::centred, true);
    }
}

} // namespace xyzpan
