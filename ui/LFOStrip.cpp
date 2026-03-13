#include "LFOStrip.h"

// ParamID string constants duplicated to avoid including plugin headers.
// Must stay in sync with plugin/ParamIDs.h.
namespace {
    constexpr const char* kLFOTempoSync = "lfo_tempo_sync";
}

LFOStrip::LFOStrip(char axis, juce::AudioProcessorValueTreeState& apvts)
{
    // Resolve parameter IDs from axis char
    // ParamIDs for LFO follow pattern: "lfo_x_rate", "lfo_y_rate", etc.
    const juce::String axisLower = juce::String::charToString(
        static_cast<juce::juce_wchar>(std::tolower(axis)));

    const juce::String rateID     = "lfo_" + axisLower + "_rate";
    const juce::String depthID    = "lfo_" + axisLower + "_depth";
    const juce::String phaseID    = "lfo_" + axisLower + "_phase";
    const juce::String waveformID = "lfo_" + axisLower + "_waveform";
    // Tempo sync is shared — same param for all axes
    const juce::String syncID     = juce::String(kLFOTempoSync);

    // Configure knobs as small rotary sliders
    for (auto* knob : { &rateKnob_, &depthKnob_, &phaseKnob_ }) {
        knob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 12);
        addAndMakeVisible(knob);
    }

    // Labels
    rateLabel_.setText("Rate", juce::dontSendNotification);
    depthLabel_.setText("Depth", juce::dontSendNotification);
    phaseLabel_.setText("Phase", juce::dontSendNotification);
    for (auto* lbl : { &rateLabel_, &depthLabel_, &phaseLabel_ }) {
        lbl->setJustificationType(juce::Justification::centred);
        lbl->setFont(juce::Font(10.0f));
        addAndMakeVisible(lbl);
    }

    // SYNC button
    syncBtn_.setClickingTogglesState(true);
    addAndMakeVisible(syncBtn_);

    // Waveform display button
    addAndMakeVisible(waveBtn_);

    // APVTS attachments — must be created after addAndMakeVisible
    rateAtt_  = std::make_unique<SA>(apvts, rateID,  rateKnob_);
    depthAtt_ = std::make_unique<SA>(apvts, depthID, depthKnob_);
    phaseAtt_ = std::make_unique<SA>(apvts, phaseID, phaseKnob_);
    syncAtt_  = std::make_unique<BA>(apvts, syncID,  syncBtn_);

    // Bind waveform button to APVTS parameter
    waveBtn_.setParam(apvts, waveformID);
}

void LFOStrip::resized()
{
    // Layout within the strip:
    // [WaveBtn] [Rate knob+label] [Depth knob+label] [Phase knob+label] [SYNC]
    //
    // The strip is typically 100px wide x 120px tall (from PluginEditor layout).

    auto b = getLocalBounds();
    const int totalW = b.getWidth();
    const int totalH = b.getHeight();

    // Waveform button: left portion, square-ish
    const int waveW = juce::jmin(28, totalW / 4);
    const int waveH = juce::jmin(28, totalH - 4);
    waveBtn_.setBounds(b.getX(), b.getY() + (totalH - waveH) / 2, waveW, waveH);

    // Remaining width for knobs + SYNC
    const int remaining = totalW - waveW - 2;
    const int syncW = 32;
    const int knobAreaW = remaining - syncW;
    const int knobW = knobAreaW / 3;

    int x = b.getX() + waveW + 2;
    const int labelH = 14;
    const int knobH = totalH - labelH;

    auto placeKnob = [&](juce::Slider& knob, juce::Label& label) {
        knob.setBounds(x, b.getY(), knobW, knobH);
        label.setBounds(x, b.getY() + knobH, knobW, labelH);
        x += knobW;
    };

    placeKnob(rateKnob_,  rateLabel_);
    placeKnob(depthKnob_, depthLabel_);
    placeKnob(phaseKnob_, phaseLabel_);

    // SYNC button: right edge, vertically centred
    syncBtn_.setBounds(x, b.getY() + (totalH - 20) / 2, syncW, 20);
}
