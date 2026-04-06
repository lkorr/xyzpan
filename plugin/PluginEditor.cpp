#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "xyzpan/Constants.h"
#include "xyzpan/dsp/SineLUT.h"
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

    // Tooltip window — nullptr parent makes it an independent desktop window
    // so it floats above OpenGL surfaces
    tooltipWindow_ = std::make_unique<juce::TooltipWindow>(nullptr, 600);

    // Tooltip toggle — "?" button, checked by default
    tooltipToggle_.setClickingTogglesState(true);
    tooltipToggle_.setToggleState(true, juce::dontSendNotification);
    tooltipToggle_.setColour(juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
    tooltipToggle_.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0x40FFD700));
    tooltipToggle_.setColour(juce::TextButton::textColourOffId,  juce::Colours::white.withAlpha(0.4f));
    tooltipToggle_.setColour(juce::TextButton::textColourOnId,   juce::Colours::gold);
    tooltipToggle_.onClick = [this] {
        if (tooltipToggle_.getToggleState())
            tooltipWindow_ = std::make_unique<juce::TooltipWindow>(nullptr, 600);
        else
            tooltipWindow_.reset();
    };
    addAndMakeVisible(tooltipToggle_);

    // ----- GL view — added FIRST so devPanel_ (added later) paints on top of it -----
    addAndMakeVisible(glView_);
    glView_.setAccumulator(&listenerAccum_);

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
    // Width — hero rotary knob (gateway control for stereo orbit)
    stereoWidthKnob_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    stereoWidthKnob_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 14);
    addAndMakeVisible(&stereoWidthKnob_);
    stereoWidthLabel_.setText("Width", juce::dontSendNotification);
    stereoWidthLabel_.setJustificationType(juce::Justification::centredRight);
    stereoWidthLabel_.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    addAndMakeVisible(&stereoWidthLabel_);
    // Phase + Offset — rotary knobs (placed in row with Face Observer)
    for (auto* knob : {&orbitPhaseKnob_, &orbitOffsetKnob_}) {
        knob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 14);
        addAndMakeVisible(knob);
    }
    orbitPhaseLabel_.setText("Phase", juce::dontSendNotification);
    orbitPhaseLabel_.setJustificationType(juce::Justification::centredRight);
    orbitPhaseLabel_.setFont(juce::Font(juce::FontOptions(10.0f)));
    addAndMakeVisible(orbitPhaseLabel_);
    orbitOffsetLabel_.setText("Offset", juce::dontSendNotification);
    orbitOffsetLabel_.setJustificationType(juce::Justification::centredRight);
    orbitOffsetLabel_.setFont(juce::Font(juce::FontOptions(10.0f)));
    addAndMakeVisible(orbitOffsetLabel_);

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

    faceListenerToggle_.setButtonText("");
    faceListenerToggle_.setClickingTogglesState(true);
    addAndMakeVisible(faceListenerToggle_);
    faceListenerAtt_ = std::make_unique<BA>(p.apvts, ParamID::STEREO_FACE_LISTENER, faceListenerToggle_);

    faceListenerLabel_.setText("Face Observer", juce::dontSendNotification);
    faceListenerLabel_.setFont(juce::Font(juce::FontOptions(9.0f)));
    faceListenerLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(faceListenerLabel_);

    // ----- Listener head orientation knobs (icon above, value below) -----
    for (auto* knob : {&listenerYawKnob_, &listenerPitchKnob_, &listenerRollKnob_}) {
        knob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 14);
        knob->setTextValueSuffix(juce::String::fromUTF8("\xC2\xB0"));  // degree symbol
        // Full-circle wrap: dragging past +180° wraps to -180° and vice versa.
        // stopAtEnd=false makes the rotary arc seamless (no dead zone).
        knob->setRotaryParameters(juce::MathConstants<float>::pi, 3.0f * juce::MathConstants<float>::pi, false);
        addAndMakeVisible(knob);
    }
    // Per-axis colours matching source/walker: Yaw=Cinnabar, Pitch=Aqua, Roll=Gold
    listenerYawKnob_.setColour(juce::Slider::rotarySliderFillColourId,
                               juce::Colour(xyzpan::AlchemyLookAndFeel::kCinnabar));
    listenerPitchKnob_.setColour(juce::Slider::rotarySliderFillColourId,
                                  juce::Colour(xyzpan::AlchemyLookAndFeel::kAqua));
    listenerRollKnob_.setColour(juce::Slider::rotarySliderFillColourId,
                                 juce::Colour(xyzpan::AlchemyLookAndFeel::kGoldLeaf));

    listenerYawAtt_   = std::make_unique<SA>(p.apvts, ParamID::LISTENER_YAW,   listenerYawKnob_);
    listenerPitchAtt_ = std::make_unique<SA>(p.apvts, ParamID::LISTENER_PITCH, listenerPitchKnob_);
    listenerRollAtt_  = std::make_unique<SA>(p.apvts, ParamID::LISTENER_ROLL,  listenerRollKnob_);

    // Labels hidden — icons above knobs replace text labels
    for (auto* lbl : {&listenerYawLabel_, &listenerPitchLabel_, &listenerRollLabel_})
        lbl->setVisible(false);

    // Roll lock toggle — prevents mouse movement from changing roll
    rollLockBtn_.setButtonText(juce::String::fromUTF8("\xF0\x9F\x94\x93"));  // 🔓 unlock icon
    rollLockBtn_.setClickingTogglesState(true);
    rollLockBtn_.setColour(juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
    rollLockBtn_.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0x40FFD700));  // subtle gold when locked
    rollLockBtn_.setColour(juce::TextButton::textColourOffId,  juce::Colours::white.withAlpha(0.6f));
    rollLockBtn_.setColour(juce::TextButton::textColourOnId,   juce::Colours::gold);
    rollLockBtn_.onClick = [this]() {
        const bool locked = rollLockBtn_.getToggleState();
        listenerAccum_.setRollLocked(locked);
        rollLockBtn_.setButtonText(locked
            ? juce::String::fromUTF8("\xF0\x9F\x94\x92")   // 🔒 locked
            : juce::String::fromUTF8("\xF0\x9F\x94\x93")); // 🔓 unlocked
    };
    addAndMakeVisible(rollLockBtn_);

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
    listenerLinkToggle_.onClick = [this] {
        updateListenerControlsEnabled();
        // Show/hide instance list overlay based on link state
        glView_.setShowInstanceList(listenerLinkToggle_.getToggleState());
    };

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

    walkerXLabel_.setText("X", juce::dontSendNotification);
    walkerYLabel_.setText("Y", juce::dontSendNotification);
    walkerZLabel_.setText("Z", juce::dontSendNotification);
    for (auto* lbl : {&walkerXLabel_, &walkerYLabel_, &walkerZLabel_}) {
        lbl->setJustificationType(juce::Justification::centred);
        lbl->setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
        lbl->setColour(juce::Label::textColourId,
                       juce::Colour(xyzpan::AlchemyLookAndFeel::kGoldLeafPale));
        addAndMakeVisible(lbl);
    }

    // Per-axis arc colours for walker knobs (matching source X/Y/Z)
    walkerXKnob_.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(xyzpan::AlchemyLookAndFeel::kCinnabar));
    walkerYKnob_.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(xyzpan::AlchemyLookAndFeel::kAqua));
    walkerZKnob_.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(xyzpan::AlchemyLookAndFeel::kGoldLeaf));

    wasdToggle_.setButtonText("WASD Control");
    wasdToggle_.setClickingTogglesState(true);
    addAndMakeVisible(wasdToggle_);
    wasdAtt_ = std::make_unique<BA>(p.apvts, ParamID::WASD_CONTROL, wasdToggle_);

    // Left column starts on Source tab; listener + orbit always visible in bottom row
    setActiveLeftTab(LeftTab::Source);
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
    stereoWidthKnob_.onValueChange = [this] { updateOrbitEnabled(); repaint(); };
    updateOrbitEnabled();

    // ----- Sphere Radius knob (bottom row) -----
    sphereRadiusKnob_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    sphereRadiusKnob_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 14);
    addAndMakeVisible(sphereRadiusKnob_);
    sphereRadiusAtt_ = std::make_unique<SA>(p.apvts, ParamID::SPHERE_RADIUS, sphereRadiusKnob_);
    sphereRadiusLabel_.setText("Sphere", juce::dontSendNotification);
    sphereRadiusLabel_.setJustificationType(juce::Justification::centred);
    sphereRadiusLabel_.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
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
    dopplerKnob_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 14);
    addAndMakeVisible(dopplerKnob_);
    dopplerAtt_ = std::make_unique<SA>(p.apvts, ParamID::DIST_DELAY_MAX_MS, dopplerKnob_);
    dopplerLabel_.setText("Doppler", juce::dontSendNotification);
    dopplerLabel_.setJustificationType(juce::Justification::centred);
    dopplerLabel_.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
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

    // Snap buttons removed — camera is orbit-only


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
    rebuildPresetCombo();
    presetCombo_.onChange = [this]() {
        int idx = presetCombo_.getSelectedId() - 1; // Convert back to 0-based
        if (idx >= 0 && idx < proc_.presetManager.getNumPresets()) {
            proc_.presetManager.loadPreset(idx);
            proc_.getUndoManager().clearUndoHistory();
        }
    };

    addAndMakeVisible(presetPrevBtn_);
    presetPrevBtn_.onClick = [this]() {
        int n = proc_.presetManager.getNumPresets();
        if (n == 0) return;
        int cur = proc_.presetManager.getCurrentIndex();
        int next = (cur <= 0) ? n - 1 : cur - 1;
        proc_.presetManager.loadPreset(next);
        proc_.getUndoManager().clearUndoHistory();
        presetCombo_.setSelectedId(next + 1, juce::dontSendNotification);
    };

    addAndMakeVisible(presetNextBtn_);
    presetNextBtn_.onClick = [this]() {
        int n = proc_.presetManager.getNumPresets();
        if (n == 0) return;
        int cur = proc_.presetManager.getCurrentIndex();
        int next = (cur + 1) % n;
        proc_.presetManager.loadPreset(next);
        proc_.getUndoManager().clearUndoHistory();
        presetCombo_.setSelectedId(next + 1, juce::dontSendNotification);
    };

    addAndMakeVisible(presetSaveBtn_);
    presetSaveBtn_.onClick = [this]() {
        auto userDir = PresetManager::getUserPresetDir();
        auto chooser = std::make_shared<juce::FileChooser>(
            "Save Preset", userDir, "*.xml");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file == juce::File{})
                    return;
                auto name = file.getFileNameWithoutExtension();
                proc_.presetManager.saveUserPreset(name);
                rebuildPresetCombo();
            });
    };

    addAndMakeVisible(presetLoadBtn_);
    presetLoadBtn_.onClick = [this]() {
        auto userDir = PresetManager::getUserPresetDir();
        auto chooser = std::make_shared<juce::FileChooser>(
            "Load Preset", userDir, "*.xml");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file == juce::File{} || !file.existsAsFile())
                    return;
                auto xml = juce::parseXML(file);
                if (xml != nullptr && xml->hasTagName(proc_.apvts.state.getType())) {
                    proc_.apvts.replaceState(juce::ValueTree::fromXml(*xml));
                    proc_.getUndoManager().clearUndoHistory();
                    proc_.presetManager.setCurrentIndex(-1);
                }
                presetCombo_.setSelectedId(0, juce::dontSendNotification);
            });
    };

    // ----- Undo / Redo buttons -----
    addAndMakeVisible(undoBtn_);
    undoBtn_.onClick = [this]() { proc_.getUndoManager().undo(); };
    undoBtn_.setTooltip("Undo (Ctrl+Z)");
    undoBtn_.setEnabled(false);

    addAndMakeVisible(redoBtn_);
    redoBtn_.onClick = [this]() { proc_.getUndoManager().redo(); };
    redoBtn_.setTooltip("Redo (Ctrl+Y)");
    redoBtn_.setEnabled(false);

    // ----- User preferences (theme + avatar persistence) -----
    userPrefs_ = std::make_unique<xyzpan::UserPreferences>();
    lookAndFeel_.applyTheme(userPrefs_->activeTheme());
    glView_.setColorTheme(userPrefs_->activeTheme());
    glView_.setAvatarParams(userPrefs_->avatarParams());
    glView_.setSceneParams(userPrefs_->sceneParams());

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

    // ----- Customize tab: sky combo -----
    skyLabel_.setText("Sky", juce::dontSendNotification);
    skyLabel_.setJustificationType(juce::Justification::centredLeft);
    skyLabel_.setFont(juce::Font(juce::FontOptions(11.0f)));
    customizeContent_.addAndMakeVisible(skyLabel_);

    skyCombo_.addItem("None",       1);
    skyCombo_.addItem("Day Clouds", 2);
    skyCombo_.addItem("Night Sky",  3);
    skyCombo_.addItem("Sunset",     4);
    skyCombo_.addItem("Overcast",   5);
    skyCombo_.addItem("Aurora",     6);
    skyCombo_.addItem("Contours",   7);
    skyCombo_.addItem("Voronoi",    8);
    skyCombo_.addItem("Wireframe",  9);
    skyCombo_.addItem("Noise",     10);
    skyCombo_.setSelectedId(userPrefs_->sceneParams().skyType + 1, juce::dontSendNotification);
    skyCombo_.onChange = [this] {
        int idx = skyCombo_.getSelectedId() - 1;
        if (idx >= 0 && idx < xyzpan::kNumSkyTypes) {
            auto sp = userPrefs_->sceneParams();
            sp.skyType = idx;
            userPrefs_->setSceneParams(sp);
            glView_.setSceneParams(sp);
        }
    };
    customizeContent_.addAndMakeVisible(skyCombo_);

    // ----- Customize tab: ground combo -----
    groundLabel_.setText("Ground", juce::dontSendNotification);
    groundLabel_.setJustificationType(juce::Justification::centredLeft);
    groundLabel_.setFont(juce::Font(juce::FontOptions(11.0f)));
    customizeContent_.addAndMakeVisible(groundLabel_);

    groundCombo_.addItem("None",         1);
    groundCombo_.addItem("Grass",        2);
    groundCombo_.addItem("Sand Dunes",   3);
    groundCombo_.addItem("City",         4);
    groundCombo_.addItem("Snow",         5);
    groundCombo_.addItem("Ocean",        6);
    groundCombo_.addItem("Polar Grid",   7);
    groundCombo_.addItem("Contour Map",  8);
    groundCombo_.addItem("Voronoi",      9);
    groundCombo_.addItem("Terraces",        10);
    groundCombo_.addItem("Cartesian Grid", 11);
    groundCombo_.setSelectedId(userPrefs_->sceneParams().groundType + 1, juce::dontSendNotification);
    groundCombo_.onChange = [this] {
        int idx = groundCombo_.getSelectedId() - 1;
        if (idx >= 0 && idx < xyzpan::kNumGroundTypes) {
            auto sp = userPrefs_->sceneParams();
            sp.groundType = idx;
            userPrefs_->setSceneParams(sp);
            glView_.setSceneParams(sp);
        }
    };
    customizeContent_.addAndMakeVisible(groundCombo_);

    // ----- Customize tab: ground height slider -----
    groundHeightSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    groundHeightSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 42, 16);
    groundHeightSlider_.setRange(0.0, 1.0, 0.01);
    groundHeightSlider_.setValue(static_cast<double>(userPrefs_->sceneParams().groundHeight), juce::dontSendNotification);
    groundHeightSlider_.onValueChange = [this] {
        auto sp = userPrefs_->sceneParams();
        sp.groundHeight = static_cast<float>(groundHeightSlider_.getValue());
        userPrefs_->setSceneParams(sp);
        glView_.setSceneParams(sp);
    };
    groundHeightLabel_.setText("Depth", juce::dontSendNotification);
    groundHeightLabel_.setJustificationType(juce::Justification::centredLeft);
    groundHeightLabel_.setFont(juce::Font(juce::FontOptions(10.0f)));
    customizeContent_.addAndMakeVisible(groundHeightSlider_);
    customizeContent_.addAndMakeVisible(groundHeightLabel_);

    // ----- Customize tab: ground hills slider -----
    groundHillsSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    groundHillsSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 42, 16);
    groundHillsSlider_.setRange(0.0, 1.0, 0.01);
    groundHillsSlider_.setValue(static_cast<double>(userPrefs_->sceneParams().groundHills), juce::dontSendNotification);
    groundHillsSlider_.onValueChange = [this] {
        auto sp = userPrefs_->sceneParams();
        sp.groundHills = static_cast<float>(groundHillsSlider_.getValue());
        userPrefs_->setSceneParams(sp);
        glView_.setSceneParams(sp);
    };
    groundHillsLabel_.setText("Hills", juce::dontSendNotification);
    groundHillsLabel_.setJustificationType(juce::Justification::centredLeft);
    groundHillsLabel_.setFont(juce::Font(juce::FontOptions(10.0f)));
    customizeContent_.addAndMakeVisible(groundHillsSlider_);
    customizeContent_.addAndMakeVisible(groundHillsLabel_);

    // ----- Customize tab: swap panels toggle -----
    swapPanels_ = userPrefs_->sceneParams().swapPanels;
    swapPanelsToggle_.setToggleState(swapPanels_, juce::dontSendNotification);
    swapPanelsToggle_.onClick = [this] {
        swapPanels_ = swapPanelsToggle_.getToggleState();
        auto sp = userPrefs_->sceneParams();
        sp.swapPanels = swapPanels_;
        userPrefs_->setSceneParams(sp);
        resized();
        repaint();
    };
    customizeContent_.addAndMakeVisible(swapPanelsToggle_);

    showLabelsToggle_.setToggleState(userPrefs_->sceneParams().showLabels, juce::dontSendNotification);
    showLabelsToggle_.onClick = [this] {
        auto sp = userPrefs_->sceneParams();
        sp.showLabels = showLabelsToggle_.getToggleState();
        userPrefs_->setSceneParams(sp);
        glView_.setSceneParams(sp);
    };
    customizeContent_.addAndMakeVisible(showLabelsToggle_);

    showArrowToggle_.setToggleState(userPrefs_->sceneParams().showArrow, juce::dontSendNotification);
    showArrowToggle_.onClick = [this] {
        auto sp = userPrefs_->sceneParams();
        sp.showArrow = showArrowToggle_.getToggleState();
        userPrefs_->setSceneParams(sp);
        glView_.setSceneParams(sp);
    };
    customizeContent_.addAndMakeVisible(showArrowToggle_);

    // ----- Customize tab: source shape combo -----
    sourceShapeLabel_.setText("Source Shape", juce::dontSendNotification);
    sourceShapeLabel_.setJustificationType(juce::Justification::centredLeft);
    sourceShapeLabel_.setFont(juce::Font(juce::FontOptions(10.0f)));
    customizeContent_.addAndMakeVisible(sourceShapeLabel_);
    sourceShapeCombo_.addItem("Sphere",              1);
    sourceShapeCombo_.addItem("Pyramid",             2);
    sourceShapeCombo_.addItem("Cube",                3);
    sourceShapeCombo_.addItem("Octahedron",          4);
    sourceShapeCombo_.addItem("Ring",                5);
    sourceShapeCombo_.addItem("Cluster: Spheres",    6);
    sourceShapeCombo_.addItem("Cluster: Pyramids",   7);
    sourceShapeCombo_.addItem("Cluster: Cubes",      8);
    sourceShapeCombo_.addItem("Cluster: Octahedrons",9);
    sourceShapeCombo_.addItem("Cluster: Rings",     10);
    sourceShapeCombo_.setSelectedId(userPrefs_->sceneParams().sourceShape + 1, juce::dontSendNotification);
    sourceShapeCombo_.onChange = [this] {
        if (sourceShapeCombo_.getSelectedId() > 0) {
            auto sp = userPrefs_->sceneParams();
            sp.sourceShape = sourceShapeCombo_.getSelectedId() - 1;
            userPrefs_->setSceneParams(sp);
            glView_.setSceneParams(sp);
            proc_.setSourceShape(sp.sourceShape);
            const bool isCluster = sp.sourceShape >= xyzpan::kShapeClusterSpheres;
            clusterCountSlider_.setVisible(isCluster);
            clusterCountLabel_.setVisible(isCluster);
            resized();
        }
    };
    customizeContent_.addAndMakeVisible(sourceShapeCombo_);
    // Set initial processor source shape for cross-instance rendering
    proc_.setSourceShape(userPrefs_->sceneParams().sourceShape);

    // ----- Customize tab: cluster count slider (visible only for cluster shapes) -----
    clusterCountLabel_.setText("Cluster Size", juce::dontSendNotification);
    clusterCountLabel_.setJustificationType(juce::Justification::centredLeft);
    clusterCountLabel_.setFont(juce::Font(juce::FontOptions(10.0f)));
    customizeContent_.addAndMakeVisible(clusterCountLabel_);
    clusterCountSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    clusterCountSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 28, 16);
    clusterCountSlider_.setRange(1.0, 7.0, 1.0);
    clusterCountSlider_.setValue(userPrefs_->sceneParams().clusterCount, juce::dontSendNotification);
    clusterCountSlider_.onValueChange = [this] {
        auto sp = userPrefs_->sceneParams();
        sp.clusterCount = static_cast<int>(clusterCountSlider_.getValue());
        userPrefs_->setSceneParams(sp);
        glView_.setSceneParams(sp);
    };
    customizeContent_.addAndMakeVisible(clusterCountSlider_);
    // Initial visibility based on current shape
    {
        const bool isCluster = userPrefs_->sceneParams().sourceShape >= xyzpan::kShapeClusterSpheres;
        clusterCountSlider_.setVisible(isCluster);
        clusterCountLabel_.setVisible(isCluster);
    }

    showAudibleSphereToggle_.setClickingTogglesState(true);
    showAudibleSphereAtt_ = std::make_unique<BA>(p.apvts, ParamID::SHOW_AUDIBLE_SPHERE, showAudibleSphereToggle_);
    customizeContent_.addAndMakeVisible(showAudibleSphereToggle_);

    // ----- Customize tab: wave count slider (coupled to intensity) -----
    waveCountSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    waveCountSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 16);
    waveCountAtt_ = std::make_unique<SA>(p.apvts, ParamID::WAVE_COUNT, waveCountSlider_);
    waveCountSlider_.onValueChange = [this] {
        const float count = static_cast<float>(waveCountSlider_.getValue());
        // 0.75x opacity per doubling: intensity = 3.5 * (count/3)^log2(0.75)
        const float intensity = 3.5f * std::pow(count / 3.0f, -0.415f);
        if (auto* p = proc_.apvts.getParameter(ParamID::WAVE_INTENSITY))
            p->setValueNotifyingHost(p->convertTo0to1(intensity));
    };
    waveCountLabel_.setText("Waves", juce::dontSendNotification);
    waveCountLabel_.setJustificationType(juce::Justification::centredLeft);
    waveCountLabel_.setFont(juce::Font(juce::FontOptions(10.0f)));
    customizeContent_.addAndMakeVisible(waveCountSlider_);
    customizeContent_.addAndMakeVisible(waveCountLabel_);

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

    // ----- Customize tab: body type combo -----
    bodyTypeLabel_.setText("Body", juce::dontSendNotification);
    bodyTypeLabel_.setJustificationType(juce::Justification::centredLeft);
    bodyTypeLabel_.setFont(juce::Font(juce::FontOptions(10.0f)));
    customizeContent_.addAndMakeVisible(bodyTypeLabel_);
    bodyTypeCombo_.addItem("Solid",  1);
    bodyTypeCombo_.addItem("Grid",   2);
    bodyTypeCombo_.addItem("Ghost",  3);
    bodyTypeCombo_.addItem("Glass",  4);
    bodyTypeCombo_.addItem("None",   5);
    bodyTypeCombo_.setSelectedId(userPrefs_->avatarParams().bodyType + 1, juce::dontSendNotification);
    bodyTypeCombo_.onChange = [this] {
        if (bodyTypeCombo_.getSelectedId() > 0) {
            auto ap = userPrefs_->avatarParams();
            ap.bodyType = bodyTypeCombo_.getSelectedId() - 1;
            userPrefs_->setAvatarParams(ap);
            pushAvatarToGL();
        }
    };
    customizeContent_.addAndMakeVisible(bodyTypeCombo_);

    // Apply initial Cyclops state for eyeSpacing/eyeHeight mode
    syncEyeSpacingSliderMode(userPrefs_->avatarParams().eyeType == xyzpan::kEyeCyclops);
    // Googly slider only active for Googly eyes
    googlySlider_.setEnabled(userPrefs_->avatarParams().eyeType == xyzpan::kEyeGoogly);

    // ----- Customize tab: avatar sliders -----
    auto configAvatarSlider = [this](juce::Slider& s, juce::Label& l, const juce::String& name,
                                      float min, float max, float val, float resetVal) {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 42, 16);
        s.setRange(static_cast<double>(min), static_cast<double>(max), 0.01);
        s.setDoubleClickReturnValue(true, static_cast<double>(resetVal));
        s.setValue(static_cast<double>(val), juce::dontSendNotification);
        l.setText(name, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centredLeft);
        l.setFont(juce::Font(juce::FontOptions(10.0f)));
        customizeContent_.addAndMakeVisible(&s);
        customizeContent_.addAndMakeVisible(&l);
    };

    const auto& ap = userPrefs_->avatarParams();
    configAvatarSlider(headElongationSlider_, headElongationLabel_, "Elongation", 0.3f, 2.5f, ap.headElongation, 1.0f);
    configAvatarSlider(eyeSizeSlider_,        eyeSizeLabel_,        "Eye Size",   0.2f, 3.0f, ap.eyeSize,        1.0f);
    configAvatarSlider(eyeSpacingSlider_,     eyeSpacingLabel_,     "Eye Space",  0.2f, 3.0f, ap.eyeSpacing,     1.0f);
    configAvatarSlider(earSizeSlider_,        earSizeLabel_,        "Ear Size",   0.2f, 3.0f, ap.earSize,        1.0f);
    configAvatarSlider(headSizeSlider_,       headSizeLabel_,       "Head Size",  0.7f, 1.5f, ap.headSize,       1.0f);
    configAvatarSlider(pupilSizeSlider_,     pupilSizeLabel_,     "Pupil Size", 0.0f, 1.0f, ap.pupilSize,      0.35f);
    // Single "Googly" slider drives both gravity (0→1) and spring (1→0) via exponential curves.
    // Curve: gravity = t^0.322, spring = (1-t)^0.322  — concentrates resolution around 0.8.
    // Invert to find initial slider position from stored gravity: t = gravity^(1/0.322)
    {
        float initT = std::pow(std::clamp(ap.googlyGravity, 0.0f, 1.0f), 1.0f / 0.322f);
        configAvatarSlider(googlySlider_, googlyLabel_, "Googly", 0.0f, 1.0f, initT, 0.0f);
    }
    configAvatarSlider(earRotationSlider_,   earRotationLabel_,   "Ear Rot",    -180.0f, 180.0f, ap.earRotation, 0.0f);
    configAvatarSlider(hatSizeSlider_,       hatSizeLabel_,       "Hat Size",   0.2f, 2.0f, ap.hatSize,        1.0f);
    configAvatarSlider(noseSizeSlider_,      noseSizeLabel_,      "Nose Size",  0.2f, 3.0f, ap.noseSize,       1.0f);

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
        // Collapsible AVATAR header
        {
            const int aw = customizeContent_.getWidth();
            auto hdr = juce::Rectangle<int>(0, avatarHeaderY_, aw, 20);
            g.setColour(juce::Colour(0xFFC9A84C).withAlpha(0.15f));
            g.fillRect(hdr);
            g.setColour(juce::Colour(0xFFC9A84C).withAlpha(0.8f));
            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
            juce::String arrow = avatarCollapsed_ ? juce::String(juce::CharPointer_UTF8("\xe2\x96\xb6"))
                                                  : juce::String(juce::CharPointer_UTF8("\xe2\x96\xbc"));
            g.drawText(arrow + " AVATAR", 6, avatarHeaderY_, aw - 12, 20, juce::Justification::centredLeft);
            g.setColour(juce::Colour(0xFFC9A84C).withAlpha(0.4f));
            g.drawHorizontalLine(avatarHeaderY_ + 19, 0.0f, static_cast<float>(aw));
        }

        if (!avatarCollapsed_) {
            g.setColour(juce::Colour(0xFFC9A84C).withAlpha(0.7f));
            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
            g.drawText("EYES", 4, eyesSectionHeaderY_, 60, 16, juce::Justification::centredLeft);
            g.drawText("EARS", 4, earsSectionHeaderY_, 60, 16, juce::Justification::centredLeft);
            g.drawText("NOSE", 4, noseSectionHeaderY_, 60, 16, juce::Justification::centredLeft);
            g.drawText("HATS", 4, hatsSectionHeaderY_, 60, 16, juce::Justification::centredLeft);
        }
    };

    customizeContent_.onMouseDown = [this](const juce::MouseEvent& e) {
        // Click on AVATAR header toggles collapse
        if (e.y >= avatarHeaderY_ && e.y < avatarHeaderY_ + 20) {
            avatarCollapsed_ = !avatarCollapsed_;
            resized();
            customizeContent_.repaint();
        }
    };

    // Set up scrollable viewport for customize tab
    customizeViewport_.setViewedComponent(&customizeContent_, false);
    customizeViewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(customizeViewport_);

    // Customize tab starts hidden
    customizeViewport_.setVisible(false);

    // Remote button removed — instance list now auto-shown when link listener is active

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

    // Cache APVTS parameter pointers for timerCallback (avoid per-frame string lookup)
    cachedWalkerX_       = proc_.apvts.getParameter(ParamID::WALKER_X);
    cachedWalkerY_       = proc_.apvts.getParameter(ParamID::WALKER_Y);
    cachedWalkerZ_       = proc_.apvts.getParameter(ParamID::WALKER_Z);
    cachedListenerRoll_  = proc_.apvts.getParameter(ParamID::LISTENER_ROLL);
    cachedListenerYaw_   = proc_.apvts.getParameter(ParamID::LISTENER_YAW);
    cachedListenerPitch_ = proc_.apvts.getParameter(ParamID::LISTENER_PITCH);
    cachedRawYaw_        = proc_.apvts.getRawParameterValue(ParamID::LISTENER_YAW);
    cachedRawPitch_      = proc_.apvts.getRawParameterValue(ParamID::LISTENER_PITCH);
    cachedRawRoll_       = proc_.apvts.getRawParameterValue(ParamID::LISTENER_ROLL);

    // ----- Tooltips for non-obvious controls -----
    sphereRadiusKnob_.setTooltip("Spatial boundary radius");
    dopplerKnob_.setTooltip("Distance delay (adds latency, not compensated)");
    inputGainKnob_.setTooltip("Pre-processing input boost");
    binauralToggle_.setTooltip("3D spatial cues (ITD, ILD, head shadow, pinna)");
    earlyReflToggle_.setTooltip("Simulated wall reflections");
    faceListenerToggle_.setTooltip("L/R nodes rotate to face the listener");
    rollLockBtn_.setTooltip("Lock roll during head rotation");
    headFollowsToggle_.setTooltip("Head orientation tracks camera angle");
    wasdToggle_.setTooltip("WASD keys move listener position");
    listenerLinkToggle_.setTooltip("Share listener across instances");
    listenerPilotToggle_.setTooltip("Only the pilot moves the shared listener");
    stereoWidthKnob_.setTooltip("Split input into orbiting L/R nodes (0 = mono)");
    orbitPhaseKnob_.setTooltip("L/R orbit starting phase");
    orbitSpeedMulKnob_.setTooltip("Orbit LFO speed multiplier");
    orbitOffsetKnob_.setTooltip("Angular offset between L and R orbits");
    lfoSpeedMulKnob_.setTooltip("Position LFO speed multiplier");
    resetXYZPhasesBtn_.setTooltip("Reset position LFO phases");
    resetOrbitPhasesBtn_.setTooltip("Reset orbit LFO phases");
    devToggle_.setTooltip("Advanced DSP tuning panel");
    listenerYawKnob_.setTooltip("Head yaw (horizontal)");
    listenerPitchKnob_.setTooltip("Head pitch (up/down)");
    listenerRollKnob_.setTooltip("Head roll (tilt)");
    walkerXKnob_.setTooltip("Listener left/right");
    walkerYKnob_.setTooltip("Listener front/back");
    walkerZKnob_.setTooltip("Listener up/down");
    xKnob_.setTooltip("Source left/right position");
    yKnob_.setTooltip("Source front/back position");
    zKnob_.setTooltip("Source up/down position");
    verbSize_.setTooltip("Reverb room size");
    verbDecay_.setTooltip("Reverb tail length");
    verbDamping_.setTooltip("HF absorption in reverb tail");
    verbWet_.setTooltip("Reverb wet/dry mix");

    // Keyboard shortcut listener (captures Alt+R for module randomization, WASD for movement)
    addKeyListener(this);
    startTimerHz(60);  // 60 Hz poll for WASD movement

    // Listen to mouse clicks on all child components for last-clicked zone tracking
    addMouseListener(this, true);

    // Window sizing
    setResizable(true, true);
    setResizeLimits(kMinW, kMinH, kMaxW, kMaxH);
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
XYZPanEditor::Layout XYZPanEditor::Layout::compute(int totalW, int totalH, bool swapPanels)
{
    Layout l;
    l.contentY    = kPresetBarH;  // left column: Source/Customize header starts below preset bar
    l.leftColH    = juce::jmax(1, totalH - kBottomH);  // full height above bottom row
    l.bottomY     = juce::jmax(1, totalH - kBottomH);
    l.reverbX     = juce::jmax(1, totalW - kMeterW - kReverbSectionW);

    if (!swapPanels) {
        // Default: Listener (left, kLeftColW) | Stereo Orbit (right, flex) | Reverb
        l.listenerX   = 0;
        l.listenerW   = kLeftColW;
        l.orbitX      = kLeftColW;
        l.orbitW      = juce::jmax(1, l.reverbX - kLeftColW);
    } else {
        // Swapped: Stereo Orbit (left, kLeftColW) | Listener (right, flex) | Reverb
        l.orbitX      = 0;
        l.orbitW      = kLeftColW;
        l.listenerX   = kLeftColW;
        l.listenerW   = juce::jmax(1, l.reverbX - kLeftColW);
    }

    l.contentTop  = l.bottomY + kSectionHdrH;

    // Left column content: full height between header and bottom row
    l.leftContentTop = l.contentY + kSectionHdrH;
    l.leftContentH   = juce::jmax(1, l.bottomY - l.leftContentTop);

    return l;
}

