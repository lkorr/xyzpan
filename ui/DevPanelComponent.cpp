#include "DevPanelComponent.h"

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
}

DevPanelComponent::DevPanelComponent(juce::AudioProcessorValueTreeState& apvts)
{
    // Viewport owns the content component (false = don't delete on reassign)
    viewport_.setViewedComponent(&content_, false);
    viewport_.setScrollBarsShown(true, false);  // vertical scroll only
    addAndMakeVisible(viewport_);

    int yPos = kPadding;

    // -----------------------------------------------------------------------
    // Group 1: Binaural (7 params)
    // -----------------------------------------------------------------------
    addGroupHeader("Binaural", yPos);
    addDevSlider(kITD_MAX_MS,       yPos, apvts);
    addDevSlider(kHEAD_SHADOW_HZ,   yPos, apvts);
    addDevSlider(kILD_MAX_DB,       yPos, apvts);
    addDevSlider(kREAR_SHADOW_HZ,   yPos, apvts);
    addDevSlider(kSMOOTH_ITD_MS,    yPos, apvts);
    addDevSlider(kSMOOTH_FILTER_MS, yPos, apvts);
    addDevSlider(kSMOOTH_GAIN_MS,   yPos, apvts);

    yPos += kPadding;

    // -----------------------------------------------------------------------
    // Group 2: Comb Filters (21 params: 10 delays + 10 feedback + 1 wet max)
    // -----------------------------------------------------------------------
    addGroupHeader("Comb Filters", yPos);
    for (int i = 0; i < 10; ++i) {
        addDevSlider(kCOMB_DELAY[i], yPos, apvts);
    }
    for (int i = 0; i < 10; ++i) {
        addDevSlider(kCOMB_FB[i], yPos, apvts);
    }
    addDevSlider(kCOMB_WET_MAX, yPos, apvts);

    yPos += kPadding;

    // -----------------------------------------------------------------------
    // Group 3: Elevation (7 params)
    // -----------------------------------------------------------------------
    addGroupHeader("Elevation", yPos);
    addDevSlider(kPINNA_NOTCH_HZ, yPos, apvts);
    addDevSlider(kPINNA_NOTCH_Q,  yPos, apvts);
    addDevSlider(kPINNA_SHELF_HZ, yPos, apvts);
    addDevSlider(kCHEST_DELAY_MS, yPos, apvts);
    addDevSlider(kCHEST_GAIN_DB,  yPos, apvts);
    addDevSlider(kFLOOR_DELAY_MS, yPos, apvts);
    addDevSlider(kFLOOR_GAIN_DB,  yPos, apvts);

    yPos += kPadding;

    // -----------------------------------------------------------------------
    // Group 4: Distance (5 params — DOPPLER_ENABLED is a ToggleButton)
    // -----------------------------------------------------------------------
    addGroupHeader("Distance", yPos);
    addDevSlider(kDIST_DELAY_MAX_MS, yPos, apvts);
    addDevSlider(kDIST_SMOOTH_MS,    yPos, apvts);
    addDevToggle(kDOPPLER_ENABLED,   yPos, apvts);
    addDevSlider(kAIR_ABS_MAX_HZ,    yPos, apvts);
    addDevSlider(kAIR_ABS_MIN_HZ,    yPos, apvts);

    yPos += kPadding;

    // Size the inner content to fit everything
    content_.setSize(kPadding * 2 + kLabelW + kSliderW + 8, yPos);
}

void DevPanelComponent::addGroupHeader(const juce::String& title, int& yPos)
{
    auto* header = groupHeaders_.emplace_back(
        std::make_unique<juce::Label>()).get();

    header->setText(title, juce::dontSendNotification);
    header->setFont(juce::Font(12.0f, juce::Font::bold));
    header->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    header->setJustificationType(juce::Justification::centredLeft);
    header->setBounds(kPadding, yPos, kLabelW + kSliderW + 8, kGroupH);
    content_.addAndMakeVisible(header);

    yPos += kGroupH;
}

void DevPanelComponent::addDevSlider(const juce::String& paramID, int& yPos,
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
    lbl->setBounds(kPadding, yPos, kLabelW, kRowH);
    content_.addAndMakeVisible(lbl);

    // Slider (linear horizontal)
    auto* slider = sliders_.emplace_back(std::make_unique<juce::Slider>()).get();
    slider->setSliderStyle(juce::Slider::LinearHorizontal);
    slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 16);
    slider->setColour(juce::Slider::backgroundColourId, juce::Colours::darkgrey);
    slider->setBounds(kPadding + kLabelW + 4, yPos + (kRowH - 24) / 2, kSliderW, 24);
    content_.addAndMakeVisible(slider);

    // SliderAttachment — created and kept alive permanently
    attachments_.emplace_back(
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, paramID, *slider));

    yPos += kRowH;
}

void DevPanelComponent::addDevToggle(const juce::String& paramID, int& yPos,
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
    lbl->setBounds(kPadding, yPos, kLabelW, kRowH);
    content_.addAndMakeVisible(lbl);

    // ToggleButton
    auto* toggle = toggles_.emplace_back(std::make_unique<juce::ToggleButton>()).get();
    toggle->setButtonText({});
    toggle->setBounds(kPadding + kLabelW + 4, yPos + (kRowH - 20) / 2, 24, 20);
    content_.addAndMakeVisible(toggle);

    // ButtonAttachment
    toggleAtts_.emplace_back(
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, paramID, *toggle));

    yPos += kRowH;
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
