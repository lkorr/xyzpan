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

    // Listener tab components initially hidden (Source tab is default)
    // This also hides perspective controls (yaw/pitch/roll, head follows, link)
    // which now live in the Listener tab instead of the bottom row Perspective tab.
    setActiveLeftTab(LeftTab::Source);

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

    // ----- Remote tab: instance list + name editor -----
    // Rows are non-interactive labels; clicks handled in mouseDown via hit-testing
    for (int i = 0; i < kMaxRemoteRows; ++i) {
        instanceRows_[i].setJustificationType(juce::Justification::centredLeft);
        instanceRows_[i].setInterceptsMouseClicks(false, false);
        addAndMakeVisible(instanceRows_[i]);
        instanceRows_[i].setVisible(false);
    }

    // Remote status label
    remoteStatusLabel_.setText("No linked instances", juce::dontSendNotification);
    remoteStatusLabel_.setJustificationType(juce::Justification::centred);
    remoteStatusLabel_.setColour(juce::Label::textColourId,
                                 juce::Colour(lookAndFeel_.currentTheme().bronze));
    addAndMakeVisible(remoteStatusLabel_);
    remoteStatusLabel_.setVisible(false);

    // Own-instance name editor
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
        glView_.setOwnInstanceName(instanceNameEditor_.getText().trim());
    };
    instanceNameEditor_.onFocusLost = [this] {
        proc_.setInstanceName(instanceNameEditor_.getText().trim());
        glView_.setOwnInstanceName(instanceNameEditor_.getText().trim());
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
            if (activeLeftTab_ == LeftTab::Remote)
                setActiveLeftTab(LeftTab::Source);
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
    setResizeLimits(915, 650, 1800, 1600);
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
    l.contentY    = kPresetBarH;
    l.leftColH    = totalH - kBottomH - kPresetBarH;
    l.bottomY     = totalH - kBottomH;
    const int maxStripW = 185;
    const int availLfoW = totalW - kMeterW - kOrbitCtrlW - kReverbSectionW;
    l.lfoTotalW   = juce::jmin(availLfoW, maxStripW * 3);
    l.lfoX        = kOrbitCtrlW;
    l.reverbX     = l.lfoX + l.lfoTotalW;
    l.orbitTotalW = l.reverbX;
    l.contentTop  = l.bottomY + kSectionHdrH;
    return l;
}

