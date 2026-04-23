#include "OutputMeter.h"
#include "AlchemyLookAndFeel.h"

namespace xyzpan {

OutputMeter::OutputMeter() {
    startTimerHz(30);
}

void OutputMeter::setRMSSources(std::atomic<float>* left, std::atomic<float>* right) {
    rmsL_ = left;
    rmsR_ = right;
}

void OutputMeter::timerCallback() {
    const float rawL = rmsL_ ? rmsL_->load(std::memory_order_relaxed) : 0.f;
    const float rawR = rmsR_ ? rmsR_->load(std::memory_order_relaxed) : 0.f;

    // Smooth RMS (asymmetric: fast attack, slower release)
    auto smooth = [](float current, float target) {
        const float coeff = target > current ? 0.4f : kSmoothCoeff;
        return current + coeff * (target - current);
    };
    smoothL_ = smooth(smoothL_, rawL);
    smoothR_ = smooth(smoothR_, rawR);

    // Peak hold with decay
    auto updatePeak = [](float rms, float& peak, float& decay) {
        if (rms >= peak) {
            peak = rms;
            decay = kPeakHoldSec * 30.0f; // hold frames
        } else if (decay > 0.f) {
            decay -= 1.0f;
        } else {
            peak -= kPeakDecayRate;
            if (peak < 0.f) peak = 0.f;
        }
    };
    updatePeak(smoothL_, peakL_, peakDecayL_);
    updatePeak(smoothR_, peakR_, peakDecayR_);

    // Latch clip when signal exceeds 0 dB
    if (rawL > 1.0f) clipL_ = true;
    if (rawR > 1.0f) clipR_ = true;

    // Skip repaint when meter is fully silent, settled, and no clip latched
    if (smoothL_ < 1e-6f && smoothR_ < 1e-6f && peakL_ < 1e-6f && peakR_ < 1e-6f
        && !clipL_ && !clipR_)
        return;
    repaint();
}

void OutputMeter::paint(juce::Graphics& g) {
    using ALF = AlchemyLookAndFeel;

    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    // Background
    g.setColour(juce::Colour(ALF::kObsidian));
    g.fillRect(bounds);

    // Thin bronze border on left edge
    g.setColour(juce::Colour(ALF::kBronze));
    g.fillRect(bounds.getX(), bounds.getY(), 1.0f, h);

    const float pad = 2.0f;
    const float barGap = 2.0f;
    const float barW = (w - pad * 2 - barGap) / 2.0f;
    const float barH = h - pad * 2;

    // dB scale tick marks
    g.setColour(juce::Colour(ALF::kVerdigris).withAlpha(0.4f));
    for (float db : {-48.f, -36.f, -24.f, -12.f, -6.f, 0.f}) {
        float norm = dbToNorm(db);
        float tickY = pad + barH * (1.0f - norm);
        g.drawHorizontalLine(static_cast<int>(tickY), pad, pad + barW * 2 + barGap);
    }

    // Draw a single meter bar
    auto drawBar = [&](float x, float smoothedRms, float peak, juce::Colour colour) {
        float dbSmooth = linearToDb(smoothedRms);
        float normSmooth = dbToNorm(dbSmooth);
        float barFillH = barH * normSmooth;

        // Gradient fill: gold at bottom, cinnabar at top (hot)
        float barTop = pad + barH - barFillH;
        if (barFillH > 0.5f) {
            juce::ColourGradient grad(
                colour.withAlpha(0.9f), x, pad + barH,
                juce::Colour(ALF::kCinnabar), x, pad, false);
            g.setGradientFill(grad);
            g.fillRect(x, barTop, barW, barFillH);
        }

        // Peak indicator line
        float dbPeak = linearToDb(peak);
        float normPeak = dbToNorm(dbPeak);
        if (normPeak > 0.01f) {
            float peakY = pad + barH * (1.0f - normPeak);
            g.setColour(juce::Colour(ALF::kGoldLeafPale));
            g.fillRect(x, peakY, barW, 1.5f);
        }
    };

    float lx = pad;
    float rx = pad + barW + barGap;
    drawBar(lx, smoothL_, peakL_, juce::Colour(ALF::kGoldLeaf));
    drawBar(rx, smoothR_, peakR_, juce::Colour(ALF::kGoldLeaf));

    // Clip indicators — red rect at top of each bar
    const float clipH = 4.0f;
    if (clipL_) {
        g.setColour(juce::Colour(ALF::kCinnabarLight));
        g.fillRect(lx, pad, barW, clipH);
    }
    if (clipR_) {
        g.setColour(juce::Colour(ALF::kCinnabarLight));
        g.fillRect(rx, pad, barW, clipH);
    }

    // "L" / "R" labels at bottom
    g.setColour(juce::Colour(ALF::kAgedPapyrusDark).withAlpha(0.7f));
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    g.drawText("L", static_cast<int>(lx), static_cast<int>(pad + barH - 12),
               static_cast<int>(barW), 12, juce::Justification::centred);
    g.drawText("R", static_cast<int>(rx), static_cast<int>(pad + barH - 12),
               static_cast<int>(barW), 12, juce::Justification::centred);
}

void OutputMeter::mouseDown(const juce::MouseEvent&) {
    clipL_ = false;
    clipR_ = false;
    repaint();
}

} // namespace xyzpan
