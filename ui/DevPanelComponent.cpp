#include "DevPanelComponent.h"
#include <cstdio>

// ParamID string constants duplicated to avoid including plugin headers.
// Must stay in sync with plugin/ParamIDs.h.
namespace {
    // Binaural
    constexpr const char* kITD_MAX_MS       = "itd_max_ms";
    constexpr const char* kHEAD_SHADOW_HZ   = "head_shadow_hz";
    constexpr const char* kILD_MAX_DB       = "ild_max_db";
    constexpr const char* kREAR_SHADOW_HZ   = "rear_shadow_hz";
    constexpr const char* kSMOOTH_ITD_MS    = "smooth_itd_ms";
    constexpr const char* kSMOOTH_FILTER_MS = "smooth_filter_ms";
    constexpr const char* kSMOOTH_GAIN_MS   = "smooth_gain_ms";

    // Comb
    constexpr const char* kCOMB_DELAY[10] = {
        "comb_delay_0", "comb_delay_1", "comb_delay_2", "comb_delay_3", "comb_delay_4",
        "comb_delay_5", "comb_delay_6", "comb_delay_7", "comb_delay_8", "comb_delay_9"
    };
    constexpr const char* kCOMB_FB[10] = {
        "comb_fb_0", "comb_fb_1", "comb_fb_2", "comb_fb_3", "comb_fb_4",
        "comb_fb_5", "comb_fb_6", "comb_fb_7", "comb_fb_8", "comb_fb_9"
    };
    constexpr const char* kCOMB_WET_MAX     = "comb_wet_max";

    // Elevation
    constexpr const char* kPINNA_NOTCH_HZ   = "pinna_notch_hz";
    constexpr const char* kPINNA_NOTCH_Q    = "pinna_notch_q";
    constexpr const char* kPINNA_SHELF_HZ   = "pinna_shelf_hz";
    constexpr const char* kCHEST_DELAY_MS   = "chest_delay_ms";
    constexpr const char* kCHEST_GAIN_DB    = "chest_gain_db";
    constexpr const char* kFLOOR_DELAY_MS   = "floor_delay_ms";
    constexpr const char* kFLOOR_GAIN_DB    = "floor_gain_db";

    // Distance
    constexpr const char* kDIST_DELAY_MAX_MS = "dist_delay_max_ms";
    constexpr const char* kDIST_SMOOTH_MS    = "dist_smooth_ms";
    constexpr const char* kDOPPLER_ENABLED   = "doppler_enabled";
    constexpr const char* kAIR_ABS_MAX_HZ    = "air_abs_max_hz";
    constexpr const char* kAIR_ABS_MIN_HZ    = "air_abs_min_hz";

    // Presence shelf
    constexpr const char* kPRES_SHELF_FREQ   = "presence_shelf_freq_hz";
    constexpr const char* kPRES_SHELF_MAX_DB = "presence_shelf_max_db";

    // Ear canal
    constexpr const char* kEAR_CANAL_FREQ    = "ear_canal_freq_hz";
    constexpr const char* kEAR_CANAL_Q       = "ear_canal_q";
    constexpr const char* kEAR_CANAL_MAX_DB  = "ear_canal_max_db";

    // Aux send
    constexpr const char* kAUX_SEND_MAX_DB   = "aux_send_gain_max_db";

    // Geometry
    constexpr const char* kSPHERE_RADIUS     = "sphere_radius";
    constexpr const char* kVERT_MONO_CYL     = "vert_mono_cyl_radius";

    // Test tone
    constexpr const char* kTEST_TONE_ENABLED  = "test_tone_enabled";
    constexpr const char* kTEST_TONE_GAIN_DB  = "test_tone_gain_db";
    constexpr const char* kTEST_TONE_PITCH_HZ = "test_tone_pitch_hz";
    constexpr const char* kTEST_TONE_PULSE_HZ = "test_tone_pulse_hz";
    constexpr const char* kTEST_TONE_WAVEFORM = "test_tone_waveform";
}

