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
#include "PresetManager.h"
#include "WelcomeOverlay.h"

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
//   Left column (420px) — "Source" | "Customize" tabs:
//     Source tab: X/Y/Z knobs + LFO strips + Speed slider + Options (Sphere/Doppler/Gain/Binaural/ER)
//     Customize tab: scrollable avatar customization
//   Main area: XYZPanGLView (OpenGL 3D spatial view)
//     Top-right corner: three snap buttons (XY, XZ, YZ)
//     GL overlay (right 30%): DevPanelComponent — hidden by default
//   Bottom row (340px), left-to-right — all always visible:
//     LISTENER section: Walker X/Y/Z, Yaw/Pitch/Roll, toggles
//     STEREO ORBIT section: XY/XZ/YZ LFO strips + Speed/Reset + Width/Offset/Phase/FaceListener
//     REVERB: Size/Decay/Damp/Wet knobs + DEV toggle (120px)
//   Remote: popup button in listener section (visible when linked instances >= 2)
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
    std::unique_ptr<juce::TooltipWindow> tooltipWindow_;
    juce::TextButton tooltipToggle_{"?"};
    std::unique_ptr<juce::Component> helpGuide_;       // HelpGuideOverlay (defined in .cpp)
    std::unique_ptr<xyzpan::WelcomeOverlay> welcomeOverlay_;
    struct StatusIndicator : public juce::TextButton {
        StatusIndicator() {}
        void paintButton(juce::Graphics& g, bool over, bool down) override;
    };
    StatusIndicator statusBtn_;
    void showWelcome();
    int welcomeTicks_ = 0;  // delayed re-check counter

    // GL spatial view (fills majority of window)
    xyzpan::XYZPanGLView glView_;

    // View snap buttons (XY / XZ / YZ orthographic)
    juce::ToggleButton snapXY_, snapXZ_, snapYZ_;

    enum class SnapState { None, TopDown, Side, Front };
    SnapState currentSnap_ = SnapState::None;
    void updateSnapButtonStates();

    // Layout constants
    static constexpr int kLeftColW      = 470;     // position column width
    static constexpr int kBottomH       = 275;     // bottom row height (listener + orbit + reverb)
    static constexpr int kPresetBarH    = 32;      // preset dropdown + buttons height
    static constexpr int kSnapBtnW      = 40;
    static constexpr int kSnapBtnH      = 24;
    static constexpr int kSectionHdrH   = 24;      // section header height
    static constexpr int kDividerW      = 1;       // vertical divider width
    static constexpr int kPadding       = 10;      // general inner padding
    static constexpr int kReverbSectionW = 120;     // vertical reverb column
    static constexpr int kOptionsH       = 100;     // top options panel height
    static constexpr int kMeterW         = 24;       // output meter strip width

    static constexpr int kMinW = 1119;
    static constexpr int kMinH = 794;
    static constexpr int kMaxW = 1800;
    static constexpr int kMaxH = 1600;
    static constexpr int kDefaultW = kMinW;
    static constexpr int kDefaultH = kMinH;

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

    // XYZ LFO depth multiplier slider (below speed slider)
    juce::Slider lfoDepthMulKnob_;
    juce::Label  lfoDepthMulLabel_;
    std::unique_ptr<SA> lfoDepthMulAtt_;

    // Reset All LFO Phases button (position section, next to LFO Speed)
    juce::TextButton resetXYZPhasesBtn_{"Reset"};

    // Stereo orbit controls
    juce::Slider stereoWidthKnob_, orbitPhaseKnob_, orbitOffsetKnob_, orbitSpeedMulKnob_, orbitDepthMulKnob_;
    juce::Label  stereoWidthLabel_, orbitPhaseLabel_, orbitOffsetLabel_, orbitSpeedMulLabel_, orbitDepthMulLabel_;
    std::unique_ptr<SA> stereoWidthAtt_, orbitPhaseAtt_, orbitOffsetAtt_, orbitSpeedMulAtt_, orbitDepthMulAtt_;

    // Reset All Orbit LFO Phases button (orbit section)
    juce::TextButton resetOrbitPhasesBtn_{"Reset"};

    juce::ToggleButton faceListenerToggle_;
    juce::Label        faceListenerLabel_;
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

    // Pilot — only the pilot instance drives shared listener position when linked
    juce::ToggleButton listenerPilotToggle_;
    std::unique_ptr<BA> listenerPilotAtt_;
    juce::Label pilotStatusLabel_;  // shows "X is pilot" when linked-non-pilot
    juce::Label nonPilotHintLabel_; // hint shown when linked-non-pilot

    void updateListenerControlsEnabled();

    // Remote instance focus — control another linked instance's parameters
    XYZPanProcessor* remoteFocusProc_ = nullptr;  // null = controlling self
    int remoteFocusIndex_ = -1;                    // -1 = self
    int lastKnownLinkedCount_ = 0;
    bool forceListRebuild_ = false;                // set by setRemoteFocus to refresh highlighting
    int remoteFocusValidationCounter_ = 0;         // throttle spinlock validation to ~6Hz
    int selectorRebuildCounter_ = 0;               // throttle selector rebuild to ~6Hz
    void broadcastToggleToLinked(const juce::String& paramID, bool newValue);
    void setRemoteFocus(int linkedIndex);
    void rebuildInstanceList();
    void detachAndRebindTo(juce::AudioProcessorValueTreeState& target, XYZPanProcessor* targetProc);

    // Remote tab — clickable instance list + own-name editor
    static constexpr int kMaxRemoteRows = 9;  // Self + up to 8 linked
    juce::Label instanceRows_[kMaxRemoteRows]; // clickable row labels
    int instanceRowCount_ = 0;                  // currently visible rows
    juce::Label instanceNameLabel_;             // "Name:" label
    juce::TextEditor instanceNameEditor_;       // editable own-instance name

    // Left column tab: Source (X/Y/Z + LFOs + Options) vs Customize (avatar)
    enum class LeftTab { Source, Customize };
    LeftTab activeLeftTab_ = LeftTab::Source;
    void setActiveLeftTab(LeftTab tab);

    enum class CustomizeSubTab { Environment, Wave, Avatar };
    CustomizeSubTab activeCustomizeSubTab_ = CustomizeSubTab::Environment;
    void setActiveCustomizeSubTab(CustomizeSubTab t);
    void applyCustomizeSubTabVisibility();
    int customizeSubTabHeaderY_ = 0;

    // Remote popup button (visible when linked instances >= 2)
    juce::TextButton remoteBtn_{"Remote"};
    juce::Label remoteStatusLabel_;

    // (OptionsTab removed — options are always visible in left column Source tab)

    // Walker knobs (always active)
    juce::Slider walkerXKnob_, walkerYKnob_, walkerZKnob_;
    juce::Label  walkerXLabel_, walkerYLabel_, walkerZLabel_;
    std::unique_ptr<SA> walkerXAtt_, walkerYAtt_, walkerZAtt_;

    // WASD control toggle + keyboard movement
    juce::ToggleButton wasdToggle_;
    std::unique_ptr<BA> wasdAtt_;
    bool wasdGestureActive_ = false;

    // WASD speed multiplier slider (enabled only when WASD control is on)
    juce::Slider wasdSpeedKnob_;
    juce::Label  wasdSpeedLabel_;
    std::unique_ptr<SA> wasdSpeedAtt_;

    // Brand label
    juce::Label brandLabel_;
    xyzpan::ListenerQuatAccumulator listenerAccum_;

    // Cached APVTS parameter pointers for timerCallback (avoid per-frame string lookup)
    juce::RangedAudioParameter* cachedWalkerX_  = nullptr;
    juce::RangedAudioParameter* cachedWalkerY_  = nullptr;
    juce::RangedAudioParameter* cachedWalkerZ_  = nullptr;
    juce::RangedAudioParameter* cachedListenerRoll_  = nullptr;
    juce::RangedAudioParameter* cachedListenerYaw_   = nullptr;
    juce::RangedAudioParameter* cachedListenerPitch_ = nullptr;
    std::atomic<float>* cachedRawYaw_   = nullptr;
    std::atomic<float>* cachedRawPitch_ = nullptr;
    std::atomic<float>* cachedRawRoll_  = nullptr;
    juce::TextButton rollLockBtn_;
    void endWasdGestureIfActive();
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
    juce::TextButton presetPrevBtn_{"<"};
    juce::TextButton presetNextBtn_{">"};
    juce::TextButton presetSaveBtn_{"Save"};
    juce::TextButton presetLoadBtn_{"Load"};
    juce::TextButton undoBtn_{"Undo"};
    juce::TextButton redoBtn_{"Redo"};
    void rebuildPresetCombo();

    // Customize tab controls
    std::unique_ptr<xyzpan::UserPreferences> userPrefs_;
    juce::ComboBox    themeCombo_;
    juce::Label       themeLabel_;
    juce::ComboBox    skyCombo_;
    juce::Label       skyLabel_;
    juce::ComboBox    groundCombo_;
    juce::Label       groundLabel_;
    juce::Slider      groundHeightSlider_;
    juce::Label       groundHeightLabel_;
    juce::Slider      groundHillsSlider_;
    juce::Label       groundHillsLabel_;
    juce::Slider      groundRippleSlider_;
    juce::Label       groundRippleLabel_;
    juce::Slider      groundFogSlider_;
    juce::Label       groundFogLabel_;
    juce::ToggleButton showLabelsToggle_;
    juce::Label        showLabelsLabel_;
    juce::ToggleButton showArrowToggle_;
    juce::Label        showArrowLabel_;
    juce::ComboBox sourceShapeCombo_;
    juce::Label    sourceShapeLabel_;
    juce::Slider   clusterCountSlider_;
    juce::Label    clusterCountLabel_;
    juce::ToggleButton showAudibleSphereToggle_;
    juce::Label        showAudibleSphereLabel_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> showAudibleSphereAtt_;
    juce::Slider waveCountSlider_;
    juce::Label  waveCountLabel_;
    std::unique_ptr<SA> waveCountAtt_;
    juce::Slider   waveOpacitySlider_;
    juce::Label    waveOpacityLabel_;
    std::unique_ptr<SA> waveOpacityAtt_;
    juce::Slider   waveSpeedSlider_;
    juce::Label    waveSpeedLabel_;
    std::unique_ptr<SA> waveSpeedAtt_;
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
        std::function<void(const juce::MouseEvent&)> onMouseDown;
        void paint(juce::Graphics& g) override { if (onPaint) onPaint(g); }
        void mouseDown(const juce::MouseEvent& e) override { if (onMouseDown) onMouseDown(e); }
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

    // Body type combo
    juce::ComboBox bodyTypeCombo_;
    juce::Label    bodyTypeLabel_;

    void applyCurrentTheme();
    void pushAvatarToGL();
    void syncEyeSpacingSliderMode(bool isCyclops);

    // Cross-instance preference sync — reloads from disk and refreshes all
    // customize controls + GL view when another instance bumps the hub version.
    void applyPreferencesToUI();
    uint32_t lastPreferencesVersion_ = 0;

    // Reset buttons — one per customize sub-tab, placed at top of active panel
    juce::TextButton resetEnvBtn_{"Reset"};
    juce::TextButton resetWavesBtn_{"Reset"};
    juce::TextButton resetAvatarBtn_{"Reset"};

    void resetEnvironment();
    void resetWaves();
    void resetAvatarAll();

    // Shared geometry: computed once per paint/resized call to avoid drift
    struct Layout {
        int contentY;       // = kPresetBarH
        int leftColH;       // = totalH - kBottomH - kPresetBarH
        int bottomY;        // = totalH - kBottomH
        int reverbX;        // = totalW - kMeterW - kReverbSectionW
        int listenerX;      // X offset of listener section in bottom row
        int listenerW;      // width of listener section in bottom row
        int orbitX;         // start of stereo orbit section
        int orbitW;         // = reverbX - listenerW
        int contentTop;     // = bottomY + kSectionHdrH
        int leftContentTop; // = contentY + kSectionHdrH (top of left column content)
        int leftContentH;   // = bottomY - leftContentTop (left column content height)

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
