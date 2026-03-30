#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "xyzpan/Constants.h"
#include <cmath>
#include <random>

// Palette names/colours matching the GL foreign source palette
static const char* kPaletteNames[8] = {
    "Purple", "Teal", "Orange", "Blue", "Rose", "Olive", "Amber", "Lavender"
};
static const juce::Colour kPaletteColours[8] = {
    juce::Colour::fromFloatRGBA(0.60f, 0.40f, 0.80f, 1.0f),
    juce::Colour::fromFloatRGBA(0.30f, 0.70f, 0.50f, 1.0f),
    juce::Colour::fromFloatRGBA(0.80f, 0.45f, 0.30f, 1.0f),
    juce::Colour::fromFloatRGBA(0.40f, 0.55f, 0.85f, 1.0f),
    juce::Colour::fromFloatRGBA(0.75f, 0.35f, 0.55f, 1.0f),
    juce::Colour::fromFloatRGBA(0.50f, 0.70f, 0.35f, 1.0f),
    juce::Colour::fromFloatRGBA(0.85f, 0.65f, 0.30f, 1.0f),
    juce::Colour::fromFloatRGBA(0.45f, 0.45f, 0.70f, 1.0f),
};

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
XYZPanEditor::XYZPanEditor(XYZPanProcessor& p)
    : AudioProcessorEditor(p),
      proc_(p),
      glView_(p.apvts, &p, p.positionBridge, p.foreignSourceBridge, p.getReceivingBroadcastFlag()),
      xLFO_('X', p.apvts),
      yLFO_('Y', p.apvts),
      zLFO_('Z', p.apvts),
      orbitXYLFO_("stereo_orbit_xy", ParamID::STEREO_ORBIT_XY_TEMPO_SYNC, p.apvts),
      orbitXZLFO_("stereo_orbit_xz", ParamID::STEREO_ORBIT_XZ_TEMPO_SYNC, p.apvts),
      orbitYZLFO_("stereo_orbit_yz", ParamID::STEREO_ORBIT_YZ_TEMPO_SYNC, p.apvts),
      devPanel_(p.apvts, &p.dspStateBridge)
{
    // Apply alchemy look and feel to this editor; child components inherit via addAndMakeVisible
    setLookAndFeel(&lookAndFeel_);

    // ----- GL view — added FIRST so devPanel_ (added later) paints on top of it -----
    addAndMakeVisible(glView_);

    // ----- Position knobs (X / Y / Z) — left column, 2x large -----
    for (auto* knob : {&xKnob_, &yKnob_, &zKnob_}) {
        knob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 63, 16);
        addAndMakeVisible(knob);
    }

    // Per-axis arc colours: X=Cinnabar (red), Y=Aqua (cyan), Z=Gold Leaf (gold)
    xKnob_.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(xyzpan::AlchemyLookAndFeel::kCinnabar));
    yKnob_.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(xyzpan::AlchemyLookAndFeel::kAqua));
    zKnob_.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(xyzpan::AlchemyLookAndFeel::kGoldLeaf));

    xAtt_ = std::make_unique<SA>(p.apvts, ParamID::X, xKnob_);
    yAtt_ = std::make_unique<SA>(p.apvts, ParamID::Y, yKnob_);
    zAtt_ = std::make_unique<SA>(p.apvts, ParamID::Z, zKnob_);

    xLabel_.setText("X", juce::dontSendNotification);
    yLabel_.setText("Y", juce::dontSendNotification);
    zLabel_.setText("Z", juce::dontSendNotification);
    for (auto* lbl : {&xLabel_, &yLabel_, &zLabel_}) {
        lbl->setJustificationType(juce::Justification::centred);
        lbl->setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
        lbl->setColour(juce::Label::textColourId,
                       juce::Colour(xyzpan::AlchemyLookAndFeel::kGoldLeafPale));
        addAndMakeVisible(lbl);
    }

    // ----- LFO strips (X / Y / Z) -----
    addAndMakeVisible(xLFO_);
    addAndMakeVisible(yLFO_);
    addAndMakeVisible(zLFO_);

    // Wire LFO output sources for waveform history display
    xLFO_.setOutputSource(&p.lfoOutputX);
    yLFO_.setOutputSource(&p.lfoOutputY);
    zLFO_.setOutputSource(&p.lfoOutputZ);

    // ----- XYZ LFO Speed slider (below LFO strips) -----
    lfoSpeedMulKnob_.setSliderStyle(juce::Slider::LinearHorizontal);
    lfoSpeedMulKnob_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 16);
    lfoSpeedMulKnob_.setColour(juce::Slider::rotarySliderFillColourId,
                               juce::Colour(xyzpan::AlchemyLookAndFeel::kBrightGold));
    addAndMakeVisible(lfoSpeedMulKnob_);
    lfoSpeedMulAtt_ = std::make_unique<SA>(p.apvts, ParamID::LFO_SPEED_MUL, lfoSpeedMulKnob_);
    lfoSpeedMulLabel_.setText("LFO Speed", juce::dontSendNotification);
    lfoSpeedMulLabel_.setJustificationType(juce::Justification::centredLeft);
    lfoSpeedMulLabel_.setFont(juce::Font(juce::FontOptions(11.0f)));
    addAndMakeVisible(lfoSpeedMulLabel_);

    // ----- Reset XYZ LFO Phases button -----
    resetXYZPhasesBtn_.onClick = [this] {
        proc_.resetXYZLfoPhases.store(true);
    };
    addAndMakeVisible(resetXYZPhasesBtn_);

    // ----- Stereo orbit controls -----
    // Width / Phase / Offset — horizontal sliders (matching LFO Speed style)
    auto configOrbitSlider = [this](juce::Slider& s, juce::Label& l, const juce::String& name) {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 16);
        l.setText(name, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centredLeft);
        l.setFont(juce::Font(juce::FontOptions(11.0f)));
        addAndMakeVisible(&s);
        addAndMakeVisible(&l);
    };
    configOrbitSlider(stereoWidthKnob_, stereoWidthLabel_, "Width");
    configOrbitSlider(orbitPhaseKnob_,  orbitPhaseLabel_,  "Phase");
    configOrbitSlider(orbitOffsetKnob_, orbitOffsetLabel_, "Offset");

    // Speed — stays as LinearHorizontal (horizontal slot at bottom of orbit LFO section)
    orbitSpeedMulKnob_.setSliderStyle(juce::Slider::LinearHorizontal);
    orbitSpeedMulKnob_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 16);
    orbitSpeedMulLabel_.setText("Speed", juce::dontSendNotification);
    orbitSpeedMulLabel_.setJustificationType(juce::Justification::centredLeft);
    orbitSpeedMulLabel_.setFont(juce::Font(juce::FontOptions(11.0f)));
    addAndMakeVisible(&orbitSpeedMulKnob_);
    addAndMakeVisible(&orbitSpeedMulLabel_);

    stereoWidthAtt_  = std::make_unique<SA>(p.apvts, ParamID::STEREO_WIDTH,          stereoWidthKnob_);
    orbitPhaseAtt_   = std::make_unique<SA>(p.apvts, ParamID::STEREO_ORBIT_PHASE,    orbitPhaseKnob_);
    orbitOffsetAtt_  = std::make_unique<SA>(p.apvts, ParamID::STEREO_ORBIT_OFFSET,   orbitOffsetKnob_);
    orbitSpeedMulAtt_ = std::make_unique<SA>(p.apvts, ParamID::STEREO_ORBIT_SPEED_MUL, orbitSpeedMulKnob_);

    // Hero styling for all orbit sliders
    for (auto* knob : {&stereoWidthKnob_, &orbitSpeedMulKnob_, &orbitOffsetKnob_, &orbitPhaseKnob_})
        knob->setColour(juce::Slider::rotarySliderFillColourId,
                         juce::Colour(xyzpan::AlchemyLookAndFeel::kBrightGold));

    faceListenerToggle_.setButtonText("Always Face Observer");
    faceListenerToggle_.setClickingTogglesState(true);
    addAndMakeVisible(faceListenerToggle_);
    faceListenerAtt_ = std::make_unique<BA>(p.apvts, ParamID::STEREO_FACE_LISTENER, faceListenerToggle_);

    // ----- Listener head orientation knobs (continuous / wrapping) -----
    for (auto* knob : {&listenerYawKnob_, &listenerPitchKnob_, &listenerRollKnob_}) {
        knob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 55, 14);
        knob->setColour(juce::Slider::rotarySliderFillColourId,
                        juce::Colour(xyzpan::AlchemyLookAndFeel::kWarmGold));
        knob->setRotaryParameters(0.0f, juce::MathConstants<float>::twoPi, false);
        addAndMakeVisible(knob);
    }
    listenerYawAtt_   = std::make_unique<SA>(p.apvts, ParamID::LISTENER_YAW,   listenerYawKnob_);
    listenerPitchAtt_ = std::make_unique<SA>(p.apvts, ParamID::LISTENER_PITCH, listenerPitchKnob_);
    listenerRollAtt_  = std::make_unique<SA>(p.apvts, ParamID::LISTENER_ROLL,  listenerRollKnob_);
    listenerYawLabel_.setText("Yaw", juce::dontSendNotification);
    listenerPitchLabel_.setText("Pitch", juce::dontSendNotification);
    listenerRollLabel_.setText("Roll", juce::dontSendNotification);
    for (auto* lbl : {&listenerYawLabel_, &listenerPitchLabel_, &listenerRollLabel_}) {
        lbl->setJustificationType(juce::Justification::centred);
        lbl->setFont(juce::Font(juce::FontOptions(10.0f)));
        addAndMakeVisible(lbl);
    }

    // Head-follows-camera toggle
    headFollowsToggle_.setButtonText("Head Follows Camera");
    headFollowsToggle_.setClickingTogglesState(true);
    addAndMakeVisible(headFollowsToggle_);
    headFollowsAtt_ = std::make_unique<BA>(p.apvts, ParamID::HEAD_FOLLOWS_CAMERA, headFollowsToggle_);

    // Link Listener toggle
    listenerLinkToggle_.setButtonText("Link Listener");
    listenerLinkToggle_.setClickingTogglesState(true);
    addAndMakeVisible(listenerLinkToggle_);
    listenerLinkAtt_ = std::make_unique<BA>(p.apvts, ParamID::LISTENER_LINK, listenerLinkToggle_);
    listenerLinkToggle_.onClick = [this] { updateListenerControlsEnabled(); };

    // Pilot toggle — only pilot can drive shared listener position
    listenerPilotToggle_.setButtonText("Pilot");
    listenerPilotToggle_.setClickingTogglesState(true);
    addAndMakeVisible(listenerPilotToggle_);
    listenerPilotAtt_ = std::make_unique<BA>(p.apvts, ParamID::LISTENER_PILOT, listenerPilotToggle_);
    listenerPilotToggle_.onClick = [this] { updateListenerControlsEnabled(); };

    pilotStatusLabel_.setJustificationType(juce::Justification::centredLeft);
    pilotStatusLabel_.setFont(juce::Font(juce::FontOptions(11.0f)));
    pilotStatusLabel_.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(pilotStatusLabel_);

    // ----- Walker mode knobs (Listener tab in left column) -----
    for (auto* knob : {&walkerXKnob_, &walkerYKnob_, &walkerZKnob_}) {
        knob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 63, 16);
        knob->setColour(juce::Slider::rotarySliderFillColourId,
                        juce::Colour(xyzpan::AlchemyLookAndFeel::kAqua));
        addAndMakeVisible(knob);
    }
    walkerXAtt_ = std::make_unique<SA>(p.apvts, ParamID::WALKER_X, walkerXKnob_);
    walkerYAtt_ = std::make_unique<SA>(p.apvts, ParamID::WALKER_Y, walkerYKnob_);
    walkerZAtt_ = std::make_unique<SA>(p.apvts, ParamID::WALKER_Z, walkerZKnob_);

    walkerXLabel_.setText("Walk X", juce::dontSendNotification);
    walkerYLabel_.setText("Walk Y", juce::dontSendNotification);
    walkerZLabel_.setText("Walk Z", juce::dontSendNotification);
    for (auto* lbl : {&walkerXLabel_, &walkerYLabel_, &walkerZLabel_}) {
        lbl->setJustificationType(juce::Justification::centred);
        lbl->setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        lbl->setColour(juce::Label::textColourId,
                       juce::Colour(xyzpan::AlchemyLookAndFeel::kGoldLeafPale));
        addAndMakeVisible(lbl);
    }

    wasdToggle_.setButtonText("WASD Control");
    wasdToggle_.setClickingTogglesState(true);
    addAndMakeVisible(wasdToggle_);
    wasdAtt_ = std::make_unique<BA>(p.apvts, ParamID::WASD_CONTROL, wasdToggle_);

    // All controls visible by default in two-zone layout
    updateListenerControlsEnabled();

    binauralToggle_.setButtonText("");
    binauralToggle_.setClickingTogglesState(true);
    addAndMakeVisible(binauralToggle_);
    binauralAtt_ = std::make_unique<BA>(p.apvts, ParamID::BINAURAL_ENABLED, binauralToggle_);

    binauralLabel_.setText("Binaural", juce::dontSendNotification);
    binauralLabel_.setJustificationType(juce::Justification::centredLeft);
    binauralLabel_.setFont(juce::Font(juce::FontOptions(8.0f)));
    addAndMakeVisible(binauralLabel_);

    earlyReflToggle_.setButtonText("");
    earlyReflToggle_.setClickingTogglesState(true);
    addAndMakeVisible(earlyReflToggle_);
    earlyReflAtt_ = std::make_unique<BA>(p.apvts, ParamID::ER_ENABLED, earlyReflToggle_);

    earlyReflLabel_.setText("Early Reflections", juce::dontSendNotification);
    earlyReflLabel_.setJustificationType(juce::Justification::centredLeft);
    earlyReflLabel_.setFont(juce::Font(juce::FontOptions(8.0f)));
    addAndMakeVisible(earlyReflLabel_);

    // ----- Reset Orbit LFO Phases button -----
    resetOrbitPhasesBtn_.onClick = [this] {
        proc_.resetOrbitLfoPhases.store(true);
    };
    addAndMakeVisible(resetOrbitPhasesBtn_);

    // ----- Orbit LFO strips -----
    addAndMakeVisible(orbitXYLFO_);
    addAndMakeVisible(orbitXZLFO_);
    addAndMakeVisible(orbitYZLFO_);

    orbitXYLFO_.setOutputSource(&p.lfoOutputOrbitXY);
    orbitXZLFO_.setOutputSource(&p.lfoOutputOrbitXZ);
    orbitYZLFO_.setOutputSource(&p.lfoOutputOrbitYZ);

    // ----- Width-gated enable/disable for orbit controls -----
    stereoWidthKnob_.onValueChange = [this] { updateOrbitEnabled(); };
    updateOrbitEnabled();

    // ----- Sphere Radius knob (bottom row) -----
    sphereRadiusKnob_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    sphereRadiusKnob_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 55, 14);
    addAndMakeVisible(sphereRadiusKnob_);
    sphereRadiusAtt_ = std::make_unique<SA>(p.apvts, ParamID::SPHERE_RADIUS, sphereRadiusKnob_);
    sphereRadiusLabel_.setText("Sphere", juce::dontSendNotification);
    sphereRadiusLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(sphereRadiusLabel_);

    // ----- Reverb knobs (Size / Decay / Damping / Wet) — bottom row -----
    auto configVerbKnob = [this](juce::Slider& s, juce::Label& l, const juce::String& name) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 14);
        l.setText(name, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(&s);
        addAndMakeVisible(&l);
    };
    configVerbKnob(verbSize_,    verbSizeL_,    "Size");
    configVerbKnob(verbDecay_,   verbDecayL_,   "Decay");
    configVerbKnob(verbDamping_, verbDampingL_, "Damp");
    configVerbKnob(verbWet_,     verbWetL_,     "Wet");

    verbSizeAtt_    = std::make_unique<SA>(p.apvts, ParamID::VERB_SIZE,    verbSize_);
    verbDecayAtt_   = std::make_unique<SA>(p.apvts, ParamID::VERB_DECAY,   verbDecay_);
    verbDampingAtt_ = std::make_unique<SA>(p.apvts, ParamID::VERB_DAMPING, verbDamping_);
    verbWetAtt_     = std::make_unique<SA>(p.apvts, ParamID::VERB_WET,     verbWet_);

    // ----- Doppler knob (distance delay amount) -----
    dopplerKnob_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    dopplerKnob_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 55, 14);
    addAndMakeVisible(dopplerKnob_);
    dopplerAtt_ = std::make_unique<SA>(p.apvts, ParamID::DIST_DELAY_MAX_MS, dopplerKnob_);
    dopplerLabel_.setText("Doppler", juce::dontSendNotification);
    dopplerLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(dopplerLabel_);
    dopplerSubLabel_.setText("(adds delay)", juce::dontSendNotification);
    dopplerSubLabel_.setJustificationType(juce::Justification::centred);
    dopplerSubLabel_.setFont(juce::Font(juce::FontOptions(9.0f)));
    dopplerSubLabel_.setColour(juce::Label::textColourId,
                               juce::Colour(xyzpan::AlchemyLookAndFeel::kAgedPapyrusDark).withAlpha(0.6f));
    addAndMakeVisible(dopplerSubLabel_);

    // ----- Input Gain knob (0–12 dB) -----
    inputGainKnob_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    inputGainKnob_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 55, 14);
    inputGainKnob_.setTextValueSuffix(" dB");
    addAndMakeVisible(inputGainKnob_);
    inputGainAtt_ = std::make_unique<SA>(p.apvts, ParamID::INPUT_GAIN_DB, inputGainKnob_);
    inputGainLabel_.setText("In Gain", juce::dontSendNotification);
    inputGainLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(inputGainLabel_);

    // ----- Snap buttons -----
    snapXY_.setButtonText("XY");
    snapXZ_.setButtonText("XZ");
    snapYZ_.setButtonText("YZ");

    snapXY_.setClickingTogglesState(false);
    snapXZ_.setClickingTogglesState(false);
    snapYZ_.setClickingTogglesState(false);

    snapXY_.onClick = [this] {
        glView_.setSnapView(xyzpan::XYZPanGLView::SnapView::TopDown);
        currentSnap_ = SnapState::TopDown;
        updateSnapButtonStates();
    };
    snapXZ_.onClick = [this] {
        glView_.setSnapView(xyzpan::XYZPanGLView::SnapView::Side);
        currentSnap_ = SnapState::Side;
        updateSnapButtonStates();
    };
    snapYZ_.onClick = [this] {
        glView_.setSnapView(xyzpan::XYZPanGLView::SnapView::Front);
        currentSnap_ = SnapState::Front;
        updateSnapButtonStates();
    };

    for (auto* btn : {&snapXY_, &snapXZ_, &snapYZ_})
        glView_.addAndMakeVisible(btn);

    // When the user drags the camera while in a snap view, the camera
    // transitions back to orbit internally — sync button highlight states.
    glView_.onSnapExited = [this] {
        currentSnap_ = SnapState::None;
        updateSnapButtonStates();
    };

    // ----- Dev panel toggle (bottom row) -----
    devToggle_.onClick = [this] {
        devPanel_.setVisible(!devPanel_.isVisible());
    };
    addAndMakeVisible(devToggle_);

    // Dev panel: child of glView_ (not the editor) so it composites on top of the
    // OpenGL surface. Bounds are set in resized() in glView_-local coordinates.
    devPanel_.setVisible(false);
    glView_.addAndMakeVisible(devPanel_);

    // ----- Output meter (right edge) -----
    addAndMakeVisible(outputMeter_);
    outputMeter_.setRMSSources(&p.outputRmsL, &p.outputRmsR);

    // ----- Preset controls (top bar) -----
    addAndMakeVisible(presetCombo_);
    for (int i = 0; i < XYZPresets::kNumPresets; ++i)
        presetCombo_.addItem(XYZPresets::kFactoryPresets[i].name, i + 1); // ComboBox IDs are 1-based
    presetCombo_.setSelectedId(proc_.getCurrentProgram() + 1, juce::dontSendNotification);
    presetCombo_.onChange = [this]() {
        int idx = presetCombo_.getSelectedId() - 1; // Convert back to 0-based
        if (idx >= 0 && idx < XYZPresets::kNumPresets)
            proc_.setCurrentProgram(idx);
    };

    addAndMakeVisible(presetSaveBtn_);
    presetSaveBtn_.onClick = [this]() {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Save Preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.xml");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file == juce::File{})
                    return;
                // Ensure .xml extension
                if (!file.hasFileExtension("xml"))
                    file = file.withFileExtension("xml");
                auto state = proc_.apvts.copyState();
                auto xml = state.createXml();
                if (xml != nullptr)
                    xml->writeTo(file);
            });
    };

    addAndMakeVisible(presetLoadBtn_);
    presetLoadBtn_.onClick = [this]() {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Load Preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.xml");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file == juce::File{} || !file.existsAsFile())
                    return;
                auto xml = juce::parseXML(file);
                if (xml != nullptr && xml->hasTagName(proc_.apvts.state.getType()))
                    proc_.apvts.replaceState(juce::ValueTree::fromXml(*xml));
                // After loading user preset, deselect factory preset in combo
                presetCombo_.setSelectedId(0, juce::dontSendNotification);
            });
    };

    // ----- User preferences (theme + avatar persistence) -----
    userPrefs_ = std::make_unique<xyzpan::UserPreferences>();
    lookAndFeel_.applyTheme(userPrefs_->activeTheme());
    glView_.setColorTheme(userPrefs_->activeTheme());
    glView_.setAvatarParams(userPrefs_->avatarParams());

    // ----- Customize tab: theme combo -----
    themeLabel_.setText("Theme", juce::dontSendNotification);
    themeLabel_.setJustificationType(juce::Justification::centredLeft);
    themeLabel_.setFont(juce::Font(juce::FontOptions(11.0f)));
    customizeContent_.addAndMakeVisible(themeLabel_);

    for (int i = 0; i < xyzpan::kNumThemes; ++i)
        themeCombo_.addItem(xyzpan::getThemeEntry(i).name, i + 1);
    themeCombo_.setSelectedId(userPrefs_->themeIndex() + 1, juce::dontSendNotification);
    themeCombo_.onChange = [this] {
        int idx = themeCombo_.getSelectedId() - 1;
        if (idx >= 0 && idx < xyzpan::kNumThemes) {
            userPrefs_->setThemeIndex(idx);
            applyCurrentTheme();
        }
    };
    customizeContent_.addAndMakeVisible(themeCombo_);

    // ----- Customize tab: color swatches -----
    {
        auto initLabel = [this](juce::Label& label, const juce::String& name) {
            label.setText(name, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centredLeft);
            label.setFont(juce::Font(juce::FontOptions(10.0f)));
            customizeContent_.addAndMakeVisible(label);
        };
        initLabel(headColorLabel_,  "Head");
        initLabel(noseColorLabel_,  "Nose");
        initLabel(hatColorLabel_,   "Hat");
        initLabel(eyeColorLabel_,   "Eyes");

        auto toJuceCol = [](const glm::vec3& v) {
            return juce::Colour::fromFloatRGBA(v.r, v.g, v.b, 1.0f);
        };

        const auto& avp = userPrefs_->avatarParams();
        const auto& thm = userPrefs_->activeTheme();

        headColorSwatch_.setColour(avp.headColor ? juce::Colour(avp.headColor) : toJuceCol(thm.glListenerHead));
        headColorSwatch_.onChange = [this](juce::Colour c) {
            auto a = userPrefs_->avatarParams(); a.headColor = c.getARGB(); userPrefs_->setAvatarParams(a);
            pushAvatarToGL();
        };
        headColorSwatch_.onReset = [this]() {
            auto a = userPrefs_->avatarParams(); a.headColor = 0; userPrefs_->setAvatarParams(a);
            headColorSwatch_.setColour(juce::Colour::fromFloatRGBA(
                userPrefs_->activeTheme().glListenerHead.r,
                userPrefs_->activeTheme().glListenerHead.g,
                userPrefs_->activeTheme().glListenerHead.b, 1.0f));
            pushAvatarToGL();
        };
        customizeContent_.addAndMakeVisible(headColorSwatch_);

        noseColorSwatch_.setColour(avp.noseColor ? juce::Colour(avp.noseColor) : toJuceCol(thm.glNose));
        noseColorSwatch_.onChange = [this](juce::Colour c) {
            auto a = userPrefs_->avatarParams(); a.noseColor = c.getARGB(); userPrefs_->setAvatarParams(a);
            pushAvatarToGL();
        };
        noseColorSwatch_.onReset = [this]() {
            auto a = userPrefs_->avatarParams(); a.noseColor = 0; userPrefs_->setAvatarParams(a);
            noseColorSwatch_.setColour(juce::Colour::fromFloatRGBA(
                userPrefs_->activeTheme().glNose.r,
                userPrefs_->activeTheme().glNose.g,
                userPrefs_->activeTheme().glNose.b, 1.0f));
            pushAvatarToGL();
        };
        customizeContent_.addAndMakeVisible(noseColorSwatch_);

        hatColorSwatch_.setColour(avp.hatColor ? juce::Colour(avp.hatColor) : toJuceCol(thm.glHat));
        hatColorSwatch_.onChange = [this](juce::Colour c) {
            auto a = userPrefs_->avatarParams(); a.hatColor = c.getARGB(); userPrefs_->setAvatarParams(a);
            pushAvatarToGL();
        };
        hatColorSwatch_.onReset = [this]() {
            auto a = userPrefs_->avatarParams(); a.hatColor = 0; userPrefs_->setAvatarParams(a);
            hatColorSwatch_.setColour(juce::Colour::fromFloatRGBA(
                userPrefs_->activeTheme().glHat.r,
                userPrefs_->activeTheme().glHat.g,
                userPrefs_->activeTheme().glHat.b, 1.0f));
            pushAvatarToGL();
        };
        customizeContent_.addAndMakeVisible(hatColorSwatch_);

        eyeColorSwatch_.setColour(avp.eyeColor ? juce::Colour(avp.eyeColor) : toJuceCol(thm.glEyeWhite));
        eyeColorSwatch_.onChange = [this](juce::Colour c) {
            auto a = userPrefs_->avatarParams(); a.eyeColor = c.getARGB(); userPrefs_->setAvatarParams(a);
            pushAvatarToGL();
        };
        eyeColorSwatch_.onReset = [this]() {
            auto a = userPrefs_->avatarParams(); a.eyeColor = 0; userPrefs_->setAvatarParams(a);
            eyeColorSwatch_.setColour(juce::Colour::fromFloatRGBA(
                userPrefs_->activeTheme().glEyeWhite.r,
                userPrefs_->activeTheme().glEyeWhite.g,
                userPrefs_->activeTheme().glEyeWhite.b, 1.0f));
            pushAvatarToGL();
        };
        customizeContent_.addAndMakeVisible(eyeColorSwatch_);
    }

    // ----- Customize tab: eye type combo -----
    eyeTypeLabel_.setText("Eye Type", juce::dontSendNotification);
    eyeTypeLabel_.setJustificationType(juce::Justification::centredLeft);
    eyeTypeLabel_.setFont(juce::Font(juce::FontOptions(11.0f)));
    customizeContent_.addAndMakeVisible(eyeTypeLabel_);

    eyeTypeCombo_.addItem("None",    1);
    eyeTypeCombo_.addItem("Normal",  2);
    eyeTypeCombo_.addItem("Googly",  3);
    eyeTypeCombo_.addItem("X-Eyes",  4);
    eyeTypeCombo_.addItem("Cyclops", 5);
    eyeTypeCombo_.setSelectedId(userPrefs_->avatarParams().eyeType + 1, juce::dontSendNotification);
    eyeTypeCombo_.onChange = [this] {
        int idx = eyeTypeCombo_.getSelectedId() - 1;
        if (idx >= 0 && idx <= 4) {
            auto ap = userPrefs_->avatarParams();
            ap.eyeType = idx;
            userPrefs_->setAvatarParams(ap);
            pushAvatarToGL();
            // Repurpose eyeSpacing slider as Eye Height for Cyclops
            syncEyeSpacingSliderMode(idx == xyzpan::kEyeCyclops);
            // Googly slider only makes sense for Googly eyes
            googlySlider_.setEnabled(idx == xyzpan::kEyeGoogly);
        }
    };
    customizeContent_.addAndMakeVisible(eyeTypeCombo_);

    // ----- Customize tab: ear type combo -----
    earTypeLabel_.setText("Ear Type", juce::dontSendNotification);
    earTypeLabel_.setJustificationType(juce::Justification::centredLeft);
    earTypeLabel_.setFont(juce::Font(juce::FontOptions(11.0f)));
    customizeContent_.addAndMakeVisible(earTypeLabel_);

    earTypeCombo_.addItem("Default", 1);
    earTypeCombo_.addItem("Pointy",  2);
    earTypeCombo_.addItem("Round",   3);
    earTypeCombo_.addItem("Cat",     4);
    earTypeCombo_.setSelectedId(userPrefs_->avatarParams().earType + 1, juce::dontSendNotification);
    earTypeCombo_.onChange = [this] {
        int idx = earTypeCombo_.getSelectedId() - 1;
        if (idx >= 0 && idx <= 3) {
            auto ap = userPrefs_->avatarParams();
            ap.earType = idx;
            userPrefs_->setAvatarParams(ap);
            pushAvatarToGL();
        }
    };
    customizeContent_.addAndMakeVisible(earTypeCombo_);

    // ----- Customize tab: hat type combo -----
    hatTypeLabel_.setText("Hat Type", juce::dontSendNotification);
    hatTypeLabel_.setJustificationType(juce::Justification::centredLeft);
    hatTypeLabel_.setFont(juce::Font(juce::FontOptions(11.0f)));
    customizeContent_.addAndMakeVisible(hatTypeLabel_);

    hatTypeCombo_.addItem("None",        1);
    hatTypeCombo_.addItem("Party Hat",   2);
    hatTypeCombo_.addItem("Top Hat",     3);
    hatTypeCombo_.addItem("Halo",        4);
    hatTypeCombo_.addItem("Beanie",      5);
    hatTypeCombo_.addItem("Devil Horns", 6);
    hatTypeCombo_.addItem("Ponytail",    7);
    hatTypeCombo_.setSelectedId(userPrefs_->avatarParams().hatType + 1, juce::dontSendNotification);
    hatTypeCombo_.onChange = [this] {
        int idx = hatTypeCombo_.getSelectedId() - 1;
        if (idx >= 0 && idx <= 6) {
            auto ap = userPrefs_->avatarParams();
            ap.hatType = idx;
            userPrefs_->setAvatarParams(ap);
            pushAvatarToGL();
        }
    };
    customizeContent_.addAndMakeVisible(hatTypeCombo_);

    // ----- Customize tab: nose type combo -----
    noseTypeLabel_.setText("Nose Type", juce::dontSendNotification);
    noseTypeLabel_.setJustificationType(juce::Justification::centredLeft);
    noseTypeLabel_.setFont(juce::Font(juce::FontOptions(11.0f)));
    customizeContent_.addAndMakeVisible(noseTypeLabel_);

    noseTypeCombo_.addItem("Cone",    1);
    noseTypeCombo_.addItem("Button",  2);
    noseTypeCombo_.addItem("Snout",   3);
    noseTypeCombo_.addItem("Clown",   4);
    noseTypeCombo_.addItem("Pointed", 5);
    noseTypeCombo_.addItem("None",    6);
    noseTypeCombo_.setSelectedId(userPrefs_->avatarParams().noseType + 1, juce::dontSendNotification);
    noseTypeCombo_.onChange = [this] {
        int idx = noseTypeCombo_.getSelectedId() - 1;
        if (idx >= 0 && idx <= 5) {
            auto ap = userPrefs_->avatarParams();
            ap.noseType = idx;
            userPrefs_->setAvatarParams(ap);
            pushAvatarToGL();
        }
    };
    customizeContent_.addAndMakeVisible(noseTypeCombo_);

    // Apply initial Cyclops state for eyeSpacing/eyeHeight mode
    syncEyeSpacingSliderMode(userPrefs_->avatarParams().eyeType == xyzpan::kEyeCyclops);
    // Googly slider only active for Googly eyes
    googlySlider_.setEnabled(userPrefs_->avatarParams().eyeType == xyzpan::kEyeGoogly);

    // ----- Customize tab: avatar sliders -----
    auto configAvatarSlider = [this](juce::Slider& s, juce::Label& l, const juce::String& name,
                                      float min, float max, float def) {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 42, 16);
        s.setRange(static_cast<double>(min), static_cast<double>(max), 0.01);
        s.setValue(static_cast<double>(def), juce::dontSendNotification);
        l.setText(name, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centredLeft);
        l.setFont(juce::Font(juce::FontOptions(10.0f)));
        customizeContent_.addAndMakeVisible(&s);
        customizeContent_.addAndMakeVisible(&l);
    };

    const auto& ap = userPrefs_->avatarParams();
    configAvatarSlider(headElongationSlider_, headElongationLabel_, "Elongation", 0.3f, 2.5f, ap.headElongation);
    configAvatarSlider(eyeSizeSlider_,        eyeSizeLabel_,        "Eye Size",   0.2f, 3.0f, ap.eyeSize);
    configAvatarSlider(eyeSpacingSlider_,     eyeSpacingLabel_,     "Eye Space",  0.2f, 3.0f, ap.eyeSpacing);
    configAvatarSlider(earSizeSlider_,        earSizeLabel_,        "Ear Size",   0.2f, 3.0f, ap.earSize);
    configAvatarSlider(headSizeSlider_,       headSizeLabel_,       "Head Size",  0.7f, 1.5f, ap.headSize);
    configAvatarSlider(pupilSizeSlider_,     pupilSizeLabel_,     "Pupil Size", 0.0f, 1.0f, ap.pupilSize);
    // Single "Googly" slider drives both gravity (0→1) and spring (1→0) via exponential curves.
    // Curve: gravity = t^0.322, spring = (1-t)^0.322  — concentrates resolution around 0.8.
    // Invert to find initial slider position from stored gravity: t = gravity^(1/0.322)
    {
        float initT = std::pow(std::clamp(ap.googlyGravity, 0.0f, 1.0f), 1.0f / 0.322f);
        configAvatarSlider(googlySlider_, googlyLabel_, "Googly", 0.0f, 1.0f, initT);
    }
    configAvatarSlider(earRotationSlider_,   earRotationLabel_,   "Ear Rot",    -180.0f, 180.0f, ap.earRotation);
    configAvatarSlider(hatSizeSlider_,       hatSizeLabel_,       "Hat Size",   0.2f, 2.0f, ap.hatSize);
    configAvatarSlider(noseSizeSlider_,      noseSizeLabel_,      "Nose Size",  0.2f, 3.0f, ap.noseSize);

    auto onAvatarChange = [this] {
        auto a = userPrefs_->avatarParams();
        a.headElongation = static_cast<float>(headElongationSlider_.getValue());
        a.eyeSize        = static_cast<float>(eyeSizeSlider_.getValue());
        if (a.eyeType == xyzpan::kEyeCyclops)
            a.eyeHeight  = static_cast<float>(eyeSpacingSlider_.getValue());
        else
            a.eyeSpacing = static_cast<float>(eyeSpacingSlider_.getValue());
        a.earSize        = static_cast<float>(earSizeSlider_.getValue());
        a.headSize       = static_cast<float>(headSizeSlider_.getValue());
        a.pupilSize      = static_cast<float>(pupilSizeSlider_.getValue());
        {
            float t = static_cast<float>(googlySlider_.getValue());
            a.googlyGravity = std::pow(t, 0.322f);
            a.googlySpring  = std::pow(1.0f - t, 0.322f);
        }
        a.earRotation    = static_cast<float>(earRotationSlider_.getValue());
        a.hatSize        = static_cast<float>(hatSizeSlider_.getValue());
        a.noseSize       = static_cast<float>(noseSizeSlider_.getValue());
        userPrefs_->setAvatarParams(a);
        pushAvatarToGL();
    };
    headElongationSlider_.onValueChange = onAvatarChange;
    eyeSizeSlider_.onValueChange        = onAvatarChange;
    eyeSpacingSlider_.onValueChange     = onAvatarChange;
    earSizeSlider_.onValueChange        = onAvatarChange;
    headSizeSlider_.onValueChange       = onAvatarChange;
    pupilSizeSlider_.onValueChange      = onAvatarChange;
    googlySlider_.onValueChange          = onAvatarChange;
    earRotationSlider_.onValueChange    = onAvatarChange;
    hatSizeSlider_.onValueChange        = onAvatarChange;
    noseSizeSlider_.onValueChange       = onAvatarChange;

    // Section header paint callback
    customizeContent_.onPaint = [this](juce::Graphics& g) {
        g.setColour(juce::Colour(0xFFC9A84C).withAlpha(0.7f));
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("EYES", 4, eyesSectionHeaderY_, 60, 16, juce::Justification::centredLeft);
        g.drawText("EARS", 4, earsSectionHeaderY_, 60, 16, juce::Justification::centredLeft);
        g.drawText("NOSE", 4, noseSectionHeaderY_, 60, 16, juce::Justification::centredLeft);
        g.drawText("HATS", 4, hatsSectionHeaderY_, 60, 16, juce::Justification::centredLeft);
    };

    // Set up scrollable viewport for customize tab
    customizeViewport_.setViewedComponent(&customizeContent_, false);
    customizeViewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(customizeViewport_);

    // Customize tab starts hidden
    customizeViewport_.setVisible(false);

    // ----- Remote popup button — visible only when linked instances >= 2 -----
    remoteBtn_.onClick = [this] {
        glView_.setShowInstanceList(!glView_.getShowInstanceList());
    };
    addAndMakeVisible(remoteBtn_);

    // Wire GL view instance list click callback to remote focus
    glView_.onInstanceClicked = [this](int linkedIndex) {
        setRemoteFocus(linkedIndex);
    };

    // Wire GL view rename callback to update processor instance name
    glView_.onInstanceRenamed = [this](const juce::String& newName) {
        proc_.setInstanceName(newName);
        const auto resolved = proc_.getInstanceNameValue();
        instanceNameEditor_.setText(resolved, false);
        glView_.setOwnInstanceName(resolved);
    };

    // ----- Remote: instance list + name editor -----
    // Rows are non-interactive labels; clicks handled in mouseDown via hit-testing
    for (int i = 0; i < kMaxRemoteRows; ++i) {
        instanceRows_[i].setJustificationType(juce::Justification::centredLeft);
        instanceRows_[i].setInterceptsMouseClicks(false, false);
        addAndMakeVisible(instanceRows_[i]);
        instanceRows_[i].setVisible(false);
    }

    // Instance list now rendered in GL view overlay — keep row labels hidden
    instanceRowCount_ = 0;

    // Remote status label
    remoteStatusLabel_.setText("No linked instances", juce::dontSendNotification);
    remoteStatusLabel_.setJustificationType(juce::Justification::centred);
    remoteStatusLabel_.setColour(juce::Label::textColourId,
                                 juce::Colour(lookAndFeel_.currentTheme().bronze));
    addAndMakeVisible(remoteStatusLabel_);
    remoteStatusLabel_.setVisible(false);

    // Own-instance name editor (kept functional but hidden — remote UI is now in GL overlay)
    instanceNameLabel_.setText("Name:", juce::dontSendNotification);
    instanceNameLabel_.setColour(juce::Label::textColourId,
                                 juce::Colour(lookAndFeel_.currentTheme().bronze));
    addAndMakeVisible(instanceNameLabel_);
    instanceNameLabel_.setVisible(false);

    instanceNameEditor_.setJustification(juce::Justification::centredLeft);
    instanceNameEditor_.setColour(juce::TextEditor::backgroundColourId,
                                   juce::Colour(lookAndFeel_.currentTheme().darkIron).brighter(0.1f));
    instanceNameEditor_.setColour(juce::TextEditor::textColourId,
                                   juce::Colour(lookAndFeel_.currentTheme().brightGold));
    instanceNameEditor_.setColour(juce::TextEditor::outlineColourId,
                                   juce::Colour(lookAndFeel_.currentTheme().bronze).withAlpha(0.5f));
    instanceNameEditor_.setText(proc_.getInstanceNameValue(), false);
    instanceNameEditor_.onReturnKey = [this] {
        proc_.setInstanceName(instanceNameEditor_.getText().trim());
        const auto resolved = proc_.getInstanceNameValue();
        instanceNameEditor_.setText(resolved, false);
        glView_.setOwnInstanceName(resolved);
    };
    instanceNameEditor_.onFocusLost = [this] {
        proc_.setInstanceName(instanceNameEditor_.getText().trim());
        const auto resolved = proc_.getInstanceNameValue();
        instanceNameEditor_.setText(resolved, false);
        glView_.setOwnInstanceName(resolved);
    };
    addAndMakeVisible(instanceNameEditor_);
    instanceNameEditor_.setVisible(false);

    // Initialize GL view with current instance name
    glView_.setOwnInstanceName(proc_.getInstanceNameValue());

    // Register removal callback so we detach if remote instance is destroyed.
    // This fires outside the hub's spinlock, so it's safe to do full
    // attachment teardown + rebuild synchronously.
    proc_.getListenerHub().addRemovalCallback(this, [this](SharedListenerHub::Listener* removed) {
        if (remoteFocusProc_ != nullptr &&
            static_cast<SharedListenerHub::Listener*>(remoteFocusProc_) == removed) {
            // Immediately revert to self — detach all remote attachments
            // before the remote processor's destructor continues
            remoteFocusProc_ = nullptr;
            remoteFocusIndex_ = -1;
            detachAndRebindTo(proc_.apvts, &proc_);
            glView_.setFocusedForeignSource(-1);
            lastKnownLinkedCount_ = 0;  // force list rebuild on next timer tick
            // Hide remote popup if open
            instanceNameLabel_.setVisible(false);
            instanceNameEditor_.setVisible(false);
            for (int ri = 0; ri < kMaxRemoteRows; ++ri)
                instanceRows_[ri].setVisible(false);
            repaint();
        }
    });

    // Generate noise texture for panel overlay
    noiseTexture_ = generateNoiseTexture(256);

    // Keyboard shortcut listener (captures Alt+R for module randomization, WASD for movement)
    addKeyListener(this);
    startTimerHz(60);  // 60 Hz poll for WASD movement

    // Listen to mouse clicks on all child components for last-clicked zone tracking
    addMouseListener(this, true);

    // Window sizing
    setResizable(true, true);
    setResizeLimits(900, 500, 1800, 1600);
    setSize(kDefaultW, kDefaultH);
}

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------
XYZPanEditor::~XYZPanEditor()
{
    // Remove callback first so it can't fire during teardown
    proc_.getListenerHub().removeRemovalCallback(this);

    // Revert to self before destruction to avoid dangling attachments
    if (remoteFocusProc_ != nullptr)
        setRemoteFocus(-1);

    endWasdGestureIfActive();
    stopTimer();
    removeMouseListener(this);
    removeKeyListener(this);
    setLookAndFeel(nullptr);
}

