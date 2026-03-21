#include "PluginEditor.h"
#include "PluginProcessor.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
XYZPanEditor::XYZPanEditor(XYZPanProcessor& p)
    : AudioProcessorEditor(p),
      proc_(p),
      glView_(p.apvts, &p, p.positionBridge),
      xLFO_('X', p.apvts),
      yLFO_('Y', p.apvts),
      zLFO_('Z', p.apvts),
      orbitXYLFO_("stereo_orbit_xy", ParamID::STEREO_ORBIT_TEMPO_SYNC, p.apvts),
      orbitXZLFO_("stereo_orbit_xz", ParamID::STEREO_ORBIT_TEMPO_SYNC, p.apvts),
      orbitYZLFO_("stereo_orbit_yz", ParamID::STEREO_ORBIT_TEMPO_SYNC, p.apvts),
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

    // Per-axis arc colours: X=red, Y=blue, Z=green
    xKnob_.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFFE05050));
    yKnob_.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFF5080E0));
    zKnob_.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFF50C060));

    xAtt_ = std::make_unique<SA>(p.apvts, ParamID::X, xKnob_);
    yAtt_ = std::make_unique<SA>(p.apvts, ParamID::Y, yKnob_);
    zAtt_ = std::make_unique<SA>(p.apvts, ParamID::Z, zKnob_);

    xLabel_.setText("X", juce::dontSendNotification);
    yLabel_.setText("Y", juce::dontSendNotification);
    zLabel_.setText("Z", juce::dontSendNotification);
    for (auto* lbl : {&xLabel_, &yLabel_, &zLabel_}) {
        lbl->setJustificationType(juce::Justification::centred);
        lbl->setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
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

    // ----- XYZ LFO Speed slider (below LFO strips, above Utilities) -----
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
    // Width / Phase / Offset — rotary knobs (consistent with position and reverb sections)
    auto configOrbitKnob = [this](juce::Slider& s, juce::Label& l, const juce::String& name) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 55, 14);
        l.setText(name, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setFont(juce::Font(juce::FontOptions(11.0f)));
        addAndMakeVisible(&s);
        addAndMakeVisible(&l);
    };
    configOrbitKnob(stereoWidthKnob_, stereoWidthLabel_, "Width");
    configOrbitKnob(orbitPhaseKnob_,  orbitPhaseLabel_,  "Phase");
    configOrbitKnob(orbitOffsetKnob_, orbitOffsetLabel_, "Offset");

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

    orbitTempoSyncToggle_.setButtonText("Sync");
    orbitTempoSyncToggle_.setClickingTogglesState(true);
    addAndMakeVisible(orbitTempoSyncToggle_);
    orbitTempoSyncAtt_ = std::make_unique<BA>(p.apvts, ParamID::STEREO_ORBIT_TEMPO_SYNC, orbitTempoSyncToggle_);

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
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 55, 14);
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
    dopplerSubLabel_.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(dopplerSubLabel_);

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
        addAndMakeVisible(btn);

    // ----- Dev panel toggle (bottom row) -----
    devToggle_.onClick = [this] {
        devPanel_.setVisible(!devPanel_.isVisible());
    };
    addAndMakeVisible(devToggle_);

    // Dev panel: child of glView_ (not the editor) so it composites on top of the
    // OpenGL surface. Bounds are set in resized() in glView_-local coordinates.
    devPanel_.setVisible(false);
    glView_.addAndMakeVisible(devPanel_);

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

    // Window sizing
    setResizable(true, true);
    setResizeLimits(950, 750, 1800, 1600);
    setSize(kDefaultW, kDefaultH);
}

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------
XYZPanEditor::~XYZPanEditor()
{
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
    l.posSectionH = l.leftColH - (kSectionHdrH + kMiscSectionH);
    l.miscTop     = l.contentY + l.posSectionH;
    l.bottomY     = totalH - kBottomH;
    l.devW        = 48;
    l.reverbX     = totalW - l.devW - kReverbSectionW;
    l.orbitTotalW = l.reverbX;
    l.contentTop  = l.bottomY + kSectionHdrH;
    return l;
}

