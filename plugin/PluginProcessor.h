#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <memory>
#include "xyzpan/Engine.h"
#include "xyzpan/DSPStateBridge.h"
#include "xyzpan/dsp/OnePoleSmooth.h"
#include "ParamLayout.h"
#include "PositionBridge.h"
#include "PresetManager.h"
#include "SharedListenerHub.h"
#include "SessionConfig.h"

class XYZPanProcessor : public juce::AudioProcessor,
                        public SharedListenerHub::Listener,
                        public juce::AudioProcessorValueTreeState::Listener,
                        private juce::Timer {
public:
    XYZPanProcessor();
    ~XYZPanProcessor() override;

    // AudioProcessor overrides
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "XYZPan"; }

    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.37; }
    // 300ms distance + 20ms floor bounce + 5000ms max reverb T60 + 50ms pre-delay max

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Track / instance naming
    void updateTrackProperties(const TrackProperties& properties) override;
    void setInstanceName(const juce::String& name);
    juce::String getInstanceNameValue() const { return instanceName_; }
    int getSourceShapeParam() const { return sourceShapeParam_ ? static_cast<int>(sourceShapeParam_->load(std::memory_order_relaxed)) : 0; }

    // Pilot queries — for editor UI to check whether listener controls should be enabled
    bool isLinkedPilot() const;
    bool isLinkedNonPilot() const;
    juce::String getPilotName() const;

    // Undo/redo support — declared before apvts (member init order)
    juce::UndoManager undoManager_ { 30000, 30 };
    juce::UndoManager& getUndoManager() { return undoManager_; }

    // APVTS — public so editor and parameter attachments can access it
    juce::AudioProcessorValueTreeState apvts;

    // Momentary reset flags — set by UI or preset load, consumed (cleared) by processBlock
    std::atomic<bool> resetXYZLfoPhases{false};
    std::atomic<bool> resetOrbitLfoPhases{false};

    // Preset management (factory + user presets)
    PresetManager presetManager;

    // Session configuration
    SessionConfig sessionCfg_;
    std::atomic<bool> cfgStale_{false};

    // Phase 6: PositionBridge for audio-to-GL position transfer (UI-07)
    // Public so XYZPanEditor / XYZPanGLView can hold a reference
    xyzpan::PositionBridge positionBridge;

    // Source export for linked-instance visualization (audio → message thread)
    xyzpan::SourceExportBuffer sourceExport;

    // Foreign source bridge for linked-instance visualization (message → GL thread)
    xyzpan::ForeignSourceBridge foreignSourceBridge;

    // DSP state bridge for dev panel readouts (audio thread → UI thread)
    xyzpan::DSPStateBridge dspStateBridge;

    // Shared flag: true while processor is applying a broadcast from another linked instance.
    // GLView checks this to suppress cross-instance head-follows feedback loops.
    auto getReceivingBroadcastFlag() const { return receivingBroadcast_; }

    // Access to the shared listener hub (for remote instance control)
    SharedListenerHub& getListenerHub() { return *listenerHub_; }

    // LFO output values (tick*depth) — written by audio thread, read by UI displays
    std::atomic<float> lfoOutputX{0.f}, lfoOutputY{0.f}, lfoOutputZ{0.f};
    std::atomic<float> lfoOutputOrbitXY{0.f}, lfoOutputOrbitXZ{0.f}, lfoOutputOrbitYZ{0.f};

    // Output RMS levels — written by audio thread, read by UI meter
    std::atomic<float> outputRmsL{0.f}, outputRmsR{0.f};