// ---------------------------------------------------------------------------
// paint
// ---------------------------------------------------------------------------
void XYZPanEditor::paint(juce::Graphics& g)
{
    const auto& ct = lookAndFeel_.currentTheme();
    const auto lo = Layout::compute(getWidth(), getHeight());
    const int bx = 0;
    const int bw = getWidth() - kMeterW;

    // ===== BACKGROUNDS =====
    g.fillAll(juce::Colour(ct.background));

    // Left column background (below preset bar)
    g.setColour(juce::Colour(ct.darkIron));
    g.fillRect(0, lo.contentY, kLeftColW, lo.leftColH);

    // Bottom row background
    g.setColour(juce::Colour(ct.darkIron));
    g.fillRect(0, lo.bottomY, bw, kBottomH);

    // ===== SECTION HEADERS =====

    // Helper lambda for section headers
    auto drawHeader = [&](int x, int y, int w, const juce::String& text) {
        auto hdrRect = juce::Rectangle<int>(x, y, w, kSectionHdrH);
        g.setGradientFill(juce::ColourGradient(
            juce::Colour(ct.bronze).withAlpha(0.25f), static_cast<float>(x), static_cast<float>(y),
            juce::Colour(ct.darkIron), static_cast<float>(x + w), static_cast<float>(y), false));
        g.fillRect(hdrRect);
        g.setColour(juce::Colour(ct.brightGold));
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawText(text, hdrRect.reduced(10, 0), juce::Justification::centredLeft);
        g.setColour(juce::Colour(ct.bronze).withAlpha(0.6f));
        g.drawHorizontalLine(y + kSectionHdrH - 1, static_cast<float>(x), static_cast<float>(x + w));
    };

    // Left column tabbed header — "Source" | "Listener" (replaces static "POSITION")
    {
        const int hdrY = lo.contentY;
        auto hdrRect = juce::Rectangle<int>(0, hdrY, kLeftColW, kSectionHdrH);
        g.setGradientFill(juce::ColourGradient(
            juce::Colour(ct.bronze).withAlpha(0.25f), 0.0f, static_cast<float>(hdrY),
            juce::Colour(ct.darkIron), static_cast<float>(kLeftColW), static_cast<float>(hdrY), false));
        g.fillRect(hdrRect);
        g.setColour(juce::Colour(ct.bronze).withAlpha(0.6f));
        g.drawHorizontalLine(hdrY + kSectionHdrH - 1, 0.0f, static_cast<float>(kLeftColW));

        const int thirdW = kLeftColW / 3;
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));

        g.setColour(activeLeftTab_ == LeftTab::Source
            ? juce::Colour(ct.brightGold)
            : juce::Colour(ct.bronze));
        g.drawText("Source", 0, hdrY, thirdW, kSectionHdrH, juce::Justification::centred);

        g.setColour(activeLeftTab_ == LeftTab::Listener
            ? juce::Colour(ct.brightGold)
            : juce::Colour(ct.bronze));
        g.drawText("Listener", thirdW, hdrY, thirdW, kSectionHdrH, juce::Justification::centred);

        // Remote tab — grayed out when <2 linked instances
        const bool remoteAvailable = (lastKnownLinkedCount_ >= 2);
        if (activeLeftTab_ == LeftTab::Remote)
            g.setColour(juce::Colour(ct.brightGold));
        else if (remoteAvailable)
            g.setColour(juce::Colour(ct.bronze));
        else
            g.setColour(juce::Colour(ct.bronze).withAlpha(0.3f));
        g.drawText("Remote", thirdW * 2, hdrY, kLeftColW - thirdW * 2, kSectionHdrH, juce::Justification::centred);
    }

    // Remote focus indicator — colored bar at top of left column content area
    if (remoteFocusIndex_ >= 0) {
        const int barH = 3;
        const int barY = lo.contentY + kSectionHdrH;
        g.setColour(kPaletteColours[remoteFocusIndex_ % 8]);
        g.fillRect(0, barY, kLeftColW, barH);
    }

    // Tabbed header — bottom row, orbit portion: "Options" | "Perspective" tabs + LFO plane labels
    {
        const int hdrY = lo.bottomY;
        const int fullW = kOrbitCtrlW + lo.lfoTotalW;

        // Background gradient for full header width (same as drawHeader)
        auto hdrRect = juce::Rectangle<int>(bx, hdrY, fullW, kSectionHdrH);
        g.setGradientFill(juce::ColourGradient(
            juce::Colour(ct.bronze).withAlpha(0.25f), static_cast<float>(bx), static_cast<float>(hdrY),
            juce::Colour(ct.darkIron), static_cast<float>(bx + fullW), static_cast<float>(hdrY), false));
        g.fillRect(hdrRect);

        // Bottom line
        g.setColour(juce::Colour(ct.bronze).withAlpha(0.6f));
        g.drawHorizontalLine(hdrY + kSectionHdrH - 1, static_cast<float>(bx), static_cast<float>(bx + fullW));

        // Tab labels in the 240px controls area — two halves (Perspective moved to left column)
        const int halfTab = kOrbitCtrlW / 2;
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));

        auto tabColor = [&](OptionsTab t) {
            return activeTab_ == t
                ? juce::Colour(lookAndFeel_.currentTheme().brightGold)
                : juce::Colour(lookAndFeel_.currentTheme().bronze);
        };

        g.setColour(tabColor(OptionsTab::Options));
        g.drawText("Options", bx, hdrY, halfTab, kSectionHdrH, juce::Justification::centred);

        g.setColour(tabColor(OptionsTab::Customize));
        g.drawText("Custom", bx + halfTab, hdrY, kOrbitCtrlW - halfTab, kSectionHdrH, juce::Justification::centred);

        // Orbit LFO plane labels (XY / XZ / YZ) in the right portion
        const int lfoX = lo.lfoX;
        const int stripW = lo.lfoTotalW / 3;
        g.setColour(juce::Colour(ct.brightGold));
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("XY", lfoX,              hdrY, stripW,     kSectionHdrH, juce::Justification::centred);
        g.drawText("XZ", lfoX + stripW,     hdrY, stripW,     kSectionHdrH, juce::Justification::centred);
        g.drawText("YZ", lfoX + stripW * 2, hdrY, stripW,     kSectionHdrH, juce::Justification::centred);
    }

    // "REVERB" header — bottom row, reverb portion
    drawHeader(lo.reverbX, lo.bottomY, kReverbSectionW, "REVERB");

    // ===== DIVIDERS — POSITION SECTION (Source tab only) =====
    if (activeLeftTab_ == LeftTab::Source) {
        // Thin bronze separator above LFO Speed slider row
        {
            const int lfoSpeedRowH = 32;
            const int lfoSpeedSepY = lo.bottomY - lfoSpeedRowH;
            g.setColour(juce::Colour(ct.bronze).withAlpha(0.4f));
            g.drawHorizontalLine(lfoSpeedSepY, 0.0f, static_cast<float>(kLeftColW));
        }

        // Vertical dividers between X | Y | Z sub-columns (extend to bottom of left column)
        {
            const int subColW = kLeftColW / 3;
            const float divTop = static_cast<float>(lo.contentY + kSectionHdrH);
            const int lfoSpeedRowH = 32;
            const float divBot = static_cast<float>(lo.bottomY - lfoSpeedRowH);
            g.setColour(juce::Colour(ct.bronze).withAlpha(0.5f));
            g.drawVerticalLine(subColW,     divTop, divBot);
            g.drawVerticalLine(subColW * 2, divTop, divBot);
        }
    }

    // ===== DIVIDERS — BOTTOM ROW =====
    // Vertical divider between orbit controls and orbit LFO strips
    g.setColour(juce::Colour(ct.bronze).withAlpha(0.5f));
    g.drawVerticalLine(lo.lfoX, static_cast<float>(lo.contentTop), static_cast<float>(lo.bottomY + kBottomH));

    // Vertical dividers between orbit LFO strips (XY | XZ | YZ)
    // and thin horizontal separator above orbit speed row
    {
        const int lfoX = lo.lfoX;
        const int lfoTotalW = lo.lfoTotalW;
        const int stripW = lfoTotalW / 3;
        const int speedRowH = 32;
        const int speedSepY = lo.bottomY + kBottomH - speedRowH;
        g.drawVerticalLine(lfoX + stripW,     static_cast<float>(lo.contentTop), static_cast<float>(speedSepY));
        g.drawVerticalLine(lfoX + stripW * 2, static_cast<float>(lo.contentTop), static_cast<float>(speedSepY));
        // Thin bronze separator above orbit speed slider row
        g.setColour(juce::Colour(ct.bronze).withAlpha(0.4f));
        g.drawHorizontalLine(speedSepY, static_cast<float>(lfoX), static_cast<float>(lfoX + lfoTotalW));
    }

    // Vertical divider between orbit section and reverb section
    g.setColour(juce::Colour(ct.bronze));
    g.fillRect(lo.reverbX - 1, lo.bottomY, 2, kBottomH);

    // ===== MAIN STRUCTURAL DIVIDERS =====
    // Vertical separator (left column | GL view), from below preset bar
    g.setColour(juce::Colour(ct.bronze));
    g.fillRect(kLeftColW - 1, lo.contentY, 2, lo.leftColH);

    // Horizontal separator (above bottom row)
    g.fillRect(0, lo.bottomY, bw, 2);

    // Thin separator below preset bar
    g.setColour(juce::Colour(ct.bronze).withAlpha(0.5f));
    g.drawHorizontalLine(kPresetBarH - 1, 0.0f, static_cast<float>(bw));

    // ===== NOISE TEXTURE OVERLAY =====
    // Tile procedural noise at low opacity over left column and bottom row panels
    if (noiseTexture_.isValid()) {
        const int nw = noiseTexture_.getWidth();
        const int nh = noiseTexture_.getHeight();

        g.saveState();
        g.setOpacity(0.04f);

        // Left column
        g.reduceClipRegion(0, lo.contentY, kLeftColW, lo.leftColH);
        for (int ty = lo.contentY; ty < lo.contentY + lo.leftColH; ty += nh)
            for (int tx = 0; tx < kLeftColW; tx += nw)
                g.drawImageAt(noiseTexture_, tx, ty);

        g.restoreState();
        g.saveState();
        g.setOpacity(0.04f);

        // Bottom row
        g.reduceClipRegion(0, lo.bottomY, bw, kBottomH);
        for (int ty = lo.bottomY; ty < lo.bottomY + kBottomH; ty += nh)
            for (int tx = 0; tx < bw; tx += nw)
                g.drawImageAt(noiseTexture_, tx, ty);

        g.restoreState();
    }
}