DevPanelComponent::DevPanelComponent(juce::AudioProcessorValueTreeState& apvts,
                                     xyzpan::DSPStateBridge* dspBridge)
    : dspBridge_(dspBridge)
{
    // Viewport owns the content component (false = don't delete on reassign)
    viewport_.setViewedComponent(&content_, false);
    viewport_.setScrollBarsShown(true, false);  // vertical scroll only
    addAndMakeVisible(viewport_);

    // -------------------------------------------------------------------
    // Section 1: Test Tone (dev utility — stays at top)
    // -------------------------------------------------------------------
    beginSection("Test Tone");
    addDevToggle(kTEST_TONE_ENABLED,  apvts);
    addDevSlider(kTEST_TONE_GAIN_DB,  apvts);
    addDevSlider(kTEST_TONE_PITCH_HZ, apvts);
    addDevSlider(kTEST_TONE_PULSE_HZ, apvts);
    addDevSlider(kTEST_TONE_WAVEFORM, apvts);

    // -------------------------------------------------------------------
    // Section 2: X-Axis — Left/Right (binaural azimuth cues)
    // -------------------------------------------------------------------
    beginSection("X-Axis: Left/Right");
    addDevSlider(kITD_MAX_MS,     apvts);
    addDevSlider(kHEAD_SHADOW_HZ, apvts);
    addDevSlider(kILD_MAX_DB,     apvts);
    addDevSlider(kVERT_MONO_CYL,  apvts);

    // -------------------------------------------------------------------
    // Section 3: Y-Axis — Front/Back (front-rear spectral cues)
    // -------------------------------------------------------------------
    beginSection("Y-Axis: Front/Back");
    addDevSlider(kREAR_SHADOW_HZ,    apvts);
    addDevSlider(kPRES_SHELF_FREQ,   apvts);
    addDevSlider(kPRES_SHELF_MAX_DB, apvts);
    addDevSlider(kEAR_CANAL_FREQ,    apvts);
    addDevSlider(kEAR_CANAL_Q,       apvts);
    addDevSlider(kEAR_CANAL_MAX_DB,  apvts);

    // -------------------------------------------------------------------
    // Section 4: Z-Axis — Above/Below (elevation cues)
    // -------------------------------------------------------------------
    beginSection("Z-Axis: Above/Below");
    addDevSlider(kPINNA_NOTCH_HZ, apvts);
    addDevSlider(kPINNA_NOTCH_Q,  apvts);
    addDevSlider(kPINNA_SHELF_HZ, apvts);
    addDevSlider(kCHEST_DELAY_MS, apvts);
    addDevSlider(kCHEST_GAIN_DB,  apvts);
    addDevSlider(kFLOOR_DELAY_MS, apvts);
    addDevSlider(kFLOOR_GAIN_DB,  apvts);
    for (int i = 0; i < 10; ++i)
        addDevSlider(kCOMB_DELAY[i], apvts);
    for (int i = 0; i < 10; ++i)
        addDevSlider(kCOMB_FB[i], apvts);
    addDevSlider(kCOMB_WET_MAX, apvts);

    // -------------------------------------------------------------------
    // Section 5: Distance (propagation cues)
    // -------------------------------------------------------------------
    beginSection("Distance");
    addDevSlider(kDIST_DELAY_MAX_MS, apvts);
    addDevSlider(kDIST_SMOOTH_MS,    apvts);
    addDevToggle(kDOPPLER_ENABLED,   apvts);
    addDevSlider(kAIR_ABS_MAX_HZ,    apvts);
    addDevSlider(kAIR_ABS_MIN_HZ,    apvts);
    addDevSlider(kAUX_SEND_MAX_DB,   apvts);

    // -------------------------------------------------------------------
    // Section 6: Smoothing (cross-axis utility)
    // -------------------------------------------------------------------
    beginSection("Smoothing");
    addDevSlider(kSMOOTH_ITD_MS,    apvts);
    addDevSlider(kSMOOTH_FILTER_MS, apvts);
    addDevSlider(kSMOOTH_GAIN_MS,   apvts);

    // -------------------------------------------------------------------
    // DSP Readouts (live telemetry — always last, not collapsible)
    // -------------------------------------------------------------------
    if (dspBridge_ != nullptr) {
        // Use beginSection but mark it non-collapsible by not adding to
        // the clickable set — we handle this in relayout() by never hiding
        // readout children. Actually simpler: just use a plain header label.
        auto* header = groupHeaders_.emplace_back(
            std::make_unique<juce::Label>()).get();
        header->setText("DSP Readouts", juce::dontSendNotification);
        header->setFont(juce::Font(12.0f, juce::Font::bold));
        header->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        content_.addAndMakeVisible(header);

        addReadonlyLabel("ITD Samples");
        addReadonlyLabel("Shadow Cutoff");
        addReadonlyLabel("ILD Gain");
        addReadonlyLabel("Rear Cutoff");
        addReadonlyLabel("Comb Wet");
        addReadonlyLabel("Mono Blend");
        addReadonlyLabel("Sample Rate");
        addReadonlyLabel("Dist Delay");
        addReadonlyLabel("Dist Gain");
        addReadonlyLabel("Air Cutoff");
        addReadonlyLabel("Mod X");

        startTimerHz(15);
    }

    relayout();
}

