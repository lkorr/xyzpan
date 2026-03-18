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
         "lfo_" + axisLower + "_smooth",
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
         prefix + "_smooth",
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
                    const juce::String& smoothID, const juce::String& beatDivID,
                    const juce::String& syncID,
                    juce::AudioProcessorValueTreeState& apvts)
{
    apvts_ = &apvts;
    syncParamID_ = syncID;

    // Configure knobs as small rotary sliders
    for (auto* knob : { &rateKnob_, &depthKnob_, &phaseKnob_, &smoothKnob_ }) {
        knob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 65, 13);
        addAndMakeVisible(knob);
    }

    // Labels
    rateLabel_.setText("Rate", juce::dontSendNotification);
    depthLabel_.setText("Depth", juce::dontSendNotification);
    phaseLabel_.setText("Phase", juce::dontSendNotification);
    smoothLabel_.setText("Smooth", juce::dontSendNotification);
    for (auto* lbl : { &rateLabel_, &depthLabel_, &phaseLabel_, &smoothLabel_ }) {
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

    // Shape selector row
    addAndMakeVisible(shapeSelector_);

    // APVTS attachments — must be created after addAndMakeVisible
    rateAtt_    = std::make_unique<SA>(apvts, rateID,    rateKnob_);
    depthAtt_   = std::make_unique<SA>(apvts, depthID,   depthKnob_);
    phaseAtt_   = std::make_unique<SA>(apvts, phaseID,   phaseKnob_);
    smoothAtt_  = std::make_unique<SA>(apvts, smoothID,  smoothKnob_);
    beatDivAtt_ = std::make_unique<CA>(apvts, beatDivID, beatDivCombo_);
    syncAtt_    = std::make_unique<BA>(apvts, syncID,    syncBtn_);

    // Bind shape selector to APVTS parameter
    shapeSelector_.setParam(apvts, waveformID);

    // Live waveform display
    addAndMakeVisible(waveformDisplay_);
    waveformDisplay_.setParams(apvts, waveformID, phaseID, smoothID, depthID);

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

void LFOStrip::setPhaseSource(std::atomic<float>* src)
{
    waveformDisplay_.setPhaseSource(src);
}

void LFOStrip::resized()
{
    // 4-row layout:
    //
    // Row 0: Shape selector (full width, kShapeRowH px)
    // Row 1: Live waveform display (flexible height, 30–80 px)
    // Row 2: Depth knob (centered) + Smooth knob (small, bottom-right)
    // Row 3: SYNC + Rate/BeatDiv (left) + Phase (right) — 42 px knobs

    auto b = getLocalBounds();
    const int totalW = b.getWidth();
    const int totalH = b.getHeight();

    const int syncGap  = 2;
    const int row2H    = kKnobSize + kLabelH;                                // 71 + 13 = 84
    const int row3KnobH = kRatePhaseKnobSz + kLabelH;                        // 42 + 13 = 55
    const int row3H    = kSyncH + syncGap + row3KnobH;                       // 18 + 2 + 55 = 75
    const int row0H    = kShapeTopMargin + kShapeRowH + 2;                    // 4 + 18 + 2 = 24
    const int fixedH   = row0H + row2H + row3H;                              // 24 + 84 + 75 = 183
    const int displayH = juce::jlimit(kDisplayMinH, kDisplayMaxH,
                                       totalH - fixedH);
    const int contentH = fixedH + displayH;
    const int topY     = b.getY() + juce::jmax(0, (totalH - contentH) / 2);

    const int row0Y    = topY;
    const int row1Y    = row0Y + row0H;
    const int row2Y    = row1Y + displayH;
    const int row3Y    = row2Y + row2H;

    // Row 0: Shape selector
    shapeSelector_.setBounds(b.getX() + kShapeLRMargin, row0Y + kShapeTopMargin,
                             totalW - kShapeLRMargin * 2, kShapeRowH);

    // Row 1: Waveform display
    waveformDisplay_.setBounds(b.getX() + kDisplayLRMargin, row1Y,
                               totalW - kDisplayLRMargin * 2, displayH);

    // Row 2: Depth knob (centered) + Smooth knob (small, bottom-right)
    const int halfW = totalW / 2;
    {
        const int knobW = juce::jmin(kKnobSize, totalW - 4);
        const int depthX = b.getX() + (totalW - knobW) / 2;
        depthKnob_.setBounds(depthX, row2Y, knobW, kKnobSize);
        depthLabel_.setBounds(b.getX(), row2Y + kKnobSize, totalW, kLabelH);
    }
    {
        const int smW = juce::jmin(kSmallKnobSize, halfW - 4);
        const int smoothX = b.getX() + totalW - smW - 2;
        const int smoothY = row2Y + kKnobSize - kSmallKnobSize;
        smoothKnob_.setBounds(smoothX, smoothY, smW, kSmallKnobSize);
        smoothLabel_.setBounds(smoothX - 4, smoothY + kSmallKnobSize, smW + 8, kLabelH);
        smoothLabel_.setJustificationType(juce::Justification::centred);
    }

    // Row 3: SYNC + Rate/BeatDiv (left half) + Phase (right half) — smaller knobs
    {
        const int syncX = b.getX() + (halfW - kSyncW) / 2;
        syncBtn_.setBounds(syncX, row3Y, kSyncW, kSyncH);
    }

    const int rateAreaY = row3Y + kSyncH + syncGap;

    if (syncOn_) {
        const int comboW = juce::jmin(64, halfW - 4);
        const int comboH = 22;
        const int comboX = b.getX() + (halfW - comboW) / 2;
        const int comboY = rateAreaY + (row3KnobH - comboH) / 2;
        beatDivCombo_.setBounds(comboX, comboY, comboW, comboH);
    } else {
        const int knobW = juce::jmin(kRatePhaseKnobSz, halfW - 4);
        const int rateX = b.getX() + (halfW - knobW) / 2;
        rateKnob_.setBounds(rateX, rateAreaY, knobW, kRatePhaseKnobSz);
        rateLabel_.setBounds(b.getX(), rateAreaY + kRatePhaseKnobSz, halfW, kLabelH);
    }

    {
        const int knobW = juce::jmin(kRatePhaseKnobSz, halfW - 4);
        const int phaseX = b.getX() + halfW + (halfW - knobW) / 2;
        phaseKnob_.setBounds(phaseX, rateAreaY, knobW, kRatePhaseKnobSz);
        phaseLabel_.setBounds(b.getX() + halfW, rateAreaY + kRatePhaseKnobSz, halfW, kLabelH);
    }
}