// ---------------------------------------------------------------------------
// Layout::compute — single source of structural geometry used by paint + resized
// ---------------------------------------------------------------------------
XYZPanEditor::Layout XYZPanEditor::Layout::compute(int totalW, int totalH)
{
    Layout l;
    l.contentW   = totalW - kMeterW;

    // Two-zone split: top = GL viewport, bottom = control columns
    l.viewportH  = juce::jmax(kMinViewportH,
                     static_cast<int>(totalH * kViewportFrac));
    l.bottomH    = juce::jmax(kMinBottomH, totalH - l.viewportH);
    l.viewportH  = totalH - l.bottomH;  // re-derive after clamping bottomH
    l.bottomY    = l.viewportH;

    // Tab bar sits at the very bottom of the window
    l.tabBarY    = totalH - kTabBarH;

    // Column content area: below section headers, above tab bar
    l.contentY   = l.bottomY + kSectionHdrH;
    l.contentH   = l.tabBarY - l.contentY;

    // Three equal columns (with separators eaten from center)
    const int availW = l.contentW - kColSepW * 2;  // 2 vertical separators
    l.leftColW   = availW / 3;
    l.centerColW = availW / 3;
    l.rightColW  = availW - l.leftColW - l.centerColW;  // absorb rounding
    l.centerColX = l.leftColW + kColSepW;
    l.rightColX  = l.centerColX + l.centerColW + kColSepW;

    return l;
}

