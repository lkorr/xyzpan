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
    // 2-row layout — fits all controls in a 100px x 120px cell (standard strip size).
    //
    // Row 1 (top half, ~60px):  [WaveBtn ~30px] [Rate knob fills remaining] [SYNC 32px]
    // Row 2 (bottom half, ~60px): [Depth knob 50%] [Phase knob 50%]
    //
    // Each knob gets 40-50px diameter — large enough to grab with a mouse.

    auto b = getLocalBounds();
    const int totalW = b.getWidth();
    const int totalH = b.getHeight();

    const int row1H = totalH / 2;
    const int row2H = totalH - row1H;
    const int row1Y = b.getY();
    const int row2Y = b.getY() + row1H;

    // Row 1 dimensions
    const int waveW = juce::jmin(30, totalW / 3);
    const int syncW = 32;
    const int rateAreaW = totalW - waveW - syncW - 2;  // 2px gap between wave and rate

    // Row 1: Waveform button (left)
    waveBtn_.setBounds(b.getX(), row1Y + (row1H - waveW) / 2, waveW, waveW);

    // Row 1: Rate knob (fills remaining space between wave and sync)
    const int labelH = 14;
    const int rateKnobH = row1H - labelH;
    rateKnob_.setBounds(b.getX() + waveW + 2, row1Y, rateAreaW, rateKnobH);
    rateLabel_.setBounds(b.getX() + waveW + 2, row1Y + rateKnobH, rateAreaW, labelH);

    // Row 1: SYNC button (right)
    syncBtn_.setBounds(b.getX() + totalW - syncW, row1Y + (row1H - 20) / 2, syncW, 20);

    // Row 2: Depth and Phase knobs side by side
    const int halfW = totalW / 2;
    const int knobH2 = row2H - labelH;

    depthKnob_.setBounds(b.getX(),          row2Y, halfW, knobH2);
    depthLabel_.setBounds(b.getX(),          row2Y + knobH2, halfW, labelH);

    phaseKnob_.setBounds(b.getX() + halfW,  row2Y, halfW, knobH2);
    phaseLabel_.setBounds(b.getX() + halfW,  row2Y + knobH2, halfW, labelH);
}
