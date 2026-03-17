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

    faceListenerToggle_.setButtonText("Always Face Observer");
    faceListenerToggle_.setClickingTogglesState(true);
    addAndMakeVisible(faceListenerToggle_);
    faceListenerAtt_ = std::make_unique<BA>(p.apvts, ParamID::STEREO_FACE_LISTENER, faceListenerToggle_);

    orbitTempoSyncToggle_.setButtonText("Sync");
    orbitTempoSyncToggle_.setClickingTogglesState(true);
    addAndMakeVisible(orbitTempoSyncToggle_);
    orbitTempoSyncAtt_ = std::make_unique<BA>(p.apvts, ParamID::STEREO_ORBIT_TEMPO_SYNC, orbitTempoSyncToggle_);

    // ----- Orbit LFO strips -----
    addAndMakeVisible(orbitXYLFO_);
    addAndMakeVisible(orbitXZLFO_);
    addAndMakeVisible(orbitYZLFO_);

    // ----- Sphere Radius knob (bottom row) -----
    sphereRadiusKnob_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    sphereRadiusKnob_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
    addAndMakeVisible(sphereRadiusKnob_);
    sphereRadiusAtt_ = std::make_unique<SA>(p.apvts, ParamID::SPHERE_RADIUS, sphereRadiusKnob_);
    sphereRadiusLabel_.setText("Sphere", juce::dontSendNotification);
    sphereRadiusLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(sphereRadiusLabel_);

    // ----- Reverb knobs (Size / Decay / Damping / Wet) — bottom row -----
    auto configVerbKnob = [this](juce::Slider& s, juce::Label& l, const juce::String& name) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
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

    // ----- Doppler toggle -----
    dopplerToggle_.setButtonText("Doppler");
    dopplerToggle_.setClickingTogglesState(true);
    addAndMakeVisible(dopplerToggle_);
    dopplerAtt_ = std::make_unique<BA>(p.apvts, ParamID::DOPPLER_ENABLED, dopplerToggle_);

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
    setResizeLimits(700, 1090, 1800, 1600);
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
    const float colW = static_cast<float>(kLeftColW);

    // Compute posH identically to resized() — fixed minimum heights, split remainder
    const int knobH_   = 108;
    const int posPad_  = 6;
    const int posKnobRowH_ = posPad_ + knobH_ + 16 + 2;
    const int minLFOStripH_ = 200;
    const int minPosSectionH_ = kSectionHdrH + posKnobRowH_ + minLFOStripH_;
    const int minOrbitSectionH_ = kSectionHdrH + 108 + 200;
    const int totalMinH_ = minPosSectionH_ + minOrbitSectionH_;
    const int extraH_ = juce::jmax(0, leftColH - totalMinH_);
    const int posH = minPosSectionH_ + extraH_ / 2;

    // ===== BACKGROUNDS =====
    g.fillAll(juce::Colour(ALF::kBackground));

    // Left column background
    g.setColour(juce::Colour(ALF::kDarkIron));
    g.fillRect(0, 0, kLeftColW, leftColH);

    // Bottom row background
    g.setColour(juce::Colour(ALF::kDarkIron));
    g.fillRect(0, getHeight() - kBottomH, getWidth(), kBottomH);

    // ===== SECTION HEADERS =====
    // "POSITION" header — top of left column
    {
        auto hdrRect = juce::Rectangle<int>(0, 0, kLeftColW, kSectionHdrH);
        // Subtle gradient background for header
        g.setGradientFill(juce::ColourGradient(
            juce::Colour(ALF::kBronze).withAlpha(0.25f), 0.0f, 0.0f,
            juce::Colour(ALF::kDarkIron), colW, 0.0f, false));
        g.fillRect(hdrRect);
        // Header text
        g.setColour(juce::Colour(ALF::kBrightGold));
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawText("POSITION", hdrRect.reduced(10, 0), juce::Justification::centredLeft);
        // Bottom border of header
        g.setColour(juce::Colour(ALF::kBronze).withAlpha(0.6f));
        g.drawHorizontalLine(kSectionHdrH - 1, 0.0f, colW);
    }

    // "STEREO ORBIT" header — at position/orbit boundary
    {
        auto hdrRect = juce::Rectangle<int>(0, posH, kLeftColW, kSectionHdrH);
        g.setGradientFill(juce::ColourGradient(
            juce::Colour(ALF::kBronze).withAlpha(0.25f), 0.0f, static_cast<float>(posH),
            juce::Colour(ALF::kDarkIron), colW, static_cast<float>(posH), false));
        g.fillRect(hdrRect);
        g.setColour(juce::Colour(ALF::kBrightGold));
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawText("STEREO ORBIT", hdrRect.reduced(10, 0), juce::Justification::centredLeft);
        // Bottom border
        g.setColour(juce::Colour(ALF::kBronze).withAlpha(0.6f));
        g.drawHorizontalLine(posH + kSectionHdrH - 1, 0.0f, colW);
    }

    // ===== DIVIDERS — POSITION SECTION =====
    // Thick horizontal divider between Position and Stereo Orbit
    g.setColour(juce::Colour(ALF::kBronze));
    g.fillRect(0, posH - 1, kLeftColW, 2);

    // Vertical dividers between X | Y | Z sub-columns
    {
        const int subColW = kLeftColW / 3;
        const float divTop = static_cast<float>(kSectionHdrH);
        const float divBot = static_cast<float>(posH - 1);
        g.setColour(juce::Colour(ALF::kBronze).withAlpha(0.5f));
        g.drawVerticalLine(subColW,     divTop, divBot);
        g.drawVerticalLine(subColW * 2, divTop, divBot);
    }

    // ===== DIVIDERS — ORBIT SECTION =====
    // Vertical dividers between orbit LFO sub-columns (XY | XZ | YZ)
    {
        const int orbitCtrlH = 108;
        const int orbitBodyTop = posH + kSectionHdrH + orbitCtrlH;
        const int subColW = kLeftColW / 3;
        const float divTop = static_cast<float>(orbitBodyTop);
        const float divBot = static_cast<float>(leftColH);
        g.setColour(juce::Colour(ALF::kBronze).withAlpha(0.5f));
        g.drawVerticalLine(subColW,     divTop, divBot);
        g.drawVerticalLine(subColW * 2, divTop, divBot);
    }

    // Thin horizontal line above orbit LFO strips (below orbit knobs)
    {
        const int orbitCtrlH = 108;
        const int lineY = posH + kSectionHdrH + orbitCtrlH;
        g.setColour(juce::Colour(ALF::kBronze).withAlpha(0.35f));
        g.drawHorizontalLine(lineY, 0.0f, colW);
    }

    // ===== MAIN STRUCTURAL DIVIDERS =====
    // Vertical separator (left column | GL view)
    g.setColour(juce::Colour(ALF::kBronze));
    g.fillRect(kLeftColW - 1, 0, 2, leftColH);

    // Horizontal separator (above bottom row)
    g.fillRect(0, getHeight() - kBottomH, getWidth(), 2);
}