// ---------------------------------------------------------------------------
// paint
// ---------------------------------------------------------------------------
void XYZPanEditor::paint(juce::Graphics& g)
{
    const auto& ct = lookAndFeel_.currentTheme();
    const auto lo = Layout::compute(getWidth(), getHeight());

    // ===== BACKGROUNDS =====
    g.fillAll(juce::Colour(ct.background));

    // Bottom zone background
    g.setColour(juce::Colour(ct.darkIron));
    g.fillRect(0, lo.bottomY, lo.contentW, lo.bottomH);

    // ===== SECTION HEADERS (one per column) =====
    auto drawHeader = [&](int x, int y, int w, const juce::String& text) {
        auto hdrRect = juce::Rectangle<int>(x, y, w, kSectionHdrH);
        g.setGradientFill(juce::ColourGradient(
            juce::Colour(ct.bronze).withAlpha(0.25f), static_cast<float>(x), static_cast<float>(y),
            juce::Colour(ct.darkIron), static_cast<float>(x + w), static_cast<float>(y), false));
        g.fillRect(hdrRect);
        g.setColour(juce::Colour(ct.brightGold));
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText(text, hdrRect.reduced(8, 0), juce::Justification::centredLeft);
        g.setColour(juce::Colour(ct.bronze).withAlpha(0.6f));
        g.drawHorizontalLine(y + kSectionHdrH - 1, static_cast<float>(x), static_cast<float>(x + w));
    };

    if (activeViewTab_ == ViewTab::Source) {
        // Column headers
        drawHeader(0,             lo.bottomY, lo.leftColW,   "STEREO");
        drawHeader(lo.centerColX, lo.bottomY, lo.centerColW, "SOURCE");
        drawHeader(lo.rightColX,  lo.bottomY, lo.rightColW,  "LISTENER");

        // Remote focus indicator — colored bar below source header
        if (remoteFocusIndex_ >= 0) {
            const int barH = 3;
            const int barY = lo.bottomY + kSectionHdrH;
            g.setColour(kPaletteColours[remoteFocusIndex_ % 8]);
            g.fillRect(lo.centerColX, barY, lo.centerColW, barH);
        }

        // ===== COLUMN DIVIDERS =====
        g.setColour(juce::Colour(ct.bronze).withAlpha(0.4f));
        // Left | Center separator
        g.drawVerticalLine(lo.centerColX - 1,
                           static_cast<float>(lo.bottomY),
                           static_cast<float>(lo.tabBarY));
        // Center | Right separator
        g.drawVerticalLine(lo.rightColX - 1,
                           static_cast<float>(lo.bottomY),
                           static_cast<float>(lo.tabBarY));

        // Horizontal separator between STEREO and REVERB in left column (midpoint)
        {
            const int midY = lo.contentY + lo.contentH / 2;
            g.setColour(juce::Colour(ct.bronze).withAlpha(0.3f));
            g.drawHorizontalLine(midY, 0.0f, static_cast<float>(lo.leftColW));
            // "REVERB" sub-header
            g.setColour(juce::Colour(ct.brightGold).withAlpha(0.8f));
            g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
            g.drawText("REVERB", 8, midY + 2, lo.leftColW - 16, 14, juce::Justification::centredLeft);
        }

        // Horizontal separator between SOURCE and OPTIONS in center column
        {
            const int midY = lo.contentY + lo.contentH / 2;
            g.setColour(juce::Colour(ct.bronze).withAlpha(0.3f));
            g.drawHorizontalLine(midY, static_cast<float>(lo.centerColX),
                                 static_cast<float>(lo.centerColX + lo.centerColW));
            // "OPTIONS" sub-header
            g.setColour(juce::Colour(ct.brightGold).withAlpha(0.8f));
            g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
            g.drawText("OPTIONS", lo.centerColX + 8, midY + 2, lo.centerColW - 16, 14,
                       juce::Justification::centredLeft);
        }
    } else {
        // Customize tab — single header
        drawHeader(0, lo.bottomY, lo.contentW, "CUSTOMIZE");
    }

    // ===== MAIN STRUCTURAL DIVIDERS =====
    // Horizontal separator between viewport and bottom zone
    g.setColour(juce::Colour(ct.bronze));
    g.fillRect(0, lo.bottomY, lo.contentW, 2);

    // ===== TAB BAR =====
    {
        const int ty = lo.tabBarY;
        // Tab bar background
        g.setColour(juce::Colour(ct.darkIron).darker(0.15f));
        g.fillRect(0, ty, lo.contentW, kTabBarH);

        // Separator above tab bar
        g.setColour(juce::Colour(ct.bronze).withAlpha(0.4f));
        g.drawHorizontalLine(ty, 0.0f, static_cast<float>(lo.contentW));

        // Tab labels
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        const int tabW = lo.contentW / 2;

        auto tabColor = [&](ViewTab t) {
            return activeViewTab_ == t
                ? juce::Colour(ct.brightGold)
                : juce::Colour(ct.bronze);
        };

        g.setColour(tabColor(ViewTab::Source));
        g.drawText("Source", 0, ty, tabW, kTabBarH, juce::Justification::centred);

        g.setColour(tabColor(ViewTab::Customize));
        g.drawText("Customize", tabW, ty, lo.contentW - tabW, kTabBarH, juce::Justification::centred);

        // Gold underline on active tab
        {
            const int ulH = 2;
            g.setColour(juce::Colour(ct.brightGold));
            if (activeViewTab_ == ViewTab::Source)
                g.fillRect(tabW / 4, ty + kTabBarH - ulH, tabW / 2, ulH);
            else
                g.fillRect(tabW + tabW / 4, ty + kTabBarH - ulH, (lo.contentW - tabW) / 2, ulH);
        }
    }

    // ===== PRESET BAR BACKGROUND (semi-transparent overlay at top of viewport) =====
    g.setColour(juce::Colour(ct.darkIron).withAlpha(0.75f));
    g.fillRect(0, 0, lo.contentW, kPresetBarH);
    g.setColour(juce::Colour(ct.bronze).withAlpha(0.5f));
    g.drawHorizontalLine(kPresetBarH - 1, 0.0f, static_cast<float>(lo.contentW));

    // ===== NOISE TEXTURE OVERLAY =====
    if (noiseTexture_.isValid()) {
        const int nw = noiseTexture_.getWidth();
        const int nh = noiseTexture_.getHeight();

        g.saveState();
        g.setOpacity(0.04f);

        // Bottom zone only
        g.reduceClipRegion(0, lo.bottomY, lo.contentW, lo.bottomH);
        for (int ty = lo.bottomY; ty < lo.bottomY + lo.bottomH; ty += nh)
            for (int tx = 0; tx < lo.contentW; tx += nw)
                g.drawImageAt(noiseTexture_, tx, ty);

        g.restoreState();
    }
}

