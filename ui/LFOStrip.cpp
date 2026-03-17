#include "LFOStrip.h"
#include "xyzpan/Constants.h"

// ParamID string constants duplicated to avoid including plugin headers.
// Must stay in sync with plugin/ParamIDs.h.
namespace {
    constexpr const char* kLFOTempoSync = "lfo_tempo_sync";
}

LFOStrip::LFOStrip(char axis, juce::AudioProcessorValueTreeState& apvts)
{
    const juce::String axisLower = juce::String::charToString(
        static_cast<juce::juce_wchar>(std::tolower(axis)));

    init("lfo_" + axisLower + "_rate",
         "lfo_" + axisLower + "_depth",
         "lfo_" + axisLower + "_phase",
         "lfo_" + axisLower + "_waveform",
         "lfo_" + axisLower + "_beat_div",
         juce::String(kLFOTempoSync),
         apvts);
}

LFOStrip::LFOStrip(const juce::String& prefix, const juce::String& syncParamID,
                   juce::AudioProcessorValueTreeState& apvts)
{
    init(prefix + "_rate",
         prefix + "_depth",
         prefix + "_phase",
         prefix + "_waveform",
         prefix + "_beat_div",
         syncParamID,
         apvts);
}

LFOStrip::~LFOStrip()
{
    if (apvts_ != nullptr)
        apvts_->removeParameterListener(syncParamID_, this);
}

void LFOStrip::init(const juce::String& rateID, const juce::String& depthID,
                    const juce::String& phaseID, const juce::String& waveformID,
                    const juce::String& beatDivID, const juce::String& syncID,
                    juce::AudioProcessorValueTreeState& apvts)
{
    apvts_ = &apvts;
    syncParamID_ = syncID;

    // Configure knobs as small rotary sliders
    for (auto* knob : { &rateKnob_, &depthKnob_, &phaseKnob_ }) {
        knob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 65, 13);
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

    // BeatDiv combo box — discrete musical values
    for (int i = 0; i < xyzpan::kBeatDivCount; ++i)
        beatDivCombo_.addItem(xyzpan::kBeatDivLabels[i], i + 1);  // JUCE ComboBox IDs are 1-based
    addChildComponent(beatDivCombo_);  // hidden by default (shown when sync on)

    // SYNC button — small font to fit in compact button
    syncBtn_.setClickingTogglesState(true);
    syncBtn_.setButtonText("Sync");
    addAndMakeVisible(syncBtn_);

    // Waveform display button
    addAndMakeVisible(waveBtn_);

    // APVTS attachments — must be created after addAndMakeVisible
    rateAtt_    = std::make_unique<SA>(apvts, rateID,    rateKnob_);
    depthAtt_   = std::make_unique<SA>(apvts, depthID,   depthKnob_);
    phaseAtt_   = std::make_unique<SA>(apvts, phaseID,   phaseKnob_);
    beatDivAtt_ = std::make_unique<CA>(apvts, beatDivID, beatDivCombo_);
    syncAtt_    = std::make_unique<BA>(apvts, syncID,    syncBtn_);

    // Bind waveform button to APVTS parameter
    waveBtn_.setParam(apvts, waveformID);

    // Read initial sync state and register listener
    if (auto* syncParam = apvts.getRawParameterValue(syncID))
        syncOn_ = syncParam->load() >= 0.5f;
    apvts.addParameterListener(syncID, this);
    updateSyncVisibility();
}

void LFOStrip::parameterChanged(const juce::String& /*parameterID*/, float newValue)
{
    syncOn_ = newValue >= 0.5f;
    // Schedule visibility update on message thread (parameterChanged may be called from audio thread)
    auto safeThis = juce::Component::SafePointer<LFOStrip>(this);
    juce::MessageManager::callAsync([safeThis]() {
        if (auto* self = safeThis.getComponent()) {
            self->updateSyncVisibility();
        }
    });
}

void LFOStrip::updateSyncVisibility()
{
    rateKnob_.setVisible(!syncOn_);
    rateLabel_.setVisible(!syncOn_);
    beatDivCombo_.setVisible(syncOn_);
    resized();
}

void LFOStrip::resized()
{
    // 2-row layout with fixed pixel sizes.
    //
    // Row 1: [WaveBtn] [Depth knob (fills center)]
    // Row 2: [SYNC btn above Rate] [Rate knob OR BeatDiv, left half] [Phase knob, right half]

    auto b = getLocalBounds();
    const int totalW = b.getWidth();
    const int totalH = b.getHeight();

    const int syncGap = 2;             // space between SYNC and rate knob
    const int rowH    = kKnobSize + kLabelH;
    const int row2H   = kSyncH + syncGap + rowH;  // SYNC + gap + knob + label
    const int row1H   = rowH;
    const int row1Y   = b.getY() + (totalH - row1H - row2H) / 2;
    const int row2Y   = row1Y + row1H;

    // Row 1: Waveform button (left, with small inset from divider)
    waveBtn_.setBounds(b.getX() + kWavePadL, row1Y + (row1H - kWaveW) / 2, kWaveW, kWaveW);

    // Row 1: Depth knob fills remaining center area
    {
        const int centerAreaX = b.getX() + kWavePadL + kWaveW + 2;
        const int centerAreaW = totalW - kWavePadL - kWaveW - 4;
        const int knobW = juce::jmin(kKnobSize, centerAreaW);
        const int knobX = centerAreaX + (centerAreaW - knobW) / 2;
        depthKnob_.setBounds(knobX, row1Y, knobW, kKnobSize);
        depthLabel_.setBounds(centerAreaX, row1Y + kKnobSize, centerAreaW, kLabelH);
    }

    // Row 2: Rate (or BeatDiv) and Phase — each centered in their half
    // SYNC button sits directly above Rate knob, centered in left half
    const int halfW = totalW / 2;

    {
        // SYNC button — centered in left half, at top of row 2
        const int syncX = b.getX() + (halfW - kSyncW) / 2;
        syncBtn_.setBounds(syncX, row2Y, kSyncW, kSyncH);
    }

    const int rateAreaY = row2Y + kSyncH + syncGap;

    if (syncOn_) {
        // BeatDiv combo — centered in left half, below SYNC
        const int comboW = juce::jmin(64, halfW - 4);
        const int comboH = 22;
        const int comboX = b.getX() + (halfW - comboW) / 2;
        const int comboY = rateAreaY + (rowH - comboH) / 2;
        beatDivCombo_.setBounds(comboX, comboY, comboW, comboH);
    } else {
        // Rate knob — centered in left half, below SYNC
        const int knobW = juce::jmin(kKnobSize, halfW - 4);
        const int rateX = b.getX() + (halfW - knobW) / 2;
        rateKnob_.setBounds(rateX, rateAreaY, knobW, kKnobSize);
        rateLabel_.setBounds(b.getX(), rateAreaY + kKnobSize, halfW, kLabelH);
    }

    {
        // Phase knob — centered in right half, aligned with rate knob
        const int knobW = juce::jmin(kKnobSize, halfW - 4);
        const int phaseX = b.getX() + halfW + (halfW - knobW) / 2;
        phaseKnob_.setBounds(phaseX, rateAreaY, knobW, kKnobSize);
        phaseLabel_.setBounds(b.getX() + halfW, rateAreaY + kKnobSize, halfW, kLabelH);
    }
}
