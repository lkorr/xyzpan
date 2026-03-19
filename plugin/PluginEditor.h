#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>
#include "XYZPanGLView.h"
#include "AlchemyLookAndFeel.h"
#include "LFOStrip.h"
#include "DevPanelComponent.h"
#include "ParamIDs.h"
#include "Presets.h"

// Forward declaration to break circular include; full type used in .cpp
class XYZPanProcessor;

// ---------------------------------------------------------------------------
// XYZPanEditor — custom plugin editor.
//
// Layout:
//   Left column (400px):
//     "POSITION" header + 3 sub-columns (X | Y | Z) with large knobs + tall LFO strips
//     "UTILITIES" header + Sphere Radius knob + Doppler knob (compact, 80px)
//   Main area: XYZPanGLView (OpenGL 3D spatial view)
//     Top-right corner: three snap buttons (XY, XZ, YZ)
//     GL overlay (right 30%): DevPanelComponent — hidden by default
//   Bottom row (240px), left-to-right:
//     "STEREO ORBIT" — orbit sliders (240px) + 3 orbit LFO strips (flexible)
//     "REVERB" — Size/Decay/Damp/Wet knobs (320px) + DEV toggle (48px)
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
    static constexpr int kLeftColW      = 400;     // wider for larger knobs
    static constexpr int kBottomH       = 240;     // was 400 — orbit moved here
    static constexpr int kPresetBarH    = 32;      // preset dropdown + buttons height
    static constexpr int kSnapBtnW      = 40;
    static constexpr int kSnapBtnH      = 24;
    static constexpr int kDefaultW      = 1100;
    static constexpr int kDefaultH      = 1100;
    static constexpr int kSectionHdrH   = 24;      // section header height
    static constexpr int kDividerW      = 1;       // vertical divider width
    static constexpr int kPadding       = 6;       // general inner padding
    static constexpr int kOrbitCtrlW    = 240;     // orbit sliders+buttons width in bottom row
    static constexpr int kReverbColW    = 56;      // reverb knob column width
    static constexpr int kReverbKnobSz  = 48;      // reverb knob diameter (smaller to fit)
    static constexpr int kReverbSectionW = kReverbColW * 4;  // 320px
    static constexpr int kMiscSectionH  = 80;      // sphere+doppler area height (excl header)

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

    // XYZ LFO speed multiplier slider (below LFO strips, above Utilities)
    juce::Slider lfoSpeedMulKnob_;
    juce::Label  lfoSpeedMulLabel_;
    std::unique_ptr<SA> lfoSpeedMulAtt_;

    // Reset All LFO Phases button (position section, next to LFO Speed)
    juce::TextButton resetXYZPhasesBtn_{"Reset"};

    // Stereo orbit controls
    juce::Slider stereoWidthKnob_, orbitPhaseKnob_, orbitOffsetKnob_, orbitSpeedMulKnob_;
    juce::Label  stereoWidthLabel_, orbitPhaseLabel_, orbitOffsetLabel_, orbitSpeedMulLabel_;
    std::unique_ptr<SA> stereoWidthAtt_, orbitPhaseAtt_, orbitOffsetAtt_, orbitSpeedMulAtt_;

    // Reset All Orbit LFO Phases button (orbit section)
    juce::TextButton resetOrbitPhasesBtn_{"Reset"};

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

    // Doppler knob (distance delay) — utilities section
    juce::Slider dopplerKnob_;
    juce::Label  dopplerLabel_;
    juce::Label  dopplerSubLabel_;  // "(adds delay)" small text
    std::unique_ptr<SA> dopplerAtt_;

    // Dev panel toggle (bottom row) + overlay panel
    juce::TextButton devToggle_{"DEV"};
    DevPanelComponent devPanel_;

    // Preset controls -- top bar
    juce::ComboBox presetCombo_;
    juce::TextButton presetSaveBtn_{"Save"};
    juce::TextButton presetLoadBtn_{"Load"};

    void updateOrbitEnabled();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XYZPanEditor)
};
