#include "LFOWaveformDisplay.h"
#include "LFOShapeSelector.h"
#include "AlchemyLookAndFeel.h"
#include <cmath>

LFOWaveformDisplay::LFOWaveformDisplay() {}

LFOWaveformDisplay::~LFOWaveformDisplay()
{
    if (waveformParam_) waveformParam_->removeListener(this);
    if (phaseParam_)    phaseParam_->removeListener(this);
    if (smoothParam_)   smoothParam_->removeListener(this);
    if (depthParam_)    depthParam_->removeListener(this);
}

void LFOWaveformDisplay::setParams(juce::AudioProcessorValueTreeState& apvts,
                                    const juce::String& waveformID,
                                    const juce::String& phaseID,
                                    const juce::String& smoothID,
                                    const juce::String& depthID)
{
    auto bind = [&](juce::AudioProcessorParameter*& dest, const juce::String& id) {
        if (dest) dest->removeListener(this);
        dest = apvts.getParameter(id);
        if (dest) dest->addListener(this);
    };

    bind(waveformParam_, waveformID);
    bind(phaseParam_,    phaseID);
    bind(smoothParam_,   smoothID);
    bind(depthParam_,    depthID);

    readParams();
}

void LFOWaveformDisplay::setPhaseSource(std::atomic<float>* src)
{
    phaseSource_ = src;
}

void LFOWaveformDisplay::setSHSource(std::atomic<float>* src)
{
    shSource_ = src;
}

void LFOWaveformDisplay::readParams()
{
    int prevWaveform = waveform_;

    if (waveformParam_) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(waveformParam_);
        waveform_ = ranged ? static_cast<int>(std::round(ranged->convertFrom0to1(waveformParam_->getValue())))
                           : static_cast<int>(std::round(waveformParam_->getValue() * 5.0f));
    }

    // Reset S&H history when waveform selection changes
    if (waveform_ != prevWaveform) {
        shCount_          = 0;
        shWritePos_       = 0;
        shLastValue_      = 0.0f;
        shDisplayPhase_   = 0.0f;
        shPrevAnimPhase_  = -1.0f;
    }
    if (phaseParam_) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(phaseParam_);
        phase_ = ranged ? ranged->convertFrom0to1(phaseParam_->getValue())
                        : phaseParam_->getValue();
    }
    if (smoothParam_) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(smoothParam_);
        smooth_ = ranged ? ranged->convertFrom0to1(smoothParam_->getValue())
                         : smoothParam_->getValue();
    }
    if (depthParam_) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(depthParam_);
        depth_ = ranged ? ranged->convertFrom0to1(depthParam_->getValue())
                        : depthParam_->getValue();
    }
}

void LFOWaveformDisplay::parameterValueChanged(int /*parameterIndex*/, float /*newValue*/)
{
    readParams();
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
    // Sample S&H history for staircase display
    if (waveform_ == 5 && shSource_) {
        float val = shSource_->load(std::memory_order_relaxed);
        if (std::fabs(val - shLastValue_) > 0.001f) {
            shHistory_[static_cast<size_t>(shWritePos_)] = val;
            shWritePos_ = (shWritePos_ + 1) % kSHHistorySize;
            if (shCount_ < kSHHistorySize) ++shCount_;
            shLastValue_ = val;
        }
    }

    // Maintain smooth monotonic S&H display phase
    if (waveform_ == 5 && phaseSource_) {
        float raw = phaseSource_->load(std::memory_order_relaxed);
        if (shPrevAnimPhase_ < 0.0f) {
            shPrevAnimPhase_ = raw;
        } else {
            float delta = raw - shPrevAnimPhase_;
            if (delta < 0.0f) delta += 1.0f;  // handle wrap
            if (delta > 0.5f) delta = 0.5f;   // clamp to prevent aliasing
            shDisplayPhase_ += delta;
            shDisplayPhase_ -= std::floor(shDisplayPhase_);
            shPrevAnimPhase_ = raw;
        }
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

    // Draw area with padding
    const float pad = 4.0f;
    const float drawW = w - 2.0f * pad;
    const float drawH = h - 2.0f * pad;
    const float midY = bounds.getCentreY();

    if (drawW < 4.0f || drawH < 4.0f) return;

    // Read real LFO phase from audio thread (or 0 if not connected)
    const float animPhase = phaseSource_ ? phaseSource_->load(std::memory_order_relaxed) : 0.0f;

    // Draw ~2 cycles of waveform, scrolling based on animPhase
    const int numPoints = juce::jmax(32, static_cast<int>(drawW));
    const float cycles = 2.0f;

    // One-pole smoothing coefficient — convert [0,1] to ms like DSP (Engine.cpp:549)
    const float smoothMs = smooth_ * 300.0f;  // 0-1 → 0-300 ms
    const float smoothAlpha = (smoothMs > 0.1f)
        ? std::exp(-1.0f / (smoothMs * 0.001f * static_cast<float>(numPoints) * 0.5f))
        : 0.0f;

    juce::Path path;
    float prevY = 0.0f;
    bool first = true;

    for (int i = 0; i < numPoints; ++i) {
        const float frac = static_cast<float>(i) / static_cast<float>(numPoints - 1);

        // t = phase position in waveform, scrolling with animPhase
        float t = frac * cycles + animPhase;
        t -= std::floor(t);  // wrap to [0, 1)

        float y;
        if (waveform_ == 5) {
            if (shCount_ > 0) {
                // Use smooth local phase (immune to aliasing) at 2x scroll speed
                float shPhase = shDisplayPhase_ * 2.0f;
                shPhase -= std::floor(shPhase);

                float distFromRight = 1.0f - frac;
                float newestWidth = shPhase / cycles;
                int stepsBack;
                if (distFromRight <= newestWidth) {
                    stepsBack = 0;
                } else {
                    stepsBack = 1 + static_cast<int>((distFromRight - newestWidth) * cycles);
                }
                int entryIdx = (shCount_ - 1) - stepsBack;
                if (entryIdx >= 0 && entryIdx < shCount_) {
                    int bufIdx = (shWritePos_ - shCount_ + entryIdx + kSHHistorySize) % kSHHistorySize;
                    y = shHistory_[static_cast<size_t>(bufIdx)] * depth_;
                } else {
                    y = 0.0f;
                }
            } else {
                y = 0.0f;
            }
        } else {
            y = LFOShapeSelector::computeWaveformY(t, waveform_) * depth_;
        }

        // Apply visual smoothing
        if (smoothAlpha > 0.0f) {
            if (first)
                prevY = y;
            else
                y = smoothAlpha * prevY + (1.0f - smoothAlpha) * y;
            prevY = y;
        }

        const float px = bounds.getX() + pad + frac * drawW;
        const float py = midY - y * drawH * 0.42f;

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

    // Bright gold dot at right edge marking "current" output
    {
        float t = animPhase;
        t -= std::floor(t);
        float y;
        if (waveform_ == 5 && shSource_)
            y = shSource_->load(std::memory_order_relaxed) * depth_;
        else
            y = LFOShapeSelector::computeWaveformY(t, waveform_) * depth_;

        // Apply same smoothing as the line end
        if (smoothAlpha > 0.0f)
            y = smoothAlpha * prevY + (1.0f - smoothAlpha) * y;

        const float dotX = bounds.getX() + pad + drawW;
        const float dotY = midY - y * drawH * 0.42f;
        const float dotR = 3.0f;

        g.setColour(juce::Colour(xyzpan::AlchemyLookAndFeel::kBrightGold));
        g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);
    }
}
