#include "LFOWaveformDisplay.h"
#include "AlchemyLookAndFeel.h"
#include <cmath>

LFOWaveformDisplay::LFOWaveformDisplay() {}

void LFOWaveformDisplay::setOutputSource(std::atomic<float>* src)
{
    outputSource_ = src;
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
        history_[static_cast<size_t>(historyWritePos_)] = val;
        historyWritePos_ = (historyWritePos_ + 1) % kHistorySize;
        if (historyCount_ < kHistorySize) ++historyCount_;
    }
    repaint();
}

void LFOWaveformDisplay::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();
    if (w < 2.0f || h < 2.0f) return;

    // Background fill
    g.setColour(juce::Colour(xyzpan::AlchemyLookAndFeel::kBackground));
    g.fillRoundedRectangle(bounds, 3.0f);

    // Border
    g.setColour(juce::Colour(xyzpan::AlchemyLookAndFeel::kBronze));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.0f);

    if (historyCount_ == 0) return;

    // Draw area with padding
    const float pad = 4.0f;
    const float drawW = w - 2.0f * pad;
    const float drawH = h - 2.0f * pad;
    const float midY = bounds.getCentreY();

    if (drawW < 4.0f || drawH < 4.0f) return;

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
        float py = midY - val * drawH * 0.42f;
        lastPy = py;

        if (first) {
            path.startNewSubPath(px, py);
            first = false;
        } else {
            path.lineTo(px, py);
        }
    }

    // Waveform line
    g.setColour(juce::Colour(xyzpan::AlchemyLookAndFeel::kWarmGold));
    g.strokePath(path, juce::PathStrokeType(1.5f));

    // Bright gold dot at right edge marking current value
    {
        float dotX = bounds.getX() + pad + drawW;
        float dotY = lastPy;
        float dotR = 3.0f;

        g.setColour(juce::Colour(xyzpan::AlchemyLookAndFeel::kBrightGold));
        g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);
    }
}
