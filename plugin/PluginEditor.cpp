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
      devPanel_(p.apvts)
{
    // Apply alchemy look and feel globally for this editor
    setLookAndFeel(&lookAndFeel_);
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel_);

    // ----- Position knobs (X / Y / Z / R) -----
    for (auto* knob : {&xKnob_, &yKnob_, &zKnob_, &rKnob_}) {
        knob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
        addAndMakeVisible(knob);
    }

    // IMPORTANT: Use ParamID:: constants — actual IDs are lowercase ("x", "y", "z", "r").
    // Passing uppercase string literals causes JUCE asserts and unbound knobs.
    xAtt_ = std::make_unique<SA>(p.apvts, ParamID::X, xKnob_);
    yAtt_ = std::make_unique<SA>(p.apvts, ParamID::Y, yKnob_);
    zAtt_ = std::make_unique<SA>(p.apvts, ParamID::Z, zKnob_);
    rAtt_ = std::make_unique<SA>(p.apvts, ParamID::R, rKnob_);

    xLabel_.setText("X", juce::dontSendNotification);
    yLabel_.setText("Y", juce::dontSendNotification);
    zLabel_.setText("Z", juce::dontSendNotification);
    rLabel_.setText("R", juce::dontSendNotification);
    for (auto* lbl : {&xLabel_, &yLabel_, &zLabel_, &rLabel_}) {
        lbl->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(lbl);
    }

    // ----- LFO strips -----
    addAndMakeVisible(xLFO_);
    addAndMakeVisible(yLFO_);
    addAndMakeVisible(zLFO_);

    // ----- Reverb knobs (Size / Decay / Damping / Wet) -----
    auto configVerbKnob = [](juce::Slider& s, juce::Label& l, const juce::String& name) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
        l.setText(name, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
    };
    configVerbKnob(verbSize_,    verbSizeL_,    "Size");
    configVerbKnob(verbDecay_,   verbDecayL_,   "Decay");
    configVerbKnob(verbDamping_, verbDampingL_, "Damp");
    configVerbKnob(verbWet_,     verbWetL_,     "Wet");

    // IMPORTANT: Use ParamID:: constants — actual IDs are lowercase ("verb_size", etc.).
    verbSizeAtt_    = std::make_unique<SA>(p.apvts, ParamID::VERB_SIZE,    verbSize_);
    verbDecayAtt_   = std::make_unique<SA>(p.apvts, ParamID::VERB_DECAY,   verbDecay_);
    verbDampingAtt_ = std::make_unique<SA>(p.apvts, ParamID::VERB_DAMPING, verbDamping_);
    verbWetAtt_     = std::make_unique<SA>(p.apvts, ParamID::VERB_WET,     verbWet_);

    for (auto* s : {&verbSize_, &verbDecay_, &verbDamping_, &verbWet_})
        addAndMakeVisible(s);
    for (auto* l : {&verbSizeL_, &verbDecayL_, &verbDampingL_, &verbWetL_})
        addAndMakeVisible(l);

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

    // ----- Dev panel toggle -----
    devToggle_.onClick = [this] {
        devPanel_.setVisible(!devPanel_.isVisible());
    };
    addAndMakeVisible(devToggle_);

    // Dev panel: always a child component; visibility controls appearance
    devPanel_.setVisible(false);  // hidden by default
    addAndMakeVisible(devPanel_);

    // ----- GL view -----
    addAndMakeVisible(glView_);

    // Window sizing (same as Phase 6-02)
    setResizable(true, true);
    setResizeLimits(500, 380, 1600, 1200);
    setSize(kDefaultW, kDefaultH);
}

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------
XYZPanEditor::~XYZPanEditor()
{
    // Must clear look and feel BEFORE member destruction to avoid dangling pointer
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

// ---------------------------------------------------------------------------
// paint
// ---------------------------------------------------------------------------
void XYZPanEditor::paint(juce::Graphics& g)
{
    // GL view covers most of the window; paint fills the strip area at the bottom
    g.fillAll(juce::Colour(xyzpan::AlchemyLookAndFeel::kBackground));
}

// ---------------------------------------------------------------------------
// resized
// ---------------------------------------------------------------------------
void XYZPanEditor::resized()
{
    auto b = getLocalBounds();
    const int stripH = kStripH;

    // Carve out GL view area (everything except bottom strip)
    auto glArea = b.removeFromTop(b.getHeight() - stripH);

    // Snap buttons: top-right corner of GL area
    {
        auto snapRow = glArea.removeFromTop(kSnapBtnH + 4)
                            .removeFromRight(3 * (kSnapBtnW + 4));
        snapXY_.setBounds(snapRow.removeFromLeft(kSnapBtnW + 4).reduced(2));
        snapXZ_.setBounds(snapRow.removeFromLeft(kSnapBtnW + 4).reduced(2));
        snapYZ_.setBounds(snapRow.reduced(2));
    }

    // Remaining glArea (after snap row removed) → GL view
    glView_.setBounds(glArea);

    // Dev toggle button: top-left of GL area (after snap row has been removed)
    devToggle_.setBounds(glArea.getX() + 4, glArea.getY() + 4, 40, 22);

    // Dev panel overlays right 30% of GL view when visible
    const int panelW = static_cast<int>(glArea.getWidth() * 0.30f);
    devPanel_.setBounds(glArea.getRight() - panelW, glArea.getY(),
                        panelW, glArea.getHeight());

    // ----- Bottom strip layout -----
    // Column width for each position knob group
    const int colW  = 100;
    const int knobH = 80;   // height allocated for the position knob + label
    const int lfoH  = stripH - knobH;  // remaining height for LFO strip (120px)

    auto strip = b;  // remaining area after removeFromTop
    int x = strip.getX();

    auto placeKnobAndLFO = [&](juce::Slider& knob, juce::Label& label, LFOStrip* lfo) {
        knob.setBounds(x, strip.getY(), colW, knobH - 16);
        label.setBounds(x, strip.getY() + knobH - 16, colW, 16);
        if (lfo != nullptr)
            lfo->setBounds(x, strip.getY() + knobH, colW, lfoH);
        x += colW;
    };

    placeKnobAndLFO(xKnob_, xLabel_, &xLFO_);
    placeKnobAndLFO(yKnob_, yLabel_, &yLFO_);
    placeKnobAndLFO(zKnob_, zLabel_, &zLFO_);
    placeKnobAndLFO(rKnob_, rLabel_, nullptr);  // R knob has no LFO strip

    // Reverb section: 4 knobs after R column
    auto placeVerbKnob = [&](juce::Slider& knob, juce::Label& label) {
        knob.setBounds(x, strip.getY(), colW, knobH - 16);
        label.setBounds(x, strip.getY() + knobH - 16, colW, 16);
        x += colW;
    };
    placeVerbKnob(verbSize_,    verbSizeL_);
    placeVerbKnob(verbDecay_,   verbDecayL_);
    placeVerbKnob(verbDamping_, verbDampingL_);
    placeVerbKnob(verbWet_,     verbWetL_);
}
