#include "LFOWaveformDisplay.h"
#include "AlchemyLookAndFeel.h"
#include <cmath>

LFOWaveformDisplay::LFOWaveformDisplay() {}

void LFOWaveformDisplay::setOutputSource(std::atomic<float>* src)
{
    outputSource_ = src;
}

void LFOWaveformDisplay::setTotalDepthSource(std::function<float()> src)
{
    totalDepthSource_ = std::move(src);
}

void LFOWaveformDisplay::visibilityChanged()
{
    if (isVisible())
        startTimerHz(kFps);
    else
        stopTimer();
}

void LFOWaveformDisplay::timerCallback()
{
    if (outputSource_) {
        float val = outputSource_->load(std::memory_order_relaxed);
        lastSampledValue_ = val;
        history_[static_cast<size_t>(historyWritePos_)] = val;
        historyWritePos_ = (historyWritePos_ + 1) % kHistorySize;
        if (historyCount_ < kHistorySize) ++historyCount_;
    }
    if (totalDepthSource_)
        cachedTotalDepth_ = totalDepthSource_();
    repaint();
}

void LFOWaveformDisplay::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();
    if (w < 2.0f || h < 2.0f) return;

    // Resolve theme colors from LookAndFeel (falls back to Alchemy defaults)
    xyzpan::ColorTheme theme;
    if (auto* alf = dynamic_cast<xyzpan::AlchemyLookAndFeel*>(&getLookAndFeel()))
        theme = alf->currentTheme();

    // Background fill
    g.setColour(juce::Colour(theme.background));
    g.fillRoundedRectangle(bounds, 3.0f);

    // Border
    g.setColour(juce::Colour(theme.bronze));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.0f);

    if (historyCount_ == 0) return;

    // Draw area with padding
    const float pad = 4.0f;
    const float drawW = w - 2.0f * pad;
    const float drawH = h - 2.0f * pad;
    const float midY = bounds.getCentreY();

    if (drawW < 4.0f || drawH < 4.0f) return;

    // Scale waveform so it always fits; reference lines show where ±1 is
    const float totalDepth = juce::jmax(1.0f, cachedTotalDepth_);
    const float ampScale = drawH * 0.42f / totalDepth;

    // Dotted ±1 reference lines — always drawn, naturally at edges when depth=1
    // and slide inward as depth increases. Clipped by component bounds.
    {
        const float refY = ampScale; // pixel distance from midY to ±1 boundary
        const float fadeIn = 1.0f - std::exp(-8.0f * (totalDepth - 1.0f));
        const float maxAlpha = isEnabled() ? 0.45f : 0.2f;
        const float lineAlpha = maxAlpha * fadeIn;
        g.setColour(juce::Colour(theme.lfoAccent).withAlpha(lineAlpha));

        const float dashLengths[] = { 3.0f, 4.0f };
        const float leftX = bounds.getX() + pad;
        const float rightX = bounds.getX() + pad + drawW;

        // +1 reference line (dashed)
        {
            juce::Path topRef;
            topRef.startNewSubPath(leftX, midY - refY);
            topRef.lineTo(rightX, midY - refY);
            juce::Path dashedTop;
            juce::PathStrokeType(0.75f).createDashedStroke(dashedTop, topRef, dashLengths, 2);
            g.fillPath(dashedTop);
        }

        // -1 reference line (dashed)
        {
            juce::Path botRef;
            botRef.startNewSubPath(leftX, midY + refY);
            botRef.lineTo(rightX, midY + refY);
            juce::Path dashedBot;
            juce::PathStrokeType(0.75f).createDashedStroke(dashedBot, botRef, dashLengths, 2);
            g.fillPath(dashedBot);
        }
    }

    // Iterate history oldest→newest, map to pixels (rightmost = newest)
    juce::Path path;
    bool first = true;
    float lastPy = midY;

    for (int i = 0; i < historyCount_; ++i) {
        // oldest entry is at index (writePos - count + i), wrapped
        int bufIdx = (historyWritePos_ - historyCount_ + i + kHistorySize) % kHistorySize;
        float val = history_[static_cast<size_t>(bufIdx)];

        // Map i to x position: oldest at left edge, newest at right edge
        float frac = static_cast<float>(i) / static_cast<float>(juce::jmax(1, historyCount_ - 1));
        float px = bounds.getX() + pad + frac * drawW;
        float py = midY - val * ampScale;
        lastPy = py;

        if (first) {
            path.startNewSubPath(px, py);
            first = false;
        } else {
            path.lineTo(px, py);
        }
    }

    // Waveform line
    const float alpha = isEnabled() ? 1.0f : 0.4f;
    g.setColour(juce::Colour(theme.lfoAccent).withAlpha(alpha));
    g.strokePath(path, juce::PathStrokeType(1.5f));

    // Bright dot at right edge marking current value
    {
        float dotX = bounds.getX() + pad + drawW;
        float dotY = lastPy;
        float dotR = 3.0f;

        g.setColour(juce::Colour(theme.lfoAccentBright).withAlpha(alpha));
        g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);
    }
}
