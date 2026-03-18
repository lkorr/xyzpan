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
    // Apply alchemy look and feel globally for this editor
    setLookAndFeel(&lookAndFeel_);
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel_);

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

    // Wire real LFO phase sources (drift-free sync with DSP)
    xLFO_.setPhaseSource(&p.lfoPhaseX);
    yLFO_.setPhaseSource(&p.lfoPhaseY);
    zLFO_.setPhaseSource(&p.lfoPhaseZ);

    // Wire S&H held value sources for accurate S&H waveform display
    xLFO_.setSHSource(&p.lfoSHValueX);
    yLFO_.setSHSource(&p.lfoSHValueY);
    zLFO_.setSHSource(&p.lfoSHValueZ);

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
    configOrbitSlider(orbitSpeedMulKnob_, orbitSpeedMulLabel_, "Speed");

    stereoWidthAtt_  = std::make_unique<SA>(p.apvts, ParamID::STEREO_WIDTH,          stereoWidthKnob_);
    orbitPhaseAtt_   = std::make_unique<SA>(p.apvts, ParamID::STEREO_ORBIT_PHASE,    orbitPhaseKnob_);
    orbitOffsetAtt_  = std::make_unique<SA>(p.apvts, ParamID::STEREO_ORBIT_OFFSET,   orbitOffsetKnob_);
    orbitSpeedMulAtt_ = std::make_unique<SA>(p.apvts, ParamID::STEREO_ORBIT_SPEED_MUL, orbitSpeedMulKnob_);

    // Hero styling for Width and Speed orbit sliders
    stereoWidthKnob_.setColour(juce::Slider::rotarySliderFillColourId,
                               juce::Colour(xyzpan::AlchemyLookAndFeel::kBrightGold));
    orbitSpeedMulKnob_.setColour(juce::Slider::rotarySliderFillColourId,
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

    orbitXYLFO_.setPhaseSource(&p.lfoPhaseOrbitXY);
    orbitXZLFO_.setPhaseSource(&p.lfoPhaseOrbitXZ);
    orbitYZLFO_.setPhaseSource(&p.lfoPhaseOrbitYZ);

    orbitXYLFO_.setSHSource(&p.lfoSHValueOrbitXY);
    orbitXZLFO_.setSHSource(&p.lfoSHValueOrbitXZ);
    orbitYZLFO_.setSHSource(&p.lfoSHValueOrbitYZ);

    // ----- Stereo toggle button — click to expand (hides itself) -----
    stereoToggle_.onClick = [this] {
        stereoExpanded_ = true;
        stereoToggle_.setVisible(false);
        for (auto* c : {(juce::Component*)&stereoWidthKnob_, (juce::Component*)&stereoWidthLabel_,
                        (juce::Component*)&orbitPhaseKnob_,  (juce::Component*)&orbitPhaseLabel_,
                        (juce::Component*)&orbitOffsetKnob_, (juce::Component*)&orbitOffsetLabel_,
                        (juce::Component*)&orbitSpeedMulKnob_, (juce::Component*)&orbitSpeedMulLabel_,
                        (juce::Component*)&faceListenerToggle_, (juce::Component*)&orbitTempoSyncToggle_,
                        (juce::Component*)&resetOrbitPhasesBtn_,
                        (juce::Component*)&orbitXYLFO_, (juce::Component*)&orbitXZLFO_,
                        (juce::Component*)&orbitYZLFO_, (juce::Component*)&monoBtn_})
            c->setVisible(true);
        resized();
        repaint();
    };
    addAndMakeVisible(stereoToggle_);

    // All orbit controls start hidden
    for (auto* c : {(juce::Component*)&stereoWidthKnob_, (juce::Component*)&stereoWidthLabel_,
                    (juce::Component*)&orbitPhaseKnob_,  (juce::Component*)&orbitPhaseLabel_,
                    (juce::Component*)&orbitOffsetKnob_, (juce::Component*)&orbitOffsetLabel_,
                    (juce::Component*)&orbitSpeedMulKnob_, (juce::Component*)&orbitSpeedMulLabel_,
                    (juce::Component*)&faceListenerToggle_, (juce::Component*)&orbitTempoSyncToggle_,
                    (juce::Component*)&resetOrbitPhasesBtn_,
                    (juce::Component*)&orbitXYLFO_, (juce::Component*)&orbitXZLFO_,
                    (juce::Component*)&orbitYZLFO_})
        c->setVisible(false);

    // ----- Mono button — collapses orbit panel and re-shows Stereo button -----
    monoBtn_.onClick = [this] {
        if (auto* param = proc_.apvts.getParameter(ParamID::STEREO_WIDTH))
            param->setValueNotifyingHost(0.0f);
        stereoExpanded_ = false;
        for (auto* c : {(juce::Component*)&stereoWidthKnob_, (juce::Component*)&stereoWidthLabel_,
                        (juce::Component*)&orbitPhaseKnob_,  (juce::Component*)&orbitPhaseLabel_,
                        (juce::Component*)&orbitOffsetKnob_, (juce::Component*)&orbitOffsetLabel_,
                        (juce::Component*)&orbitSpeedMulKnob_, (juce::Component*)&orbitSpeedMulLabel_,
                        (juce::Component*)&faceListenerToggle_, (juce::Component*)&orbitTempoSyncToggle_,
                        (juce::Component*)&resetOrbitPhasesBtn_,
                        (juce::Component*)&orbitXYLFO_, (juce::Component*)&orbitXZLFO_,
                        (juce::Component*)&orbitYZLFO_, (juce::Component*)&monoBtn_})
            c->setVisible(false);
        stereoToggle_.setVisible(true);
        resized();
        repaint();
    };
    addAndMakeVisible(monoBtn_);
    monoBtn_.setVisible(false);

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
    snapXY_.onClick = [this] {
        glView_.setSnapView(xyzpan::XYZPanGLView::SnapView::TopDown);
    };
    snapXZ_.onClick = [this] {
        glView_.setSnapView(xyzpan::XYZPanGLView::SnapView::Side);
    };
    snapYZ_.onClick = [this] {
        glView_.setSnapView(xyzpan::XYZPanGLView::SnapView::Front);
    };
    for (auto* btn : {&snapXY_, &snapXZ_, &snapYZ_}) {
        btn->setClickingTogglesState(false);
        addAndMakeVisible(btn);
    }

    // ----- Dev panel toggle (bottom row) -----
    devToggle_.onClick = [this] {
        devPanel_.setVisible(!devPanel_.isVisible());
    };
    addAndMakeVisible(devToggle_);

    // Dev panel: child of glView_ (not the editor) so it composites on top of the
    // OpenGL surface. Bounds are set in resized() in glView_-local coordinates.
    devPanel_.setVisible(false);
    glView_.addAndMakeVisible(devPanel_);

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
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

// ---------------------------------------------------------------------------
// paint
// ---------------------------------------------------------------------------
void XYZPanEditor::paint(juce::Graphics& g)
{
    using ALF = xyzpan::AlchemyLookAndFeel;
    const int leftColH = getHeight() - kBottomH;

    // Compute miscTop identically to resized()
    const int miscTotalH = kSectionHdrH + kMiscSectionH;
    const int posSectionH = leftColH - miscTotalH;
    const int miscTop = posSectionH;

    // Bottom row geometry (matches resized())
    const int bx = 0;
    const int by = getHeight() - kBottomH;
    const int bw = getWidth();
    const int devW = 48;
    const int reverbX = bx + bw - devW - kReverbSectionW;
    const int orbitTotalW = reverbX - bx;
    const int contentTop = by + kSectionHdrH;

    // ===== BACKGROUNDS =====
    g.fillAll(juce::Colour(ALF::kBackground));

    // Left column background
    g.setColour(juce::Colour(ALF::kDarkIron));
    g.fillRect(0, 0, kLeftColW, leftColH);

    // Bottom row background
    g.setColour(juce::Colour(ALF::kDarkIron));
    g.fillRect(0, by, bw, kBottomH);

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

    // "POSITION" header — top of left column
    drawHeader(0, 0, kLeftColW, "POSITION");

    // "UTILITIES" header — at position/utilities boundary
    drawHeader(0, miscTop, kLeftColW, "UTILITIES");

    // "STEREO ORBIT" header — bottom row, orbit portion
    drawHeader(bx, by, orbitTotalW, "STEREO ORBIT");

    // "REVERB" header — bottom row, reverb portion
    drawHeader(reverbX, by, kReverbSectionW + devW, "REVERB");

    // ===== DIVIDERS — POSITION SECTION =====
    // Thin bronze separator above LFO Speed slider row
    {
        const int lfoSpeedRowH = 32;
        const int lfoSpeedSepY = miscTop - lfoSpeedRowH;
        g.setColour(juce::Colour(ALF::kBronze).withAlpha(0.4f));
        g.drawHorizontalLine(lfoSpeedSepY, 0.0f, static_cast<float>(kLeftColW));
    }

    // Thick horizontal divider between Position and Utilities
    g.setColour(juce::Colour(ALF::kBronze));
    g.fillRect(0, miscTop - 1, kLeftColW, 2);

    // Vertical dividers between X | Y | Z sub-columns
    {
        const int subColW = kLeftColW / 3;
        const float divTop = static_cast<float>(kSectionHdrH);
        const float divBot = static_cast<float>(miscTop - 1);
        g.setColour(juce::Colour(ALF::kBronze).withAlpha(0.5f));
        g.drawVerticalLine(subColW,     divTop, divBot);
        g.drawVerticalLine(subColW * 2, divTop, divBot);
    }

    // ===== DIVIDERS — BOTTOM ROW =====
    if (stereoExpanded_)
    {
        // Vertical divider between orbit controls and orbit LFO strips
        g.setColour(juce::Colour(ALF::kBronze).withAlpha(0.5f));
        g.drawVerticalLine(bx + kOrbitCtrlW, static_cast<float>(contentTop), static_cast<float>(by + kBottomH));

        // Vertical dividers between orbit LFO strips (XY | XZ | YZ)
        {
            const int lfoX = bx + kOrbitCtrlW;
            const int lfoTotalW = orbitTotalW - kOrbitCtrlW;
            const int stripW = lfoTotalW / 3;
            g.drawVerticalLine(lfoX + stripW,     static_cast<float>(contentTop), static_cast<float>(by + kBottomH));
            g.drawVerticalLine(lfoX + stripW * 2, static_cast<float>(contentTop), static_cast<float>(by + kBottomH));
        }
    }

    // Vertical divider between orbit section and reverb section
    g.setColour(juce::Colour(ALF::kBronze));
    g.fillRect(reverbX - 1, by, 2, kBottomH);

    // ===== MAIN STRUCTURAL DIVIDERS =====
    // Vertical separator (left column | GL view)
    g.setColour(juce::Colour(ALF::kBronze));
    g.fillRect(kLeftColW - 1, 0, 2, leftColH);

    // Horizontal separator (above bottom row)
    g.fillRect(0, by, bw, 2);
}

// ---------------------------------------------------------------------------
// resized
// ---------------------------------------------------------------------------
void XYZPanEditor::resized()
{
    auto b = getLocalBounds();

    // Carve out bottom row
    auto bottomRow = b.removeFromBottom(kBottomH);
    // Carve out left column (from what remains above bottom row)
    auto leftCol = b.removeFromLeft(kLeftColW);
    // Remainder = GL view area
    auto glArea = b;

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
    const int leftColH = leftCol.getHeight();
    const int posColW = kLeftColW / 3;
    const int knobH   = 108;
    const int labelH  = 16;
    const int posPad  = 6;
    const int posKnobRowH = posPad + knobH + labelH + 2;

    // Utilities section (sphere + doppler) is fixed at bottom of left column
    const int miscTotalH = kSectionHdrH + kMiscSectionH;  // 24 + 80 = 104

    // Position section gets everything above utilities
    const int posSectionH = leftColH - miscTotalH;
    const int miscTop = posSectionH;

    // --- POSITION SECTION ---
    {
        // Reserve 32px at the bottom of the position section for the LFO Speed slider
        const int lfoSpeedRowH = 32;
        const int posSectionBottom = kSectionHdrH + (posSectionH - kSectionHdrH) - lfoSpeedRowH;
        auto posSection = juce::Rectangle<int>(0, kSectionHdrH, kLeftColW, posSectionBottom - kSectionHdrH);

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
        const int by = bottomRow.getY();
        const int bw = bottomRow.getWidth();
        const int contentH = kBottomH - kSectionHdrH;  // 216
        const int contentTop = by + kSectionHdrH;
        const int devW = 48;

        // Reverb section: fixed width from right edge (before DEV toggle)
        const int reverbX = bx + bw - devW - kReverbSectionW;

        // Orbit section: everything to the left of reverb
        const int orbitTotalW = reverbX - bx;

        // --- STEREO TOGGLE BUTTON (centered in full orbit panel, hidden when expanded) ---
        {
            const int toggleW = 100;
            const int toggleH = 36;
            const int toggleX = bx + (orbitTotalW - toggleW) / 2;
            const int toggleY = contentTop + (contentH - toggleH) / 2;
            stereoToggle_.setBounds(toggleX, toggleY, toggleW, toggleH);
        }

        // --- ORBIT CONTROLS + LFO STRIPS (only when expanded) ---
        if (stereoExpanded_)
        {
            // --- ORBIT CONTROLS (fixed 240px left portion of orbit section) ---
            {
                const int ox = bx + 6;
                const int ow = kOrbitCtrlW - 12;
                int oy = contentTop + 2;
                const int heroH    = 28;
                const int stdH     = 18;
                const int labelW   = 48;
                const int gap      = 2;

                auto placeSlider = [&](juce::Label& lbl, juce::Slider& slider, int h) {
                    lbl.setBounds(ox, oy, labelW, h);
                    slider.setBounds(ox + labelW, oy, ow - labelW, h);
                    oy += h + gap;
                };

                placeSlider(stereoWidthLabel_,    stereoWidthKnob_,    heroH);
                placeSlider(orbitSpeedMulLabel_,   orbitSpeedMulKnob_,  heroH);
                placeSlider(orbitOffsetLabel_,     orbitOffsetKnob_,    stdH);
                placeSlider(orbitPhaseLabel_,      orbitPhaseKnob_,     stdH);

                const int btnH = 22;
                const int faceBtnW = ow * 2 / 3;
                const int syncBtnW = ow - faceBtnW - 8;
                faceListenerToggle_.setBounds(ox, oy, faceBtnW, btnH);
                orbitTempoSyncToggle_.setBounds(ox + faceBtnW + 8, oy, syncBtnW, btnH);
                oy += btnH + 4;

                // Reset + Mono buttons below toggles
                const int resetW = 44;
                resetOrbitPhasesBtn_.setBounds(ox, oy, resetW, btnH);
                monoBtn_.setBounds(ox + resetW + 4, oy, ow - resetW - 4, btnH);
            }

            // --- ORBIT LFO STRIPS (flexible width, between orbit controls and reverb) ---
            {
                const int lfoX = bx + kOrbitCtrlW;
                const int lfoTotalW = orbitTotalW - kOrbitCtrlW;
                const int stripW = lfoTotalW / 3;
                const int lastStripW = lfoTotalW - stripW * 2;

                orbitXYLFO_.setBounds(lfoX,              contentTop, stripW,     contentH);
                orbitXZLFO_.setBounds(lfoX + stripW,     contentTop, stripW,     contentH);
                orbitYZLFO_.setBounds(lfoX + stripW * 2, contentTop, lastStripW, contentH);
            }
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
        devToggle_.setBounds(bx + bw - devW + 4, by + (kBottomH - 24) / 2, devW - 8, 24);
    }
}
