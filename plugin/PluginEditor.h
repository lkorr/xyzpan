#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>
#include "XYZPanGLView.h"
#include "AlchemyLookAndFeel.h"
#include "LFOStrip.h"
#include "DevPanelComponent.h"
#include "ParamIDs.h"

// Forward declaration to break circular include; full type used in .cpp
class XYZPanProcessor;

// ---------------------------------------------------------------------------
// XYZPanEditor — custom plugin editor (Phase 6-03).
//
// Layout:
//   Top area (window height - 200px): XYZPanGLView (OpenGL 3D spatial view)
//     Top-right corner: three snap buttons (XY, XZ, YZ)
//     Top-left corner: DEV toggle button
//     GL overlay (right 30%): DevPanelComponent — hidden by default
//   Bottom strip (200px):
//     Position knobs (X/Y/Z/R) with LFO strips (Rate/Depth/Phase/SYNC/Waveform) beneath X/Y/Z
//     Reverb section (Size/Decay/Damping/Wet) right of R knob
// ---------------------------------------------------------------------------
class XYZPanEditor : public juce::AudioProcessorEditor {
public:
    explicit XYZPanEditor(XYZPanProcessor& p);
    ~XYZPanEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    XYZPanProcessor& proc_;
    xyzpan::AlchemyLookAndFeel lookAndFeel_;

    // GL spatial view (fills majority of window)
    xyzpan::XYZPanGLView glView_;

    // View snap buttons (XY / XZ / YZ orthographic)
    juce::TextButton snapXY_{"XY"}, snapXZ_{"XZ"}, snapYZ_{"YZ"};

    // Bottom strip height — extended in Phase 6-03 to accommodate LFO strips
    static constexpr int kStripH   = 200;
    static constexpr int kSnapBtnW = 40;
    static constexpr int kSnapBtnH = 24;
    static constexpr int kDefaultW = 900;
    static constexpr int kDefaultH = 620;

    // Position knobs (X/Y/Z/R)
    juce::Slider xKnob_, yKnob_, zKnob_, rKnob_;
    juce::Label  xLabel_, yLabel_, zLabel_, rLabel_;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SA> xAtt_, yAtt_, zAtt_, rAtt_;

    // LFO strips — one per spatial axis (X, Y, Z only; R has no LFO)
    LFOStrip xLFO_, yLFO_, zLFO_;

    // Reverb section (right of R knob in bottom strip)
    juce::Slider verbSize_, verbDecay_, verbDamping_, verbWet_;
    juce::Label  verbSizeL_, verbDecayL_, verbDampingL_, verbWetL_;
    std::unique_ptr<SA> verbSizeAtt_, verbDecayAtt_, verbDampingAtt_, verbWetAtt_;

    // Dev panel toggle + overlay panel
    juce::TextButton devToggle_{"DEV"};
    DevPanelComponent devPanel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XYZPanEditor)
};
