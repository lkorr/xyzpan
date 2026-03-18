#include "LFOShapeSelector.h"
#include "AlchemyLookAndFeel.h"
#include <cmath>

// ============================================================================
// LFOShapeSelector
// ============================================================================

LFOShapeSelector::LFOShapeSelector() {
    for (auto& btn : buttons_)
        addAndMakeVisible(btn);
}

LFOShapeSelector::~LFOShapeSelector() {
    if (param_ != nullptr)
        param_->removeListener(this);
}

void LFOShapeSelector::setParam(juce::AudioProcessorValueTreeState& apvts,
                                 const juce::String& waveformParamID) {
    if (param_ != nullptr)
        param_->removeListener(this);

    param_ = apvts.getParameter(waveformParamID);
    if (param_ != nullptr) {
        // Range is [0, 5], step 1. getValue() returns normalized [0, 1].
        selected_ = static_cast<int>(std::round(
            param_->getValue() * static_cast<float>(xyzpan::dsp::kLFOWaveformCount - 1)));
        param_->addListener(this);
        repaint();
    }
}

void LFOShapeSelector::resized() {
    auto b = getLocalBounds();
    const int count = xyzpan::dsp::kLFOWaveformCount;
    const int gap = 4;
    const int totalGaps = (count - 1) * gap;
    const int btnW = (b.getWidth() - totalGaps) / count;
    const int btnH = b.getHeight();

    for (int i = 0; i < count; ++i) {
        buttons_[i].setBounds(b.getX() + i * (btnW + gap), b.getY(), btnW, btnH);
    }
}

void LFOShapeSelector::selectShape(int index) {
    if (param_ == nullptr) return;
    selected_ = index;
    const float normalized = static_cast<float>(index)
                           / static_cast<float>(xyzpan::dsp::kLFOWaveformCount - 1);
    param_->setValueNotifyingHost(normalized);
    repaint();
}

void LFOShapeSelector::parameterValueChanged(int /*parameterIndex*/, float newValue) {
    int newSel = static_cast<int>(std::round(
        newValue * static_cast<float>(xyzpan::dsp::kLFOWaveformCount - 1)));
    if (newSel != selected_) {
        selected_ = newSel;
        auto safeThis = juce::Component::SafePointer<LFOShapeSelector>(this);
        juce::MessageManager::callAsync([safeThis]() {
            if (auto* self = safeThis.getComponent())
                self->repaint();
        });
    }
}

float LFOShapeSelector::computeWaveformY(float t, int waveform) {
    switch (waveform) {
        case 0: // Sine
            return std::sin(t * 2.0f * juce::MathConstants<float>::pi);
        case 1: // Triangle
            return 1.0f - 4.0f * std::abs(t - 0.5f);
        case 2: // Saw
            return 2.0f * t - 1.0f;
        case 3: // RampDown
            return 1.0f - 2.0f * t;
        case 4: // Square
            return t < 0.5f ? 1.0f : -1.0f;
        case 5: // Sample & Hold — hardcoded stair-step preview
        {
            // 5 horizontal segments at representative Y values
            if (t < 0.2f)       return  0.6f;
            else if (t < 0.4f)  return -0.4f;
            else if (t < 0.6f)  return  0.8f;
            else if (t < 0.8f)  return -0.7f;
            else                return  0.3f;
        }
        default:
            return 0.0f;
    }
}

// ============================================================================
// ShapeButton
// ============================================================================

LFOShapeSelector::ShapeButton::ShapeButton(LFOShapeSelector& owner, int index)
    : owner_(owner), index_(index) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void LFOShapeSelector::ShapeButton::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();
    const bool selected = (index_ == owner_.selected_);

    // Background
    g.setColour(juce::Colour(xyzpan::AlchemyLookAndFeel::kBackground));
    g.fillRoundedRectangle(bounds, 2.0f);

    // Border: warm gold if selected, bronze if not
    const auto borderColour = selected
        ? juce::Colour(xyzpan::AlchemyLookAndFeel::kWarmGold)
        : (hovered_ ? juce::Colour(xyzpan::AlchemyLookAndFeel::kWarmGold).withAlpha(0.6f)
                     : juce::Colour(xyzpan::AlchemyLookAndFeel::kBronze));
    g.setColour(borderColour);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 2.0f, 1.0f);

    // Draw waveform polyline
    const int numPoints = 32;
    const float padding = 2.0f;
    const float drawW = w - 2.0f * padding;
    const float drawH = h - 2.0f * padding;
    const float midY = bounds.getCentreY();

    juce::Path path;
    bool first = true;
    for (int i = 0; i < numPoints; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(numPoints - 1);
        const float y = computeWaveformY(t, index_);
        const float px = padding + t * drawW;
        const float py = midY - (y * drawH * 0.38f);

        if (first) {
            path.startNewSubPath(px, py);
            first = false;
        } else {
            path.lineTo(px, py);
        }
    }

    const auto lineColour = selected
        ? juce::Colour(xyzpan::AlchemyLookAndFeel::kBrightGold)
        : (hovered_ ? juce::Colour(xyzpan::AlchemyLookAndFeel::kWarmGold)
                     : juce::Colour(xyzpan::AlchemyLookAndFeel::kBronze).brighter(0.3f));
    g.setColour(lineColour);
    g.strokePath(path, juce::PathStrokeType(1.2f));
}

void LFOShapeSelector::ShapeButton::mouseUp(const juce::MouseEvent&) {
    owner_.selectShape(index_);
}

void LFOShapeSelector::ShapeButton::mouseEnter(const juce::MouseEvent&) {
    hovered_ = true;
    repaint();
}

void LFOShapeSelector::ShapeButton::mouseExit(const juce::MouseEvent&) {
    hovered_ = false;
    repaint();
}
