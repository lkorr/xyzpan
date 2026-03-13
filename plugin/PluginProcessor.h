#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "xyzpan/Engine.h"
#include "ParamLayout.h"

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
    double getTailLengthSeconds() const override { return 0.320; }
    // 300ms distance delay + 20ms floor bounce

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // APVTS — public so editor and parameter attachments can access it
    juce::AudioProcessorValueTreeState apvts;

private:
    xyzpan::XYZPanEngine engine;

    // Raw parameter value pointers (thread-safe atomics managed by APVTS)
    // Spatial position (Phase 1)
    std::atomic<float>* xParam = nullptr;
    std::atomic<float>* yParam = nullptr;
    std::atomic<float>* zParam = nullptr;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XYZPanProcessor)
};