// ---------------------------------------------------------------------------
// resized
// ---------------------------------------------------------------------------
void XYZPanEditor::resized()
{
    const auto lo = Layout::compute(getWidth(), getHeight());

    // Output meter strip — full height right edge
    outputMeter_.setBounds(lo.contentW, 0, kMeterW, getHeight());

    // ===== GL VIEW (full width, top zone) =====
    glView_.setBounds(0, 0, lo.contentW, lo.viewportH);

    // Preset bar overlays top of GL view (stays as editor child, z-ordered on top)
    {
        auto presetBar = juce::Rectangle<int>(0, 0, lo.contentW, kPresetBarH);
        auto saveBtnArea = presetBar.removeFromRight(60);
        auto loadBtnArea = presetBar.removeFromRight(60);
        presetCombo_.setBounds(presetBar.reduced(kPadding, 2));
        presetSaveBtn_.setBounds(saveBtnArea.reduced(2));
        presetLoadBtn_.setBounds(loadBtnArea.reduced(2));
    }

    // Snap buttons: top-right corner of GL area (children of glView_, so use local coords)
    {
        auto snapRow = juce::Rectangle<int>(0, 0, lo.contentW, lo.viewportH);
        snapRow = snapRow.removeFromTop(kSnapBtnH + 4)
                         .removeFromRight(3 * (kSnapBtnW + 4));
        snapXY_.setBounds(snapRow.removeFromLeft(kSnapBtnW + 4).reduced(2));
        snapXZ_.setBounds(snapRow.removeFromLeft(kSnapBtnW + 4).reduced(2));
        snapYZ_.setBounds(snapRow.reduced(2));
    }

    // Dev panel: child of glView_, right 30% of GL area (or user-dragged width)
    {
        const int defaultPanelW = static_cast<int>(lo.contentW * 0.30f);
        const int panelW = devPanel_.getCustomWidth() > 0
            ? juce::jlimit(200, lo.contentW - 50, devPanel_.getCustomWidth())
            : defaultPanelW;
        devPanel_.setBounds(lo.contentW - panelW, 0, panelW, lo.viewportH);
    }

    // ===== BOTTOM ZONE — 3-column layout or Customize tab =====
    if (activeViewTab_ == ViewTab::Source) {
        auto leftArea   = juce::Rectangle<int>(0,             lo.contentY, lo.leftColW,   lo.contentH);
        auto centerArea = juce::Rectangle<int>(lo.centerColX, lo.contentY, lo.centerColW, lo.contentH);
        auto rightArea  = juce::Rectangle<int>(lo.rightColX,  lo.contentY, lo.rightColW,  lo.contentH);

        layoutLeftCol(leftArea);
        layoutCenterCol(centerArea);
        layoutRightCol(rightArea);
    } else {
        auto custArea = juce::Rectangle<int>(0, lo.contentY, lo.contentW, lo.contentH);
        layoutCustomizeTab(custArea);
    }
}

