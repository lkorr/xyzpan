#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>
#include "XYZPanGLView.h"
#include "AlchemyLookAndFeel.h"
#include "ParamIDs.h"

// Forward declaration to break circular include; full type used in .cpp
class XYZPanProcessor;

// ---------------------------------------------------------------------------
// XYZPanEditor — custom plugin editor replacing GenericAudioProcessorEditor.
//
// Layout (Phase 6-02):
//   Top area (window height - 80px): XYZPanGLView (OpenGL 3D spatial view)
//     Top-right corner: three snap buttons (XY, XZ, YZ)
//   Bottom strip (80px): X / Y / Z / R knobs with labels
//
// LFO controls and reverb controls will be added in Phase 6-03.
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

    // Bottom strip: X/Y/Z/R knobs (Phase 6-02 scope — LFO/reverb added in 06-03)
    juce::Slider xKnob_, yKnob_, zKnob_, rKnob_;
    juce::Label  xLabel_, yLabel_, zLabel_, rLabel_;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SA> xAtt_, yAtt_, zAtt_, rAtt_;

    static constexpr int kStripH   = 80;   // height of bottom knob strip
    static constexpr int kSnapBtnW = 40;   // width of each snap button
    static constexpr int kSnapBtnH = 24;
    static constexpr int kDefaultW = 900;
    static constexpr int kDefaultH = 620;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XYZPanEditor)
};
