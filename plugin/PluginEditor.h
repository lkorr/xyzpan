#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_opengl/juce_opengl.h>
#include <random>
#include "XYZPanGLView.h"
#include "AlchemyLookAndFeel.h"
#include "LFOStrip.h"
#include "DevPanelComponent.h"
#include "OutputMeter.h"
#include "UserPreferences.h"
#include "ParamIDs.h"
#include "Presets.h"

// Forward declaration to break circular include; full type used in .cpp
class XYZPanProcessor;

// ---------------------------------------------------------------------------
// ColorSwatch — clickable color rectangle that opens a ColourSelector popup.
// ---------------------------------------------------------------------------
class ColorSwatch : public juce::Component {
public:
    ColorSwatch() { setRepaintsOnMouseActivity(true); }

    void setColour(juce::Colour c) { colour_ = c; repaint(); }
    juce::Colour getColour() const { return colour_; }

    std::function<void(juce::Colour)> onChange;
    std::function<void()> onReset;  // double-click resets to theme default

    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(colour_);
        g.fillRoundedRectangle(r, 3.0f);
        g.setColour(juce::Colours::white.withAlpha(isMouseOver() ? 0.5f : 0.25f));
        g.drawRoundedRectangle(r, 3.0f, 1.0f);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (e.mods.isPopupMenu() || e.getNumberOfClicks() >= 2) {
            if (onReset) onReset();
            return;
        }
        auto selector = std::make_unique<juce::ColourSelector>(
            juce::ColourSelector::showColourAtTop
            | juce::ColourSelector::showSliders
            | juce::ColourSelector::showColourspace);
        selector->setCurrentColour(colour_);
        selector->setSize(250, 280);
        selector->addChangeListener(changeRelay_.get());
        changeRelay_->callback = [this, raw = selector.get()](juce::ChangeBroadcaster*) {
            colour_ = raw->getCurrentColour();
            repaint();
            if (onChange) onChange(colour_);
        };
        juce::CallOutBox::launchAsynchronously(std::move(selector), getScreenBounds(), nullptr);
    }

private:
    juce::Colour colour_{juce::Colours::grey};

    struct ChangeRelay : public juce::ChangeListener {
        std::function<void(juce::ChangeBroadcaster*)> callback;
        void changeListenerCallback(juce::ChangeBroadcaster* src) override {
            if (callback) callback(src);
        }
    };
    std::unique_ptr<ChangeRelay> changeRelay_ = std::make_unique<ChangeRelay>();
};