void DevPanelComponent::beginSection(const juce::String& title)
{
    auto* header = groupHeaders_.emplace_back(
        std::make_unique<juce::Label>()).get();

    header->setText(juce::String(juce::CharPointer_UTF8("\xe2\x96\xbe ")) + title,
                    juce::dontSendNotification);
    header->setFont(juce::Font(12.0f, juce::Font::bold));
    header->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    content_.addAndMakeVisible(header);

    // Make header clickable
    header->setInterceptsMouseClicks(true, false);
    header->addMouseListener(this, false);

    sections_.push_back({ header, {}, false });
    currentSectionIdx_ = static_cast<int>(sections_.size()) - 1;
}

void DevPanelComponent::addDevSlider(const juce::String& paramID,
                                      juce::AudioProcessorValueTreeState& apvts)
{
    auto* param = apvts.getParameter(paramID);
    if (param == nullptr) return;  // safety: skip unknown params

    // Label
    auto* lbl = labels_.emplace_back(std::make_unique<juce::Label>()).get();
    lbl->setText(param->getName(50), juce::dontSendNotification);
    lbl->setFont(juce::Font(11.0f));
    lbl->setColour(juce::Label::textColourId, juce::Colours::silver);
    lbl->setJustificationType(juce::Justification::centredLeft);
    content_.addAndMakeVisible(lbl);

    // Slider (linear horizontal)
    auto* slider = sliders_.emplace_back(std::make_unique<juce::Slider>()).get();
    slider->setSliderStyle(juce::Slider::LinearHorizontal);
    slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 16);
    slider->setColour(juce::Slider::backgroundColourId, juce::Colours::darkgrey);
    content_.addAndMakeVisible(slider);

    // SliderAttachment — created and kept alive permanently
    attachments_.emplace_back(
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, paramID, *slider));

    // Register with current section
    if (currentSectionIdx_ >= 0) {
        sections_[static_cast<size_t>(currentSectionIdx_)].children.push_back(lbl);
        sections_[static_cast<size_t>(currentSectionIdx_)].children.push_back(slider);
    }
}

void DevPanelComponent::addDevToggle(const juce::String& paramID,
                                      juce::AudioProcessorValueTreeState& apvts)
{
    auto* param = apvts.getParameter(paramID);
    if (param == nullptr) return;

    // Label
    auto* lbl = labels_.emplace_back(std::make_unique<juce::Label>()).get();
    lbl->setText(param->getName(50), juce::dontSendNotification);
    lbl->setFont(juce::Font(11.0f));
    lbl->setColour(juce::Label::textColourId, juce::Colours::silver);
    lbl->setJustificationType(juce::Justification::centredLeft);
    content_.addAndMakeVisible(lbl);

    // ToggleButton
    auto* toggle = toggles_.emplace_back(std::make_unique<juce::ToggleButton>()).get();
    toggle->setButtonText({});
    content_.addAndMakeVisible(toggle);

    // ButtonAttachment
    toggleAtts_.emplace_back(
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, paramID, *toggle));

    // Register with current section
    if (currentSectionIdx_ >= 0) {
        sections_[static_cast<size_t>(currentSectionIdx_)].children.push_back(lbl);
        sections_[static_cast<size_t>(currentSectionIdx_)].children.push_back(toggle);
    }
}

void DevPanelComponent::addReadonlyLabel(const juce::String& name)
{
    // Name label
    auto* nameLbl = readoutNameLabels_.emplace_back(std::make_unique<juce::Label>()).get();
    nameLbl->setText(name, juce::dontSendNotification);
    nameLbl->setFont(juce::Font(11.0f));
    nameLbl->setColour(juce::Label::textColourId, juce::Colours::silver);
    nameLbl->setJustificationType(juce::Justification::centredLeft);
    content_.addAndMakeVisible(nameLbl);

    // Value label
    auto* valLbl = readoutValueLabels_.emplace_back(std::make_unique<juce::Label>()).get();
    valLbl->setText("---", juce::dontSendNotification);
    valLbl->setFont(juce::Font(11.0f));
    valLbl->setColour(juce::Label::textColourId, juce::Colours::aqua);
    valLbl->setJustificationType(juce::Justification::centredLeft);
    content_.addAndMakeVisible(valLbl);
}

