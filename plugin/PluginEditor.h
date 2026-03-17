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
// XYZPanEditor — custom plugin editor.
//
// Layout:
//   Left column (400px):
//     "POSITION" header + 3 sub-columns (X | Y | Z) with large knobs + LFO strips
//     Horizontal divider
//     "STEREO ORBIT" header + orbit controls (Width/Speed top, Phase/Offset/Face/Sync below)
//     3 orbit LFO sub-columns (XY | XZ | YZ) with dividers
//   Main area: XYZPanGLView (OpenGL 3D spatial view)
//     Top-right corner: three snap buttons (XY, XZ, YZ)
//     Top-left corner: DEV toggle button
//     GL overlay (right 30%): DevPanelComponent — hidden by default
//   Bottom row (400px): Sphere Radius knob, Reverb (Size/Decay/Damp/Wet), Doppler toggle
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

    // Layout constants
    static constexpr int kLeftColW    = 400;     // wider for larger knobs
    static constexpr int kBottomH     = 400;
    static constexpr int kSnapBtnW    = 40;
    static constexpr int kSnapBtnH    = 24;
    static constexpr int kDefaultW    = 1100;
    static constexpr int kDefaultH   = 1100;
    static constexpr int kSectionHdrH = 24;      // section header height
    static constexpr int kDividerW    = 1;        // vertical divider width
    static constexpr int kPadding     = 6;        // general inner padding

    // Position knobs (X/Y/Z)
    juce::Slider xKnob_, yKnob_, zKnob_;
    juce::Label  xLabel_, yLabel_, zLabel_;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SA> xAtt_, yAtt_, zAtt_;

    // Sphere Radius knob (bottom row)
    juce::Slider sphereRadiusKnob_;
    juce::Label  sphereRadiusLabel_;
    std::unique_ptr<SA> sphereRadiusAtt_;

    // LFO strips — one per spatial axis (X, Y, Z only; R has no LFO)
    LFOStrip xLFO_, yLFO_, zLFO_;

    // Stereo orbit controls
    juce::Slider stereoWidthKnob_, orbitPhaseKnob_, orbitOffsetKnob_, orbitSpeedMulKnob_;
    juce::Label  stereoWidthLabel_, orbitPhaseLabel_, orbitOffsetLabel_, orbitSpeedMulLabel_;
    std::unique_ptr<SA> stereoWidthAtt_, orbitPhaseAtt_, orbitOffsetAtt_, orbitSpeedMulAtt_;

    juce::ToggleButton faceListenerToggle_;
    juce::ToggleButton orbitTempoSyncToggle_;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<BA> faceListenerAtt_, orbitTempoSyncAtt_;

    // Stereo orbit LFO strips (XY / XZ / YZ planes)
    LFOStrip orbitXYLFO_, orbitXZLFO_, orbitYZLFO_;

    // Reverb section (bottom row)
    juce::Slider verbSize_, verbDecay_, verbDamping_, verbWet_;
    juce::Label  verbSizeL_, verbDecayL_, verbDampingL_, verbWetL_;
    std::unique_ptr<SA> verbSizeAtt_, verbDecayAtt_, verbDampingAtt_, verbWetAtt_;

    // Doppler toggle — bottom row
    juce::ToggleButton dopplerToggle_;
    std::unique_ptr<BA> dopplerAtt_;

    // Dev panel toggle (bottom row) + overlay panel
    juce::TextButton devToggle_{"DEV"};
    DevPanelComponent devPanel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XYZPanEditor)
};