// ---------------------------------------------------------------------------
// XYZPanEditor — custom plugin editor.
//
// Layout:
//   Left column (672px):
//     "POSITION" header + 3 sub-columns (X | Y | Z) with large knobs + tall LFO strips
//   Main area: XYZPanGLView (OpenGL 3D spatial view)
//     Top-right corner: three snap buttons (XY, XZ, YZ)
//     GL overlay (right 30%): DevPanelComponent — hidden by default
//   Bottom row (240px), left-to-right:
//     "STEREO ORBIT" — Sphere/Doppler knobs + orbit sliders (240px) + 3 orbit LFO strips
//     "REVERB" — Size/Decay/Damp/Wet knobs vertical stack (80px) + DEV toggle at bottom
// ---------------------------------------------------------------------------
class XYZPanEditor : public juce::AudioProcessorEditor,
                     public juce::KeyListener,
                     private juce::Timer {
public:
    explicit XYZPanEditor(XYZPanProcessor& p);
    ~XYZPanEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;

private:
    XYZPanProcessor& proc_;
    xyzpan::AlchemyLookAndFeel lookAndFeel_;

    // GL spatial view (fills majority of window)
    xyzpan::XYZPanGLView glView_;

    // View snap buttons (XY / XZ / YZ orthographic)
    juce::ToggleButton snapXY_, snapXZ_, snapYZ_;

    enum class SnapState { None, TopDown, Side, Front };
    SnapState currentSnap_ = SnapState::None;
    void updateSnapButtonStates();

    // Layout constants
    static constexpr int kLeftColW      = 470;     // position column width
    static constexpr int kBottomH       = 240;     // was 400 — orbit moved here
    static constexpr int kPresetBarH    = 32;      // preset dropdown + buttons height
    static constexpr int kSnapBtnW      = 40;
    static constexpr int kSnapBtnH      = 24;
    static constexpr int kDefaultW      = 1000;
    static constexpr int kDefaultH      = 650;
    static constexpr int kSectionHdrH   = 24;      // section header height
    static constexpr int kDividerW      = 1;       // vertical divider width
    static constexpr int kPadding       = 6;       // general inner padding
    static constexpr int kOrbitCtrlW    = 240;     // orbit sliders+buttons width in bottom row
    static constexpr int kReverbSectionW = 120;     // vertical reverb column
    static constexpr int kMeterW         = 24;       // output meter strip width

    // Position knobs (X/Y/Z)
    juce::Slider xKnob_, yKnob_, zKnob_;
    juce::Label  xLabel_, yLabel_, zLabel_;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SA> xAtt_, yAtt_, zAtt_;

    // Sphere Radius knob (orbit controls column)
    juce::Slider sphereRadiusKnob_;
    juce::Label  sphereRadiusLabel_;
    std::unique_ptr<SA> sphereRadiusAtt_;

    // LFO strips — one per spatial axis (X, Y, Z only; R has no LFO)
    LFOStrip xLFO_, yLFO_, zLFO_;

    // XYZ LFO speed multiplier slider (below LFO strips)
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
    juce::ToggleButton binauralToggle_;
    juce::Label        binauralLabel_;
    juce::ToggleButton earlyReflToggle_;
    juce::Label        earlyReflLabel_;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<BA> faceListenerAtt_;
    std::unique_ptr<BA> binauralAtt_;
    std::unique_ptr<BA> earlyReflAtt_;

    // Listener head orientation knobs
    juce::Slider listenerYawKnob_, listenerPitchKnob_, listenerRollKnob_;
    juce::Label  listenerYawLabel_, listenerPitchLabel_, listenerRollLabel_;
    std::unique_ptr<SA> listenerYawAtt_, listenerPitchAtt_, listenerRollAtt_;

    // Head follows camera toggle
    juce::ToggleButton headFollowsToggle_;
    std::unique_ptr<BA> headFollowsAtt_;

    // Link listener orientation across instances
    juce::ToggleButton listenerLinkToggle_;
    std::unique_ptr<BA> listenerLinkAtt_;

    // Remote instance focus — control another linked instance's parameters
    XYZPanProcessor* remoteFocusProc_ = nullptr;  // null = controlling self
    int remoteFocusIndex_ = -1;                    // -1 = self
    int lastKnownLinkedCount_ = 0;
    bool forceListRebuild_ = false;                // set by setRemoteFocus to refresh highlighting
    int remoteFocusValidationCounter_ = 0;         // throttle spinlock validation to ~6Hz
    int selectorRebuildCounter_ = 0;               // throttle selector rebuild to ~6Hz
    void setRemoteFocus(int linkedIndex);
    void rebuildInstanceList();
    void detachAndRebindTo(juce::AudioProcessorValueTreeState& target, XYZPanProcessor* targetProc);

    // Remote tab — clickable instance list + own-name editor
    static constexpr int kMaxRemoteRows = 9;  // Self + up to 8 linked
    juce::Label instanceRows_[kMaxRemoteRows]; // clickable row labels
    int instanceRowCount_ = 0;                  // currently visible rows
    juce::Label instanceNameLabel_;             // "Name:" label
    juce::TextEditor instanceNameEditor_;       // editable own-instance name

    // Tab state for Options / Perspective / Customize split
    enum class OptionsTab { Options, Perspective, Customize };
    OptionsTab activeTab_ = OptionsTab::Options;
    void setActiveTab(OptionsTab tab);

    // Left column tab state — Source (X/Y/Z + LFOs) vs Listener (walker + perspective) vs Remote
    enum class LeftTab { Source, Listener, Remote };
    LeftTab activeLeftTab_ = LeftTab::Source;
    void setActiveLeftTab(LeftTab tab);
    juce::Label remoteStatusLabel_;

    // Walker knobs (always active)
    juce::Slider walkerXKnob_, walkerYKnob_, walkerZKnob_;
    juce::Label  walkerXLabel_, walkerYLabel_, walkerZLabel_;
    std::unique_ptr<SA> walkerXAtt_, walkerYAtt_, walkerZAtt_;

    // WASD control toggle + keyboard movement
    juce::ToggleButton wasdToggle_;
    std::unique_ptr<BA> wasdAtt_;
    void timerCallback() override;

    // Stereo orbit LFO strips (XY / XZ / YZ planes)
    LFOStrip orbitXYLFO_, orbitXZLFO_, orbitYZLFO_;

    // Reverb section (bottom row)
    juce::Slider verbSize_, verbDecay_, verbDamping_, verbWet_;
    juce::Label  verbSizeL_, verbDecayL_, verbDampingL_, verbWetL_;
    std::unique_ptr<SA> verbSizeAtt_, verbDecayAtt_, verbDampingAtt_, verbWetAtt_;

    // Doppler knob (distance delay) — orbit controls column
    juce::Slider dopplerKnob_;
    juce::Label  dopplerLabel_;
    juce::Label  dopplerSubLabel_;  // "(adds delay)" small text
    std::unique_ptr<SA> dopplerAtt_;

    // Input gain knob — options tab, next to sphere/doppler
    juce::Slider inputGainKnob_;
    juce::Label  inputGainLabel_;
    std::unique_ptr<SA> inputGainAtt_;

    // Dev panel toggle (bottom row) + overlay panel
    juce::TextButton devToggle_{"DEV"};
    DevPanelComponent devPanel_;

    // Preset controls -- top bar
    juce::ComboBox presetCombo_;
    juce::TextButton presetSaveBtn_{"Save"};
    juce::TextButton presetLoadBtn_{"Load"};

    // Customize tab controls
    std::unique_ptr<xyzpan::UserPreferences> userPrefs_;
    juce::ComboBox    themeCombo_;
    juce::Label       themeLabel_;
    juce::Slider      headElongationSlider_, eyeSizeSlider_, eyeSpacingSlider_,
                      earSizeSlider_, headSizeSlider_;
    juce::Label       headElongationLabel_, eyeSizeLabel_, eyeSpacingLabel_,
                      earSizeLabel_, headSizeLabel_;
    juce::ComboBox     eyeTypeCombo_;
    juce::Label        eyeTypeLabel_;
    juce::ComboBox     earTypeCombo_;
    juce::Label        earTypeLabel_;

    // Color swatches (avatar color overrides) — placed under their respective subpanels
    ColorSwatch headColorSwatch_, noseColorSwatch_, hatColorSwatch_, eyeColorSwatch_;
    juce::Label headColorLabel_, noseColorLabel_, hatColorLabel_, eyeColorLabel_;

    // Scrollable customize content
    juce::Viewport customizeViewport_;
    struct CustomizeContent : public juce::Component {
        std::function<void(juce::Graphics&)> onPaint;
        void paint(juce::Graphics& g) override { if (onPaint) onPaint(g); }
    };
    CustomizeContent customizeContent_;
    int eyesSectionHeaderY_ = 0, earsSectionHeaderY_ = 0, hatsSectionHeaderY_ = 0;

    // Hat type combo + size slider
    juce::ComboBox hatTypeCombo_;
    juce::Label    hatTypeLabel_;
    juce::Slider   hatSizeSlider_;
    juce::Label    hatSizeLabel_;

    // Nose type combo + size slider
    juce::ComboBox noseTypeCombo_;
    juce::Label    noseTypeLabel_;
    juce::Slider   noseSizeSlider_;
    juce::Label    noseSizeLabel_;
    int noseSectionHeaderY_ = 0;

    // Pupil size / ear rotation / googly sliders
    juce::Slider pupilSizeSlider_, earRotationSlider_, googlySlider_;
    juce::Label  pupilSizeLabel_, earRotationLabel_, googlyLabel_;

    void applyCurrentTheme();
    void pushAvatarToGL();
    void syncEyeSpacingSliderMode(bool isCyclops);

    // Shared geometry: computed once per paint/resized call to avoid drift
    struct Layout {
        int contentY;       // = kPresetBarH
        int leftColH;       // = totalH - kBottomH - kPresetBarH
        int bottomY;        // = totalH - kBottomH
        int reverbX;        // = totalW - kReverbSectionW
        int orbitTotalW;    // = reverbX
        int lfoX;           // = left edge of orbit LFO strips (after cap)
        int lfoTotalW;      // = capped width of orbit LFO strips
        int contentTop;     // = bottomY + kSectionHdrH

        static Layout compute(int totalW, int totalH);
    };

    void updateOrbitEnabled();

    // Output L/R RMS meter (right edge of window)
    xyzpan::OutputMeter outputMeter_;

    // Procedural noise texture (256x256 greyscale) tiled over JUCE panels at low opacity
    juce::Image noiseTexture_;
    static juce::Image generateNoiseTexture(int size);

    // --- Alt+R module randomization ---
    enum class RandZone {
        None, Position, StereoOrbit, Reverb, Perspective,
        CustColors, CustEyes, CustEars, CustNose, CustHats,
        LfoX, LfoY, LfoZ, OrbitLfoXY, OrbitLfoXZ, OrbitLfoYZ
    };
    std::mt19937 rng_{std::random_device{}()};
    RandZone lastClickedZone_ = RandZone::None;

    RandZone classifyRandZone(juce::Point<int> pos) const;
    void randomizeAPVTSParam(const char* paramID, float min, float max);
    void randomizeLFOStrip(const juce::String& prefix);
    void randomizePosition();
    void randomizeStereoOrbit();
    void randomizeReverb();
    void randomizePerspective();
    void randomizeCustColors();
    void randomizeCustEyes();
    void randomizeCustEars();
    void randomizeCustNose();
    void randomizeCustHats();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XYZPanEditor)
};
