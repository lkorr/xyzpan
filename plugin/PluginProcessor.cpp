#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParamIDs.h"
#include "xyzpan/Types.h"
#include <cmath>

XYZPanProcessor::XYZPanProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input",   juce::AudioChannelSet::mono(),   true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "XYZPanState", createParameterLayout()) {
    // Spatial position (Phase 1)
    xParam = apvts.getRawParameterValue(ParamID::X);
    yParam = apvts.getRawParameterValue(ParamID::Y);
    zParam = apvts.getRawParameterValue(ParamID::Z);

    jassert(xParam != nullptr);
    jassert(yParam != nullptr);
    jassert(zParam != nullptr);

    // Phase 6: R scale/radius (PARAM-01)
    rParam = apvts.getRawParameterValue(ParamID::R);
    jassert(rParam != nullptr);

    // Dev panel: binaural panning tuning (Phase 2)
    itdMaxParam       = apvts.getRawParameterValue(ParamID::ITD_MAX_MS);
    headShadowHzParam = apvts.getRawParameterValue(ParamID::HEAD_SHADOW_HZ);
    ildMaxDbParam     = apvts.getRawParameterValue(ParamID::ILD_MAX_DB);
    rearShadowHzParam = apvts.getRawParameterValue(ParamID::REAR_SHADOW_HZ);
    smoothItdParam    = apvts.getRawParameterValue(ParamID::SMOOTH_ITD_MS);
    smoothFilterParam = apvts.getRawParameterValue(ParamID::SMOOTH_FILTER_MS);
    smoothGainParam   = apvts.getRawParameterValue(ParamID::SMOOTH_GAIN_MS);

    jassert(itdMaxParam       != nullptr);
    jassert(headShadowHzParam != nullptr);
    jassert(ildMaxDbParam     != nullptr);
    jassert(rearShadowHzParam != nullptr);
    jassert(smoothItdParam    != nullptr);
    jassert(smoothFilterParam != nullptr);
    jassert(smoothGainParam   != nullptr);

    // Dev panel: Comb filter bank (Phase 3)
    for (int i = 0; i < 10; ++i) {
        combDelayParam[i] = apvts.getRawParameterValue(ParamID::COMB_DELAY[i]);
        combFbParam[i]    = apvts.getRawParameterValue(ParamID::COMB_FB[i]);
        jassert(combDelayParam[i] != nullptr);
        jassert(combFbParam[i]    != nullptr);
    }
    combWetMaxParam   = apvts.getRawParameterValue(ParamID::COMB_WET_MAX);
    jassert(combWetMaxParam != nullptr);

    // Dev panel: Elevation filters (Phase 3)
    pinnaNotchHzParam = apvts.getRawParameterValue(ParamID::PINNA_NOTCH_HZ);
    pinnaNotchQParam  = apvts.getRawParameterValue(ParamID::PINNA_NOTCH_Q);
    pinnaShelfHzParam = apvts.getRawParameterValue(ParamID::PINNA_SHELF_HZ);
    chestDelayMsParam = apvts.getRawParameterValue(ParamID::CHEST_DELAY_MS);
    chestGainDbParam  = apvts.getRawParameterValue(ParamID::CHEST_GAIN_DB);
    floorDelayMsParam = apvts.getRawParameterValue(ParamID::FLOOR_DELAY_MS);
    floorGainDbParam  = apvts.getRawParameterValue(ParamID::FLOOR_GAIN_DB);

    jassert(pinnaNotchHzParam != nullptr);
    jassert(pinnaNotchQParam  != nullptr);
    jassert(pinnaShelfHzParam != nullptr);
    jassert(chestDelayMsParam != nullptr);
    jassert(chestGainDbParam  != nullptr);
    jassert(floorDelayMsParam != nullptr);
    jassert(floorGainDbParam  != nullptr);

    // Dev panel: Distance processing (Phase 4)
    distDelayMaxMsParam = apvts.getRawParameterValue(ParamID::DIST_DELAY_MAX_MS);
    distSmoothMsParam   = apvts.getRawParameterValue(ParamID::DIST_SMOOTH_MS);
    dopplerEnabledParam = apvts.getRawParameterValue(ParamID::DOPPLER_ENABLED);
    airAbsMaxHzParam    = apvts.getRawParameterValue(ParamID::AIR_ABS_MAX_HZ);
    airAbsMinHzParam    = apvts.getRawParameterValue(ParamID::AIR_ABS_MIN_HZ);

    jassert(distDelayMaxMsParam != nullptr);
    jassert(distSmoothMsParam   != nullptr);
    jassert(dopplerEnabledParam != nullptr);
    jassert(airAbsMaxHzParam    != nullptr);
    jassert(airAbsMinHzParam    != nullptr);

    // Phase 5: Reverb (VERB-03)
    verbSizeParam     = apvts.getRawParameterValue(ParamID::VERB_SIZE);
    verbDecayParam    = apvts.getRawParameterValue(ParamID::VERB_DECAY);
    verbDampingParam  = apvts.getRawParameterValue(ParamID::VERB_DAMPING);
    verbWetParam      = apvts.getRawParameterValue(ParamID::VERB_WET);
    verbPreDelayParam = apvts.getRawParameterValue(ParamID::VERB_PRE_DELAY);

    jassert(verbSizeParam     != nullptr);
    jassert(verbDecayParam    != nullptr);
    jassert(verbDampingParam  != nullptr);
    jassert(verbWetParam      != nullptr);
    jassert(verbPreDelayParam != nullptr);

    // Phase 5: LFO — per axis (LFO-01 through LFO-05)
    lfoXRateParam     = apvts.getRawParameterValue(ParamID::LFO_X_RATE);
    lfoXDepthParam    = apvts.getRawParameterValue(ParamID::LFO_X_DEPTH);
    lfoXPhaseParam    = apvts.getRawParameterValue(ParamID::LFO_X_PHASE);
    lfoXWaveformParam = apvts.getRawParameterValue(ParamID::LFO_X_WAVEFORM);
    lfoYRateParam     = apvts.getRawParameterValue(ParamID::LFO_Y_RATE);
    lfoYDepthParam    = apvts.getRawParameterValue(ParamID::LFO_Y_DEPTH);
    lfoYPhaseParam    = apvts.getRawParameterValue(ParamID::LFO_Y_PHASE);
    lfoYWaveformParam = apvts.getRawParameterValue(ParamID::LFO_Y_WAVEFORM);
    lfoZRateParam     = apvts.getRawParameterValue(ParamID::LFO_Z_RATE);
    lfoZDepthParam    = apvts.getRawParameterValue(ParamID::LFO_Z_DEPTH);
    lfoZPhaseParam    = apvts.getRawParameterValue(ParamID::LFO_Z_PHASE);
    lfoZWaveformParam = apvts.getRawParameterValue(ParamID::LFO_Z_WAVEFORM);
    lfoTempoSyncParam = apvts.getRawParameterValue(ParamID::LFO_TEMPO_SYNC);
    lfoXBeatDivParam  = apvts.getRawParameterValue(ParamID::LFO_X_BEAT_DIV);
    lfoYBeatDivParam  = apvts.getRawParameterValue(ParamID::LFO_Y_BEAT_DIV);
    lfoZBeatDivParam  = apvts.getRawParameterValue(ParamID::LFO_Z_BEAT_DIV);

    jassert(lfoXRateParam     != nullptr);
    jassert(lfoXDepthParam    != nullptr);
    jassert(lfoXPhaseParam    != nullptr);
    jassert(lfoXWaveformParam != nullptr);
    jassert(lfoYRateParam     != nullptr);
    jassert(lfoYDepthParam    != nullptr);
    jassert(lfoYPhaseParam    != nullptr);
    jassert(lfoYWaveformParam != nullptr);
    jassert(lfoZRateParam     != nullptr);
    jassert(lfoZDepthParam    != nullptr);
    jassert(lfoZPhaseParam    != nullptr);
    jassert(lfoZWaveformParam != nullptr);
    jassert(lfoTempoSyncParam != nullptr);
    jassert(lfoXBeatDivParam  != nullptr);
    jassert(lfoYBeatDivParam  != nullptr);
    jassert(lfoZBeatDivParam  != nullptr);
}

void XYZPanProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine.prepare(sampleRate, samplesPerBlock);

    // Phase 6: prepare R smoother — 20ms matches engine's internal position smoothing window
    rSmooth_.prepare(20.0f, static_cast<float>(sampleRate));
    rSmooth_.reset(rParam != nullptr ? rParam->load() : 1.0f);
}

void XYZPanProcessor::releaseResources() {
    engine.reset();
}

bool XYZPanProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    // XYZPan requires exactly mono input and stereo output.
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::mono())   return false;
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    return true;
}

void XYZPanProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& /*midiMessages*/) {
    juce::ScopedNoDenormals noDenormals;

    // Snapshot current parameter values from APVTS atomics (safe on audio thread)
    xyzpan::EngineParams params;
    // Phase 6: smoothed R multiplier — rSmooth_ prevents zipper noise during automation (PARAM-03)
    // Process once per block (R multiplies position, not audio; per-block smoothing is sufficient)
    const float r = rSmooth_.process(rParam->load());
    // Spatial position — scaled by smoothed R
    params.x = xParam->load() * r;
    params.y = yParam->load() * r;
    params.z = zParam->load() * r;
    // Dev panel: binaural panning tuning
    params.maxITD_ms       = itdMaxParam->load();
    params.headShadowMinHz = headShadowHzParam->load();
    params.ildMaxDb        = ildMaxDbParam->load();
    params.rearShadowMinHz = rearShadowHzParam->load();
    params.smoothMs_ITD    = smoothItdParam->load();
    params.smoothMs_Filter = smoothFilterParam->load();
    params.smoothMs_Gain   = smoothGainParam->load();

    // Dev panel: Comb filter bank (Phase 3)
    for (int i = 0; i < 10; ++i) {
        params.combDelays_ms[i] = combDelayParam[i]->load();
        params.combFeedback[i]  = combFbParam[i]->load();
    }
    params.combWetMax       = combWetMaxParam->load();

    // Dev panel: Elevation filters (Phase 3)
    params.pinnaNotchFreqHz = pinnaNotchHzParam->load();
    params.pinnaNotchQ      = pinnaNotchQParam->load();
    params.pinnaShelfFreqHz = pinnaShelfHzParam->load();
    params.chestDelayMaxMs  = chestDelayMsParam->load();
    params.chestGainDb      = chestGainDbParam->load();
    params.floorDelayMaxMs  = floorDelayMsParam->load();
    params.floorGainDb      = floorGainDbParam->load();

    // Dev panel: Distance processing (Phase 4)
    params.distDelayMaxMs = distDelayMaxMsParam->load();
    params.distSmoothMs   = distSmoothMsParam->load();
    params.dopplerEnabled = dopplerEnabledParam->load() >= 0.5f;  // float->bool conversion
    params.airAbsMaxHz    = airAbsMaxHzParam->load();
    params.airAbsMinHz    = airAbsMinHzParam->load();

    // Phase 5: Reverb (VERB-03)
    params.verbSize        = verbSizeParam->load();
    params.verbDecay       = verbDecayParam->load();
    params.verbDamping     = verbDampingParam->load();
    params.verbWet         = verbWetParam->load();
    params.verbPreDelayMax = verbPreDelayParam->load();

    // Phase 5: LFO (LFO-01 through LFO-05)
    params.lfoXRate      = lfoXRateParam->load();
    params.lfoXDepth     = lfoXDepthParam->load();
    params.lfoXPhase     = lfoXPhaseParam->load();
    params.lfoXWaveform  = static_cast<int>(std::round(lfoXWaveformParam->load()));
    params.lfoYRate      = lfoYRateParam->load();
    params.lfoYDepth     = lfoYDepthParam->load();
    params.lfoYPhase     = lfoYPhaseParam->load();
    params.lfoYWaveform  = static_cast<int>(std::round(lfoYWaveformParam->load()));
    params.lfoZRate      = lfoZRateParam->load();
    params.lfoZDepth     = lfoZDepthParam->load();
    params.lfoZPhase     = lfoZPhaseParam->load();
    params.lfoZWaveform  = static_cast<int>(std::round(lfoZWaveformParam->load()));
    params.lfoTempoSync  = lfoTempoSyncParam->load() >= 0.5f;
    params.lfoXBeatDiv   = lfoXBeatDivParam->load();
    params.lfoYBeatDiv   = lfoYBeatDivParam->load();
    params.lfoZBeatDiv   = lfoZBeatDivParam->load();
    // Note: lfoXPhase (and Y/Z) are snapshotted but the engine applies them as initial
    // accumulator offsets only via lfoX_.reset() in Engine::reset(). Live phase changes
    // in a running LFO are not applied per-block — intentional for v1 (no phase-jump clicks).

    // Phase 5: Read host BPM for LFO tempo sync (LFO-05)
    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            if (pos->getBpm().hasValue())
                params.hostBpm = static_cast<float>(*pos->getBpm());
            // else: hostBpm stays at its EngineParams default (120.0f)
        }
    }

    // Build input channel pointer array.
    // Use getTotalNumInputChannels() for numIn — NOT buffer.getNumChannels(), which
    // returns total channel slots (inputs + outputs). For mono-in/stereo-out, the buffer
    // has 2 channels but only channel 0 is valid input; channel 1 is uninitialized output.
    const int numIn = getTotalNumInputChannels();
    const float* inputs[2] = {
        numIn > 0 ? buffer.getReadPointer(0) : nullptr,
        numIn > 1 ? buffer.getReadPointer(1) : nullptr
    };

    // Output channel pointers (buffer has stereo output per bus layout)
    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getWritePointer(1);

    engine.setParams(params);
    engine.process(inputs, numIn, outL, outR, buffer.getNumSamples());

    // Phase 6: update PositionBridge with last modulated position (UI-07)
    // Written here on audio thread; XYZPanGLView reads it on GL thread via bridge_.read()
    auto mp = engine.getLastModulatedPosition();
    xyzpan::SourcePositionSnapshot snap;
    snap.x = mp.x;
    snap.y = mp.y;
    snap.z = mp.z;
    snap.distance = std::sqrt(mp.x * mp.x + mp.y * mp.y + mp.z * mp.z);
    positionBridge.write(snap);
}

juce::AudioProcessorEditor* XYZPanProcessor::createEditor() {
    return new XYZPanEditor(*this);
}

void XYZPanProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void XYZPanProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

// Plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new XYZPanProcessor();
}
