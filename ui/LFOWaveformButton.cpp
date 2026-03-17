#include "LFOWaveformButton.h"
#include "AlchemyLookAndFeel.h"
#include <cmath>

LFOWaveformButton::LFOWaveformButton()
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void LFOWaveformButton::setParam(juce::AudioProcessorValueTreeState& apvts,
                                  const juce::String& waveformParamID)
{
    param_ = apvts.getParameter(waveformParamID);
    if (param_ != nullptr) {
        // Sync display from current value
        waveform_ = static_cast<int>(std::round(
            param_->getValue() * 4.0f));  // normalized [0,1] -> [0,4]
        repaint();
    }
}

void LFOWaveformButton::parameterChanged()
{
    if (param_ != nullptr) {
        waveform_ = static_cast<int>(std::round(
            param_->getValue() * 4.0f));
        repaint();
    }
}

float LFOWaveformButton::computeWaveformY(float t, int waveform)
{
    switch (waveform) {
        case 0: // Sine
            return std::sin(t * 2.0f * juce::MathConstants<float>::pi);
        case 1: // Triangle — matches engine formula: 1.0 - 4.0 * |acc - 0.5|
            return 1.0f - 4.0f * std::abs(t - 0.5f);
        case 2: // Sawtooth
            return 2.0f * t - 1.0f;
        case 3: // Square
            return t < 0.5f ? 1.0f : -1.0f;
        case 4: // Ramp Down
            return 1.0f - 2.0f * t;
        default:
            return 0.0f;
    }
}

void LFOWaveformButton::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    // Background: dark alchemy brown
    g.setColour(juce::Colour(xyzpan::AlchemyLookAndFeel::kBackground));
    g.fillRoundedRectangle(bounds, 2.0f);

    // Border: bronze, brighter on hover
    const auto borderColour = hovered_
        ? juce::Colour(xyzpan::AlchemyLookAndFeel::kWarmGold)
        : juce::Colour(xyzpan::AlchemyLookAndFeel::kBronze);
    g.setColour(borderColour);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 2.0f, 1.0f);

    // Draw 64-point waveform polyline
    const int numPoints = 64;
    const float padding = 3.0f;
    const float drawW = w - 2.0f * padding;
    const float drawH = h - 2.0f * padding;
    const float midY = bounds.getCentreY();

    juce::Path path;
    bool first = true;
    for (int i = 0; i < numPoints; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(numPoints - 1);
        const float y = computeWaveformY(t, waveform_);
        // y in [-1,1] → screen y (flip: +1 = top)
        const float px = padding + t * drawW;
        const float py = midY - (y * drawH * 0.40f);

        if (first) {
            path.startNewSubPath(px, py);
            first = false;
        } else {
            path.lineTo(px, py);
        }
    }

    const auto lineColour = hovered_
        ? juce::Colour(xyzpan::AlchemyLookAndFeel::kBrightGold)
        : juce::Colour(xyzpan::AlchemyLookAndFeel::kWarmGold);
    g.setColour(lineColour);
    g.strokePath(path, juce::PathStrokeType(1.5f));
}

void LFOWaveformButton::mouseUp(const juce::MouseEvent&)
{
    if (param_ == nullptr) return;

    // Advance waveform (0→1→2→3→4→0)
    waveform_ = (waveform_ + 1) % 5;

    // Map back to normalized [0,1] for a param with range [0,4]
    // AudioProcessorParameter::setValue() takes normalized value
    const float normalized = static_cast<float>(waveform_) / 4.0f;
    param_->setValueNotifyingHost(normalized);

    repaint();
}

void LFOWaveformButton::mouseEnter(const juce::MouseEvent&)
{
    hovered_ = true;
    repaint();
}

void LFOWaveformButton::mouseExit(const juce::MouseEvent&)
{
    hovered_ = false;
    repaint();
}