// ---------------------------------------------------------------------------
// layoutLeftCol — STEREO (top half) + REVERB (bottom half)
// ---------------------------------------------------------------------------
void XYZPanEditor::layoutLeftCol(juce::Rectangle<int> area)
{
    const int pad = kPadding;
    const int knobSz = kSmallKnobSz;
    const int labelH = 13;
    const int sliderH = 20;
    const int gap = 3;

    // Split into top (stereo) and bottom (reverb) halves
    const int halfH = area.getHeight() / 2;
    auto stereoArea = area.removeFromTop(halfH);
    auto reverbArea = area;  // remaining

    // --- STEREO SECTION ---
    {
        const int sx = stereoArea.getX() + pad;
        const int sw = stereoArea.getWidth() - pad * 2;
        int sy = stereoArea.getY() + 2;

        // Stereo Width / Offset / Phase as horizontal sliders
        const int labelW = 46;
        auto placeSlider = [&](juce::Slider& slider, juce::Label& label) {
            label.setBounds(sx, sy, labelW, sliderH);
            slider.setBounds(sx + labelW, sy, sw - labelW, sliderH);
            sy += sliderH + gap;
        };
        placeSlider(stereoWidthKnob_, stereoWidthLabel_);
        placeSlider(orbitOffsetKnob_, orbitOffsetLabel_);
        placeSlider(orbitPhaseKnob_,  orbitPhaseLabel_);

        // Face Listener toggle
        faceListenerToggle_.setBounds(sx, sy, sw, 20);
        sy += 22;

        // Orbit LFO strips — stack remaining space among 3 strips
        const int lfoTop = sy;
        const int lfoAvail = stereoArea.getBottom() - lfoTop;
        const int speedRowH = 26;
        const int stripH = juce::jmax(0, (lfoAvail - speedRowH) / 3);

        orbitXYLFO_.setBounds(stereoArea.getX(), lfoTop,                stereoArea.getWidth(), stripH);
        orbitXZLFO_.setBounds(stereoArea.getX(), lfoTop + stripH,      stereoArea.getWidth(), stripH);
        orbitYZLFO_.setBounds(stereoArea.getX(), lfoTop + stripH * 2,  stereoArea.getWidth(), stripH);

        // Orbit Speed + Reset row
        const int speedY = lfoTop + stripH * 3;
        const int speedLabelW = 48;
        const int resetBtnW = 40;
        orbitSpeedMulLabel_.setBounds(sx, speedY + 2, speedLabelW, speedRowH - 4);
        orbitSpeedMulKnob_.setBounds(sx + speedLabelW, speedY + 2,
                                      sw - speedLabelW - resetBtnW - 4, speedRowH - 4);
        resetOrbitPhasesBtn_.setBounds(sx + sw - resetBtnW, speedY + 2, resetBtnW, speedRowH - 4);
    }

    // --- REVERB SECTION (below midpoint separator — skip sub-header space) ---
    {
        const int rx = reverbArea.getX() + pad;
        const int rw = reverbArea.getWidth() - pad * 2;
        int ry = reverbArea.getY() + 18;  // skip "REVERB" sub-header painted in paint()

        // 2×2 knob grid
        const int colW = rw / 2;
        const int cellH = knobSz + labelH + 2;

        auto placeKnob = [&](juce::Slider& knob, juce::Label& label, int col, int row) {
            int kx = rx + col * colW + (colW - knobSz) / 2;
            int ky = ry + row * cellH;
            knob.setBounds(kx, ky, knobSz, knobSz);
            label.setBounds(rx + col * colW, ky + knobSz, colW, labelH);
        };
        placeKnob(verbSize_,    verbSizeL_,    0, 0);
        placeKnob(verbDecay_,   verbDecayL_,   1, 0);
        placeKnob(verbDamping_, verbDampingL_, 0, 1);
        placeKnob(verbWet_,     verbWetL_,     1, 1);

        // Early Reflections toggle + Binaural toggle below reverb knobs
        const int toggleY = ry + cellH * 2 + 2;
        const int toggleH = 18;
        earlyReflToggle_.setBounds(rx, toggleY, 20, toggleH);
        earlyReflLabel_.setBounds(rx + 22, toggleY, rw - 22, toggleH);
    }
}

// ---------------------------------------------------------------------------
// layoutCenterCol — SOURCE (X/Y/Z knobs + LFOs, top) + OPTIONS (bottom)
// ---------------------------------------------------------------------------
void XYZPanEditor::layoutCenterCol(juce::Rectangle<int> area)
{
    const int pad = kPadding;
    const int knobSz = 68;  // slightly larger for hero X/Y/Z knobs
    const int labelH = 18;

    // Split into top (source) and bottom (options) halves
    const int halfH = area.getHeight() / 2;
    auto sourceArea = area.removeFromTop(halfH);
    auto optionsArea = area;

    // --- SOURCE: X / Y / Z knobs in a row ---
    {
        const int colW = sourceArea.getWidth() / 3;
        int sy = sourceArea.getY() + 2;

        auto layoutKnobCol = [&](juce::Slider& knob, juce::Label& label, LFOStrip& lfo,
                                  int colIdx) {
            int cx = sourceArea.getX() + colIdx * colW;
            label.setBounds(cx, sy, colW, labelH);
            int kx = cx + (colW - knobSz) / 2;
            knob.setBounds(kx, sy + labelH, knobSz, knobSz);

            // LFO strip fills remaining space below knob
            int lfoTop = sy + labelH + knobSz + 2;
            int lfoH = juce::jmax(0, sourceArea.getBottom() - lfoTop - 26);  // reserve speed row
            lfo.setBounds(cx, lfoTop, colW, lfoH);
        };

        layoutKnobCol(xKnob_, xLabel_, xLFO_, 0);
        layoutKnobCol(yKnob_, yLabel_, yLFO_, 1);
        layoutKnobCol(zKnob_, zLabel_, zLFO_, 2);

        // LFO Speed + Reset row at bottom of source area
        const int speedRowH = 24;
        const int speedY = sourceArea.getBottom() - speedRowH;
        const int speedLabelW = 56;
        const int resetBtnW = 40;
        const int sx = sourceArea.getX() + pad;
        const int sw = sourceArea.getWidth() - pad * 2;
        lfoSpeedMulLabel_.setBounds(sx, speedY + 2, speedLabelW, speedRowH - 4);
        lfoSpeedMulKnob_.setBounds(sx + speedLabelW, speedY + 2,
                                    sw - speedLabelW - resetBtnW - 4, speedRowH - 4);
        resetXYZPhasesBtn_.setBounds(sx + sw - resetBtnW, speedY + 2, resetBtnW, speedRowH - 4);
    }

    // --- OPTIONS (below midpoint) ---
    {
        const int ox = optionsArea.getX() + pad;
        const int ow = optionsArea.getWidth() - pad * 2;
        int oy = optionsArea.getY() + 18;  // skip "OPTIONS" sub-header

        const int optKnobSz = kSmallKnobSz;
        const int optLabelH = 13;
        const int colW = ow / 3;

        // Sphere Radius | Doppler | Input Gain — three knobs in a row
        auto placeOptKnob = [&](juce::Slider& knob, juce::Label& label, int col) {
            int kx = ox + col * colW + (colW - optKnobSz) / 2;
            knob.setBounds(kx, oy, optKnobSz, optKnobSz);
            label.setBounds(ox + col * colW, oy + optKnobSz, colW, optLabelH);
        };
        placeOptKnob(sphereRadiusKnob_, sphereRadiusLabel_, 0);
        placeOptKnob(dopplerKnob_,      dopplerLabel_,      1);
        placeOptKnob(inputGainKnob_,    inputGainLabel_,    2);

        // Doppler sub-label
        dopplerSubLabel_.setBounds(ox + colW, oy + optKnobSz + optLabelH, colW, 10);

        // Binaural toggle below knobs
        const int cbY = oy + optKnobSz + optLabelH + 14;
        binauralToggle_.setBounds(ox, cbY, 20, 18);
        binauralLabel_.setBounds(ox + 22, cbY, 80, 18);

        // DEV toggle
        devToggle_.setBounds(ox + ow - 48, cbY, 48, 18);
    }
}

// ---------------------------------------------------------------------------
// layoutRightCol — LISTENER (Walker + Yaw/Pitch/Roll + toggles)
// ---------------------------------------------------------------------------
void XYZPanEditor::layoutRightCol(juce::Rectangle<int> area)
{
    const int pad = kPadding;
    const int knobSz = 52;
    const int labelH = 13;
    const int toggleH = 18;

    const int rx = area.getX() + pad;
    const int rw = area.getWidth() - pad * 2;
    int ry = area.getY() + 2;

    // Walker X / Y / Z knobs — top row
    {
        const int colW = rw / 3;
        auto placeKnob = [&](juce::Slider& knob, juce::Label& label, int col) {
            int kx = rx + col * colW + (colW - knobSz) / 2;
            knob.setBounds(kx, ry, knobSz, knobSz);
            label.setBounds(rx + col * colW, ry + knobSz, colW, labelH);
        };
        placeKnob(walkerXKnob_, walkerXLabel_, 0);
        placeKnob(walkerYKnob_, walkerYLabel_, 1);
        placeKnob(walkerZKnob_, walkerZLabel_, 2);
    }
    ry += knobSz + labelH + 4;

    // Yaw / Pitch / Roll knobs — second row
    {
        const int colW = rw / 3;
        auto placeKnob = [&](juce::Slider& knob, juce::Label& label, int col) {
            int kx = rx + col * colW + (colW - knobSz) / 2;
            knob.setBounds(kx, ry, knobSz, knobSz);
            label.setBounds(rx + col * colW, ry + knobSz, colW, labelH);
        };
        placeKnob(listenerYawKnob_,   listenerYawLabel_,   0);
        placeKnob(listenerPitchKnob_, listenerPitchLabel_, 1);
        placeKnob(listenerRollKnob_,  listenerRollLabel_,  2);
    }
    ry += knobSz + labelH + 6;

    // Toggles — 2 per row
    const int halfW = (rw - pad) / 2;
    wasdToggle_.setBounds(rx, ry, halfW, toggleH);
    headFollowsToggle_.setBounds(rx + halfW + pad, ry, halfW, toggleH);
    ry += toggleH + 3;

    listenerLinkToggle_.setBounds(rx, ry, halfW, toggleH);
    listenerPilotToggle_.setBounds(rx + halfW + pad, ry, halfW, toggleH);
    ry += toggleH + 3;

    // Remote button + pilot status
    remoteBtn_.setBounds(rx, ry, halfW, toggleH);
    pilotStatusLabel_.setBounds(rx + halfW + pad, ry, halfW, toggleH);
}

// ---------------------------------------------------------------------------
// layoutCustomizeTab — full-width customize/avatar panel
// ---------------------------------------------------------------------------
void XYZPanEditor::layoutCustomizeTab(juce::Rectangle<int> area)
{
    const int pad = 6;
    const int sliderH  = 20;
    const int comboH   = 22;
    const int labelW   = 68;
    const int gap      = 4;
    const int headerH  = 16;

    // Viewport fills the entire content area
    customizeViewport_.setBounds(area);

    // Content width matches viewport (use area width for layout)
    const int cw = area.getWidth();
    int cy = 2;

    // Theme label + combo
    themeLabel_.setBounds(pad, cy, labelW, comboH);
    themeCombo_.setBounds(pad + labelW, cy, cw - pad * 2 - labelW, comboH);
    cy += comboH + gap;

    auto placeAvatarSlider = [&](juce::Slider& slider, juce::Label& label) {
        label.setBounds(pad, cy, labelW, sliderH);
        slider.setBounds(pad + labelW, cy, cw - pad * 2 - labelW, sliderH);
        cy += sliderH + gap;
    };

    auto placeColorSwatch = [&](ColorSwatch& swatch, juce::Label& label) {
        label.setBounds(pad, cy, labelW, sliderH);
        swatch.setBounds(pad + labelW, cy, sliderH, sliderH);
        cy += sliderH + gap;
    };

    placeColorSwatch(headColorSwatch_,  headColorLabel_);
    placeAvatarSlider(headSizeSlider_,       headSizeLabel_);
    placeAvatarSlider(headElongationSlider_, headElongationLabel_);

    eyesSectionHeaderY_ = cy;
    cy += headerH + gap;

    eyeTypeLabel_.setBounds(pad, cy, labelW, comboH);
    eyeTypeCombo_.setBounds(pad + labelW, cy, cw - pad * 2 - labelW, comboH);
    cy += comboH + gap;

    placeAvatarSlider(eyeSizeSlider_,        eyeSizeLabel_);
    placeAvatarSlider(eyeSpacingSlider_,     eyeSpacingLabel_);
    placeAvatarSlider(pupilSizeSlider_,      pupilSizeLabel_);
    placeAvatarSlider(googlySlider_,          googlyLabel_);
    placeColorSwatch(eyeColorSwatch_,   eyeColorLabel_);

    earsSectionHeaderY_ = cy;
    cy += headerH + gap;

    earTypeLabel_.setBounds(pad, cy, labelW, comboH);
    earTypeCombo_.setBounds(pad + labelW, cy, cw - pad * 2 - labelW, comboH);
    cy += comboH + gap;

    placeAvatarSlider(earSizeSlider_,        earSizeLabel_);
    placeAvatarSlider(earRotationSlider_,    earRotationLabel_);

    noseSectionHeaderY_ = cy;
    cy += headerH + gap;

    noseTypeLabel_.setBounds(pad, cy, labelW, comboH);
    noseTypeCombo_.setBounds(pad + labelW, cy, cw - pad * 2 - labelW, comboH);
    cy += comboH + gap;

    placeAvatarSlider(noseSizeSlider_, noseSizeLabel_);
    placeColorSwatch(noseColorSwatch_,  noseColorLabel_);

    hatsSectionHeaderY_ = cy;
    cy += headerH + gap;

    hatTypeLabel_.setBounds(pad, cy, labelW, comboH);
    hatTypeCombo_.setBounds(pad + labelW, cy, cw - pad * 2 - labelW, comboH);
    cy += comboH + gap;

    placeAvatarSlider(hatSizeSlider_, hatSizeLabel_);
    placeColorSwatch(hatColorSwatch_,   hatColorLabel_);

    customizeContent_.setSize(cw, cy + 2);
}