// ---------------------------------------------------------------------------
// resized
// ---------------------------------------------------------------------------
void XYZPanEditor::resized()
{
    auto b = getLocalBounds();

    // Carve out bottom row
    auto bottomRow = b.removeFromBottom(kBottomH);
    // Carve out left column
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

    // Dev panel: child of glView_, right 30% of GL area
    const int panelW = static_cast<int>(glArea.getWidth() * 0.30f);
    devPanel_.setBounds(glArea.getWidth() - panelW, 0, panelW, glArea.getHeight());

    // ===== LEFT COLUMN =====
    // Use fixed minimum heights so sections never overlap regardless of window size.
    const int leftColH = leftCol.getHeight();

    const int posColW = kLeftColW / 3;
    const int knobH   = 108;                     // position knob size (10% smaller than 120)
    const int labelH  = 16;
    const int posPad  = 6;

    // Fixed minimum heights for each sub-section
    const int posKnobRowH = posPad + knobH + labelH + 2;  // knob + label
    const int minLFOStripH = 200;                          // minimum LFO strip height
    const int minPosSectionH = kSectionHdrH + posKnobRowH + minLFOStripH;

    const int orbitCtrlH = 108;  // 4 sliders + button row
    const int minOrbitStripH = 200;
    const int minOrbitSectionH = kSectionHdrH + orbitCtrlH + minOrbitStripH;

    // Distribute available height: give each section its minimum, then split remainder
    const int totalMinH = minPosSectionH + minOrbitSectionH;
    const int extraH = juce::jmax(0, leftColH - totalMinH);

    // Split extra space equally between the two LFO strip areas
    const int posH = minPosSectionH + extraH / 2;

    // --- POSITION SECTION ---
    auto posSection = juce::Rectangle<int>(0, kSectionHdrH, kLeftColW, posH - kSectionHdrH);

    auto layoutPosCol = [&](juce::Slider& knob, juce::Label& label, LFOStrip& lfo,
                            int colX, int colW, int colTop, int colBottom) {
        // Knob: centered in column
        int knobW = juce::jmin(knobH, colW - posPad * 2);
        int knobX = colX + (colW - knobW) / 2;
        knob.setBounds(knobX, colTop + posPad, knobW, knobH);
        // Label: directly below knob
        label.setBounds(colX, colTop + posPad + knobH, colW, labelH);
        // LFO strip: fills remaining space
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

    // --- STEREO ORBIT SECTION ---
    const int orbitTop = posH + kSectionHdrH;    // after orbit header
    auto orbitCtrlArea = juce::Rectangle<int>(0, orbitTop, kLeftColW, orbitCtrlH);
    const int orbitStripTop = orbitTop + orbitCtrlH;
    const int orbitStripH = juce::jmax(0, leftColH - orbitStripTop);

    // Orbit controls: 4 horizontal sliders stacked vertically + Face/Sync buttons
    {
        const int ox = orbitCtrlArea.getX() + 6;
        const int ow = kLeftColW - 12;
        int oy = orbitCtrlArea.getY() + 2;
        const int sliderH = 18;
        const int labelW  = 48;
        const int gap     = 2;

        auto placeSlider = [&](juce::Label& lbl, juce::Slider& slider) {
            lbl.setBounds(ox, oy, labelW, sliderH);
            slider.setBounds(ox + labelW, oy, ow - labelW, sliderH);
            oy += sliderH + gap;
        };

        placeSlider(stereoWidthLabel_,    stereoWidthKnob_);
        placeSlider(orbitPhaseLabel_,      orbitPhaseKnob_);
        placeSlider(orbitOffsetLabel_,     orbitOffsetKnob_);
        placeSlider(orbitSpeedMulLabel_,   orbitSpeedMulKnob_);

        // Face + Sync buttons on remaining space
        const int btnH = 22;
        const int faceBtnW = ow * 2 / 3;
        const int syncBtnW = ow - faceBtnW - 8;
        faceListenerToggle_.setBounds(ox, oy, faceBtnW, btnH);
        orbitTempoSyncToggle_.setBounds(ox + faceBtnW + 8, oy, syncBtnW, btnH);
    }

    // 3 orbit LFO strips side by side
    {
        const int orbitColW = kLeftColW / 3;
        orbitXYLFO_.setBounds(0,             orbitStripTop, orbitColW, orbitStripH);
        orbitXZLFO_.setBounds(orbitColW,     orbitStripTop, orbitColW, orbitStripH);
        orbitYZLFO_.setBounds(orbitColW * 2, orbitStripTop, kLeftColW - orbitColW * 2, orbitStripH);
    }

    // ===== BOTTOM ROW =====
    {
        const int colW = 100;
        const int knobSize = 120;
        const int labelH_b = 18;
        const int pad = 12;
        int x = bottomRow.getX() + pad;
        const int knobY = bottomRow.getY() + (kBottomH - knobSize - labelH_b) / 2;

        // Sphere Radius knob
        sphereRadiusKnob_.setBounds(x + (colW - knobSize) / 2, knobY, knobSize, knobSize);
        sphereRadiusLabel_.setBounds(x, knobY + knobSize, colW, labelH_b);
        x += colW;

        // Reverb: Size, Decay, Damp, Wet
        auto placeVerbKnob = [&](juce::Slider& knob, juce::Label& label) {
            knob.setBounds(x + (colW - knobSize) / 2, knobY, knobSize, knobSize);
            label.setBounds(x, knobY + knobSize, colW, labelH_b);
            x += colW;
        };
        placeVerbKnob(verbSize_,    verbSizeL_);
        placeVerbKnob(verbDecay_,   verbDecayL_);
        placeVerbKnob(verbDamping_, verbDampingL_);
        placeVerbKnob(verbWet_,     verbWetL_);

        // Doppler toggle
        dopplerToggle_.setBounds(x + 4, bottomRow.getY() + (kBottomH - 24) / 2, 70, 24);
        x += 70 + 8;

        // DEV panel toggle — right end of bottom row
        devToggle_.setBounds(bottomRow.getRight() - 48, bottomRow.getY() + (kBottomH - 24) / 2, 40, 24);
    }
}
