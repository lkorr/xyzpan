#include "PluginEditor.h"
#include "PluginProcessor.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
XYZPanEditor::XYZPanEditor(XYZPanProcessor& p)
    : AudioProcessorEditor(p),
      proc_(p),
      glView_(p.apvts, &p, p.positionBridge)
{
    // Apply alchemy look and feel globally for this editor
    setLookAndFeel(&lookAndFeel_);
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel_);

    // Configure knobs as rotary sliders
    for (auto* knob : {&xKnob_, &yKnob_, &zKnob_, &rKnob_}) {
        knob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(knob);
    }

    // APVTS slider attachments — keep knob in sync with parameter
    xAtt_ = std::make_unique<SA>(p.apvts, ParamID::X, xKnob_);
    yAtt_ = std::make_unique<SA>(p.apvts, ParamID::Y, yKnob_);
    zAtt_ = std::make_unique<SA>(p.apvts, ParamID::Z, zKnob_);
    rAtt_ = std::make_unique<SA>(p.apvts, ParamID::R, rKnob_);

    // Labels
    xLabel_.setText("X", juce::dontSendNotification);
    yLabel_.setText("Y", juce::dontSendNotification);
    zLabel_.setText("Z", juce::dontSendNotification);
    rLabel_.setText("R", juce::dontSendNotification);
    for (auto* lbl : {&xLabel_, &yLabel_, &zLabel_, &rLabel_}) {
        lbl->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(lbl);
    }

    // Snap buttons — onClick routes to glView_.setSnapView()
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

    // GL view
    addAndMakeVisible(glView_);

    // Window sizing
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

    // Carve out GL view area (everything except bottom strip)
    auto glArea = b.removeFromTop(b.getHeight() - kStripH);

    // Three snap buttons sit in the top-right corner of the GL view area
    {
        auto snapRow = glArea.removeFromTop(kSnapBtnH + 4)
                            .removeFromRight(3 * (kSnapBtnW + 4));
        snapXY_.setBounds(snapRow.removeFromLeft(kSnapBtnW + 4).reduced(2));
        snapXZ_.setBounds(snapRow.removeFromLeft(kSnapBtnW + 4).reduced(2));
        snapYZ_.setBounds(snapRow.reduced(2));
    }

    // Remaining glArea (after snap row removed) → GL view
    glView_.setBounds(glArea);

    // Bottom strip: 4 knobs evenly spaced
    const int knobW   = kStripH - 4;
    const int spacing = (b.getWidth() - 4 * knobW) / 5;
    int x = b.getX() + spacing;
    const int kY = b.getY();

    for (auto [knob, lbl] : std::initializer_list<std::pair<juce::Slider*, juce::Label*>>{
            {&xKnob_, &xLabel_}, {&yKnob_, &yLabel_},
            {&zKnob_, &zLabel_}, {&rKnob_, &rLabel_}}) {
        knob->setBounds(x, kY + 2, knobW, knobW - 16);
        lbl->setBounds(x, kY + knobW - 14, knobW, 14);
        x += knobW + spacing;
    }
}