// ---------------------------------------------------------------------------
// updateSnapButtonStates — set toggle state so AlchemyLookAndFeel renders
// the active snap button with gold border and inactive ones with bronze.
// ---------------------------------------------------------------------------
void XYZPanEditor::updateSnapButtonStates()
{
    snapXY_.setToggleState(currentSnap_ == SnapState::TopDown, juce::dontSendNotification);
    snapXZ_.setToggleState(currentSnap_ == SnapState::Side,    juce::dontSendNotification);
    snapYZ_.setToggleState(currentSnap_ == SnapState::Front,   juce::dontSendNotification);
}

// ---------------------------------------------------------------------------
// updateOrbitEnabled — grey out orbit controls when Width is 0
// ---------------------------------------------------------------------------
void XYZPanEditor::updateOrbitEnabled()
{
    const bool active = stereoWidthKnob_.getValue() > 0.0;

    for (auto* c : {(juce::Component*)&orbitSpeedMulKnob_,  (juce::Component*)&orbitSpeedMulLabel_,
                    (juce::Component*)&orbitOffsetKnob_,     (juce::Component*)&orbitOffsetLabel_,
                    (juce::Component*)&orbitPhaseKnob_,      (juce::Component*)&orbitPhaseLabel_,
                    (juce::Component*)&faceListenerToggle_,
                    (juce::Component*)&resetOrbitPhasesBtn_})
        c->setEnabled(active);

    for (auto* lfo : {(juce::Component*)&orbitXYLFO_, (juce::Component*)&orbitXZLFO_,
                      (juce::Component*)&orbitYZLFO_})
        lfo->setEnabled(active);
}

// ---------------------------------------------------------------------------
// applyCurrentTheme — push current theme to LookAndFeel + GL view + repaint
// ---------------------------------------------------------------------------
void XYZPanEditor::applyCurrentTheme()
{
    const auto& theme = userPrefs_->activeTheme();
    lookAndFeel_.applyTheme(theme);
    glView_.setColorTheme(theme);

    // Re-apply per-axis knob colors that are set explicitly (not from theme)
    xKnob_.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(xyzpan::AlchemyLookAndFeel::kCinnabar));
    yKnob_.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(xyzpan::AlchemyLookAndFeel::kAqua));
    zKnob_.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(xyzpan::AlchemyLookAndFeel::kGoldLeaf));

    // Re-apply hero slider styling (brightened LFO accent)
    for (auto* knob : {&stereoWidthKnob_, &orbitSpeedMulKnob_, &orbitOffsetKnob_,
                        &orbitPhaseKnob_, &lfoSpeedMulKnob_})
        knob->setColour(juce::Slider::rotarySliderFillColourId,
                         juce::Colour(theme.lfoAccentBright));

    // Re-apply listener / reverb / orbit knobs to LFO accent
    for (auto* knob : {&sphereRadiusKnob_, &dopplerKnob_, &inputGainKnob_,
                        &listenerYawKnob_, &listenerPitchKnob_, &listenerRollKnob_,
                        &verbSize_, &verbDecay_, &verbDamping_, &verbWet_})
        knob->setColour(juce::Slider::rotarySliderFillColourId,
                         juce::Colour(theme.lfoAccent));

    // Walker knobs — aqua accent
    for (auto* knob : {&walkerXKnob_, &walkerYKnob_, &walkerZKnob_})
        knob->setColour(juce::Slider::rotarySliderFillColourId,
                         juce::Colour(xyzpan::AlchemyLookAndFeel::kAqua));

    // Update hero label colors to match theme
    for (auto* lbl : {&xLabel_, &yLabel_, &zLabel_})
        lbl->setColour(juce::Label::textColourId, juce::Colour(theme.goldLeafPale));

    dopplerSubLabel_.setColour(juce::Label::textColourId,
                               juce::Colour(theme.agedPapyrusDark).withAlpha(0.6f));

    // Update color swatches that use theme defaults (avatar color == 0)
    const auto& avp = userPrefs_->avatarParams();
    auto toJuceCol = [](const glm::vec3& v) {
        return juce::Colour::fromFloatRGBA(v.r, v.g, v.b, 1.0f);
    };
    if (!avp.headColor)  headColorSwatch_.setColour(toJuceCol(theme.glListenerHead));
    if (!avp.noseColor)  noseColorSwatch_.setColour(toJuceCol(theme.glNose));
    if (!avp.hatColor)   hatColorSwatch_.setColour(toJuceCol(theme.glHat));
    if (!avp.eyeColor)   eyeColorSwatch_.setColour(toJuceCol(theme.glEyeWhite));

    repaint();
}

// ---------------------------------------------------------------------------
// pushAvatarToGL — send current avatar params to GL view
// ---------------------------------------------------------------------------
void XYZPanEditor::pushAvatarToGL()
{
    glView_.setAvatarParams(userPrefs_->avatarParams());
}

void XYZPanEditor::syncEyeSpacingSliderMode(bool isCyclops)
{
    const auto& ap = userPrefs_->avatarParams();
    if (isCyclops)
    {
        eyeSpacingLabel_.setText("Eye Height", juce::dontSendNotification);
        eyeSpacingSlider_.setRange(0.0, 1.0, 0.0);
        eyeSpacingSlider_.setValue(static_cast<double>(ap.eyeHeight), juce::dontSendNotification);
        eyeSpacingSlider_.setEnabled(true);
    }
    else
    {
        eyeSpacingLabel_.setText("Eye Space", juce::dontSendNotification);
        eyeSpacingSlider_.setRange(0.2, 3.0, 0.0);
        eyeSpacingSlider_.setValue(static_cast<double>(ap.eyeSpacing), juce::dontSendNotification);
        eyeSpacingSlider_.setEnabled(true);
    }
}

// ---------------------------------------------------------------------------
// setActiveViewTab — switch between Source (3-column) and Customize views
// ---------------------------------------------------------------------------
void XYZPanEditor::setActiveViewTab(ViewTab tab)
{
    activeViewTab_ = tab;
    const bool source    = (tab == ViewTab::Source);
    const bool customize = (tab == ViewTab::Customize);

    // Source-tab components (3-column layout)
    // Left column: stereo + reverb
    stereoWidthKnob_.setVisible(source);
    stereoWidthLabel_.setVisible(source);
    orbitOffsetKnob_.setVisible(source);
    orbitOffsetLabel_.setVisible(source);
    orbitPhaseKnob_.setVisible(source);
    orbitPhaseLabel_.setVisible(source);
    faceListenerToggle_.setVisible(source);
    orbitXYLFO_.setVisible(source);
    orbitXZLFO_.setVisible(source);
    orbitYZLFO_.setVisible(source);
    orbitSpeedMulKnob_.setVisible(source);
    orbitSpeedMulLabel_.setVisible(source);
    resetOrbitPhasesBtn_.setVisible(source);
    verbSize_.setVisible(source);
    verbSizeL_.setVisible(source);
    verbDecay_.setVisible(source);
    verbDecayL_.setVisible(source);
    verbDamping_.setVisible(source);
    verbDampingL_.setVisible(source);
    verbWet_.setVisible(source);
    verbWetL_.setVisible(source);
    earlyReflToggle_.setVisible(source);
    earlyReflLabel_.setVisible(source);

    // Center column: source + options
    xKnob_.setVisible(source);
    xLabel_.setVisible(source);
    yKnob_.setVisible(source);
    yLabel_.setVisible(source);
    zKnob_.setVisible(source);
    zLabel_.setVisible(source);
    xLFO_.setVisible(source);
    yLFO_.setVisible(source);
    zLFO_.setVisible(source);
    lfoSpeedMulKnob_.setVisible(source);
    lfoSpeedMulLabel_.setVisible(source);
    resetXYZPhasesBtn_.setVisible(source);
    sphereRadiusKnob_.setVisible(source);
    sphereRadiusLabel_.setVisible(source);
    dopplerKnob_.setVisible(source);
    dopplerLabel_.setVisible(source);
    dopplerSubLabel_.setVisible(source);
    inputGainKnob_.setVisible(source);
    inputGainLabel_.setVisible(source);
    binauralToggle_.setVisible(source);
    binauralLabel_.setVisible(source);
    devToggle_.setVisible(source);

    // Right column: listener
    walkerXKnob_.setVisible(source);
    walkerXLabel_.setVisible(source);
    walkerYKnob_.setVisible(source);
    walkerYLabel_.setVisible(source);
    walkerZKnob_.setVisible(source);
    walkerZLabel_.setVisible(source);
    listenerYawKnob_.setVisible(source);
    listenerYawLabel_.setVisible(source);
    listenerPitchKnob_.setVisible(source);
    listenerPitchLabel_.setVisible(source);
    listenerRollKnob_.setVisible(source);
    listenerRollLabel_.setVisible(source);
    wasdToggle_.setVisible(source);
    headFollowsToggle_.setVisible(source);
    listenerLinkToggle_.setVisible(source);
    listenerPilotToggle_.setVisible(source);
    pilotStatusLabel_.setVisible(source);
    remoteBtn_.setVisible(source);

    // Customize-only components
    customizeViewport_.setVisible(customize);

    if (getWidth() > 0 && getHeight() > 0) {
        resized();
        repaint();
    }
}

void XYZPanEditor::updateListenerControlsEnabled() {
    const bool linked   = proc_.isLinkedPilot() || proc_.isLinkedNonPilot();
    const bool isPilot  = proc_.isLinkedPilot();
    const bool canControl = !linked || isPilot;

    // Pilot toggle only meaningful when linked
    listenerPilotToggle_.setEnabled(linked);

    // Sync toggle state from hub truth (covers cross-instance pilot revocation)
    if (listenerPilotToggle_.getToggleState() != isPilot)
        listenerPilotToggle_.setToggleState(isPilot, juce::dontSendNotification);

    // Pilot status label — show who the pilot is when this instance isn't it
    if (linked && !isPilot) {
        auto name = proc_.getPilotName();
        if (name.isNotEmpty())
            pilotStatusLabel_.setText(name + " is the pilot", juce::dontSendNotification);
        else
            pilotStatusLabel_.setText("No pilot set", juce::dontSendNotification);
        pilotStatusLabel_.setVisible(activeViewTab_ == ViewTab::Source);
    } else {
        pilotStatusLabel_.setText("", juce::dontSendNotification);
        pilotStatusLabel_.setVisible(false);
    }

    // Walker + orientation knobs: disabled when linked-non-pilot
    for (auto* knob : {&walkerXKnob_, &walkerYKnob_, &walkerZKnob_,
                       &listenerYawKnob_, &listenerPitchKnob_, &listenerRollKnob_})
        knob->setEnabled(canControl);
    wasdToggle_.setEnabled(canControl);
    headFollowsToggle_.setEnabled(canControl);
}

// Source/Customize tab switching handled by setActiveViewTab().

// ---------------------------------------------------------------------------
// mouseDown — tab bar click detection + last-clicked zone tracking
// ---------------------------------------------------------------------------
void XYZPanEditor::mouseDown(const juce::MouseEvent& e)
{
    // Convert to editor-local coords (child components report relative to themselves)
    const auto pos = e.getEventRelativeTo(this).getPosition();

    // Track last-clicked zone for Alt+R randomization
    auto zone = classifyRandZone(pos);
    if (zone != RandZone::None)
        lastClickedZone_ = zone;

    const auto lo = Layout::compute(getWidth(), getHeight());

    // Hit-test tab bar at bottom — "Source" | "Customize"
    if (pos.y >= lo.tabBarY && pos.y < lo.tabBarY + kTabBarH && pos.x < lo.contentW) {
        const int halfW = lo.contentW / 2;
        if (pos.x < halfW)
            setActiveViewTab(ViewTab::Source);
        else
            setActiveViewTab(ViewTab::Customize);
        return;
    }

    // Instance list click handling is now in XYZPanGLView::mouseDown

    AudioProcessorEditor::mouseDown(e);
}

// ---------------------------------------------------------------------------
// generateNoiseTexture — static 256x256 greyscale procedural noise
// ---------------------------------------------------------------------------
juce::Image XYZPanEditor::generateNoiseTexture(int size)
{
    juce::Image img(juce::Image::ARGB, size, size, true);
    juce::Image::BitmapData bmp(img, juce::Image::BitmapData::writeOnly);

    std::mt19937 rng(42);  // fixed seed for deterministic noise
    std::uniform_int_distribution<int> dist(0, 255);

    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            bmp.setPixelColour(x, y, juce::Colour::fromRGBA(
                static_cast<uint8_t>(dist(rng)),
                static_cast<uint8_t>(dist(rng)),
                static_cast<uint8_t>(dist(rng)),
                255));

    return img;
}

// ---------------------------------------------------------------------------
// keyPressed — WASD movement + Alt+R randomization
// ---------------------------------------------------------------------------
bool XYZPanEditor::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    // Consume WASD keys when WASD control is active (actual movement in timerCallback)
    if (wasdToggle_.getToggleState()) {
        const int k = key.getKeyCode();
        if (k == 'W' || k == 'w' || k == 'A' || k == 'a' ||
            k == 'S' || k == 's' || k == 'D' || k == 'd')
            return true;
    }

    if (key.getModifiers().isAltDown() && key.getKeyCode() == 'R') {
        // Use last-clicked zone; fall back to hover if nothing was clicked yet
        auto zone = lastClickedZone_;
        if (zone == RandZone::None)
            zone = classifyRandZone(getMouseXYRelative());
        switch (zone) {
            case RandZone::Position:     randomizePosition();            break;
            case RandZone::StereoOrbit:  randomizeStereoOrbit();         break;
            case RandZone::Reverb:       randomizeReverb();              break;
            case RandZone::Perspective:  randomizePerspective();         break;
            case RandZone::CustColors:   randomizeCustColors();          break;
            case RandZone::CustEyes:     randomizeCustEyes();            break;
            case RandZone::CustEars:     randomizeCustEars();            break;
            case RandZone::CustNose:     randomizeCustNose();            break;
            case RandZone::CustHats:     randomizeCustHats();            break;
            case RandZone::LfoX:         randomizeLFOStrip("lfo_x");    break;
            case RandZone::LfoY:         randomizeLFOStrip("lfo_y");    break;
            case RandZone::LfoZ:         randomizeLFOStrip("lfo_z");    break;
            case RandZone::OrbitLfoXY:   randomizeLFOStrip("stereo_orbit_xy"); break;
            case RandZone::OrbitLfoXZ:   randomizeLFOStrip("stereo_orbit_xz"); break;
            case RandZone::OrbitLfoYZ:   randomizeLFOStrip("stereo_orbit_yz"); break;
            case RandZone::None:         break;
        }
        return zone != RandZone::None;
    }
    return false;
}