// ---------------------------------------------------------------------------
// paint
// ---------------------------------------------------------------------------
void XYZPanEditor::paint(juce::Graphics& g)
{
    const auto& ct = lookAndFeel_.currentTheme();

    // If host forces us too small, just fill background and bail
    if (getWidth() < 200 || getHeight() < 200)
    {
        g.fillAll(juce::Colour(ct.background));
        return;
    }

    const auto lo = Layout::compute(getWidth(), getHeight(), swapPanels_);
    const int bw = getWidth() - kMeterW;

    // ===== BACKGROUNDS =====
    g.fillAll(juce::Colour(ct.background));

    // --- Metallic panel painter ---
    // darkMix: 0.0 = pure obsidian (darkest), 1.0 = pure darkIron (lightest)
    // sheenPeak: 0.0–1.0 fraction across width where specular highlight peaks
    // bronzeAlpha: warmth intensity of top-down bronze wash
    // sheenAlpha: intensity of the horizontal specular highlight
    auto paintMetallicPanel = [&](juce::Rectangle<int> rect, float darkMix,
                                  float sheenPeak, float bronzeAlpha, float sheenAlpha) {
        const float x0 = static_cast<float>(rect.getX());
        const float y0 = static_cast<float>(rect.getY());
        const float x1 = static_cast<float>(rect.getRight());
        const float y1 = static_cast<float>(rect.getBottom());
        const float w  = static_cast<float>(rect.getWidth());

        // Base fill: blend between obsidian and darkIron
        auto base = juce::Colour(ct.obsidian).interpolatedWith(
            juce::Colour(ct.darkIron), darkMix);
        g.setColour(base);
        g.fillRect(rect);

        // Vertical gradient: bronze warmth fading from top
        g.setGradientFill(juce::ColourGradient(
            juce::Colour(ct.bronze).withAlpha(bronzeAlpha),
            0.0f, y0,
            juce::Colours::transparentBlack,
            0.0f, y1, false));
        g.fillRect(rect);

        // Horizontal metallic sheen: highlight sweeping to sheenPeak then fading out
        float peakX = x0 + w * sheenPeak;
        g.setGradientFill(juce::ColourGradient(
            juce::Colours::white.withAlpha(0.0f), x0, 0.0f,
            juce::Colours::white.withAlpha(sheenAlpha), peakX, 0.0f, false));
        g.fillRect(rect);
        g.setGradientFill(juce::ColourGradient(
            juce::Colours::white.withAlpha(sheenAlpha), peakX, 0.0f,
            juce::Colours::white.withAlpha(0.0f), x1, 0.0f, false));
        g.fillRect(rect);

        // Top edge highlight (specular ridge)
        g.setGradientFill(juce::ColourGradient(
            juce::Colour(ct.bronze).withAlpha(bronzeAlpha * 2.5f),
            0.0f, y0,
            juce::Colours::transparentBlack,
            0.0f, y0 + 3.0f, false));
        g.fillRect(rect.getX(), rect.getY(), rect.getWidth(), 3);

        // Bottom edge: thin dark inset line
        g.setColour(juce::Colour(ct.obsidian).withAlpha(0.5f));
        g.drawHorizontalLine(rect.getBottom() - 1, x0, x1);
    };

    // Left column background (including preset bar) — base darkIron fill, then metallic panels overlay
    g.setColour(juce::Colour(ct.darkIron));
    g.fillRect(0, 0, kLeftColW, lo.bottomY);

    // Options sub-panel: darkest, prominent sheen (hero knobs)
    if (activeLeftTab_ == LeftTab::Source) {
        const int optTop = lo.leftContentTop;
        paintMetallicPanel({0, optTop, kLeftColW, kOptionsH},
                           0.25f, 0.40f, 0.12f, 0.035f);

        // Source/XYZ position section: slightly lighter, sheen shifted right
        const int srcTop = optTop + kOptionsH;
        const int srcH = lo.bottomY - srcTop;
        if (srcH > 0)
            paintMetallicPanel({0, srcTop, kLeftColW, srcH},
                               0.40f, 0.55f, 0.08f, 0.025f);
    } else {
        // Customize tab: single panel, mid tone
        paintMetallicPanel({0, lo.leftContentTop, kLeftColW, lo.leftContentH},
                           0.38f, 0.50f, 0.09f, 0.028f);
    }

    // Bottom row panels — each slightly different shade
    {
        const int btmY = lo.bottomY;
        const int btmContentY = btmY + kSectionHdrH;
        const int btmContentH = kBottomH - kSectionHdrH;

        // Listener panel: warm, sheen peaks left
        paintMetallicPanel({lo.listenerX, btmContentY, lo.listenerW, btmContentH},
                           0.35f, 0.35f, 0.10f, 0.030f);

        // Stereo Orbit panel: cooler/deeper, sheen centered
        paintMetallicPanel({lo.orbitX, btmContentY, lo.orbitW, btmContentH},
                           0.30f, 0.50f, 0.07f, 0.028f);

        // Reverb panel: slightly brighter, sheen peaks right
        paintMetallicPanel({lo.reverbX, btmContentY, kReverbSectionW, btmContentH},
                           0.45f, 0.60f, 0.11f, 0.032f);
    }

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

    // Left column: tabbed header "Source | Customize"
    {
        auto hdrRect = juce::Rectangle<int>(0, lo.contentY, kLeftColW, kSectionHdrH);
        g.setGradientFill(juce::ColourGradient(
            juce::Colour(ct.bronze).withAlpha(0.25f), 0.0f, static_cast<float>(lo.contentY),
            juce::Colour(ct.darkIron), static_cast<float>(kLeftColW), static_cast<float>(lo.contentY), false));
        g.fillRect(hdrRect);

        const int halfW = kLeftColW / 2;
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));

        g.setColour(activeLeftTab_ == LeftTab::Source
            ? juce::Colour(ct.brightGold) : juce::Colour(ct.bronze));
        g.drawText("Source", 0, lo.contentY, halfW, kSectionHdrH, juce::Justification::centred);

        g.setColour(activeLeftTab_ == LeftTab::Customize
            ? juce::Colour(ct.brightGold) : juce::Colour(ct.bronze));
        g.drawText("Customize", halfW, lo.contentY, kLeftColW - halfW, kSectionHdrH, juce::Justification::centred);

        // Gold underline on active tab
        g.setColour(juce::Colour(ct.brightGold));
        const int ulH = 2;
        if (activeLeftTab_ == LeftTab::Source)
            g.fillRect(halfW / 4, lo.contentY + kSectionHdrH - ulH, halfW / 2, ulH);
        else
            g.fillRect(halfW + halfW / 4, lo.contentY + kSectionHdrH - ulH, (kLeftColW - halfW) / 2, ulH);

        g.setColour(juce::Colour(ct.bronze).withAlpha(0.6f));
        g.drawHorizontalLine(lo.contentY + kSectionHdrH - 1, 0.0f, static_cast<float>(kLeftColW));
    }

    // Remote focus indicator — colored bar below left column header
    if (remoteFocusIndex_ >= 0) {
        const int barH = 3;
        const int barY = lo.contentY + kSectionHdrH;
        g.setColour(kPaletteColours[remoteFocusIndex_ % 8]);
        g.fillRect(0, barY, kLeftColW, barH);
    }

    // Bottom row headers — Listener | Stereo Orbit | Reverb (all always visible)
    {
        const int hdrY = lo.bottomY;

        // "LISTENER" header
        drawHeader(lo.listenerX, hdrY, lo.listenerW, "LISTENER");

        // "STEREO ORBIT" header with width badge
        {
            const int ox = lo.orbitX;
            const int ow = lo.orbitW;
            auto hdrRect = juce::Rectangle<int>(ox, hdrY, ow, kSectionHdrH);
            g.setGradientFill(juce::ColourGradient(
                juce::Colour(ct.bronze).withAlpha(0.25f), static_cast<float>(ox), static_cast<float>(hdrY),
                juce::Colour(ct.darkIron), static_cast<float>(ox + ow), static_cast<float>(hdrY), false));
            g.fillRect(hdrRect);

            g.setColour(juce::Colour(ct.brightGold));
            g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
            g.drawText("STEREO ORBIT", hdrRect.reduced(10, 0), juce::Justification::centredLeft);

            g.setColour(juce::Colour(ct.bronze).withAlpha(0.6f));
            g.drawHorizontalLine(hdrY + kSectionHdrH - 1, static_cast<float>(ox), static_cast<float>(ox + ow));
        }

        // "REVERB" header
        drawHeader(lo.reverbX, hdrY, kReverbSectionW, "REVERB");
    }

    // ===== DIVIDERS — LEFT COLUMN =====
    if (activeLeftTab_ == LeftTab::Source) {
        const int subColW = kLeftColW / 3;
        const int lfoSpeedRowH = 32;

        // Horizontal separator between Options and Source sections
        // (painted by the metallic panel treatment above, just draw dividers below it)
        const int optionsSepY = lo.leftContentTop + kOptionsH;

        // Vertical dividers between X | Y | Z sub-columns (source section to speed row)
        const float divTop = static_cast<float>(optionsSepY);
        const int lfoSpeedSepY = lo.bottomY - lfoSpeedRowH;
        const float divBot = static_cast<float>(lfoSpeedSepY);
        g.setColour(juce::Colour(ct.bronze).withAlpha(0.5f));
        g.drawVerticalLine(subColW,     divTop, divBot);
        g.drawVerticalLine(subColW * 2, divTop, divBot);

        // Thin bronze separator above LFO Speed slider row
        g.setColour(juce::Colour(ct.bronze).withAlpha(0.4f));
        g.drawHorizontalLine(lfoSpeedSepY, 0.0f, static_cast<float>(kLeftColW));
    }

    // ===== DIVIDERS — BOTTOM ROW =====
    // Listener section: vertical dividers between Walker X|Y|Z columns + HEAD ORIENTATION divider
    {
        const int lx = lo.listenerX;
        const int lw = lo.listenerW;
        const int walkerColW = lw / 3;
        const int walkerKnobSz = 80;
        const int bigLabelH = 16;
        const int dividerY = lo.contentTop + bigLabelH + walkerKnobSz + 4;

        // Vertical dividers between Walker X | Y | Z columns (down to orientation divider)
        g.setColour(juce::Colour(ct.bronze).withAlpha(0.5f));
        g.drawVerticalLine(lx + walkerColW,     static_cast<float>(lo.contentTop), static_cast<float>(dividerY));
        g.drawVerticalLine(lx + walkerColW * 2, static_cast<float>(lo.contentTop), static_cast<float>(dividerY));

        // Horizontal divider between walker knobs and YPR knobs
        g.setColour(juce::Colour(ct.bronze).withAlpha(0.4f));
        g.drawHorizontalLine(dividerY, static_cast<float>(lx), static_cast<float>(lx + lw));

        // --- YPR labels + engraved arrows flanking knobs ---
        {
            const int yprTop = dividerY + 6;
            const int yprColW = lw / 3;
            const int yprLabelH = 14;
            const int yprKnobSz = 46;
            const char* yprNames[3] = { "Yaw", "Pitch", "Roll" };

            // Multi-pass engraving: deep shadow, broad glow, highlight catch, main groove
            auto drawEngraved = [&](const juce::String& sym, juce::Rectangle<int> r) {
                // Pass 1: deep shadow (2px down-right, black)
                g.setColour(juce::Colours::black.withAlpha(0.5f));
                g.drawText(sym, r.translated(2, 2), juce::Justification::centred, false);
                // Pass 2: softer shadow (1px down-right)
                g.setColour(juce::Colours::black.withAlpha(0.35f));
                g.drawText(sym, r.translated(1, 1), juce::Justification::centred, false);
                // Pass 3: highlight catch on upper-left edge (light from above-left)
                g.setColour(juce::Colour(ct.bronze).withAlpha(0.12f));
                g.drawText(sym, r.translated(-1, -1), juce::Justification::centred, false);
                // Pass 4: main groove — bronze tinted, well visible against darkIron
                g.setColour(juce::Colour(ct.bronze).withAlpha(0.28f));
                g.drawText(sym, r, juce::Justification::centred, false);
            };

            // Arrow symbols
            auto leftArrow  = juce::String::fromUTF8("\xE2\x86\x90");  // ←
            auto rightArrow = juce::String::fromUTF8("\xE2\x86\x92");  // →
            auto upArrow    = juce::String::fromUTF8("\xE2\x86\x91");  // ↑
            auto downArrow  = juce::String::fromUTF8("\xE2\x86\x93");  // ↓
            // Curved arrows for roll: ⤴ ⤵ ⤶ ⤷
            auto curveTopR  = juce::String::fromUTF8("\xE2\xA4\xB4");  // ⤴ top-right
            auto curveTopL  = juce::String::fromUTF8("\xE2\xA4\xB6");  // ⤶ top-left
            auto curveBotR  = juce::String::fromUTF8("\xE2\xA4\xB5");  // ⤵ bottom-right
            auto curveBotL  = juce::String::fromUTF8("\xE2\xA4\xB7");  // ⤷ bottom-left

            for (int col = 0; col < 3; ++col)
            {
                int colX = lx + col * yprColW;

                // "Yaw" / "Pitch" / "Roll" label
                g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
                g.setColour(juce::Colour(ct.goldLeafPale).withAlpha(0.8f));
                g.drawText(yprNames[col], colX, yprTop, yprColW, yprLabelH,
                           juce::Justification::centred);

                // Knob centre position (matches layoutYPRKnob in resized)
                int kx = colX + (yprColW - yprKnobSz) / 2;
                int ky = yprTop + yprLabelH;
                int knobCY = ky + yprKnobSz / 2;

                if (col == 0) // Yaw: ← on left, → on right of knob
                {
                    const int arrowSz = 32;
                    g.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
                    auto leftR  = juce::Rectangle<int>(kx - arrowSz, knobCY - arrowSz / 2, arrowSz, arrowSz);
                    auto rightR = juce::Rectangle<int>(kx + yprKnobSz, knobCY - arrowSz / 2, arrowSz, arrowSz);
                    drawEngraved(leftArrow, leftR);
                    drawEngraved(rightArrow, rightR);
                }
                else if (col == 1) // Pitch: ↑ on left, ↓ on right of knob
                {
                    const int arrowSz = 32;
                    g.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
                    auto leftR  = juce::Rectangle<int>(kx - arrowSz, knobCY - arrowSz / 2, arrowSz, arrowSz);
                    auto rightR = juce::Rectangle<int>(kx + yprKnobSz, knobCY - arrowSz / 2, arrowSz, arrowSz);
                    drawEngraved(upArrow, leftR);
                    drawEngraved(downArrow, rightR);
                }
                else // Roll: curved arrows on left and right of knob only
                {
                    int knobCX = kx + yprKnobSz / 2;

                    auto drawRotated = [&](const juce::String& sym, juce::Rectangle<int> r, float angle) {
                        float cx = static_cast<float>(r.getCentreX());
                        float cy = static_cast<float>(r.getCentreY());
                        juce::Graphics::ScopedSaveState sss(g);
                        g.addTransform(juce::AffineTransform::rotation(angle, cx, cy));
                        drawEngraved(sym, r);
                    };

                    // Left arrow: ⤶ rotated 215° clockwise
                    {
                        const int arrowSz = 28;
                        g.setFont(juce::Font(juce::FontOptions(26.0f, juce::Font::bold)));
                        int off = yprKnobSz / 2 + 6;
                        auto leftR = juce::Rectangle<int>(knobCX - off - arrowSz / 2, knobCY - arrowSz / 2, arrowSz, arrowSz);
                        drawRotated(curveTopL, leftR, juce::degreesToRadians(125.0f));
                    }

                    // Right arrow: ⤵ rotated 55° clockwise, bigger and further out
                    {
                        const int arrowSz = 32;
                        g.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
                        int off = yprKnobSz / 2 + 8;
                        auto rightR = juce::Rectangle<int>(knobCX + off - arrowSz / 2, knobCY - arrowSz / 2, arrowSz, arrowSz);
                        drawRotated(curveBotR, rightR, juce::degreesToRadians(55.0f));
                    }
                }
            }
        }
    }

    // Vertical divider between Listener and Stereo Orbit sections
    {
        const int divX = swapPanels_ ? lo.listenerX : lo.orbitX;
        g.setColour(juce::Colour(ct.bronze).withAlpha(0.5f));
        g.drawVerticalLine(divX, static_cast<float>(lo.contentTop), static_cast<float>(lo.bottomY + kBottomH));
    }

    // Vertical dividers between orbit LFO strips (XY | XZ | YZ) + horizontal separators
    {
        const int ox = lo.orbitX;
        const int ow = lo.orbitW;
        const int stripW = ow / 3;
        // Layout must match resized(): single top row (hero knob + Phase/Offset) + divGap, then LFOs, then speedRow
        const int sliderH = 20, divGap = 6;
        const int heroSz = 54;
        const int topRowH = heroSz;
        const int speedRowH = sliderH + 4;
        const int topCtrlH = topRowH + divGap;
        const int lfoTop = lo.contentTop + topCtrlH;
        const int contentH = lo.bottomY + kBottomH - lo.contentTop;
        const int lfoH = contentH - topCtrlH - divGap - speedRowH;
        const int lfoBot = lfoTop + lfoH;

        g.setColour(juce::Colour(ct.bronze).withAlpha(0.5f));
        g.drawVerticalLine(ox + stripW,     static_cast<float>(lfoTop), static_cast<float>(lfoBot));
        g.drawVerticalLine(ox + stripW * 2, static_cast<float>(lfoTop), static_cast<float>(lfoBot));
        // Horizontal separator above LFOs (below Width + Phase)
        g.setColour(juce::Colour(ct.bronze).withAlpha(0.4f));
        g.drawHorizontalLine(lfoTop - divGap / 2, static_cast<float>(ox), static_cast<float>(ox + ow));
        // Horizontal separator above Speed row (below LFOs)
        g.drawHorizontalLine(lfoBot + divGap / 2, static_cast<float>(ox), static_cast<float>(ox + ow));
    }

    // Vertical divider between Stereo Orbit and Reverb sections
    g.setColour(juce::Colour(ct.bronze));
    g.fillRect(lo.reverbX - 1, lo.bottomY, 2, kBottomH);

    // ===== MAIN STRUCTURAL DIVIDERS =====
    // Vertical separator (left column | GL view), full left column height
    g.setColour(juce::Colour(ct.bronze));
    g.fillRect(kLeftColW - 1, 0, 2, lo.bottomY);

    // Horizontal separator (above bottom row)
    g.fillRect(0, lo.bottomY, bw, 2);

    // Thin separator below preset bar (left column only)
    g.setColour(juce::Colour(ct.bronze).withAlpha(0.5f));
    g.drawHorizontalLine(kPresetBarH - 1, 0.0f, static_cast<float>(kLeftColW));

    // ===== NOISE TEXTURE OVERLAY =====
    // Tile procedural noise at low opacity over left column and bottom row panels
    if (noiseTexture_.isValid()) {
        const int nw = noiseTexture_.getWidth();
        const int nh = noiseTexture_.getHeight();

        g.saveState();
        g.setOpacity(0.04f);

        // Left column (including preset bar area)
        g.reduceClipRegion(0, 0, kLeftColW, lo.bottomY);
        for (int ty = 0; ty < lo.bottomY; ty += nh)
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
    // If the host forces us below minimum, skip layout to avoid crashes
    if (getWidth() < 200 || getHeight() < 200)
        return;

    auto b = getLocalBounds();

    // Carve out bottom row first (full width)
    auto bottomRow = b.removeFromBottom(kBottomH);

    // Left column: preset bar at top, then content
    auto leftCol = b.removeFromLeft(kLeftColW);
    {
        auto presetBar = leftCol.removeFromTop(kPresetBarH);
        auto saveBtnArea  = presetBar.removeFromRight(54);
        auto loadBtnArea  = presetBar.removeFromRight(54);
        auto redoBtnArea  = presetBar.removeFromRight(38);
        auto undoBtnArea  = presetBar.removeFromRight(38);
        auto nextBtnArea  = presetBar.removeFromRight(24);
        auto prevBtnArea  = presetBar.removeFromRight(24);
        auto tooltipBtnArea = presetBar.removeFromLeft(28);
        tooltipToggle_.setBounds(tooltipBtnArea.reduced(2));
        presetPrevBtn_.setBounds(prevBtnArea.reduced(1, 2));
        presetNextBtn_.setBounds(nextBtnArea.reduced(1, 2));
        presetCombo_.setBounds(presetBar.reduced(kPadding, 2));
        undoBtn_.setBounds(undoBtnArea.reduced(2));
        redoBtn_.setBounds(redoBtnArea.reduced(2));
        presetSaveBtn_.setBounds(saveBtnArea.reduced(2));
        presetLoadBtn_.setBounds(loadBtnArea.reduced(2));
    }

    // Output meter strip — spans left column height only (right edge of window)
    auto meterArea = b.removeFromRight(kMeterW);
    meterArea.setHeight(b.getHeight());  // same height as GL area
    outputMeter_.setBounds(meterArea);

    // Remainder = GL view area (full height above bottom row, no preset bar)
    auto glArea = b;

    // Shared structural geometry — used by both left column and bottom row blocks
    const auto lo = Layout::compute(getWidth(), getHeight(), swapPanels_);

    // Remote instance list is now rendered as GL view overlay — no left column layout needed

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
    const int glW = juce::jmax(1, glArea.getWidth());
    const int glH = juce::jmax(1, glArea.getHeight());
    const int defaultPanelW = static_cast<int>(glW * 0.30f);
    const int panelW = devPanel_.getCustomWidth() > 0
        ? juce::jlimit(200, juce::jmax(201, glW - 50), devPanel_.getCustomWidth())
        : defaultPanelW;
    devPanel_.setBounds(glW - panelW, 0, panelW, glH);

    // ===== LEFT COLUMN — SOURCE | CUSTOMIZE tabs =====
    if (activeLeftTab_ == LeftTab::Source) {
        const int posColW = kLeftColW / 3;
        const int knobH   = 108;
        const int posPad  = 10;
        const int lfoSpeedRowH = 32;

        // --- OPTIONS SECTION (top of left column) ---
        // Single horizontal row: 3 knobs + 2 checkboxes
        const int optTop = lo.leftContentTop;
        {
            const int knobSz = 67;           // 20% bigger than 56
            const int labelH_b = 14;
            const int doppSubLabelH = 12;

            // 3 knob columns (90px) + 2 checkbox columns (100px) = 470
            const int knobColW = 90;
            const int cbColW   = 100;
            const int knobY    = optTop + 6;

            // Sphere (column 0)
            int col0X = 0;
            int sKnobX = col0X + (knobColW - knobSz) / 2;
            sphereRadiusKnob_.setBounds(sKnobX, knobY, knobSz, knobSz);
            sphereRadiusLabel_.setBounds(col0X, knobY + knobSz, knobColW, labelH_b);

            // Doppler (column 1)
            int col1X = knobColW;
            int dKnobX = col1X + (knobColW - knobSz) / 2;
            dopplerKnob_.setBounds(dKnobX, knobY, knobSz, knobSz);
            dopplerLabel_.setBounds(col1X, knobY + knobSz, knobColW, labelH_b);
            dopplerSubLabel_.setBounds(col1X, knobY + knobSz + labelH_b, knobColW, doppSubLabelH);

            // Input Gain (column 2)
            int col2X = knobColW * 2;
            int gKnobX = col2X + (knobColW - knobSz) / 2;
            inputGainKnob_.setBounds(gKnobX, knobY, knobSz, knobSz);
            inputGainLabel_.setBounds(col2X, knobY + knobSz, knobColW, labelH_b);

            // Checkboxes — vertically centered on the knobs
            const int boxSz = 22;
            const int cbGap = 6;
            const int cbY = knobY + (knobSz - boxSz) / 2;

            // Binaural (column 3)
            int col3X = knobColW * 3;
            int cbLabelW = cbColW - boxSz - cbGap - 4;
            binauralToggle_.setBounds(col3X + 4, cbY, boxSz, boxSz);
            binauralLabel_.setBounds(col3X + 4 + boxSz + cbGap, cbY, cbLabelW, boxSz);

            // Early Reflections (column 4)
            int col4X = knobColW * 3 + cbColW;
            earlyReflToggle_.setBounds(col4X + 4, cbY, boxSz, boxSz);
            earlyReflLabel_.setBounds(col4X + 4 + boxSz + cbGap, cbY, cbColW - boxSz - cbGap - 4, boxSz);
        }

        // --- SOURCE POSITION SECTION (below options) ---
        const int sourceTop = optTop + kOptionsH;
        const int posSectionBottom = lo.bottomY - lfoSpeedRowH;

        const int bigLabelH = 20;
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
                     0,            posColW, sourceTop, posSectionBottom);
        layoutPosCol(yKnob_, yLabel_, yLFO_,
                     posColW,      posColW, sourceTop, posSectionBottom);
        layoutPosCol(zKnob_, zLabel_, zLFO_,
                     posColW * 2,  kLeftColW - posColW * 2, sourceTop, posSectionBottom);

        // LFO Speed slider row (bottom of source section)
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
    } else {
        // --- CUSTOMIZE TAB — scrollable viewport fills entire left column content ---
        customizeViewport_.setBounds(0, lo.leftContentTop, kLeftColW, lo.leftContentH);

        const int ow = kLeftColW;
        const int pad = 4;
        const int sliderH  = 20;
        const int comboH   = 22;
        const int labelW   = 68;
        const int gap      = 4;
        const int headerH  = 16;

        int cy = 2;
        themeLabel_.setBounds(pad, cy, labelW, comboH);
        themeCombo_.setBounds(pad + labelW, cy, ow - pad * 2 - labelW, comboH);
        cy += comboH + gap;

        skyLabel_.setBounds(pad, cy, labelW, comboH);
        skyCombo_.setBounds(pad + labelW, cy, ow - pad * 2 - labelW, comboH);
        cy += comboH + gap;

        groundLabel_.setBounds(pad, cy, labelW, comboH);
        groundCombo_.setBounds(pad + labelW, cy, ow - pad * 2 - labelW, comboH);
        cy += comboH + gap;

        groundHeightLabel_.setBounds(pad, cy, labelW, sliderH);
        groundHeightSlider_.setBounds(pad + labelW, cy, ow - pad * 2 - labelW, sliderH);
        cy += sliderH + gap;

        groundHillsLabel_.setBounds(pad, cy, labelW, sliderH);
        groundHillsSlider_.setBounds(pad + labelW, cy, ow - pad * 2 - labelW, sliderH);
        cy += sliderH + gap;

        swapPanelsToggle_.setBounds(pad, cy, ow - pad * 2, comboH);
        cy += comboH + gap;

        showLabelsToggle_.setBounds(pad, cy, ow - pad * 2, comboH);
        cy += comboH + gap;

        showArrowToggle_.setBounds(pad, cy, ow - pad * 2, comboH);
        cy += comboH + gap;

        sourceShapeLabel_.setBounds(pad, cy, labelW, comboH);
        sourceShapeCombo_.setBounds(pad + labelW, cy, ow - pad * 2 - labelW, comboH);
        cy += comboH + gap;

        if (clusterCountSlider_.isVisible()) {
            clusterCountLabel_.setBounds(pad, cy, labelW, sliderH);
            clusterCountSlider_.setBounds(pad + labelW, cy, ow - pad * 2 - labelW, sliderH);
            cy += sliderH + gap;
        }

        showAudibleSphereToggle_.setBounds(pad, cy, ow - pad * 2, comboH);
        cy += comboH + gap;

        waveCountLabel_.setBounds(pad, cy, labelW, sliderH);
        waveCountSlider_.setBounds(pad + labelW, cy, ow - pad * 2 - labelW, sliderH);
        cy += sliderH + gap;

        // --- Collapsible AVATAR header ---
        avatarHeaderY_ = cy;
        cy += 20 + gap;  // header height 20px

        // All avatar components — toggled visible/invisible based on collapse state
        auto avatarComponents = {
            (juce::Component*)&bodyTypeCombo_,   (juce::Component*)&bodyTypeLabel_,
            (juce::Component*)&headColorSwatch_, (juce::Component*)&headColorLabel_,
            (juce::Component*)&headSizeSlider_,  (juce::Component*)&headSizeLabel_,
            (juce::Component*)&headElongationSlider_, (juce::Component*)&headElongationLabel_,
            (juce::Component*)&eyeTypeCombo_,    (juce::Component*)&eyeTypeLabel_,
            (juce::Component*)&eyeSizeSlider_,   (juce::Component*)&eyeSizeLabel_,
            (juce::Component*)&eyeSpacingSlider_,(juce::Component*)&eyeSpacingLabel_,
            (juce::Component*)&pupilSizeSlider_, (juce::Component*)&pupilSizeLabel_,
            (juce::Component*)&googlySlider_,    (juce::Component*)&googlyLabel_,
            (juce::Component*)&eyeColorSwatch_,  (juce::Component*)&eyeColorLabel_,
            (juce::Component*)&earTypeCombo_,    (juce::Component*)&earTypeLabel_,
            (juce::Component*)&earSizeSlider_,   (juce::Component*)&earSizeLabel_,
            (juce::Component*)&earRotationSlider_,(juce::Component*)&earRotationLabel_,
            (juce::Component*)&noseTypeCombo_,   (juce::Component*)&noseTypeLabel_,
            (juce::Component*)&noseSizeSlider_,  (juce::Component*)&noseSizeLabel_,
            (juce::Component*)&noseColorSwatch_, (juce::Component*)&noseColorLabel_,
            (juce::Component*)&hatTypeCombo_,    (juce::Component*)&hatTypeLabel_,
            (juce::Component*)&hatSizeSlider_,   (juce::Component*)&hatSizeLabel_,
            (juce::Component*)&hatColorSwatch_,  (juce::Component*)&hatColorLabel_
        };

        if (avatarCollapsed_) {
            for (auto* c : avatarComponents)
                c->setVisible(false);
        } else {
            for (auto* c : avatarComponents)
                c->setVisible(true);

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

            bodyTypeLabel_.setBounds(pad, cy, labelW, comboH);
            bodyTypeCombo_.setBounds(pad + labelW, cy, ow - pad * 2 - labelW, comboH);
            cy += comboH + gap;
            placeColorSwatch(headColorSwatch_,  headColorLabel_);
            placeAvatarSlider(headSizeSlider_,       headSizeLabel_);
            placeAvatarSlider(headElongationSlider_, headElongationLabel_);

            eyesSectionHeaderY_ = cy;
            cy += headerH + gap;
            eyeTypeLabel_.setBounds(pad, cy, labelW, comboH);
            eyeTypeCombo_.setBounds(pad + labelW, cy, ow - pad * 2 - labelW, comboH);
            cy += comboH + gap;
            placeAvatarSlider(eyeSizeSlider_,        eyeSizeLabel_);
            placeAvatarSlider(eyeSpacingSlider_,     eyeSpacingLabel_);
            placeAvatarSlider(pupilSizeSlider_,      pupilSizeLabel_);
            placeAvatarSlider(googlySlider_,          googlyLabel_);
            placeColorSwatch(eyeColorSwatch_,   eyeColorLabel_);

            earsSectionHeaderY_ = cy;
            cy += headerH + gap;
            earTypeLabel_.setBounds(pad, cy, labelW, comboH);
            earTypeCombo_.setBounds(pad + labelW, cy, ow - pad * 2 - labelW, comboH);
            cy += comboH + gap;
            placeAvatarSlider(earSizeSlider_,        earSizeLabel_);
            placeAvatarSlider(earRotationSlider_,    earRotationLabel_);

            noseSectionHeaderY_ = cy;
            cy += headerH + gap;
            noseTypeLabel_.setBounds(pad, cy, labelW, comboH);
            noseTypeCombo_.setBounds(pad + labelW, cy, ow - pad * 2 - labelW, comboH);
            cy += comboH + gap;
            placeAvatarSlider(noseSizeSlider_, noseSizeLabel_);
            placeColorSwatch(noseColorSwatch_,  noseColorLabel_);

            hatsSectionHeaderY_ = cy;
            cy += headerH + gap;
            hatTypeLabel_.setBounds(pad, cy, labelW, comboH);
            hatTypeCombo_.setBounds(pad + labelW, cy, ow - pad * 2 - labelW, comboH);
            cy += comboH + gap;
            placeAvatarSlider(hatSizeSlider_, hatSizeLabel_);
            placeColorSwatch(hatColorSwatch_,   hatColorLabel_);
        }

        customizeContent_.setSize(ow, cy + 2);
    }

    // ===== BOTTOM ROW =====
    {
        const int contentH = kBottomH - kSectionHdrH;

        // Use shared Layout for structural geometry (reverb/orbit split)
        const int contentTop = lo.contentTop;
        const int reverbX    = lo.reverbX;

        // --- LISTENER SECTION (always visible) ---
        {
            const int lx = lo.listenerX;
            const int lw = lo.listenerW;
            const int walkerKnobSz = 80;
            const int bigLabelH = 16;
            const int posPad = 10;
            const int walkerColW = lw / 3;

            // Walker X/Y/Z — top row of 3 big knobs
            auto layoutWalkerKnob = [&](juce::Slider& knob, juce::Label& label,
                                        int colX, int top) {
                label.setBounds(lx + colX, top, walkerColW, bigLabelH);
                int kw = juce::jmin(walkerKnobSz, walkerColW - posPad * 2);
                int kx = lx + colX + (walkerColW - kw) / 2;
                knob.setBounds(kx, top + bigLabelH, kw, walkerKnobSz);
            };

            layoutWalkerKnob(walkerXKnob_, walkerXLabel_, 0,              contentTop);
            layoutWalkerKnob(walkerYKnob_, walkerYLabel_, walkerColW,     contentTop);
            layoutWalkerKnob(walkerZKnob_, walkerZLabel_, walkerColW * 2, contentTop);

            // Divider between walker knobs and YPR knobs (painted in paint())
            const int dividerY = contentTop + bigLabelH + walkerKnobSz + 4;

            // Yaw/Pitch/Roll — vertical stack: label (14) + knob (46) + textBelow (14)
            const int yprLabelH   = 14;
            const int yprKnobSz   = 46;
            const int yprTextH    = 14;
            const int yprTotalH   = yprLabelH + yprKnobSz + yprTextH; // 74
            const int yprTop = dividerY + 6;  // small gap after divider line

            auto layoutYPRKnob = [&](juce::Slider& knob, int colX, int top) {
                const int colW = lw / 3;
                int kw = yprKnobSz;
                int kx = lx + colX + (colW - kw) / 2;
                int ky = top + yprLabelH;
                knob.setBounds(kx, ky, kw, yprKnobSz + yprTextH);
            };

            const int yprColW = lw / 3;
            layoutYPRKnob(listenerYawKnob_,   0,              yprTop);
            layoutYPRKnob(listenerPitchKnob_, yprColW,        yprTop);
            layoutYPRKnob(listenerRollKnob_,  yprColW * 2,    yprTop);

            // Roll lock button — to the right of "Roll" label text
            {
                const int lockSz = 24;
                const int rollColX = yprColW * 2;
                const int lockX = lx + rollColX + yprColW - lockSz - 4;
                const int lockY = yprTop + (yprLabelH - lockSz) / 2;
                rollLockBtn_.setBounds(lockX, lockY, lockSz, lockSz);
            }

            // Toggles below YPR knobs
            const int toggleY = yprTop + yprTotalH + 4;
            const int bigToggleH = 20;
            const int togglePad = 10;
            const int toggleW = lw - togglePad * 2;

            const int halfW = (toggleW - togglePad) / 2;
            wasdToggle_.setBounds(lx + togglePad, toggleY, halfW, bigToggleH);
            headFollowsToggle_.setBounds(lx + togglePad + halfW + togglePad, toggleY, halfW, bigToggleH);

            // Link / Pilot / Remote row — same column split as above
            const int linkY = toggleY + bigToggleH + 4;
            const int smallToggleH = 20;
            listenerLinkToggle_.setBounds(lx + togglePad, linkY, halfW, smallToggleH);
            listenerPilotToggle_.setBounds(lx + togglePad + halfW + togglePad, linkY, halfW, smallToggleH);
            pilotStatusLabel_.setBounds(lx + togglePad, linkY + smallToggleH + 1, lw - togglePad * 2, 14);
        }

        // --- STEREO ORBIT SECTION (always visible, right of listener) ---
        {
            const int ox = lo.orbitX;
            const int ow = lo.orbitW;
            const int pad = 10;
            const int labelW = 46;
            const int sliderH = 20;
            const int divGap = 6;  // space for horizontal divider

            // Top row: [label Width knob] [label Phase knob] [label Offset knob] [Face Observer]
            // Evenly distributed across full panel width.
            const int heroSz = 54;    // Width knob — slightly larger than siblings
            const int knobSz = 50;    // Phase / Offset
            const int topRowH = heroSz;
            const int topCtrlH = topRowH + divGap;

            const int speedRowH = sliderH + 4;  // speed row just below LFOs
            const int lfoH = contentH - topCtrlH - divGap - speedRowH;
            const int maxStripW = 185;
            const int lfoTotalW = juce::jmin(ow, maxStripW * 3);
            const int stripW = lfoTotalW / 3;
            const int lastStripW = lfoTotalW - stripW * 2;

            {
                const int rowY = contentTop;
                const int boxSz = 16;
                const int cbGap = 3;
                const int cbLabelW = 64;

                // Measure fixed content widths
                const int widthLabelW = 36;   // "Width"
                const int knobLabelW  = 36;   // "Phase" / "Offset"
                const int widthCellW  = widthLabelW + heroSz;
                const int phaseCellW  = knobLabelW + knobSz;
                const int offsetCellW = knobLabelW + knobSz;
                const int faceCellW   = boxSz + cbGap + cbLabelW;

                const int totalFixedW = widthCellW + phaseCellW + offsetCellW + faceCellW;
                const int usableW = ow - pad * 2;
                const int totalGap = usableW - totalFixedW;
                const int gapCount = 3;  // gaps between 4 items
                const int gap = juce::jmax(8, totalGap / gapCount);

                int cx = ox + pad;

                // Width — label left, hero knob right
                stereoWidthLabel_.setBounds(cx, rowY + (topRowH - 14) / 2, widthLabelW, 14);
                stereoWidthKnob_.setBounds(cx + widthLabelW, rowY + (topRowH - heroSz) / 2, heroSz, heroSz);
                cx += widthCellW + gap;

                // Phase — label left, knob right
                orbitPhaseLabel_.setBounds(cx, rowY + (topRowH - 14) / 2, knobLabelW, 14);
                orbitPhaseKnob_.setBounds(cx + knobLabelW, rowY + (topRowH - knobSz) / 2, knobSz, knobSz);
                cx += phaseCellW + gap;

                // Offset — label left, knob right
                orbitOffsetLabel_.setBounds(cx, rowY + (topRowH - 14) / 2, knobLabelW, 14);
                orbitOffsetKnob_.setBounds(cx + knobLabelW, rowY + (topRowH - knobSz) / 2, knobSz, knobSz);
                cx += offsetCellW + gap;

                // Face Observer — checkbox + label, vertically centered
                const int cbCenterY = rowY + (topRowH - boxSz) / 2;
                faceListenerToggle_.setBounds(cx, cbCenterY, boxSz, boxSz);
                faceListenerLabel_.setBounds(cx + boxSz + cbGap, cbCenterY - 2, cbLabelW, boxSz + 4);
            }

            // LFO strips below top controls (after divider gap)
            const int lfoTop = contentTop + topCtrlH;
            orbitXYLFO_.setBounds(ox,              lfoTop, stripW,     lfoH);
            orbitXZLFO_.setBounds(ox + stripW,     lfoTop, stripW,     lfoH);
            orbitYZLFO_.setBounds(ox + stripW * 2, lfoTop, lastStripW, lfoH);

            // Speed + Reset — just below LFOs (tight)
            {
                const int sy = lfoTop + lfoH + 2;
                const int resetBtnW = 44;
                const int resetBtnGap = 4;
                orbitSpeedMulLabel_.setBounds(ox + pad, sy, labelW, sliderH);
                orbitSpeedMulKnob_.setBounds(ox + pad + labelW, sy,
                                              ow - pad * 2 - labelW - resetBtnW - resetBtnGap,
                                              sliderH);
                resetOrbitPhasesBtn_.setBounds(ox + ow - pad - resetBtnW, sy,
                                               resetBtnW, sliderH);
            }

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
                    (juce::Component*)&faceListenerToggle_,  (juce::Component*)&faceListenerLabel_,
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

    // Re-apply reverb / options knobs to LFO accent
    for (auto* knob : {&sphereRadiusKnob_, &dopplerKnob_, &inputGainKnob_,
                        &verbSize_, &verbDecay_, &verbDamping_, &verbWet_})
        knob->setColour(juce::Slider::rotarySliderFillColourId,
                         juce::Colour(theme.lfoAccent));

    // Yaw/Pitch/Roll knobs — per-axis colors (matching source X/Y/Z)
    listenerYawKnob_.setColour(juce::Slider::rotarySliderFillColourId,
                                juce::Colour(xyzpan::AlchemyLookAndFeel::kCinnabar));
    listenerPitchKnob_.setColour(juce::Slider::rotarySliderFillColourId,
                                  juce::Colour(xyzpan::AlchemyLookAndFeel::kAqua));
    listenerRollKnob_.setColour(juce::Slider::rotarySliderFillColourId,
                                 juce::Colour(xyzpan::AlchemyLookAndFeel::kGoldLeaf));

    // Walker knobs — per-axis colors (matching source X/Y/Z)
    walkerXKnob_.setColour(juce::Slider::rotarySliderFillColourId,
                            juce::Colour(xyzpan::AlchemyLookAndFeel::kCinnabar));
    walkerYKnob_.setColour(juce::Slider::rotarySliderFillColourId,
                            juce::Colour(xyzpan::AlchemyLookAndFeel::kAqua));
    walkerZKnob_.setColour(juce::Slider::rotarySliderFillColourId,
                            juce::Colour(xyzpan::AlchemyLookAndFeel::kGoldLeaf));

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
// setActiveLeftTab — switch left column between Source and Customize
// ---------------------------------------------------------------------------
void XYZPanEditor::setActiveLeftTab(LeftTab tab)
{
    activeLeftTab_ = tab;
    const bool source    = (tab == LeftTab::Source);
    const bool customize = (tab == LeftTab::Customize);

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

    // Options controls (part of source tab)
    sphereRadiusKnob_.setVisible(source);
    sphereRadiusLabel_.setVisible(source);
    dopplerKnob_.setVisible(source);
    dopplerLabel_.setVisible(source);
    dopplerSubLabel_.setVisible(source);
    inputGainKnob_.setVisible(source);
    inputGainLabel_.setVisible(source);
    binauralToggle_.setVisible(source);
    binauralLabel_.setVisible(source);
    earlyReflToggle_.setVisible(source);
    earlyReflLabel_.setVisible(source);

    // Customize tab (scrollable viewport)
    customizeViewport_.setVisible(customize);

    // Only trigger relayout if we've been sized (avoid crash during construction)
    if (getWidth() > 0 && getHeight() > 0) {
        resized();
        repaint();
    }
}

void XYZPanEditor::updateListenerControlsEnabled() {
    const bool linked   = proc_.isLinkedPilot() || proc_.isLinkedNonPilot();
    const bool isPilot  = proc_.isLinkedPilot();
    const bool canControl = !linked || isPilot;

    // Pilot toggle only meaningful when linked — disable and uncheck when not linked
    listenerPilotToggle_.setEnabled(linked);
    if (!linked && listenerPilotToggle_.getToggleState())
        listenerPilotToggle_.setToggleState(false, juce::dontSendNotification);

    // Sync toggle state from hub truth (covers cross-instance pilot revocation)
    if (linked && listenerPilotToggle_.getToggleState() != isPilot)
        listenerPilotToggle_.setToggleState(isPilot, juce::dontSendNotification);

    // Pilot status label — show who the pilot is when this instance isn't it
    if (linked && !isPilot) {
        auto name = proc_.getPilotName();
        if (name.isNotEmpty())
            pilotStatusLabel_.setText(name + " is the pilot", juce::dontSendNotification);
        else
            pilotStatusLabel_.setText("No pilot set", juce::dontSendNotification);
        pilotStatusLabel_.setVisible(true);  // listener always visible in bottom row
    } else {
        pilotStatusLabel_.setText("", juce::dontSendNotification);
        pilotStatusLabel_.setVisible(false);
    }

    // Instance list overlay follows link state
    glView_.setShowInstanceList(linked);

    // Notify GL view of non-pilot status so it can gate head-follows on the GL thread
    glView_.setLinkedNonPilot(linked && !isPilot);

    // Walker + orientation knobs: disabled when linked-non-pilot
    for (auto* knob : {&walkerXKnob_, &walkerYKnob_, &walkerZKnob_,
                       &listenerYawKnob_, &listenerPitchKnob_, &listenerRollKnob_})
        knob->setEnabled(canControl);
    wasdToggle_.setEnabled(canControl);
    headFollowsToggle_.setEnabled(canControl);

    // Force head-follows OFF when linked-non-pilot to prevent broken camera movement
    if (linked && !isPilot && headFollowsToggle_.getToggleState())
        headFollowsToggle_.setToggleState(false, juce::sendNotificationSync);
}

// Source|Customize toggled by left column tab; Listener + Orbit always visible in bottom row.

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

    const auto lo = Layout::compute(getWidth(), getHeight(), swapPanels_);

    // Hit-test left column tab header — "Source | Customize"
    if (pos.y >= lo.contentY && pos.y < lo.contentY + kSectionHdrH && pos.x < kLeftColW) {
        const int halfW = kLeftColW / 2;
        if (pos.x < halfW)
            setActiveLeftTab(LeftTab::Source);
        else
            setActiveLeftTab(LeftTab::Customize);
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
    // Consume WASDQE + Space/X keys when WASD control is active (movement in timerCallback)
    if (wasdToggle_.getToggleState()) {
        const int k = key.getKeyCode();
        if (k == 'W' || k == 'w' || k == 'A' || k == 'a' ||
            k == 'S' || k == 's' || k == 'D' || k == 'd' ||
            k == 'Q' || k == 'q' || k == 'E' || k == 'e' ||
            k == juce::KeyPress::spaceKey)
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

    if (key.getModifiers().isCtrlDown()) {
        if (key.getKeyCode() == 'Z') {
            if (key.getModifiers().isShiftDown())
                proc_.getUndoManager().redo();
            else
                proc_.getUndoManager().undo();
            return true;
        }
        if (key.getKeyCode() == 'Y') {
            proc_.getUndoManager().redo();
            return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// endWasdGestureIfActive — close automation gesture on walker + roll params
// ---------------------------------------------------------------------------
void XYZPanEditor::endWasdGestureIfActive()
{
    if (!wasdGestureActive_) return;
    if (cachedWalkerX_)      cachedWalkerX_->endChangeGesture();
    if (cachedWalkerY_)      cachedWalkerY_->endChangeGesture();
    if (cachedWalkerZ_)      cachedWalkerZ_->endChangeGesture();
    if (cachedListenerRoll_) cachedListenerRoll_->endChangeGesture();
    wasdGestureActive_ = false;
}

// ---------------------------------------------------------------------------
// timerCallback — WASD movement: move walker in the direction the head is looking
// ---------------------------------------------------------------------------
void XYZPanEditor::timerCallback()
{
    // Update undo/redo button enabled state
    undoBtn_.setEnabled(proc_.getUndoManager().canUndo());
    redoBtn_.setEnabled(proc_.getUndoManager().canRedo());

    // Show audible sphere while sphere knob is hovered or being dragged
    glView_.setSphereKnobActive(sphereRadiusKnob_.isMouseOver(true)
                                || sphereRadiusKnob_.isMouseButtonDown());

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
        // Keep GL view instance name + pilot status in sync
        glView_.setOwnInstanceName(proc_.getInstanceNameValue());
        glView_.setOwnIsPilot(proc_.getListenerHub().isPilot(&proc_));
        glView_.setLinkedNonPilot(proc_.isLinkedNonPilot());
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

    // Check which WASDQEZX keys are currently held
    const bool w = juce::KeyPress::isKeyCurrentlyDown('W') || juce::KeyPress::isKeyCurrentlyDown('w');
    const bool a = juce::KeyPress::isKeyCurrentlyDown('A') || juce::KeyPress::isKeyCurrentlyDown('a');
    const bool s = juce::KeyPress::isKeyCurrentlyDown('S') || juce::KeyPress::isKeyCurrentlyDown('s');
    const bool d = juce::KeyPress::isKeyCurrentlyDown('D') || juce::KeyPress::isKeyCurrentlyDown('d');
    const bool q = juce::KeyPress::isKeyCurrentlyDown('Q') || juce::KeyPress::isKeyCurrentlyDown('q');
    const bool e = juce::KeyPress::isKeyCurrentlyDown('E') || juce::KeyPress::isKeyCurrentlyDown('e');
    const bool space = juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey);
    const bool ctrl = juce::ModifierKeys::currentModifiers.isCtrlDown();

    if (!w && !a && !s && !d && !q && !e && !space && !ctrl) {
        endWasdGestureIfActive();
        return;
    }

    auto* px = cachedWalkerX_;
    auto* py = cachedWalkerY_;
    auto* pz = cachedWalkerZ_;
    auto* pr = cachedListenerRoll_;
    if (px == nullptr || py == nullptr || pz == nullptr) return;

    // Begin gesture for all movement params at once (ended together in endWasdGestureIfActive)
    if (!wasdGestureActive_) {
        px->beginChangeGesture();
        py->beginChangeGesture();
        pz->beginChangeGesture();
        if (pr) pr->beginChangeGesture();
        wasdGestureActive_ = true;
    }

    // --- Q/E: Roll control via quaternion accumulator ---
    if ((q || e) && pr != nullptr) {
        constexpr float rollSpeed = 2.8f;  // degrees per 60Hz tick (~168°/sec)
        float rollDelta = 0.0f;
        if (e) rollDelta += rollSpeed;
        if (q) rollDelta -= rollSpeed;

        listenerAccum_.drivingFromInput->store(true, std::memory_order_relaxed);
        listenerAccum_.applyRollDelta(rollDelta);
        auto rpy = listenerAccum_.bakeRPY();
        if (cachedListenerYaw_)
            cachedListenerYaw_->setValueNotifyingHost(cachedListenerYaw_->convertTo0to1(rpy.yawDeg));
        if (cachedListenerPitch_)
            cachedListenerPitch_->setValueNotifyingHost(cachedListenerPitch_->convertTo0to1(rpy.pitchDeg));
        pr->setValueNotifyingHost(pr->convertTo0to1(rpy.rollDeg));
        listenerAccum_.drivingFromInput->store(false, std::memory_order_relaxed);
    }

    // --- WASD + Space/X: Position movement ---
    if (w || a || s || d || space || ctrl) {
        float fwd = 0.0f, strafe = 0.0f, vert = 0.0f;
        if (w) fwd      += 1.0f;
        if (s) fwd      -= 1.0f;
        if (d) strafe   += 1.0f;
        if (a) strafe   -= 1.0f;
        if (space) vert += 1.0f;   // Space = up
        if (ctrl) vert  -= 1.0f;   // Ctrl = down

        constexpr float kDeg2Rad = 3.14159265358979323846f / 180.0f;
        const float yaw   = cachedRawYaw_->load()   * kDeg2Rad;
        const float pitch = cachedRawPitch_->load() * kDeg2Rad;
        const float roll  = cachedRawRoll_->load()  * kDeg2Rad;

        using LUT = xyzpan::dsp::SineLUT;
        const float cosY = LUT::cosLookupAngle(yaw);
        const float sinY = LUT::lookupAngle(yaw);
        const float cosP = LUT::cosLookupAngle(pitch);
        const float sinP = LUT::lookupAngle(pitch);
        const float cosR = LUT::cosLookupAngle(roll);
        const float sinR = LUT::lookupAngle(roll);

        // Rotation matrix R = Rz(yaw) * Rx(pitch) * Ry(roll)
        // Engine coords: X=right, Y=forward, Z=up
        // Yaw around Z, pitch around X, roll around Y (forward axis)

        // Forward (Y-axis column of R — roll doesn't change gaze direction)
        const float fwdX = -sinY * cosP;
        const float fwdY =  cosY * cosP;
        const float fwdZ =  sinP;

        // Right (X-axis column of R)
        const float rightX =  cosY * cosR - sinY * sinP * sinR;
        const float rightY =  sinY * cosR + cosY * sinP * sinR;
        const float rightZ = -cosP * sinR;

        // Up (Z-axis column of R)
        const float upX = cosY * sinR + sinY * sinP * cosR;
        const float upY = sinY * sinR - cosY * sinP * cosR;
        const float upZ = cosP * cosR;

        const float dx = fwd * fwdX + strafe * rightX + vert * upX;
        const float dy = fwd * fwdY + strafe * rightY + vert * upY;
        const float dz = fwd * fwdZ + strafe * rightZ + vert * upZ;

        constexpr float speed = 0.008f;
        const float newX = juce::jlimit(-1.0f, 1.0f, px->convertFrom0to1(px->getValue()) + dx * speed);
        const float newY = juce::jlimit(-1.0f, 1.0f, py->convertFrom0to1(py->getValue()) + dy * speed);
        const float newZ = juce::jlimit(-1.0f, 1.0f, pz->convertFrom0to1(pz->getValue()) + dz * speed);

        px->setValueNotifyingHost(px->convertTo0to1(newX));
        py->setValueNotifyingHost(py->convertTo0to1(newY));
        pz->setValueNotifyingHost(pz->convertTo0to1(newZ));
    }
}

// ---------------------------------------------------------------------------
// classifyRandZone — determine which UI module a point falls in
// ---------------------------------------------------------------------------
XYZPanEditor::RandZone XYZPanEditor::classifyRandZone(juce::Point<int> pos) const
{
    const auto lo = Layout::compute(getWidth(), getHeight(), swapPanels_);
    const int x = pos.x;
    const int y = pos.y;

    // Left column: depends on active tab (Source or Customize)
    if (x >= 0 && x < kLeftColW && y >= kPresetBarH && y < lo.bottomY) {
        if (activeLeftTab_ == LeftTab::Customize) {
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

        // Source tab — check individual LFO strips
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

        // Listener section
        if (x >= lo.listenerX && x < lo.listenerX + lo.listenerW && y >= lo.contentTop)
            return RandZone::Perspective;

        // Stereo Orbit section
        if (x >= lo.orbitX && x < lo.orbitX + lo.orbitW && y >= lo.contentTop) {
            auto inBounds = [&](const LFOStrip& strip) {
                auto b = strip.getBounds();
                return b.contains(x, y);
            };
            if (inBounds(orbitXYLFO_)) return RandZone::OrbitLfoXY;
            if (inBounds(orbitXZLFO_)) return RandZone::OrbitLfoXZ;
            if (inBounds(orbitYZLFO_)) return RandZone::OrbitLfoYZ;
            return RandZone::StereoOrbit;
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
// randomizePerspective — Yaw, Pitch, Roll + Walker X, Y, Z
// ---------------------------------------------------------------------------
void XYZPanEditor::randomizePerspective()
{
    randomizeAPVTSParam(ParamID::LISTENER_YAW,   -180.0f, 180.0f);
    randomizeAPVTSParam(ParamID::LISTENER_PITCH,  -180.0f, 180.0f);
    randomizeAPVTSParam(ParamID::LISTENER_ROLL,   -180.0f, 180.0f);
    randomizeAPVTSParam(ParamID::WALKER_X,        -1.0f, 1.0f);
    randomizeAPVTSParam(ParamID::WALKER_Y,        -1.0f, 1.0f);
    randomizeAPVTSParam(ParamID::WALKER_Z,        -1.0f, 1.0f);
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
    stereoWidthKnob_.onValueChange = [this] { updateOrbitEnabled(); repaint(); };
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

// ---------------------------------------------------------------------------
// rebuildPresetCombo — repopulate from PresetManager (factory + user)
// ---------------------------------------------------------------------------
void XYZPanEditor::rebuildPresetCombo()
{
    presetCombo_.clear(juce::dontSendNotification);

    auto& presets = proc_.presetManager.getPresets();
    bool addedSeparator = false;

    for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
        if (!presets[static_cast<size_t>(i)].isFactory && !addedSeparator) {
            presetCombo_.addSeparator();
            addedSeparator = true;
        }
        presetCombo_.addItem(presets[static_cast<size_t>(i)].name, i + 1);
    }

    int cur = proc_.presetManager.getCurrentIndex();
    if (cur >= 0 && cur < static_cast<int>(presets.size()))
        presetCombo_.setSelectedId(cur + 1, juce::dontSendNotification);
}