private:
    int lastTimerHz_ = 30;
    xyzpan::XYZPanEngine engine;

    // Raw parameter value pointers (thread-safe atomics managed by APVTS)
    // Spatial position (Phase 1)
    std::atomic<float>* xParam = nullptr;
    std::atomic<float>* yParam = nullptr;
    std::atomic<float>* zParam = nullptr;

    // Phase 6: R scale/radius (PARAM-01)
    std::atomic<float>* rParam = nullptr;
    // Phase 6: R smoother — prevents per-block step clicks during automation (PARAM-03)
    // R multiplies X/Y/Z before engine; raw rParam->load() would cause zipper noise.
    xyzpan::dsp::OnePoleSmooth rSmooth_;
    // Source position smoothers — 5ms: prevents zipper noise from mouse-drag jitter
    xyzpan::dsp::OnePoleSmooth xSmooth_;
    xyzpan::dsp::OnePoleSmooth ySmooth_;
    xyzpan::dsp::OnePoleSmooth zSmooth_;

    // Dev panel: binaural panning tuning (Phase 2)
    std::atomic<float>* itdMaxParam       = nullptr;
    std::atomic<float>* headShadowHzParam = nullptr;
    std::atomic<float>* ildMaxDbParam     = nullptr;
    std::atomic<float>* rearShadowHzParam = nullptr;
    std::atomic<float>* smoothItdParam    = nullptr;
    std::atomic<float>* smoothFilterParam = nullptr;
    std::atomic<float>* smoothGainParam   = nullptr;

    // Dev panel: Comb filter bank (Phase 3)
    std::atomic<float>* combDelayParam[10] = {};
    std::atomic<float>* combFbParam[10]    = {};
    std::atomic<float>* combWetMaxParam    = nullptr;

    // Dev panel: Elevation filters (Phase 3)
    std::atomic<float>* pinnaNotchHzParam  = nullptr;
    std::atomic<float>* pinnaNotchQParam   = nullptr;
    std::atomic<float>* pinnaShelfHzParam  = nullptr;
    std::atomic<float>* chestDelayMsParam  = nullptr;
    std::atomic<float>* chestGainDbParam   = nullptr;
    std::atomic<float>* chestHPFHzParam    = nullptr;
    std::atomic<float>* chestLPHzParam     = nullptr;
    std::atomic<float>* floorDelayMsParam  = nullptr;
    std::atomic<float>* floorGainDbParam   = nullptr;

    // Dev panel: Distance processing (Phase 4)
    std::atomic<float>* distDelayMaxMsParam = nullptr;
    std::atomic<float>* distSmoothMsParam   = nullptr;
    std::atomic<float>* airAbsMaxHzParam    = nullptr;
    std::atomic<float>* airAbsMinHzParam    = nullptr;

    // Phase 5: Reverb (VERB-03)
    std::atomic<float>* verbSizeParam      = nullptr;
    std::atomic<float>* verbDecayParam     = nullptr;
    std::atomic<float>* verbDampingParam   = nullptr;
    std::atomic<float>* verbWetParam       = nullptr;
    std::atomic<float>* verbPreDelayParam  = nullptr;
    std::atomic<float>* verbModDepthParam  = nullptr;
    std::atomic<float>* verbDiffusionParam = nullptr;

    // Phase 5: LFO — per axis
    std::atomic<float>* lfoXRateParam     = nullptr;
    std::atomic<float>* lfoXDepthParam    = nullptr;
    std::atomic<float>* lfoXPhaseParam    = nullptr;
    std::atomic<float>* lfoXWaveformParam = nullptr;
    std::atomic<float>* lfoYRateParam     = nullptr;
    std::atomic<float>* lfoYDepthParam    = nullptr;
    std::atomic<float>* lfoYPhaseParam    = nullptr;
    std::atomic<float>* lfoYWaveformParam = nullptr;
    std::atomic<float>* lfoZRateParam     = nullptr;
    std::atomic<float>* lfoZDepthParam    = nullptr;
    std::atomic<float>* lfoZPhaseParam    = nullptr;
    std::atomic<float>* lfoZWaveformParam = nullptr;
    std::atomic<float>* lfoXSmoothParam  = nullptr;
    std::atomic<float>* lfoYSmoothParam  = nullptr;
    std::atomic<float>* lfoZSmoothParam  = nullptr;
    std::atomic<float>* lfoXTempoSyncParam = nullptr;
    std::atomic<float>* lfoYTempoSyncParam = nullptr;
    std::atomic<float>* lfoZTempoSyncParam = nullptr;
    std::atomic<float>* lfoXBeatDivParam  = nullptr;
    std::atomic<float>* lfoYBeatDivParam  = nullptr;
    std::atomic<float>* lfoZBeatDivParam  = nullptr;
    std::atomic<float>* lfoSpeedMulParam  = nullptr;
    std::atomic<float>* lfoDepthMulParam  = nullptr;
    std::atomic<float>* lfoMasterPhaseParam = nullptr;

    // Stereo source node splitting
    std::atomic<float>* stereoWidthParam        = nullptr;
    std::atomic<float>* stereoFaceListenerParam = nullptr;
    std::atomic<float>* stereoOrbitPhaseParam    = nullptr;
    std::atomic<float>* stereoOrbitOffsetParam   = nullptr;

    // Stereo orbit LFOs — XY plane
    std::atomic<float>* orbitXYWaveformParam   = nullptr;
    std::atomic<float>* orbitXYRateParam       = nullptr;
    std::atomic<float>* orbitXYBeatDivParam    = nullptr;
    std::atomic<float>* orbitXYPhaseParam      = nullptr;
    std::atomic<float>* orbitXYResetPhaseParam = nullptr;
    std::atomic<float>* orbitXYDepthParam      = nullptr;
    std::atomic<float>* orbitXYSmoothParam     = nullptr;

    // Stereo orbit LFOs — XZ plane
    std::atomic<float>* orbitXZWaveformParam   = nullptr;
    std::atomic<float>* orbitXZRateParam       = nullptr;
    std::atomic<float>* orbitXZBeatDivParam    = nullptr;
    std::atomic<float>* orbitXZPhaseParam      = nullptr;
    std::atomic<float>* orbitXZResetPhaseParam = nullptr;
    std::atomic<float>* orbitXZDepthParam      = nullptr;
    std::atomic<float>* orbitXZSmoothParam     = nullptr;

    // Stereo orbit LFOs — YZ plane
    std::atomic<float>* orbitYZWaveformParam   = nullptr;
    std::atomic<float>* orbitYZRateParam       = nullptr;
    std::atomic<float>* orbitYZBeatDivParam    = nullptr;
    std::atomic<float>* orbitYZPhaseParam      = nullptr;
    std::atomic<float>* orbitYZResetPhaseParam = nullptr;
    std::atomic<float>* orbitYZDepthParam      = nullptr;
    std::atomic<float>* orbitYZSmoothParam     = nullptr;

    // Stereo orbit per-plane sync + shared speed
    std::atomic<float>* orbitXYTempoSyncParam = nullptr;
    std::atomic<float>* orbitXZTempoSyncParam = nullptr;
    std::atomic<float>* orbitYZTempoSyncParam = nullptr;
    std::atomic<float>* orbitSpeedMulParam    = nullptr;
    std::atomic<float>* orbitDepthMulParam    = nullptr;

    // Listener head orientation
    std::atomic<float>* listenerYawParam          = nullptr;
    std::atomic<float>* listenerPitchParam        = nullptr;
    std::atomic<float>* listenerRollParam         = nullptr;
    std::atomic<float>* headFollowsCameraParam    = nullptr;

    // Walker — movable listener position (always active)
    std::atomic<float>* walkerXParam     = nullptr;
    std::atomic<float>* walkerYParam     = nullptr;
    std::atomic<float>* walkerZParam     = nullptr;
    std::atomic<float>* wasdControlParam = nullptr;

    xyzpan::dsp::OnePoleSmooth walkerXSmooth_;
    xyzpan::dsp::OnePoleSmooth walkerYSmooth_;
    xyzpan::dsp::OnePoleSmooth walkerZSmooth_;

    // Instance name (auto-populated from DAW track name, user-editable)
    juce::String instanceName_;
    juce::String trackName_;           // last DAW-reported track name
    bool nameManuallySet_ = false;

    // Source shape (per-instance APVTS parameter)
    std::atomic<float>* sourceShapeParam_ = nullptr;

    // Audible sphere visibility (queried by linked instances via getShowSphere)
    std::atomic<float>* showAudibleSphereParam_ = nullptr;

    // Listener link across instances
    std::atomic<float>* listenerLinkParam         = nullptr;
    std::atomic<float>* listenerPilotParam        = nullptr;
    juce::SharedResourcePointer<SharedListenerHub> listenerHub_;
    std::shared_ptr<std::atomic<bool>> receivingBroadcast_ = std::make_shared<std::atomic<bool>>(false);
    bool restoringState_ = false;  // guard: suppress listener hub side-effects during setStateInformation
    bool needsListenerSnap_ = true; // snap engine smoothers to saved state on first processBlock

    // SharedListenerHub::Listener overrides
    void listenerOrientationChanged(float yaw, float pitch, float roll,
                                     bool headFollows) override;
    void listenerPositionChanged(float x, float y, float z) override;
    void pilotStatusChanged(bool isPilot) override;
    xyzpan::SourceExportBuffer* getSourceExportBuffer() override { return &sourceExport; }
    juce::AudioProcessor* getProcessor() override { return this; }
    juce::String getInstanceName() const override { return instanceName_; }
    int getSourceShape() const override { return sourceShapeParam_ ? static_cast<int>(sourceShapeParam_->load(std::memory_order_relaxed)) : 0; }
    bool getShowSphere() const override { return showAudibleSphereParam_ && showAudibleSphereParam_->load(std::memory_order_relaxed) >= 0.5f; }

    // juce::Timer override — collects foreign source positions for GL view
    void timerCallback() override;
    // APVTS::Listener override
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // Dev panel: Presence shelf
    std::atomic<float>* presenceShelfFreqParam = nullptr;
    std::atomic<float>* presenceShelfMaxDbParam = nullptr;

    // Dev panel: Ear canal resonance
    std::atomic<float>* earCanalFreqParam  = nullptr;
    std::atomic<float>* earCanalQParam     = nullptr;
    std::atomic<float>* earCanalMaxDbParam = nullptr;

    // Aux send
    std::atomic<float>* auxSendGainMaxDbParam = nullptr;

    // Dev panel: Pinna P1 fixed peak
    std::atomic<float>* pinnaP1FreqHzParam = nullptr;
    std::atomic<float>* pinnaP1GainDbParam = nullptr;
    std::atomic<float>* pinnaP1QParam      = nullptr;

    // Dev panel: Pinna N2 secondary notch
    std::atomic<float>* pinnaN2OffsetHzParam = nullptr;
    std::atomic<float>* pinnaN2GainDbParam   = nullptr;
    std::atomic<float>* pinnaN2QParam        = nullptr;

    // Dev panel: Pinna N1 range limits
    std::atomic<float>* pinnaN1MinHzParam = nullptr;
    std::atomic<float>* pinnaN1MaxHzParam = nullptr;

    // Dev panel: Floor bounce HF absorption
    std::atomic<float>* floorAbsHzParam = nullptr;

    // Dev panel: Near-field LF boost
    std::atomic<float>* nearFieldLFHzParam    = nullptr;
    std::atomic<float>* nearFieldLFMaxDbParam = nullptr;

    // Dev panel: Air absorption stage 2
    std::atomic<float>* airAbs2MaxHzParam = nullptr;
    std::atomic<float>* airAbs2MinHzParam = nullptr;

    // Dev panel: Distance gain law
    std::atomic<float>* distGainFloorDbParam = nullptr;
    std::atomic<float>* distGainMaxParam     = nullptr;
    std::atomic<float>* distCurveSteepParam  = nullptr;

    // Dev panel: Head shadow fully-open cap
    std::atomic<float>* headShadowFullOpenHzParam = nullptr;

    // Input gain
    std::atomic<float>* inputGainDbParam = nullptr;

    // Dev panel: Geometry
    std::atomic<float>* sphereRadiusParam       = nullptr;
    std::atomic<float>* vertMonoCylRadiusParam  = nullptr;

    // Dev panel: Test tone
    std::atomic<float>* testToneEnabledParam  = nullptr;
    std::atomic<float>* testToneGainDbParam   = nullptr;
    std::atomic<float>* testTonePitchHzParam  = nullptr;
    std::atomic<float>* testTonePulseHzParam  = nullptr;
    std::atomic<float>* testToneWaveformParam = nullptr;

    // Dev panel: Expanded pinna EQ (P5)
    std::atomic<float>* conchaNotchFreqParam  = nullptr;
    std::atomic<float>* conchaNotchQParam     = nullptr;
    std::atomic<float>* conchaNotchMaxDbParam = nullptr;
    std::atomic<float>* upperPinnaFreqParam   = nullptr;
    std::atomic<float>* upperPinnaQParam      = nullptr;
    std::atomic<float>* upperPinnaMinDbParam  = nullptr;
    std::atomic<float>* upperPinnaMaxDbParam  = nullptr;
    std::atomic<float>* shoulderPeakFreqParam = nullptr;
    std::atomic<float>* shoulderPeakQParam    = nullptr;
    std::atomic<float>* shoulderPeakMaxDbParam= nullptr;
    std::atomic<float>* tragusNotchFreqParam  = nullptr;
    std::atomic<float>* tragusNotchQParam     = nullptr;
    std::atomic<float>* tragusNotchMaxDbParam = nullptr;
    std::atomic<float>* bypassExpandedPinnaParam = nullptr;

    // Early Reflections (Image Source Method)
    std::atomic<float>* erEnabledParam    = nullptr;
    std::atomic<float>* erRoomSizeParam   = nullptr;
    std::atomic<float>* erDampingParam    = nullptr;
    std::atomic<float>* erLevelParam      = nullptr;
    std::atomic<float>* erReverbSendParam = nullptr;
    std::atomic<float>* erGainDbParam     = nullptr;
    std::atomic<float>* bypassERParam     = nullptr;

    // Binaural toggle (user-facing)
    std::atomic<float>* binauralEnabledParam  = nullptr;

    // Dev panel: Per-feature bypass toggles
    std::atomic<float>* bypassITDParam        = nullptr;
    std::atomic<float>* bypassHeadShadowParam = nullptr;
    std::atomic<float>* bypassILDParam        = nullptr;
    std::atomic<float>* bypassNearFieldParam  = nullptr;
    std::atomic<float>* bypassRearShadowParam = nullptr;
    std::atomic<float>* bypassPinnaEQParam    = nullptr;
    std::atomic<float>* bypassCombParam       = nullptr;
    std::atomic<float>* bypassChestParam      = nullptr;
    std::atomic<float>* bypassFloorParam      = nullptr;
    std::atomic<float>* bypassDistGainParam   = nullptr;
    std::atomic<float>* bypassDopplerParam    = nullptr;
    std::atomic<float>* bypassAirAbsParam     = nullptr;
    std::atomic<float>* bypassReverbParam     = nullptr;

    // Audio frame counters
    int frameIdx_ = 0;
    int cooldownLeft_ = 0;
    int driftIdx_ = 0;
    int driftLeft_ = 0;
    uint64_t sampleFrames_ = 0;
    std::atomic<bool> cfgValid_{false};   // mirrors sessionCfg_.isReady()

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XYZPanProcessor)
};