// ---------------------------------------------------------------------------
// endWasdGestureIfActive — close automation gesture on walker params
// ---------------------------------------------------------------------------
void XYZPanEditor::endWasdGestureIfActive()
{
    if (!wasdGestureActive_) return;
    if (auto* px = proc_.apvts.getParameter(ParamID::WALKER_X)) px->endChangeGesture();
    if (auto* py = proc_.apvts.getParameter(ParamID::WALKER_Y)) py->endChangeGesture();
    if (auto* pz = proc_.apvts.getParameter(ParamID::WALKER_Z)) pz->endChangeGesture();
    wasdGestureActive_ = false;
}

// ---------------------------------------------------------------------------
// timerCallback — WASD movement: move walker in the direction the head is looking
// ---------------------------------------------------------------------------
void XYZPanEditor::timerCallback()
{
    // Validate remote focus — hubAlive_ check is lock-free (every frame),
    // full linked-index check is throttled to ~6Hz to avoid unnecessary spinlock traffic.
    if (remoteFocusProc_ != nullptr) {
        auto* remoteListener = static_cast<SharedListenerHub::Listener*>(remoteFocusProc_);
        if (!remoteListener->hubAlive_.load(std::memory_order_acquire)) {
            setRemoteFocus(-1);
        } else if (++remoteFocusValidationCounter_ >= 10) {
            remoteFocusValidationCounter_ = 0;
            if (proc_.getListenerHub().getLinkedIndex(remoteListener) < 0)
                setRemoteFocus(-1);
        }
    }

    // Rebuild instance list at ~6Hz (every 10th frame) — linked count
    // rarely changes and the removal callback handles urgent detach.
    if (++selectorRebuildCounter_ >= 10) {
        selectorRebuildCounter_ = 0;
        rebuildInstanceList();
        updateListenerControlsEnabled();
        // Keep GL view instance name in sync (may change from DAW track rename)
        glView_.setOwnInstanceName(proc_.getInstanceNameValue());
    }

    if (!wasdToggle_.getToggleState()) {
        endWasdGestureIfActive();
        return;
    }

    // Only process WASD when this plugin window actually has keyboard focus
    if (!hasKeyboardFocus(true)) {
        endWasdGestureIfActive();
        return;
    }

    // Check which WASD keys are currently held
    const bool w = juce::KeyPress::isKeyCurrentlyDown('W') || juce::KeyPress::isKeyCurrentlyDown('w');
    const bool a = juce::KeyPress::isKeyCurrentlyDown('A') || juce::KeyPress::isKeyCurrentlyDown('a');
    const bool s = juce::KeyPress::isKeyCurrentlyDown('S') || juce::KeyPress::isKeyCurrentlyDown('s');
    const bool d = juce::KeyPress::isKeyCurrentlyDown('D') || juce::KeyPress::isKeyCurrentlyDown('d');

    if (!w && !a && !s && !d) {
        endWasdGestureIfActive();
        return;
    }

    // Build local-space input: fwd = forward/back, strafe = left/right
    float fwd = 0.0f, strafe = 0.0f;
    if (w) fwd    += 1.0f;
    if (s) fwd    -= 1.0f;
    if (d) strafe += 1.0f;
    if (a) strafe -= 1.0f;

    // Read head orientation (degrees → radians)
    // Engine coordinate system: yaw rotates in X/Y plane, pitch tilts into Z
    constexpr float kDeg2Rad = 3.14159265358979323846f / 180.0f;
    const float yaw   = proc_.apvts.getRawParameterValue(ParamID::LISTENER_YAW)->load()   * kDeg2Rad;
    const float pitch = proc_.apvts.getRawParameterValue(ParamID::LISTENER_PITCH)->load() * kDeg2Rad;

    // Forward vector from yaw + pitch (engine: yaw in XY plane, pitch tilts Z)
    const float cosY = std::cos(yaw);
    const float sinY = std::sin(yaw);
    const float cosP = std::cos(pitch);
    const float sinP = std::sin(pitch);

    // Forward direction the head is looking
    const float fwdX = -sinY * cosP;
    const float fwdY =  cosY * cosP;
    const float fwdZ =  sinP;

    // Right vector: perpendicular to forward in the horizontal (XY) plane
    const float rightX =  cosY;
    const float rightY =  sinY;
    // rightZ = 0 (strafe stays horizontal)

    // Combine into world-space delta
    const float dx = fwd * fwdX + strafe * rightX;
    const float dy = fwd * fwdY + strafe * rightY;
    const float dz = fwd * fwdZ;

    // Movement speed: ~0.5 units/sec at 60 Hz → 0.008 per tick
    constexpr float speed = 0.008f;

    auto* px = proc_.apvts.getParameter(ParamID::WALKER_X);
    auto* py = proc_.apvts.getParameter(ParamID::WALKER_Y);
    auto* pz = proc_.apvts.getParameter(ParamID::WALKER_Z);
    if (px == nullptr || py == nullptr || pz == nullptr) return;

    const float newX = juce::jlimit(-1.0f, 1.0f, px->convertFrom0to1(px->getValue()) + dx * speed);
    const float newY = juce::jlimit(-1.0f, 1.0f, py->convertFrom0to1(py->getValue()) + dy * speed);
    const float newZ = juce::jlimit(-1.0f, 1.0f, pz->convertFrom0to1(pz->getValue()) + dz * speed);

    if (!wasdGestureActive_) {
        px->beginChangeGesture();
        py->beginChangeGesture();
        pz->beginChangeGesture();
        wasdGestureActive_ = true;
    }

    px->setValueNotifyingHost(px->convertTo0to1(newX));
    py->setValueNotifyingHost(py->convertTo0to1(newY));
    pz->setValueNotifyingHost(pz->convertTo0to1(newZ));
}

// ---------------------------------------------------------------------------
// classifyRandZone — determine which UI module a point falls in
// ---------------------------------------------------------------------------
XYZPanEditor::RandZone XYZPanEditor::classifyRandZone(juce::Point<int> pos) const
{
    const auto lo = Layout::compute(getWidth(), getHeight());
    const int x = pos.x;
    const int y = pos.y;

    // Only bottom zone has randomizable regions
    if (y < lo.bottomY || y >= lo.tabBarY)
        return RandZone::None;

    if (activeViewTab_ == ViewTab::Customize) {
        // Customize tab — classify by section
        int localY = y - customizeViewport_.getY() + customizeViewport_.getViewPositionY();
        if (localY < eyesSectionHeaderY_)
            return RandZone::CustColors;
        if (localY < earsSectionHeaderY_)
            return RandZone::CustEyes;
        if (localY < noseSectionHeaderY_)
            return RandZone::CustEars;
        if (localY < hatsSectionHeaderY_)
            return RandZone::CustNose;
        return RandZone::CustHats;
    }

    // Source tab — classify by column
    if (x < lo.centerColX) {
        // Left column: stereo or reverb
        const int midY = lo.contentY + lo.contentH / 2;
        if (y < midY) {
            // Check orbit LFO strips
            auto inBounds = [&](const LFOStrip& strip) {
                return strip.getBounds().contains(x, y);
            };
            if (inBounds(orbitXYLFO_)) return RandZone::OrbitLfoXY;
            if (inBounds(orbitXZLFO_)) return RandZone::OrbitLfoXZ;
            if (inBounds(orbitYZLFO_)) return RandZone::OrbitLfoYZ;
            return RandZone::StereoOrbit;
        }
        return RandZone::Reverb;
    }

    if (x < lo.rightColX) {
        // Center column: source or options
        const int midY = lo.contentY + lo.contentH / 2;
        if (y < midY) {
            // Check position LFO strips
            auto inBounds = [&](const LFOStrip& strip) {
                return strip.getBounds().contains(x, y);
            };
            if (inBounds(xLFO_)) return RandZone::LfoX;
            if (inBounds(yLFO_)) return RandZone::LfoY;
            if (inBounds(zLFO_)) return RandZone::LfoZ;
            return RandZone::Position;
        }
        return RandZone::Position;  // options area is still position-adjacent
    }

    // Right column: listener
    return RandZone::Perspective;
}

// ---------------------------------------------------------------------------
// randomizeAPVTSParam — set a single APVTS parameter to a random value
// ---------------------------------------------------------------------------
void XYZPanEditor::randomizeAPVTSParam(const char* paramID, float min, float max)
{
    auto* param = proc_.apvts.getParameter(paramID);
    if (!param) return;

    std::uniform_real_distribution<float> dist(min, max);
    float value = dist(rng_);
    float norm = param->convertTo0to1(value);

    param->beginChangeGesture();
    param->setValueNotifyingHost(norm);
    param->endChangeGesture();
}

// ---------------------------------------------------------------------------
// randomizeLFOStrip — randomize all params for one LFO strip (rate, depth,
//                     phase, smooth, waveform, beat_div, tempo_sync)
// ---------------------------------------------------------------------------
void XYZPanEditor::randomizeLFOStrip(const juce::String& prefix)
{
    randomizeAPVTSParam((prefix + "_rate").toRawUTF8(),   xyzpan::kLFOMinRate, xyzpan::kLFOMaxRate);
    randomizeAPVTSParam((prefix + "_depth").toRawUTF8(),  0.0f, 1.0f);
    randomizeAPVTSParam((prefix + "_phase").toRawUTF8(),  0.0f, 1.0f);
    randomizeAPVTSParam((prefix + "_smooth").toRawUTF8(), 0.0f, 1.0f);

    // Waveform: integer 0..5
    randomizeAPVTSParam((prefix + "_waveform").toRawUTF8(), 0.0f, 5.0f);

    // Beat division: integer 0..kBeatDivCount-1
    randomizeAPVTSParam((prefix + "_beat_div").toRawUTF8(),
                        0.0f, static_cast<float>(xyzpan::kBeatDivCount - 1));

    // Tempo sync: coin flip
    auto syncID = prefix + "_tempo_sync";
    auto* syncParam = proc_.apvts.getParameter(syncID);
    if (syncParam) {
        std::uniform_int_distribution<int> coin(0, 1);
        syncParam->beginChangeGesture();
        syncParam->setValueNotifyingHost(coin(rng_) ? 1.0f : 0.0f);
        syncParam->endChangeGesture();
    }
}