// ---------------------------------------------------------------------------
// paint
// ---------------------------------------------------------------------------
void XYZPanEditor::paint(juce::Graphics& g)
{
    using ALF = xyzpan::AlchemyLookAndFeel;
    const auto lo = Layout::compute(getWidth(), getHeight());
    const int bx = 0;
    const int bw = getWidth();

    // ===== BACKGROUNDS =====
    g.fillAll(juce::Colour(ALF::kBackground));

    // Left column background (below preset bar)
    g.setColour(juce::Colour(ALF::kDarkIron));
    g.fillRect(0, lo.contentY, kLeftColW, lo.leftColH);

    // Bottom row background
    g.setColour(juce::Colour(ALF::kDarkIron));
    g.fillRect(0, lo.bottomY, bw, kBottomH);

    // ===== SECTION HEADERS =====

    // Helper lambda for section headers
    auto drawHeader = [&](int x, int y, int w, const juce::String& text) {
        auto hdrRect = juce::Rectangle<int>(x, y, w, kSectionHdrH);
        g.setGradientFill(juce::ColourGradient(
            juce::Colour(ALF::kBronze).withAlpha(0.25f), static_cast<float>(x), static_cast<float>(y),
            juce::Colour(ALF::kDarkIron), static_cast<float>(x + w), static_cast<float>(y), false));
        g.fillRect(hdrRect);
        g.setColour(juce::Colour(ALF::kBrightGold));
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawText(text, hdrRect.reduced(10, 0), juce::Justification::centredLeft);
        g.setColour(juce::Colour(ALF::kBronze).withAlpha(0.6f));
        g.drawHorizontalLine(y + kSectionHdrH - 1, static_cast<float>(x), static_cast<float>(x + w));
    };

    // "POSITION" header — top of left column (below preset bar)
    drawHeader(0, lo.contentY, kLeftColW, "POSITION");

    // "UTILITIES" header — at position/utilities boundary
    drawHeader(0, lo.miscTop, kLeftColW, "UTILITIES");

    // "STEREO ORBIT" header — bottom row, orbit portion
    drawHeader(bx, lo.bottomY, lo.orbitTotalW, "STEREO ORBIT");

    // "REVERB" header — bottom row, reverb portion
    drawHeader(lo.reverbX, lo.bottomY, kReverbSectionW + lo.devW, "REVERB");

    // ===== DIVIDERS — POSITION SECTION =====
    // Thin bronze separator above LFO Speed slider row
    {
        const int lfoSpeedRowH = 32;
        const int lfoSpeedSepY = lo.miscTop - lfoSpeedRowH;
        g.setColour(juce::Colour(ALF::kBronze).withAlpha(0.4f));
        g.drawHorizontalLine(lfoSpeedSepY, 0.0f, static_cast<float>(kLeftColW));
    }

    // Thick horizontal divider between Position and Utilities
    g.setColour(juce::Colour(ALF::kBronze));
    g.fillRect(0, lo.miscTop - 1, kLeftColW, 2);

    // Vertical dividers between X | Y | Z sub-columns
    {
        const int subColW = kLeftColW / 3;
        const float divTop = static_cast<float>(lo.contentY + kSectionHdrH);
        const float divBot = static_cast<float>(lo.miscTop - 1);
        g.setColour(juce::Colour(ALF::kBronze).withAlpha(0.5f));
        g.drawVerticalLine(subColW,     divTop, divBot);
        g.drawVerticalLine(subColW * 2, divTop, divBot);
    }

    // ===== DIVIDERS — BOTTOM ROW =====
    // Vertical divider between orbit controls and orbit LFO strips
    g.setColour(juce::Colour(ALF::kBronze).withAlpha(0.5f));
    g.drawVerticalLine(bx + kOrbitCtrlW, static_cast<float>(lo.contentTop), static_cast<float>(lo.bottomY + kBottomH));

    // Vertical dividers between orbit LFO strips (XY | XZ | YZ)
    // and thin horizontal separator above orbit speed row
    {
        const int lfoX = bx + kOrbitCtrlW;
        const int lfoTotalW = lo.orbitTotalW - kOrbitCtrlW;
        const int stripW = lfoTotalW / 3;
        const int speedRowH = 32;
        const int speedSepY = lo.bottomY + kBottomH - speedRowH;
        g.drawVerticalLine(lfoX + stripW,     static_cast<float>(lo.contentTop), static_cast<float>(speedSepY));
        g.drawVerticalLine(lfoX + stripW * 2, static_cast<float>(lo.contentTop), static_cast<float>(speedSepY));
        // Thin bronze separator above orbit speed slider row
        g.setColour(juce::Colour(ALF::kBronze).withAlpha(0.4f));
        g.drawHorizontalLine(speedSepY, static_cast<float>(lfoX), static_cast<float>(lfoX + lfoTotalW));
    }

    // Vertical divider between orbit section and reverb section
    g.setColour(juce::Colour(ALF::kBronze));
    g.fillRect(lo.reverbX - 1, lo.bottomY, 2, kBottomH);

    // ===== MAIN STRUCTURAL DIVIDERS =====
    // Vertical separator (left column | GL view), from below preset bar
    g.setColour(juce::Colour(ALF::kBronze));
    g.fillRect(kLeftColW - 1, lo.contentY, 2, lo.leftColH);

    // Horizontal separator (above bottom row)
    g.fillRect(0, lo.bottomY, bw, 2);

    // Thin separator below preset bar
    g.setColour(juce::Colour(ALF::kBronze).withAlpha(0.5f));
    g.drawHorizontalLine(kPresetBarH - 1, 0.0f, static_cast<float>(bw));
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

    // Carve out bottom row
    auto bottomRow = b.removeFromBottom(kBottomH);
    // Carve out left column (from what remains above bottom row)
    auto leftCol = b.removeFromLeft(kLeftColW);
    // Remainder = GL view area
    auto glArea = b;

    // Shared structural geometry — used by both left column and bottom row blocks
    const auto lo = Layout::compute(getWidth(), getHeight());

    // ===== GL VIEW =====
    glView_.setBounds(glArea);

    // Snap buttons: top-right corner of GL area
    {
        auto snapRow = glArea;
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

    // ===== LEFT COLUMN — POSITION + UTILITIES =====
    const int leftColTop = leftCol.getY();  // = kPresetBarH after preset bar removed
    const int posColW = kLeftColW / 3;
    const int knobH   = 108;
    const int labelH  = 16;
    const int posPad  = 6;
    const int posKnobRowH = posPad + knobH + labelH + 2;

    // Utilities section (sphere + doppler) is fixed at bottom of left column
    // Position section gets everything above utilities — use shared Layout for miscTop
    const int miscTop = lo.miscTop;

    // --- POSITION SECTION ---
    {
        // Reserve 32px at the bottom of the position section for the LFO Speed slider
        const int lfoSpeedRowH = 32;
        const int posSectionBottom = miscTop - lfoSpeedRowH;
        auto posSection = juce::Rectangle<int>(0, leftColTop + kSectionHdrH, kLeftColW,
                                               posSectionBottom - (leftColTop + kSectionHdrH));

        auto layoutPosCol = [&](juce::Slider& knob, juce::Label& label, LFOStrip& lfo,
                                int colX, int colW, int colTop, int colBottom) {
            int knobW = juce::jmin(knobH, colW - posPad * 2);
            int knobX = colX + (colW - knobW) / 2;
            knob.setBounds(knobX, colTop + posPad, knobW, knobH);
            label.setBounds(colX, colTop + posPad + knobH, colW, labelH);
            int lfoTop = colTop + posKnobRowH;
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

    // --- UTILITIES SECTION (Sphere Radius + Doppler) ---
    {
        const int contentTop = miscTop + kSectionHdrH;
        const int halfW = kLeftColW / 2;
        const int sphereKnobSz = 60;

        // Sphere Radius: rotary knob centered in left half
        int sKnobX = (halfW - sphereKnobSz) / 2;
        int sKnobY = contentTop + (kMiscSectionH - sphereKnobSz) / 2;
        sphereRadiusKnob_.setBounds(sKnobX, sKnobY, sphereKnobSz, sphereKnobSz);
        sphereRadiusLabel_.setBounds(0, contentTop, halfW, 14);

        // Doppler knob: rotary knob centered in right half
        const int doppKnobSz = 60;
        const int doppSubLabelH = 12;
        int dKnobX = halfW + (halfW - doppKnobSz) / 2;
        int dKnobY = contentTop + (kMiscSectionH - doppKnobSz - doppSubLabelH) / 2;
        dopplerKnob_.setBounds(dKnobX, dKnobY, doppKnobSz, doppKnobSz);
        dopplerLabel_.setBounds(halfW, contentTop, halfW, 14);
        dopplerSubLabel_.setBounds(halfW, dKnobY + doppKnobSz, halfW, doppSubLabelH);
    }

    // ===== BOTTOM ROW =====
    {
        const int bx = bottomRow.getX();
        const int bw = bottomRow.getWidth();
        const int contentH = kBottomH - kSectionHdrH;  // 216

        // Use shared Layout for structural geometry (reverb/orbit split, dev button position)
        const int contentTop = lo.contentTop;
        const int devW       = lo.devW;
        const int reverbX    = lo.reverbX;
        const int orbitTotalW = lo.orbitTotalW;

        // --- ORBIT CONTROLS (fixed 240px left portion of orbit section) ---
        {
            const int ox = bx;
            const int ow = kOrbitCtrlW;
            const int knobSz  = 48;
            const int labelH_o = 13;
            const int btnH    = 22;

            const int knobRowY = lo.contentTop + 2;
            const int colW_o   = ow / 3;

            auto placeOrbitKnob = [&](juce::Slider& knob, juce::Label& label, int col) {
                int cx    = ox + col * colW_o;
                int knobX = cx + (colW_o - knobSz) / 2;
                knob.setBounds(knobX, knobRowY, knobSz, knobSz);
                label.setBounds(cx, knobRowY + knobSz, colW_o, labelH_o);
            };

            placeOrbitKnob(stereoWidthKnob_, stereoWidthLabel_, 0);
            placeOrbitKnob(orbitOffsetKnob_, orbitOffsetLabel_, 1);
            placeOrbitKnob(orbitPhaseKnob_,  orbitPhaseLabel_,  2);

            const int btnY      = knobRowY + knobSz + labelH_o + 4;
            const int faceBtnW  = ow * 2 / 3 - 4;
            const int syncBtnW  = ow / 3 - 4;
            faceListenerToggle_.setBounds(ox + 4, btnY, faceBtnW, btnH);
            orbitTempoSyncToggle_.setBounds(ox + 4 + faceBtnW + 4, btnY, syncBtnW, btnH);
        }

        // --- ORBIT LFO STRIPS + SPEED/RESET ROW (flexible width, between orbit controls and reverb) ---
        {
            const int lfoX = bx + kOrbitCtrlW;
            const int lfoTotalW = orbitTotalW - kOrbitCtrlW;
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

        // --- REVERB KNOBS (fixed 320px = 4×80px columns) ---
        {
            const int labelH_b = 14;
            const int knobY = contentTop + (contentH - kReverbKnobSz - labelH_b) / 2;

            auto placeVerbKnob = [&](juce::Slider& knob, juce::Label& label, int col) {
                int cx = reverbX + col * kReverbColW;
                knob.setBounds(cx + (kReverbColW - kReverbKnobSz) / 2, knobY, kReverbKnobSz, kReverbKnobSz);
                label.setBounds(cx, knobY + kReverbKnobSz, kReverbColW, labelH_b);
            };
            placeVerbKnob(verbSize_,    verbSizeL_,    0);
            placeVerbKnob(verbDecay_,   verbDecayL_,   1);
            placeVerbKnob(verbDamping_, verbDampingL_, 2);
            placeVerbKnob(verbWet_,     verbWetL_,     3);
        }

        // --- DEV toggle (far-right corner) ---
        devToggle_.setBounds(bx + bw - devW + 4, lo.bottomY + (kBottomH - 24) / 2, devW - 8, 24);
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
                    (juce::Component*)&faceListenerToggle_,  (juce::Component*)&orbitTempoSyncToggle_,
                    (juce::Component*)&resetOrbitPhasesBtn_,
                    (juce::Component*)&orbitXYLFO_,          (juce::Component*)&orbitXZLFO_,
                    (juce::Component*)&orbitYZLFO_})
        c->setEnabled(active);
}