// ---------------------------------------------------------------------------
// resized
// ---------------------------------------------------------------------------
void XYZPanEditor::resized()
{
    auto b = getLocalBounds();

    // Preset bar at the very top (full width)
    {
        auto presetBar = b.removeFromTop(kPresetBarH);
        auto saveBtnArea = presetBar.removeFromRight(60);
        auto loadBtnArea = presetBar.removeFromRight(60);
        presetCombo_.setBounds(presetBar.reduced(kPadding, 2));
        presetSaveBtn_.setBounds(saveBtnArea.reduced(2));
        presetLoadBtn_.setBounds(loadBtnArea.reduced(2));
    }

    // Output meter strip — full height right edge (below preset bar)
    auto meterStrip = b.removeFromRight(kMeterW);
    outputMeter_.setBounds(meterStrip);

    // Carve out bottom row
    auto bottomRow = b.removeFromBottom(kBottomH);
    // Carve out left column (from what remains above bottom row)
    auto leftCol = b.removeFromLeft(kLeftColW);
    // Remainder = GL view area
    auto glArea = b;

    // Shared structural geometry — used by both left column and bottom row blocks
    const auto lo = Layout::compute(getWidth(), getHeight());

    // Remote tab content — name editor + instance list in left column content area
    {
        const int contentTop = lo.contentY + kSectionHdrH + (remoteFocusIndex_ >= 0 ? 6 : 0);
        const int pad = 8;
        int y = contentTop + pad;

        // "Name:" label + text editor
        instanceNameLabel_.setBounds(pad, y, 42, 22);
        instanceNameEditor_.setBounds(pad + 44, y, kLeftColW - 2 * pad - 44, 22);
        y += 30;

        // Status label (shown when no linked instances)
        remoteStatusLabel_.setBounds(pad, y, kLeftColW - 2 * pad, 24);
        y += 28;

        // Instance rows
        const int rowH = 24;
        for (int i = 0; i < kMaxRemoteRows; ++i) {
            instanceRows_[i].setBounds(pad, y + i * (rowH + 2), kLeftColW - 2 * pad, rowH);
        }
    }

    // ===== GL VIEW =====
    glView_.setBounds(glArea);

    // Snap buttons: top-right corner of GL area (children of glView_, so use local coords)
    {
        auto snapRow = juce::Rectangle<int>(0, 0, glArea.getWidth(), glArea.getHeight());
        snapRow = snapRow.removeFromTop(kSnapBtnH + 4)
                         .removeFromRight(3 * (kSnapBtnW + 4));
        snapXY_.setBounds(snapRow.removeFromLeft(kSnapBtnW + 4).reduced(2));
        snapXZ_.setBounds(snapRow.removeFromLeft(kSnapBtnW + 4).reduced(2));
        snapYZ_.setBounds(snapRow.reduced(2));
    }

    // Dev panel: child of glView_, right 30% of GL area (or user-dragged width)
    const int defaultPanelW = static_cast<int>(glArea.getWidth() * 0.30f);
    const int panelW = devPanel_.getCustomWidth() > 0
        ? juce::jlimit(200, glArea.getWidth() - 50, devPanel_.getCustomWidth())
        : defaultPanelW;
    devPanel_.setBounds(glArea.getWidth() - panelW, 0, panelW, glArea.getHeight());

    // ===== LEFT COLUMN — POSITION =====
    const int leftColTop = leftCol.getY();  // = kPresetBarH after preset bar removed
    const int posColW = kLeftColW / 3;
    const int knobH   = 108;
    const int posPad  = 6;

    // --- POSITION SECTION (full left column height) ---
    {
        // Reserve 32px at the bottom of the position section for the LFO Speed slider
        const int lfoSpeedRowH = 32;
        const int posSectionBottom = lo.bottomY - lfoSpeedRowH;
        auto posSection = juce::Rectangle<int>(0, leftColTop + kSectionHdrH, kLeftColW,
                                               posSectionBottom - (leftColTop + kSectionHdrH));

        const int bigLabelH = 22;
        auto layoutPosCol = [&](juce::Slider& knob, juce::Label& label, LFOStrip& lfo,
                                int colX, int colW, int colTop, int colBottom) {
            label.setBounds(colX, colTop, colW, bigLabelH);
            int knobW = juce::jmin(knobH, colW - posPad * 2);
            int knobX = colX + (colW - knobW) / 2;
            knob.setBounds(knobX, colTop + bigLabelH, knobW, knobH);
            int lfoTop = colTop + bigLabelH + knobH + 2;
            int lfoH = juce::jmax(0, colBottom - lfoTop);
            lfo.setBounds(colX, lfoTop, colW, lfoH);
        };

        layoutPosCol(xKnob_, xLabel_, xLFO_,
                     0,            posColW, posSection.getY(), posSection.getBottom());
        layoutPosCol(yKnob_, yLabel_, yLFO_,
                     posColW,      posColW, posSection.getY(), posSection.getBottom());
        layoutPosCol(zKnob_, zLabel_, zLFO_,
                     posColW * 2,  kLeftColW - posColW * 2, posSection.getY(), posSection.getBottom());

        // LFO Speed slider row — spans full left column width at the bottom of position section
        const int speedY = posSectionBottom;
        const int speedLabelW = 70;
        const int resetBtnW = 44;
        const int resetBtnGap = 4;
        lfoSpeedMulLabel_.setBounds(kPadding, speedY + 4, speedLabelW, lfoSpeedRowH - 8);
        lfoSpeedMulKnob_.setBounds(kPadding + speedLabelW, speedY + 4,
                                   kLeftColW - kPadding * 2 - speedLabelW - resetBtnW - resetBtnGap,
                                   lfoSpeedRowH - 8);
        resetXYZPhasesBtn_.setBounds(kLeftColW - kPadding - resetBtnW, speedY + 4,
                                     resetBtnW, lfoSpeedRowH - 8);
    }

    // --- LISTENER TAB layout (walker knobs + perspective knobs, same left column area) ---
    {
        const int colTop = leftColTop + kSectionHdrH;
        const int bigLabelH = 22;
        const int walkerKnobH = 108;

        // Walker X/Y/Z knobs — 3 columns, same sizing as source X/Y/Z
        auto layoutWalkerCol = [&](juce::Slider& knob, juce::Label& label,
                                   int colX, int colW, int top) {
            label.setBounds(colX, top, colW, bigLabelH);
            int knobW = juce::jmin(walkerKnobH, colW - posPad * 2);
            int knobX = colX + (colW - knobW) / 2;
            knob.setBounds(knobX, top + bigLabelH, knobW, walkerKnobH);
        };

        layoutWalkerCol(walkerXKnob_, walkerXLabel_, 0,            posColW, colTop);
        layoutWalkerCol(walkerYKnob_, walkerYLabel_, posColW,      posColW, colTop);
        layoutWalkerCol(walkerZKnob_, walkerZLabel_, posColW * 2,  kLeftColW - posColW * 2, colTop);

        // Yaw / Pitch / Roll knobs — row of 3, below walker knobs
        const int perspY = colTop + bigLabelH + walkerKnobH + 8;
        const int perspKnobSz = 58;
        const int perspLabelH = 14;
        const int perspColW = kLeftColW / 3;

        auto layoutPerspCol = [&](juce::Slider& knob, juce::Label& label, int cx, int cw) {
            int kx = cx + (cw - perspKnobSz) / 2;
            knob.setBounds(kx, perspY, perspKnobSz, perspKnobSz);
            label.setBounds(cx, perspY + perspKnobSz, cw, perspLabelH);
        };

        layoutPerspCol(listenerYawKnob_,   listenerYawLabel_,   0, perspColW);
        layoutPerspCol(listenerPitchKnob_, listenerPitchLabel_, perspColW, perspColW);
        layoutPerspCol(listenerRollKnob_,  listenerRollLabel_,  perspColW * 2, kLeftColW - perspColW * 2);

        // Toggles below perspective knobs
        const int toggleY = perspY + perspKnobSz + perspLabelH + 4;
        const int toggleH = 22;
        wasdToggle_.setBounds(kPadding, toggleY, kLeftColW - kPadding * 2, toggleH);
        headFollowsToggle_.setBounds(kPadding, toggleY + toggleH + 2, kLeftColW - kPadding * 2, toggleH);
        listenerLinkToggle_.setBounds(kPadding, toggleY + (toggleH + 2) * 2, kLeftColW - kPadding * 2, toggleH);
    }

    // ===== BOTTOM ROW =====
    {
        const int bx = bottomRow.getX();
        const int contentH = kBottomH - kSectionHdrH;  // 216

        // Use shared Layout for structural geometry (reverb/orbit split)
        const int contentTop = lo.contentTop;
        const int reverbX    = lo.reverbX;

        // --- ORBIT CONTROLS (fixed 240px left portion) ---
        // Three tab pages share this space; all get bounds, visibility managed by setActiveTab().
        {
            const int ox = bx;
            const int ow = kOrbitCtrlW;
            const int pad = 4;
            const int knobSz = 58;
            const int labelH_b = 14;

            // === OPTIONS TAB layout ===
            // Top row: Sphere | Doppler | In Gain — three knobs in equal columns
            {
                const int colW = ow / 3;
                const int doppSubLabelH = 12;

                int sKnobX = ox + (colW - knobSz) / 2;
                sphereRadiusKnob_.setBounds(sKnobX, contentTop + 2, knobSz, knobSz);
                sphereRadiusLabel_.setBounds(ox, contentTop + 60, colW, labelH_b);

                int dKnobX = ox + colW + (colW - knobSz) / 2;
                dopplerKnob_.setBounds(dKnobX, contentTop + 2, knobSz, knobSz);
                dopplerLabel_.setBounds(ox + colW, contentTop + 60, colW, labelH_b);
                dopplerSubLabel_.setBounds(ox + colW, contentTop + 74, colW, doppSubLabelH);

                int gKnobX = ox + colW * 2 + (colW - knobSz) / 2;
                inputGainKnob_.setBounds(gKnobX, contentTop + 2, knobSz, knobSz);
                inputGainLabel_.setBounds(ox + colW * 2, contentTop + 60, colW, labelH_b);
            }

            // Checkbox row: Binaural | Early Reflections
            {
                const int cbY = contentTop + 87;
                const int boxSz = 22;
                const int cbLabelW = 90;
                const int cbGap = 4;
                const int pairW = boxSz + cbGap + cbLabelW;
                const int halfW = ow / 2;

                int binPairX = ox + (halfW - pairW) / 2;
                binauralToggle_.setBounds(binPairX, cbY, boxSz, boxSz);
                binauralLabel_.setBounds(binPairX + boxSz + cbGap, cbY, cbLabelW, boxSz);

                int erPairX = ox + halfW + (halfW - pairW) / 2;
                earlyReflToggle_.setBounds(erPairX, cbY, boxSz, boxSz);
                earlyReflLabel_.setBounds(erPairX + boxSz + cbGap, cbY, cbLabelW, boxSz);
            }

            // Orbit sliders below checkbox row
            {
                const int labelW = 46;
                const int sliderH = 22;
                const int gap = 4;
                const int btnH = 22;

                int sy = contentTop + 118;
                auto placeOrbitSlider = [&](juce::Slider& slider, juce::Label& label) {
                    label.setBounds(ox + pad, sy, labelW, sliderH);
                    slider.setBounds(ox + pad + labelW, sy, ow - pad * 2 - labelW, sliderH);
                    sy += sliderH + gap;
                };

                placeOrbitSlider(stereoWidthKnob_, stereoWidthLabel_);
                placeOrbitSlider(orbitOffsetKnob_, orbitOffsetLabel_);
                placeOrbitSlider(orbitPhaseKnob_,  orbitPhaseLabel_);

                sy -= 4;
                faceListenerToggle_.setBounds(ox + pad, sy, ow - pad * 2, btnH);
            }

            // Perspective controls now in left column Listener tab (layout handled above).

            // === CUSTOMIZE TAB layout (scrollable viewport) ===
            {
                const int sliderH  = 20;
                const int comboH   = 22;
                const int labelW   = 68;
                const int gap      = 4;
                const int headerH  = 16;

                // Viewport fills the tab content area
                customizeViewport_.setBounds(ox, contentTop, ow, contentH);

                // All coordinates are LOCAL to customizeContent_ (start at 0)
                int cy = 2;

                // --- General section ---
                // Theme label + combo
                themeLabel_.setBounds(pad, cy, labelW, comboH);
                themeCombo_.setBounds(pad + labelW, cy, ow - pad * 2 - labelW, comboH);
                cy += comboH + gap;

                auto placeAvatarSlider = [&](juce::Slider& slider, juce::Label& label) {
                    label.setBounds(pad, cy, labelW, sliderH);
                    slider.setBounds(pad + labelW, cy, ow - pad * 2 - labelW, sliderH);
                    cy += sliderH + gap;
                };

                auto placeColorSwatch = [&](ColorSwatch& swatch, juce::Label& label) {
                    label.setBounds(pad, cy, labelW, sliderH);
                    swatch.setBounds(pad + labelW, cy, sliderH, sliderH);
                    cy += sliderH + gap;
                };

                // --- General head controls ---
                placeColorSwatch(headColorSwatch_,  headColorLabel_);
                placeAvatarSlider(headSizeSlider_,       headSizeLabel_);
                placeAvatarSlider(headElongationSlider_, headElongationLabel_);

                // --- EYES section header ---
                eyesSectionHeaderY_ = cy;
                cy += headerH + gap;

                // Eye Type combo (includes "None" option)
                eyeTypeLabel_.setBounds(pad, cy, labelW, comboH);
                eyeTypeCombo_.setBounds(pad + labelW, cy, ow - pad * 2 - labelW, comboH);
                cy += comboH + gap;

                // Eye Size, Eye Space, Pupil Size sliders
                placeAvatarSlider(eyeSizeSlider_,        eyeSizeLabel_);
                placeAvatarSlider(eyeSpacingSlider_,     eyeSpacingLabel_);
                placeAvatarSlider(pupilSizeSlider_,      pupilSizeLabel_);
                placeAvatarSlider(googlySlider_,          googlyLabel_);
                placeColorSwatch(eyeColorSwatch_,   eyeColorLabel_);

                // --- EARS section header ---
                earsSectionHeaderY_ = cy;
                cy += headerH + gap;

                // Ear Type combo
                earTypeLabel_.setBounds(pad, cy, labelW, comboH);
                earTypeCombo_.setBounds(pad + labelW, cy, ow - pad * 2 - labelW, comboH);
                cy += comboH + gap;

                // Ear Size, Ear Rot sliders
                placeAvatarSlider(earSizeSlider_,        earSizeLabel_);
                placeAvatarSlider(earRotationSlider_,    earRotationLabel_);

                // --- NOSE section header ---
                noseSectionHeaderY_ = cy;
                cy += headerH + gap;

                // Nose Type combo
                noseTypeLabel_.setBounds(pad, cy, labelW, comboH);
                noseTypeCombo_.setBounds(pad + labelW, cy, ow - pad * 2 - labelW, comboH);
                cy += comboH + gap;

                // Nose Size slider
                placeAvatarSlider(noseSizeSlider_, noseSizeLabel_);
                placeColorSwatch(noseColorSwatch_,  noseColorLabel_);

                // --- HATS section header ---
                hatsSectionHeaderY_ = cy;
                cy += headerH + gap;

                // Hat Type combo
                hatTypeLabel_.setBounds(pad, cy, labelW, comboH);
                hatTypeCombo_.setBounds(pad + labelW, cy, ow - pad * 2 - labelW, comboH);
                cy += comboH + gap;

                // Hat Size slider
                placeAvatarSlider(hatSizeSlider_, hatSizeLabel_);
                placeColorSwatch(hatColorSwatch_,   hatColorLabel_);

                // Set content size (width matches viewport, height = total content)
                customizeContent_.setSize(ow, cy + 2);
            }
        }

        // --- ORBIT LFO STRIPS + SPEED/RESET ROW (capped width, right-aligned against reverb) ---
        {
            const int lfoX = lo.lfoX;
            const int lfoTotalW = lo.lfoTotalW;
            const int speedRowH = 32;
            const int lfoH = contentH - speedRowH;
            const int stripW = lfoTotalW / 3;
            const int lastStripW = lfoTotalW - stripW * 2;

            orbitXYLFO_.setBounds(lfoX,              contentTop, stripW,     lfoH);
            orbitXZLFO_.setBounds(lfoX + stripW,     contentTop, stripW,     lfoH);
            orbitYZLFO_.setBounds(lfoX + stripW * 2, contentTop, lastStripW, lfoH);

            // Speed slider + Reset button row below orbit LFO strips
            const int speedY = contentTop + lfoH;
            const int speedLabelW = 70;
            const int resetBtnW = 44;
            const int resetBtnGap = 4;
            const int pad = 6;
            orbitSpeedMulLabel_.setBounds(lfoX + pad, speedY + 4, speedLabelW, speedRowH - 8);
            orbitSpeedMulKnob_.setBounds(lfoX + pad + speedLabelW, speedY + 4,
                                          lfoTotalW - pad * 2 - speedLabelW - resetBtnW - resetBtnGap,
                                          speedRowH - 8);
            resetOrbitPhasesBtn_.setBounds(lfoX + lfoTotalW - pad - resetBtnW, speedY + 4,
                                           resetBtnW, speedRowH - 8);
        }

        // --- REVERB KNOBS (2×2 grid in 120px column) ---
        {
            const int knobSz = 58;
            const int labelH_b = 12;
            const int cellH = knobSz + labelH_b;  // 84px per row
            const int colW = kReverbSectionW / 2;  // 60px per column

            auto placeVerbKnob = [&](juce::Slider& knob, juce::Label& label, int col, int row) {
                int cx = reverbX + col * colW;
                int ky = contentTop + row * cellH;
                knob.setBounds(cx + (colW - knobSz) / 2, ky, knobSz, knobSz);
                label.setBounds(cx, ky + knobSz, colW, labelH_b);
            };
            placeVerbKnob(verbSize_,    verbSizeL_,    0, 0);
            placeVerbKnob(verbDecay_,   verbDecayL_,   1, 0);
            placeVerbKnob(verbDamping_, verbDampingL_, 0, 1);
            placeVerbKnob(verbWet_,     verbWetL_,     1, 1);

            // DEV toggle below 2×2 grid
            const int devY = contentTop + 2 * cellH;
            const int devBtnW = 48;
            const int devBtnH = 20;
            devToggle_.setBounds(reverbX + (kReverbSectionW - devBtnW) / 2, devY + 4, devBtnW, devBtnH);
        }
    }
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
// setActiveLeftTab — show/hide Source vs Listener components in left column
// ---------------------------------------------------------------------------
void XYZPanEditor::setActiveLeftTab(LeftTab tab)
{
    activeLeftTab_ = tab;
    const bool source   = (tab == LeftTab::Source);
    const bool listener = (tab == LeftTab::Listener);
    const bool remote   = (tab == LeftTab::Remote);

    // Source tab components
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

    // Listener tab components: walker knobs + perspective controls
    walkerXKnob_.setVisible(listener);
    walkerXLabel_.setVisible(listener);
    walkerYKnob_.setVisible(listener);
    walkerYLabel_.setVisible(listener);
    walkerZKnob_.setVisible(listener);
    walkerZLabel_.setVisible(listener);
    wasdToggle_.setVisible(listener);
    listenerYawKnob_.setVisible(listener);
    listenerYawLabel_.setVisible(listener);
    listenerPitchKnob_.setVisible(listener);
    listenerPitchLabel_.setVisible(listener);
    listenerRollKnob_.setVisible(listener);
    listenerRollLabel_.setVisible(listener);
    headFollowsToggle_.setVisible(listener);
    listenerLinkToggle_.setVisible(listener);

    // Remote tab components
    instanceNameLabel_.setVisible(remote);
    instanceNameEditor_.setVisible(remote);
    remoteStatusLabel_.setVisible(remote && lastKnownLinkedCount_ < 2);
    for (int i = 0; i < kMaxRemoteRows; ++i)
        instanceRows_[i].setVisible(remote && i < instanceRowCount_);

    repaint();
}

// ---------------------------------------------------------------------------
// setActiveTab — show/hide components for Options vs Perspective vs Customize tab
// ---------------------------------------------------------------------------
void XYZPanEditor::setActiveTab(OptionsTab tab)
{
    activeTab_ = tab;
    const bool options   = (tab == OptionsTab::Options);
    const bool customize = (tab == OptionsTab::Customize);

    // Options-only components
    sphereRadiusKnob_.setVisible(options);
    sphereRadiusLabel_.setVisible(options);
    dopplerKnob_.setVisible(options);
    dopplerLabel_.setVisible(options);
    dopplerSubLabel_.setVisible(options);
    inputGainKnob_.setVisible(options);
    inputGainLabel_.setVisible(options);
    binauralToggle_.setVisible(options);
    binauralLabel_.setVisible(options);
    earlyReflToggle_.setVisible(options);
    earlyReflLabel_.setVisible(options);
    stereoWidthKnob_.setVisible(options);
    stereoWidthLabel_.setVisible(options);
    orbitOffsetKnob_.setVisible(options);
    orbitOffsetLabel_.setVisible(options);
    orbitPhaseKnob_.setVisible(options);
    orbitPhaseLabel_.setVisible(options);
    faceListenerToggle_.setVisible(options);

    // Perspective controls now live in left column Listener tab (managed by setActiveLeftTab)

    // Customize-only components (all children of viewport content)
    customizeViewport_.setVisible(customize);

    repaint();
}

// ---------------------------------------------------------------------------
// mouseDown — tab header click detection + last-clicked zone tracking
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

    // Hit-test left column header area — "Source" | "Listener" | "Remote" tabs
    if (pos.y >= lo.contentY && pos.y < lo.contentY + kSectionHdrH && pos.x < kLeftColW) {
        const int thirdW = kLeftColW / 3;
        if (pos.x < thirdW)
            setActiveLeftTab(LeftTab::Source);
        else if (pos.x < thirdW * 2)
            setActiveLeftTab(LeftTab::Listener);
        else if (lastKnownLinkedCount_ >= 2)
            setActiveLeftTab(LeftTab::Remote);
        return;
    }

    // Hit-test instance rows in Remote tab
    if (activeLeftTab_ == LeftTab::Remote) {
        for (int i = 0; i < instanceRowCount_; ++i) {
            if (instanceRows_[i].getBounds().contains(pos)) {
                setRemoteFocus(i == 0 ? -1 : i - 1);
                return;
            }
        }
    }

    // Hit-test bottom tab header area (the 240px orbit-controls header) — two halves
    if (pos.y >= lo.bottomY && pos.y < lo.bottomY + kSectionHdrH && pos.x < kOrbitCtrlW) {
        const int halfTab = kOrbitCtrlW / 2;
        if (pos.x < halfTab)
            setActiveTab(OptionsTab::Options);
        else
            setActiveTab(OptionsTab::Customize);
        return;
    }

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
        // Keep GL view instance name in sync (may change from DAW track rename)
        glView_.setOwnInstanceName(proc_.getInstanceNameValue());
    }

    if (!wasdToggle_.getToggleState())
        return;

    // Check which WASD keys are currently held
    const bool w = juce::KeyPress::isKeyCurrentlyDown('W') || juce::KeyPress::isKeyCurrentlyDown('w');
    const bool a = juce::KeyPress::isKeyCurrentlyDown('A') || juce::KeyPress::isKeyCurrentlyDown('a');
    const bool s = juce::KeyPress::isKeyCurrentlyDown('S') || juce::KeyPress::isKeyCurrentlyDown('s');
    const bool d = juce::KeyPress::isKeyCurrentlyDown('D') || juce::KeyPress::isKeyCurrentlyDown('d');

    if (!w && !a && !s && !d)
        return;

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

    // Position column: left column, below preset bar, above bottom row
    if (x >= 0 && x < kLeftColW && y >= kPresetBarH && y < lo.bottomY) {
        // Check individual LFO strips by testing component bounds
        auto inBounds = [&](const LFOStrip& strip) {
            auto b = strip.getBounds();
            return b.contains(x, y);
        };
        if (inBounds(xLFO_)) return RandZone::LfoX;
        if (inBounds(yLFO_)) return RandZone::LfoY;
        if (inBounds(zLFO_)) return RandZone::LfoZ;
        return RandZone::Position;
    }

    // Bottom row zones (below bottomY)
    if (y >= lo.bottomY) {
        // Reverb column (right side of bottom row)
        if (x >= lo.reverbX && x < lo.reverbX + kReverbSectionW)
            return RandZone::Reverb;

        // Orbit LFO strips (between orbit controls and reverb)
        {
            auto inBounds = [&](const LFOStrip& strip) {
                auto b = strip.getBounds();
                return b.contains(x, y);
            };
            if (inBounds(orbitXYLFO_)) return RandZone::OrbitLfoXY;
            if (inBounds(orbitXZLFO_)) return RandZone::OrbitLfoXZ;
            if (inBounds(orbitYZLFO_)) return RandZone::OrbitLfoYZ;
        }

        // Left 240px of bottom row — depends on active tab
        if (x >= 0 && x < kOrbitCtrlW && y >= lo.contentTop) {
            if (activeTab_ == OptionsTab::Options)
                return RandZone::StereoOrbit;

            if (activeTab_ == OptionsTab::Perspective)
                return RandZone::Perspective;

            if (activeTab_ == OptionsTab::Customize) {
                // Convert mouse Y to customizeContent_ local coords
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
        }
    }

    return RandZone::None;
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
        if (activeLeftTab_ == LeftTab::Remote)
            setActiveLeftTab(LeftTab::Source);
        instanceRowCount_ = 0;
        for (int i = 0; i < kMaxRemoteRows; ++i)
            instanceRows_[i].setVisible(false);
        remoteStatusLabel_.setText("No linked instances", juce::dontSendNotification);
        if (activeLeftTab_ == LeftTab::Remote)
            remoteStatusLabel_.setVisible(true);
        repaint();
        return;
    }

    if (activeLeftTab_ == LeftTab::Remote)
        remoteStatusLabel_.setVisible(false);

    // Build row list: "Self" + each linked instance
    SharedListenerHub::Listener* instances[xyzpan::kMaxLinkedSources + 1];
    const int n = hub.getLinkedInstances(instances, xyzpan::kMaxLinkedSources + 1);

    const auto& ct = lookAndFeel_.currentTheme();
    int row = 0;

    // Row 0: Self
    {
        const bool isFocused = (remoteFocusIndex_ < 0);
        instanceRows_[row].setText(juce::String(juce::CharPointer_UTF8("\xe2\x97\x8f")) + " Self",
                                   juce::dontSendNotification);
        instanceRows_[row].setColour(juce::Label::textColourId,
                                     isFocused ? juce::Colour(ct.brightGold)
                                               : juce::Colour(ct.bronze));
        if (activeLeftTab_ == LeftTab::Remote)
            instanceRows_[row].setVisible(true);
        ++row;
    }

    // Rows 1+: linked instances (skip self)
    int colorIdx = 0;
    for (int i = 0; i < n && row < kMaxRemoteRows; ++i) {
        if (instances[i] == static_cast<SharedListenerHub::Listener*>(&proc_))
            continue;
        if (!instances[i]->hubAlive_.load(std::memory_order_acquire))
            continue;

        juce::String name = instances[i]->getInstanceName();
        if (name.isEmpty()) {
            name = "Source " + juce::String(colorIdx + 1);
        }

        const bool isFocused = (remoteFocusIndex_ == colorIdx);
        const auto colour = kPaletteColours[colorIdx % 8];
        instanceRows_[row].setText(juce::String(juce::CharPointer_UTF8("\xe2\x97\x8f")) + " " + name,
                                   juce::dontSendNotification);
        instanceRows_[row].setColour(juce::Label::textColourId,
                                     isFocused ? colour.brighter(0.4f) : colour);
        if (activeLeftTab_ == LeftTab::Remote)
            instanceRows_[row].setVisible(true);
        ++row;
        ++colorIdx;
    }

    instanceRowCount_ = row;

    // Hide unused rows
    for (int i = row; i < kMaxRemoteRows; ++i)
        instanceRows_[i].setVisible(false);

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