// ---------------------------------------------------------------------------
// randomizePosition — X, Y, Z knobs only
// ---------------------------------------------------------------------------
void XYZPanEditor::randomizePosition()
{
    randomizeAPVTSParam(ParamID::X, -1.0f, 1.0f);
    randomizeAPVTSParam(ParamID::Y, -1.0f, 1.0f);
    randomizeAPVTSParam(ParamID::Z, -1.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// randomizeStereoOrbit — Width, Offset, Phase + Face Listener toggle
// ---------------------------------------------------------------------------
void XYZPanEditor::randomizeStereoOrbit()
{
    randomizeAPVTSParam(ParamID::STEREO_WIDTH,        0.0f, 1.0f);
    randomizeAPVTSParam(ParamID::STEREO_ORBIT_OFFSET, 0.0f, 1.0f);
    randomizeAPVTSParam(ParamID::STEREO_ORBIT_PHASE,  0.0f, 1.0f);

    // Randomize Face Listener bool
    auto* flParam = proc_.apvts.getParameter(ParamID::STEREO_FACE_LISTENER);
    if (flParam) {
        std::uniform_int_distribution<int> coin(0, 1);
        flParam->beginChangeGesture();
        flParam->setValueNotifyingHost(coin(rng_) ? 1.0f : 0.0f);
        flParam->endChangeGesture();
    }

    updateOrbitEnabled();
}

// ---------------------------------------------------------------------------
// randomizeReverb — Size, Decay, Damping, Wet
// ---------------------------------------------------------------------------
void XYZPanEditor::randomizeReverb()
{
    randomizeAPVTSParam(ParamID::VERB_SIZE,    0.0f, 1.0f);
    randomizeAPVTSParam(ParamID::VERB_DECAY,   0.0f, 1.0f);
    randomizeAPVTSParam(ParamID::VERB_DAMPING, 0.0f, 1.0f);
    randomizeAPVTSParam(ParamID::VERB_WET,     0.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// randomizePerspective — Yaw, Pitch, Roll
// ---------------------------------------------------------------------------
void XYZPanEditor::randomizePerspective()
{
    randomizeAPVTSParam(ParamID::LISTENER_YAW,   0.0f, 360.0f);
    randomizeAPVTSParam(ParamID::LISTENER_PITCH,  0.0f, 360.0f);
    randomizeAPVTSParam(ParamID::LISTENER_ROLL,   0.0f, 360.0f);
}

// ---------------------------------------------------------------------------
// randomizeCustColors — Head color + Head Size + Elongation
// ---------------------------------------------------------------------------
void XYZPanEditor::randomizeCustColors()
{
    auto ap = userPrefs_->avatarParams();
    std::uniform_int_distribution<uint32_t> colorDist(0x00000000u, 0x00FFFFFFu);

    ap.headColor = 0xFF000000u | colorDist(rng_);

    std::uniform_real_distribution<float> headSizeDist(0.7f, 1.5f);
    std::uniform_real_distribution<float> elongDist(0.3f, 2.5f);
    ap.headSize       = headSizeDist(rng_);
    ap.headElongation = elongDist(rng_);

    userPrefs_->setAvatarParams(ap);
    pushAvatarToGL();

    // Sync UI controls
    headColorSwatch_.setColour(juce::Colour(ap.headColor));
    headSizeSlider_.setValue(static_cast<double>(ap.headSize), juce::dontSendNotification);
    headElongationSlider_.setValue(static_cast<double>(ap.headElongation), juce::dontSendNotification);
}

// ---------------------------------------------------------------------------
// randomizeCustEyes — eyeColor, eyeType, eyeSize, eyeSpacing,
//                     pupilSize, googly
// ---------------------------------------------------------------------------
void XYZPanEditor::randomizeCustEyes()
{
    auto ap = userPrefs_->avatarParams();

    std::uniform_int_distribution<uint32_t> colorDist(0x00000000u, 0x00FFFFFFu);
    std::uniform_int_distribution<int> eyeTypeDist(0, 4);
    std::uniform_real_distribution<float> sizeDist(0.2f, 3.0f);
    std::uniform_real_distribution<float> unit01(0.0f, 1.0f);

    ap.eyeColor      = 0xFF000000u | colorDist(rng_);
    ap.eyeType       = eyeTypeDist(rng_);
    ap.eyeSize       = sizeDist(rng_);
    ap.eyeSpacing    = sizeDist(rng_);
    ap.eyeHeight     = unit01(rng_);
    ap.pupilSize     = unit01(rng_);

    // Randomize googly via the combined slider curve
    float t = unit01(rng_);
    ap.googlyGravity = std::pow(t, 0.322f);
    ap.googlySpring  = std::pow(1.0f - t, 0.322f);

    userPrefs_->setAvatarParams(ap);
    pushAvatarToGL();

    // Sync UI controls
    eyeColorSwatch_.setColour(juce::Colour(ap.eyeColor));
    eyeTypeCombo_.setSelectedId(ap.eyeType + 1, juce::dontSendNotification);
    eyeSizeSlider_.setValue(static_cast<double>(ap.eyeSize), juce::dontSendNotification);
    syncEyeSpacingSliderMode(ap.eyeType == xyzpan::kEyeCyclops);
    pupilSizeSlider_.setValue(static_cast<double>(ap.pupilSize), juce::dontSendNotification);
    googlySlider_.setValue(static_cast<double>(t), juce::dontSendNotification);

    // Update enabled states based on new eye type
    googlySlider_.setEnabled(ap.eyeType == xyzpan::kEyeGoogly);
}

// ---------------------------------------------------------------------------
// randomizeCustEars — earType, earSize, earRotation
// ---------------------------------------------------------------------------
void XYZPanEditor::randomizeCustEars()
{
    auto ap = userPrefs_->avatarParams();

    std::uniform_int_distribution<int> earTypeDist(0, 3);
    std::uniform_real_distribution<float> sizeDist(0.2f, 3.0f);
    std::uniform_real_distribution<float> rotDist(-180.0f, 180.0f);

    ap.earType     = earTypeDist(rng_);
    ap.earSize     = sizeDist(rng_);
    ap.earRotation = rotDist(rng_);

    userPrefs_->setAvatarParams(ap);
    pushAvatarToGL();

    // Sync UI controls
    earTypeCombo_.setSelectedId(ap.earType + 1, juce::dontSendNotification);
    earSizeSlider_.setValue(static_cast<double>(ap.earSize), juce::dontSendNotification);
    earRotationSlider_.setValue(static_cast<double>(ap.earRotation), juce::dontSendNotification);
}

// ---------------------------------------------------------------------------
// randomizeCustNose — noseColor, noseType, noseSize
// ---------------------------------------------------------------------------
void XYZPanEditor::randomizeCustNose()
{
    auto ap = userPrefs_->avatarParams();

    std::uniform_int_distribution<uint32_t> colorDist(0x00000000u, 0x00FFFFFFu);
    std::uniform_int_distribution<int> noseTypeDist(0, 5);
    std::uniform_real_distribution<float> sizeDist(0.2f, 3.0f);

    ap.noseColor = 0xFF000000u | colorDist(rng_);
    ap.noseType  = noseTypeDist(rng_);
    ap.noseSize  = sizeDist(rng_);

    userPrefs_->setAvatarParams(ap);
    pushAvatarToGL();

    // Sync UI controls
    noseColorSwatch_.setColour(juce::Colour(ap.noseColor));
    noseTypeCombo_.setSelectedId(ap.noseType + 1, juce::dontSendNotification);
    noseSizeSlider_.setValue(static_cast<double>(ap.noseSize), juce::dontSendNotification);
}

// ---------------------------------------------------------------------------
// randomizeCustHats — hatColor, hatType, hatSize
// ---------------------------------------------------------------------------
void XYZPanEditor::randomizeCustHats()
{
    auto ap = userPrefs_->avatarParams();

    std::uniform_int_distribution<uint32_t> colorDist(0x00000000u, 0x00FFFFFFu);
    std::uniform_int_distribution<int> hatTypeDist(0, 8);
    std::uniform_real_distribution<float> sizeDist(0.2f, 2.0f);

    ap.hatColor = 0xFF000000u | colorDist(rng_);
    ap.hatType  = hatTypeDist(rng_);
    ap.hatSize  = sizeDist(rng_);

    userPrefs_->setAvatarParams(ap);
    pushAvatarToGL();

    // Sync UI controls
    hatColorSwatch_.setColour(juce::Colour(ap.hatColor));
    hatTypeCombo_.setSelectedId(ap.hatType + 1, juce::dontSendNotification);
    hatSizeSlider_.setValue(static_cast<double>(ap.hatSize), juce::dontSendNotification);
}

// ---------------------------------------------------------------------------
// Remote instance focus — instance selector + attachment rebinding
// ---------------------------------------------------------------------------

void XYZPanEditor::rebuildInstanceList()
{
    auto& hub = proc_.getListenerHub();
    const int count = hub.getLinkedCount();

    if (count == lastKnownLinkedCount_ && !forceListRebuild_)
        return;
    lastKnownLinkedCount_ = count;
    forceListRebuild_ = false;

    if (count < 2) {
        if (remoteFocusProc_ != nullptr)
            setRemoteFocus(-1);
        repaint();
        return;
    }

    // Build row list: "Self" + each linked instance
    SharedListenerHub::Listener* instances[xyzpan::kMaxLinkedSources + 1];
    const int n = hub.getLinkedInstances(instances, xyzpan::kMaxLinkedSources + 1);

    // Count linked instances (for focus range validation)
    int colorIdx = 0;
    for (int i = 0; i < n; ++i) {
        if (instances[i] == static_cast<SharedListenerHub::Listener*>(&proc_))
            continue;
        if (!instances[i]->hubAlive_.load(std::memory_order_acquire))
            continue;
        ++colorIdx;
    }

    // Validate current focus is still in range
    if (remoteFocusIndex_ >= colorIdx && remoteFocusProc_ != nullptr)
        setRemoteFocus(-1);

    repaint();
}

void XYZPanEditor::setRemoteFocus(int linkedIndex)
{
    if (linkedIndex < 0) {
        // Revert to self
        remoteFocusIndex_ = -1;
        remoteFocusProc_ = nullptr;
        detachAndRebindTo(proc_.apvts, &proc_);
        glView_.setFocusedForeignSource(-1);
        forceListRebuild_ = true;
        repaint();
        return;
    }

    // Find the remote processor
    auto& hub = proc_.getListenerHub();
    SharedListenerHub::Listener* instances[xyzpan::kMaxLinkedSources + 1];
    const int n = hub.getLinkedInstances(instances, xyzpan::kMaxLinkedSources + 1);

    // Build list of non-self instances
    std::vector<SharedListenerHub::Listener*> others;
    for (int i = 0; i < n; ++i)
        if (instances[i] != static_cast<SharedListenerHub::Listener*>(&proc_))
            others.push_back(instances[i]);

    if (linkedIndex >= static_cast<int>(others.size())) {
        setRemoteFocus(-1);  // out of range, revert
        return;
    }

    auto* remoteListener = others[static_cast<size_t>(linkedIndex)];

    // Validate alive before dereferencing virtual methods
    if (!remoteListener->hubAlive_.load(std::memory_order_acquire)) {
        setRemoteFocus(-1);
        return;
    }

    auto* remoteProc = dynamic_cast<XYZPanProcessor*>(remoteListener->getProcessor());

    if (remoteProc == nullptr) {
        setRemoteFocus(-1);
        return;
    }

    remoteFocusIndex_ = linkedIndex;
    remoteFocusProc_ = remoteProc;
    detachAndRebindTo(remoteProc->apvts, remoteProc);
    glView_.setFocusedForeignSource(linkedIndex);
    forceListRebuild_ = true;
    repaint();
}

void XYZPanEditor::detachAndRebindTo(juce::AudioProcessorValueTreeState& target,
                                      XYZPanProcessor* targetProc)
{
    // Destroy all remoteable slider attachments
    xAtt_.reset();
    yAtt_.reset();
    zAtt_.reset();
    stereoWidthAtt_.reset();
    orbitPhaseAtt_.reset();
    orbitOffsetAtt_.reset();
    orbitSpeedMulAtt_.reset();
    lfoSpeedMulAtt_.reset();
    sphereRadiusAtt_.reset();
    verbSizeAtt_.reset();
    verbDecayAtt_.reset();
    verbDampingAtt_.reset();
    verbWetAtt_.reset();
    dopplerAtt_.reset();
    inputGainAtt_.reset();

    // Destroy remoteable button attachments
    faceListenerAtt_.reset();
    binauralAtt_.reset();
    earlyReflAtt_.reset();

    // Recreate all attachments pointing to target APVTS
    xAtt_ = std::make_unique<SA>(target, ParamID::X, xKnob_);
    yAtt_ = std::make_unique<SA>(target, ParamID::Y, yKnob_);
    zAtt_ = std::make_unique<SA>(target, ParamID::Z, zKnob_);
    stereoWidthAtt_   = std::make_unique<SA>(target, ParamID::STEREO_WIDTH, stereoWidthKnob_);
    orbitPhaseAtt_    = std::make_unique<SA>(target, ParamID::STEREO_ORBIT_PHASE, orbitPhaseKnob_);
    orbitOffsetAtt_   = std::make_unique<SA>(target, ParamID::STEREO_ORBIT_OFFSET, orbitOffsetKnob_);
    orbitSpeedMulAtt_ = std::make_unique<SA>(target, ParamID::STEREO_ORBIT_SPEED_MUL, orbitSpeedMulKnob_);
    lfoSpeedMulAtt_   = std::make_unique<SA>(target, ParamID::LFO_SPEED_MUL, lfoSpeedMulKnob_);
    sphereRadiusAtt_  = std::make_unique<SA>(target, ParamID::SPHERE_RADIUS, sphereRadiusKnob_);
    verbSizeAtt_      = std::make_unique<SA>(target, ParamID::VERB_SIZE, verbSize_);
    verbDecayAtt_     = std::make_unique<SA>(target, ParamID::VERB_DECAY, verbDecay_);
    verbDampingAtt_   = std::make_unique<SA>(target, ParamID::VERB_DAMPING, verbDamping_);
    verbWetAtt_       = std::make_unique<SA>(target, ParamID::VERB_WET, verbWet_);
    dopplerAtt_       = std::make_unique<SA>(target, ParamID::DIST_DELAY_MAX_MS, dopplerKnob_);
    inputGainAtt_     = std::make_unique<SA>(target, ParamID::INPUT_GAIN_DB, inputGainKnob_);

    faceListenerAtt_ = std::make_unique<BA>(target, ParamID::STEREO_FACE_LISTENER, faceListenerToggle_);
    binauralAtt_     = std::make_unique<BA>(target, ParamID::BINAURAL_ENABLED, binauralToggle_);
    earlyReflAtt_    = std::make_unique<BA>(target, ParamID::ER_ENABLED, earlyReflToggle_);

    // Rebind all 6 LFO strips
    xLFO_.rebindAPVTS(target);
    yLFO_.rebindAPVTS(target);
    zLFO_.rebindAPVTS(target);
    orbitXYLFO_.rebindAPVTS(target);
    orbitXZLFO_.rebindAPVTS(target);
    orbitYZLFO_.rebindAPVTS(target);

    // Update LFO waveform display output sources
    xLFO_.setOutputSource(&targetProc->lfoOutputX);
    yLFO_.setOutputSource(&targetProc->lfoOutputY);
    zLFO_.setOutputSource(&targetProc->lfoOutputZ);
    orbitXYLFO_.setOutputSource(&targetProc->lfoOutputOrbitXY);
    orbitXZLFO_.setOutputSource(&targetProc->lfoOutputOrbitXZ);
    orbitYZLFO_.setOutputSource(&targetProc->lfoOutputOrbitYZ);

    // Re-hook width-gate callback (needs to still work after rebind)
    stereoWidthKnob_.onValueChange = [this] { updateOrbitEnabled(); };
    updateOrbitEnabled();

    // Update reset buttons to target correct processor's atomics
    // Use member pointer (remoteFocusProc_ or proc_) so the lambda never
    // holds a stale captured pointer if the remote instance is destroyed.
    resetXYZPhasesBtn_.onClick = [this] {
        auto* target = remoteFocusProc_ != nullptr ? remoteFocusProc_ : &proc_;
        target->resetXYZLfoPhases.store(true);
    };
    resetOrbitPhasesBtn_.onClick = [this] {
        auto* target = remoteFocusProc_ != nullptr ? remoteFocusProc_ : &proc_;
        target->resetOrbitLfoPhases.store(true);
    };
}