void DevPanelComponent::relayout()
{
    int yPos = kPadding;
    const int contentW = kPadding * 2 + kLabelW + kSliderW + 8;

    // Layout collapsible sections
    for (auto& section : sections_) {
        // Position header
        section.header->setBounds(kPadding, yPos, kLabelW + kSliderW + 8, kGroupH);
        yPos += kGroupH;

        // Update header triangle prefix
        juce::String title = section.header->getText();
        // Strip existing triangle prefix
        if (title.startsWithChar(0x25B8) || title.startsWithChar(0x25BE))
            title = title.fromFirstOccurrenceOf(" ", false, false);
        section.header->setText(
            juce::String(section.collapsed
                ? juce::CharPointer_UTF8("\xe2\x96\xb8 ")   // ▸
                : juce::CharPointer_UTF8("\xe2\x96\xbe "))  // ▾
            + title, juce::dontSendNotification);

        if (section.collapsed) {
            // Hide all children
            for (auto* child : section.children)
                child->setVisible(false);
        } else {
            // Show and position children in pairs (label + control)
            for (size_t i = 0; i + 1 < section.children.size(); i += 2) {
                auto* lbl = section.children[i];
                auto* ctrl = section.children[i + 1];

                lbl->setVisible(true);
                lbl->setBounds(kPadding, yPos, kLabelW, kRowH);

                ctrl->setVisible(true);
                // Check if it's a toggle (ToggleButton) or slider
                if (dynamic_cast<juce::ToggleButton*>(ctrl) != nullptr)
                    ctrl->setBounds(kPadding + kLabelW + 4, yPos + (kRowH - 20) / 2, 24, 20);
                else
                    ctrl->setBounds(kPadding + kLabelW + 4, yPos + (kRowH - 24) / 2, kSliderW, 24);

                yPos += kRowH;
            }
        }

        yPos += kPadding;  // gap between sections
    }

    // Layout DSP Readouts (non-collapsible, always last)
    // The readout header is the last groupHeader_ that isn't part of sections_
    if (dspBridge_ != nullptr && !groupHeaders_.empty()) {
        auto* readoutHeader = groupHeaders_.back().get();
        // Only position if it's the DSP Readouts header (not a section header)
        bool isReadoutHeader = true;
        for (auto& section : sections_) {
            if (section.header == readoutHeader) {
                isReadoutHeader = false;
                break;
            }
        }
        if (isReadoutHeader) {
            readoutHeader->setBounds(kPadding, yPos, kLabelW + kSliderW + 8, kGroupH);
            yPos += kGroupH;

            for (size_t i = 0; i < readoutNameLabels_.size(); ++i) {
                readoutNameLabels_[i]->setBounds(kPadding, yPos, kLabelW, kRowH);
                readoutValueLabels_[i]->setBounds(kPadding + kLabelW + 4, yPos, kSliderW, kRowH);
                yPos += kRowH;
            }
        }
    }

    yPos += kPadding;
    content_.setSize(contentW, yPos);
}

void DevPanelComponent::timerCallback()
{
    if (dspBridge_ == nullptr || readoutValueLabels_.size() < 11)
        return;

    auto s = dspBridge_->read();
    char buf[32];

    auto fmt = [&](int idx, const char* format, float val) {
        std::snprintf(buf, sizeof(buf), format, static_cast<double>(val));
        readoutValueLabels_[static_cast<size_t>(idx)]->setText(buf, juce::dontSendNotification);
    };

    fmt(0,  "%.2f",   s.itdSamples);
    fmt(1,  "%.0f Hz", s.shadowCutoffHz);
    fmt(2,  "%.4f",   s.ildGainLinear);
    fmt(3,  "%.0f Hz", s.rearCutoffHz);
    fmt(4,  "%.3f",   s.combWet);
    fmt(5,  "%.3f",   s.monoBlend);
    fmt(6,  "%.0f",   s.sampleRate);
    fmt(7,  "%.1f",   s.distDelaySamp);
    fmt(8,  "%.4f",   s.distGainLinear);
    fmt(9,  "%.0f Hz", s.airCutoffHz);
    fmt(10, "%.4f",   s.modX);
}

void DevPanelComponent::mouseDown(const juce::MouseEvent& event)
{
    auto* source = event.eventComponent;
    for (auto& section : sections_) {
        if (section.header == source) {
            section.collapsed = !section.collapsed;
            relayout();
            return;
        }
    }
}

void DevPanelComponent::resized()
{
    viewport_.setBounds(getLocalBounds());
}

void DevPanelComponent::paint(juce::Graphics& g)
{
    // Dark functional background — intentionally plain (not alchemy theme)
    g.fillAll(juce::Colour(0xFF1E1E1E));

    // Thin right-edge border
    g.setColour(juce::Colours::grey.darker(0.3f));
    g.drawLine(static_cast<float>(getWidth() - 1), 0.0f,
               static_cast<float>(getWidth() - 1),
               static_cast<float>(getHeight()), 1.0f);
}
