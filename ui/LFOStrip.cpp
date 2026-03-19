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
        knob->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 14);
        addAndMakeVisible(knob);
    }

    // BeatDiv knob — discrete rotary that snaps through musical values
    beatDivKnob_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    beatDivKnob_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 14);
    beatDivKnob_.textFromValueFunction = [](double value) {
        int idx = juce::roundToInt(value);
        if (idx >= 0 && idx < xyzpan::kBeatDivCount)
            return juce::String(xyzpan::kBeatDivLabels[idx]);
        return juce::String(value, 0);
    };
    addChildComponent(beatDivKnob_);  // hidden by default (shown when sync on)

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
    beatDivAtt_ = std::make_unique<SA>(apvts, beatDivID, beatDivKnob_);
    syncAtt_    = std::make_unique<BA>(apvts, syncID,    syncBtn_);

    // Bind shape selector to APVTS parameter
    shapeSelector_.setParam(apvts, waveformID);

    // Live waveform display
    addAndMakeVisible(waveformDisplay_);

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
    beatDivKnob_.setVisible(syncOn_);
    rateLabel_.setVisible(true);  // always visible — label stays "Rate" for both modes
    resized();
}

void LFOStrip::setOutputSource(std::atomic<float>* src)
{
    waveformDisplay_.setOutputSource(src);
}

void LFOStrip::resized()
{
    // Compact 4-row layout:
    //
    // Row 0: Shape selector (full width, 24px)
    // Row 1: Waveform display (flexible height, 30–80px)
    // Row 2: [Rate] [Depth] [Phase] [Smooth] — 4 knobs horizontal, 54px + 13px label = 67px
    // Row 3: [Sync] button below Rate — 18px

    auto b = getLocalBounds();
    const int totalW = b.getWidth();
    const int totalH = b.getHeight();

    const int row0H    = kShapeTopMargin + kShapeRowH + 2;        // 24
    const int row2H    = kKnobSz + kLabelH;                       // 54 + 13 = 67
    const int row3H    = kSyncH;                                   // 18
    const int fixedH   = row0H + row2H + row3H;                   // 109
    const int displayH = juce::jlimit(0, kDisplayMaxH, totalH - fixedH);
    const int topY     = b.getY();

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
    waveformDisplay_.setVisible(displayH >= kDisplayMinH);

    // Row 2: 4 knobs in equal horizontal columns — Rate, Depth, Phase, Smooth
    const int colW = totalW / 4;
    const int knobW = juce::jmin(kKnobSz, colW - 2);

    {
        struct KnobSlot { juce::Slider* knob; juce::Label* label; int col; };
        KnobSlot slots[] = {
            { &rateKnob_,   &rateLabel_,   0 },
            { &depthKnob_,  &depthLabel_,  1 },
            { &phaseKnob_,  &phaseLabel_,  2 },
            { &smoothKnob_, &smoothLabel_, 3 },
        };

        for (auto& s : slots) {
            const int colX = b.getX() + s.col * colW;
            const int knobX = colX + (colW - knobW) / 2;
            s.knob->setBounds(knobX, row2Y, knobW, kKnobSz);
            s.label->setBounds(colX, row2Y + kKnobSz, colW, kLabelH);
        }

        // BeatDiv occupies the same slot as Rate (column 0)
        const int col0X = b.getX();
        const int bdX = col0X + (colW - knobW) / 2;
        beatDivKnob_.setBounds(bdX, row2Y, knobW, kKnobSz);
    }

    // Row 3: Sync button centered below Rate knob (column 0)
    {
        const int col0X = b.getX();
        const int syncX = col0X + (colW - kSyncW) / 2;
        syncBtn_.setBounds(syncX, row3Y, kSyncW, kSyncH);
    }
}
