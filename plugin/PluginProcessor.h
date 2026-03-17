#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "xyzpan/Engine.h"
#include "xyzpan/DSPStateBridge.h"
#include "xyzpan/dsp/OnePoleSmooth.h"
#include "ParamLayout.h"
#include "PositionBridge.h"

class XYZPanProcessor : public juce::AudioProcessor {
public:
    XYZPanProcessor();
    ~XYZPanProcessor() override = default;

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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // APVTS — public so editor and parameter attachments can access it
    juce::AudioProcessorValueTreeState apvts;

    // Phase 6: PositionBridge for audio-to-GL position transfer (UI-07)
    // Public so XYZPanEditor / XYZPanGLView can hold a reference
    xyzpan::PositionBridge positionBridge;

    // DSP state bridge for dev panel readouts (audio thread → UI thread)
    xyzpan::DSPStateBridge dspStateBridge;

private:
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
    std::atomic<float>* floorDelayMsParam  = nullptr;
    std::atomic<float>* floorGainDbParam   = nullptr;

    // Dev panel: Distance processing (Phase 4)
    std::atomic<float>* distDelayMaxMsParam = nullptr;
    std::atomic<float>* distSmoothMsParam   = nullptr;
    std::atomic<float>* dopplerEnabledParam = nullptr;  // AudioParameterBool stores as float 0/1
    std::atomic<float>* airAbsMaxHzParam    = nullptr;
    std::atomic<float>* airAbsMinHzParam    = nullptr;

    // Phase 5: Reverb (VERB-03)
    std::atomic<float>* verbSizeParam     = nullptr;
    std::atomic<float>* verbDecayParam    = nullptr;
    std::atomic<float>* verbDampingParam  = nullptr;
    std::atomic<float>* verbWetParam      = nullptr;
    std::atomic<float>* verbPreDelayParam = nullptr;

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
    std::atomic<float>* lfoTempoSyncParam = nullptr;
    std::atomic<float>* lfoXBeatDivParam  = nullptr;
    std::atomic<float>* lfoYBeatDivParam  = nullptr;
    std::atomic<float>* lfoZBeatDivParam  = nullptr;

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

    // Stereo orbit LFOs — XZ plane
    std::atomic<float>* orbitXZWaveformParam   = nullptr;
    std::atomic<float>* orbitXZRateParam       = nullptr;
    std::atomic<float>* orbitXZBeatDivParam    = nullptr;
    std::atomic<float>* orbitXZPhaseParam      = nullptr;
    std::atomic<float>* orbitXZResetPhaseParam = nullptr;
    std::atomic<float>* orbitXZDepthParam      = nullptr;

    // Stereo orbit LFOs — YZ plane
    std::atomic<float>* orbitYZWaveformParam   = nullptr;
    std::atomic<float>* orbitYZRateParam       = nullptr;
    std::atomic<float>* orbitYZBeatDivParam    = nullptr;
    std::atomic<float>* orbitYZPhaseParam      = nullptr;
    std::atomic<float>* orbitYZResetPhaseParam = nullptr;
    std::atomic<float>* orbitYZDepthParam      = nullptr;

    // Stereo orbit shared
    std::atomic<float>* orbitTempoSyncParam = nullptr;
    std::atomic<float>* orbitSpeedMulParam  = nullptr;

    // Dev panel: Presence shelf
    std::atomic<float>* presenceShelfFreqParam = nullptr;
    std::atomic<float>* presenceShelfMaxDbParam = nullptr;

    // Dev panel: Ear canal resonance
    std::atomic<float>* earCanalFreqParam  = nullptr;
    std::atomic<float>* earCanalQParam     = nullptr;
    std::atomic<float>* earCanalMaxDbParam = nullptr;

    // Dev panel: Aux send
    std::atomic<float>* auxSendGainMaxDbParam = nullptr;

    // Dev panel: Geometry
    std::atomic<float>* sphereRadiusParam       = nullptr;
    std::atomic<float>* vertMonoCylRadiusParam  = nullptr;

    // Dev panel: Test tone
    std::atomic<float>* testToneEnabledParam  = nullptr;
    std::atomic<float>* testToneGainDbParam   = nullptr;
    std::atomic<float>* testTonePitchHzParam  = nullptr;
    std::atomic<float>* testTonePulseHzParam  = nullptr;
    std::atomic<float>* testToneWaveformParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XYZPanProcessor)
};
