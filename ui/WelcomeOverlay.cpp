#include "WelcomeOverlay.h"
#include "xyzpan/obfuscate.h"

#define OBF(s) juce::String(static_cast<const char*>(AY_OBFUSCATE(s)))

namespace xyzpan {

WelcomeOverlay::WelcomeOverlay()
{
    keyInput_.setMultiLine(false);
    keyInput_.setReturnKeyStartsNewLine(false);
    keyInput_.setTextToShowWhenEmpty(OBF("Enter your license key..."), juce::Colour(0x60C8B88A));
    keyInput_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF1A1611));
    keyInput_.setColour(juce::TextEditor::textColourId, juce::Colour(0xFFE8D49A));
    keyInput_.setColour(juce::TextEditor::outlineColourId, juce::Colour(0x40C9A84C));
    keyInput_.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0x80C9A84C));
    keyInput_.setFont(juce::Font(15.0f));
    keyInput_.onReturnKey = [this] {
        if (onSubmit && !loading_)
            onSubmit(keyInput_.getText());
    };
    addAndMakeVisible(keyInput_);

    submitBtn_.setButtonText(OBF("Activate"));
    submitBtn_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF554A37));
    submitBtn_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFE8D49A));
    submitBtn_.onClick = [this] {
        if (onSubmit && !loading_)
            onSubmit(keyInput_.getText());
    };
    addAndMakeVisible(submitBtn_);

    skipBtn_.setButtonText(OBF("Continue in Demo Mode"));
    skipBtn_.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    skipBtn_.setColour(juce::TextButton::textColourOffId, juce::Colour(0x80C8B88A));
    skipBtn_.onClick = [this] {
        if (onSkip) onSkip();
    };
    addAndMakeVisible(skipBtn_);

    statusLabel_.setJustificationType(juce::Justification::centred);
    statusLabel_.setFont(juce::Font(13.0f));
    statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFFB85A3A));
    addAndMakeVisible(statusLabel_);
}

void WelcomeOverlay::showError(const juce::String& message)
{
    loading_ = false;
    statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFFB85A3A));
    statusLabel_.setText(message, juce::dontSendNotification);
    submitBtn_.setEnabled(true);
    keyInput_.setEnabled(true);
}

void WelcomeOverlay::showSuccess(const juce::String& message)
{
    loading_ = false;
    statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF5A9E8F));
    statusLabel_.setText(message, juce::dontSendNotification);
    submitBtn_.setEnabled(false);
    keyInput_.setEnabled(false);
}

void WelcomeOverlay::setLoading(bool loading)
{
    loading_ = loading;
    submitBtn_.setEnabled(!loading);
    keyInput_.setEnabled(!loading);
    if (loading)
        statusLabel_.setText(OBF("Verifying..."), juce::dontSendNotification);
}

void WelcomeOverlay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xF0101014));

    auto panelW = juce::jmin(440, getWidth() - 60);
    auto panelH = 280;
    auto panel = juce::Rectangle<int>(0, 0, panelW, panelH)
                     .withCentre(getLocalBounds().getCentre());

    g.setColour(juce::Colour(0xFF1A1611));
    g.fillRoundedRectangle(panel.toFloat(), 8.0f);
    g.setColour(juce::Colour(0x40C9A84C));
    g.drawRoundedRectangle(panel.toFloat(), 8.0f, 1.0f);

    g.setColour(juce::Colour(0xFFE8D49A));
    g.setFont(juce::Font(20.0f, juce::Font::bold));
    g.drawText(OBF("XYZPan Activation"), panel.removeFromTop(50), juce::Justification::centred);

    g.setColour(juce::Colour(0x99C8B88A));
    g.setFont(juce::Font(13.0f));
    g.drawText(OBF("Enter the license key from your Gumroad purchase"),
               panel.removeFromTop(22), juce::Justification::centred);
}

void WelcomeOverlay::resized()
{
    auto panelW = juce::jmin(440, getWidth() - 60);
    auto panelH = 280;
    auto panel = juce::Rectangle<int>(0, 0, panelW, panelH)
                     .withCentre(getLocalBounds().getCentre());

    auto inner = panel.reduced(30, 0);
    inner.removeFromTop(82);

    keyInput_.setBounds(inner.removeFromTop(30));
    inner.removeFromTop(10);
    statusLabel_.setBounds(inner.removeFromTop(20));
    inner.removeFromTop(10);
    submitBtn_.setBounds(inner.removeFromTop(30));
    inner.removeFromTop(12);
    skipBtn_.setBounds(inner.removeFromTop(24));
}

void WelcomeOverlay::mouseDown(const juce::MouseEvent&) {}

} // namespace xyzpan
