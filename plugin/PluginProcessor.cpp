#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParamIDs.h"
#include "xyzpan/Types.h"
#include "xyzpan/Constants.h"
#include <cmath>

XYZPanProcessor::XYZPanProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input",   juce::AudioChannelSet::stereo(), true)
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

    // Stereo source node splitting
    stereoWidthParam        = apvts.getRawParameterValue(ParamID::STEREO_WIDTH);
    stereoFaceListenerParam = apvts.getRawParameterValue(ParamID::STEREO_FACE_LISTENER);
    stereoOrbitPhaseParam   = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_PHASE);
    stereoOrbitOffsetParam  = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_OFFSET);

    jassert(stereoWidthParam        != nullptr);
    jassert(stereoFaceListenerParam != nullptr);
    jassert(stereoOrbitPhaseParam   != nullptr);
    jassert(stereoOrbitOffsetParam  != nullptr);

    // Stereo orbit LFOs — XY
    orbitXYWaveformParam   = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_XY_WAVEFORM);
    orbitXYRateParam       = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_XY_RATE);
    orbitXYBeatDivParam    = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_XY_BEAT_DIV);
    orbitXYPhaseParam      = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_XY_PHASE);
    orbitXYResetPhaseParam = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_XY_RESET_PHASE);
    orbitXYDepthParam      = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_XY_DEPTH);

    jassert(orbitXYWaveformParam   != nullptr);
    jassert(orbitXYRateParam       != nullptr);
    jassert(orbitXYBeatDivParam    != nullptr);
    jassert(orbitXYPhaseParam      != nullptr);
    jassert(orbitXYResetPhaseParam != nullptr);
    jassert(orbitXYDepthParam      != nullptr);

    // Stereo orbit LFOs — XZ
    orbitXZWaveformParam   = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_XZ_WAVEFORM);
    orbitXZRateParam       = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_XZ_RATE);
    orbitXZBeatDivParam    = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_XZ_BEAT_DIV);
    orbitXZPhaseParam      = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_XZ_PHASE);
    orbitXZResetPhaseParam = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_XZ_RESET_PHASE);
    orbitXZDepthParam      = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_XZ_DEPTH);

    jassert(orbitXZWaveformParam   != nullptr);
    jassert(orbitXZRateParam       != nullptr);
    jassert(orbitXZBeatDivParam    != nullptr);
    jassert(orbitXZPhaseParam      != nullptr);
    jassert(orbitXZResetPhaseParam != nullptr);
    jassert(orbitXZDepthParam      != nullptr);

    // Stereo orbit LFOs — YZ
    orbitYZWaveformParam   = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_YZ_WAVEFORM);
    orbitYZRateParam       = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_YZ_RATE);
    orbitYZBeatDivParam    = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_YZ_BEAT_DIV);
    orbitYZPhaseParam      = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_YZ_PHASE);
    orbitYZResetPhaseParam = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_YZ_RESET_PHASE);
    orbitYZDepthParam      = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_YZ_DEPTH);

    jassert(orbitYZWaveformParam   != nullptr);
    jassert(orbitYZRateParam       != nullptr);
    jassert(orbitYZBeatDivParam    != nullptr);
    jassert(orbitYZPhaseParam      != nullptr);
    jassert(orbitYZResetPhaseParam != nullptr);
    jassert(orbitYZDepthParam      != nullptr);

    // Stereo orbit shared
    orbitTempoSyncParam = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_TEMPO_SYNC);
    orbitSpeedMulParam  = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_SPEED_MUL);

    jassert(orbitTempoSyncParam != nullptr);
    jassert(orbitSpeedMulParam  != nullptr);

    // Dev panel: Presence shelf
    presenceShelfFreqParam  = apvts.getRawParameterValue(ParamID::PRESENCE_SHELF_FREQ_HZ);
    presenceShelfMaxDbParam = apvts.getRawParameterValue(ParamID::PRESENCE_SHELF_MAX_DB);

    jassert(presenceShelfFreqParam  != nullptr);
    jassert(presenceShelfMaxDbParam != nullptr);

    // Dev panel: Ear canal resonance
    earCanalFreqParam  = apvts.getRawParameterValue(ParamID::EAR_CANAL_FREQ_HZ);
    earCanalQParam     = apvts.getRawParameterValue(ParamID::EAR_CANAL_Q);
    earCanalMaxDbParam = apvts.getRawParameterValue(ParamID::EAR_CANAL_MAX_DB);

    jassert(earCanalFreqParam  != nullptr);
    jassert(earCanalQParam     != nullptr);
    jassert(earCanalMaxDbParam != nullptr);

    // Dev panel: Aux send
    auxSendGainMaxDbParam = apvts.getRawParameterValue(ParamID::AUX_SEND_GAIN_MAX_DB);
    jassert(auxSendGainMaxDbParam != nullptr);

    // Dev panel: Geometry
    sphereRadiusParam      = apvts.getRawParameterValue(ParamID::SPHERE_RADIUS);
    vertMonoCylRadiusParam = apvts.getRawParameterValue(ParamID::VERT_MONO_CYLINDER_RADIUS);

    jassert(sphereRadiusParam      != nullptr);
    jassert(vertMonoCylRadiusParam != nullptr);

    // Dev panel: Test tone
    testToneEnabledParam  = apvts.getRawParameterValue(ParamID::TEST_TONE_ENABLED);
    testToneGainDbParam   = apvts.getRawParameterValue(ParamID::TEST_TONE_GAIN_DB);
    testTonePitchHzParam  = apvts.getRawParameterValue(ParamID::TEST_TONE_PITCH_HZ);
    testTonePulseHzParam  = apvts.getRawParameterValue(ParamID::TEST_TONE_PULSE_HZ);
    testToneWaveformParam = apvts.getRawParameterValue(ParamID::TEST_TONE_WAVEFORM);

    jassert(testToneEnabledParam  != nullptr);
    jassert(testToneGainDbParam   != nullptr);
    jassert(testTonePitchHzParam  != nullptr);
    jassert(testTonePulseHzParam  != nullptr);
    jassert(testToneWaveformParam != nullptr);
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
    // Accept both mono and stereo input (engine handles both via numInputChannels).
    // Stereo input required for stereo source node splitting (width > 0).
    const auto& in = layouts.getMainInputChannelSet();
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
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

    // Beat div params are AudioParameterChoice — raw value is the choice index (0–10).
    // Convert to float multiplier via kBeatDivValues lookup.
    auto beatDivFromChoice = [](float rawIndex) -> float {
        int idx = juce::jlimit(0, xyzpan::kBeatDivCount - 1,
                               static_cast<int>(std::round(rawIndex)));
        return xyzpan::kBeatDivValues[idx];
    };
    params.lfoXBeatDiv   = beatDivFromChoice(lfoXBeatDivParam->load());
    params.lfoYBeatDiv   = beatDivFromChoice(lfoYBeatDivParam->load());
    params.lfoZBeatDiv   = beatDivFromChoice(lfoZBeatDivParam->load());
    // Note: lfoXPhase (and Y/Z) are snapshotted but the engine applies them as initial
    // accumulator offsets only via lfoX_.reset() in Engine::reset(). Live phase changes
    // in a running LFO are not applied per-block — intentional for v1 (no phase-jump clicks).

    // Stereo source node splitting
    params.stereoWidth        = stereoWidthParam->load();
    params.stereoFaceListener = stereoFaceListenerParam->load() >= 0.5f;
    params.stereoOrbitPhase   = stereoOrbitPhaseParam->load();
    params.stereoOrbitOffset  = stereoOrbitOffsetParam->load();

    // Stereo orbit LFOs — XY
    params.stereoOrbitXYWaveform   = static_cast<int>(std::round(orbitXYWaveformParam->load()));
    params.stereoOrbitXYRate       = orbitXYRateParam->load();
    params.stereoOrbitXYBeatDiv    = beatDivFromChoice(orbitXYBeatDivParam->load());
    params.stereoOrbitXYPhase      = orbitXYPhaseParam->load();
    params.stereoOrbitXYResetPhase = orbitXYResetPhaseParam->load() >= 0.5f;
    params.stereoOrbitXYDepth      = orbitXYDepthParam->load();

    // Stereo orbit LFOs — XZ
    params.stereoOrbitXZWaveform   = static_cast<int>(std::round(orbitXZWaveformParam->load()));
    params.stereoOrbitXZRate       = orbitXZRateParam->load();
    params.stereoOrbitXZBeatDiv    = beatDivFromChoice(orbitXZBeatDivParam->load());
    params.stereoOrbitXZPhase      = orbitXZPhaseParam->load();
    params.stereoOrbitXZResetPhase = orbitXZResetPhaseParam->load() >= 0.5f;
    params.stereoOrbitXZDepth      = orbitXZDepthParam->load();

    // Stereo orbit LFOs — YZ
    params.stereoOrbitYZWaveform   = static_cast<int>(std::round(orbitYZWaveformParam->load()));
    params.stereoOrbitYZRate       = orbitYZRateParam->load();
    params.stereoOrbitYZBeatDiv    = beatDivFromChoice(orbitYZBeatDivParam->load());
    params.stereoOrbitYZPhase      = orbitYZPhaseParam->load();
    params.stereoOrbitYZResetPhase = orbitYZResetPhaseParam->load() >= 0.5f;
    params.stereoOrbitYZDepth      = orbitYZDepthParam->load();

    // Stereo orbit shared
    params.stereoOrbitTempoSync = orbitTempoSyncParam->load() >= 0.5f;
    params.stereoOrbitSpeedMul  = orbitSpeedMulParam->load();

    // Dev panel: Presence shelf
    params.presenceShelfFreqHz = presenceShelfFreqParam->load();
    params.presenceShelfMaxDb  = presenceShelfMaxDbParam->load();

    // Dev panel: Ear canal resonance
    params.earCanalFreqHz = earCanalFreqParam->load();
    params.earCanalQ      = earCanalQParam->load();
    params.earCanalMaxDb  = earCanalMaxDbParam->load();

    // Dev panel: Aux send
    params.auxSendGainMaxDb = auxSendGainMaxDbParam->load();

    // Dev panel: Geometry
    params.sphereRadius           = sphereRadiusParam->load();
    params.vertMonoCylinderRadius = vertMonoCylRadiusParam->load();

    // Dev panel: Test tone
    params.testToneEnabled  = testToneEnabledParam->load() >= 0.5f;
    params.testToneGainDb   = testToneGainDbParam->load();
    params.testTonePitchHz  = testTonePitchHzParam->load();
    params.testTonePulseHz  = testTonePulseHzParam->load();
    params.testToneWaveform = static_cast<xyzpan::TestToneWaveform>(
        static_cast<int>(std::round(testToneWaveformParam->load())));

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
    engine.process(inputs, numIn, outL, outR, nullptr, nullptr, buffer.getNumSamples());

    // Phase 6: update PositionBridge with last modulated position (UI-07)
    // Written here on audio thread; XYZPanGLView reads it on GL thread via bridge_.read()
    auto mp = engine.getLastModulatedPosition();
    xyzpan::SourcePositionSnapshot snap;
    snap.x = mp.x;
    snap.y = mp.y;
    snap.z = mp.z;
    snap.distance = std::sqrt(mp.x * mp.x + mp.y * mp.y + mp.z * mp.z);

    // Stereo node positions for GL rendering
    auto sn = engine.getLastStereoNodes();
    snap.lNodeX = sn.lx;  snap.lNodeY = sn.ly;  snap.lNodeZ = sn.lz;
    snap.rNodeX = sn.rx;  snap.rNodeY = sn.ry;  snap.rNodeZ = sn.rz;
    snap.stereoWidth = sn.width;

    snap.sphereRadius = sphereRadiusParam->load();

    positionBridge.write(snap);

    // DSP state bridge for dev panel readouts
    dspStateBridge.write(engine.getLastDSPState());
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
