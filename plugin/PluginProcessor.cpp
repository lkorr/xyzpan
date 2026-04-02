#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParamIDs.h"
#include "Presets.h"
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
    chestHPFHzParam   = apvts.getRawParameterValue(ParamID::CHEST_HPF_HZ);
    chestLPHzParam    = apvts.getRawParameterValue(ParamID::CHEST_LP_HZ);

    jassert(chestDelayMsParam != nullptr);
    jassert(chestGainDbParam  != nullptr);
    jassert(chestHPFHzParam   != nullptr);
    jassert(chestLPHzParam    != nullptr);
    jassert(floorDelayMsParam != nullptr);
    jassert(floorGainDbParam  != nullptr);

    // Dev panel: Distance processing (Phase 4)
    distDelayMaxMsParam = apvts.getRawParameterValue(ParamID::DIST_DELAY_MAX_MS);
    distSmoothMsParam   = apvts.getRawParameterValue(ParamID::DIST_SMOOTH_MS);
    airAbsMaxHzParam    = apvts.getRawParameterValue(ParamID::AIR_ABS_MAX_HZ);
    airAbsMinHzParam    = apvts.getRawParameterValue(ParamID::AIR_ABS_MIN_HZ);

    jassert(distDelayMaxMsParam != nullptr);
    jassert(distSmoothMsParam   != nullptr);
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
    lfoXTempoSyncParam = apvts.getRawParameterValue(ParamID::LFO_X_TEMPO_SYNC);
    lfoYTempoSyncParam = apvts.getRawParameterValue(ParamID::LFO_Y_TEMPO_SYNC);
    lfoZTempoSyncParam = apvts.getRawParameterValue(ParamID::LFO_Z_TEMPO_SYNC);
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
    lfoXSmoothParam  = apvts.getRawParameterValue(ParamID::LFO_X_SMOOTH);
    lfoYSmoothParam  = apvts.getRawParameterValue(ParamID::LFO_Y_SMOOTH);
    lfoZSmoothParam  = apvts.getRawParameterValue(ParamID::LFO_Z_SMOOTH);
    jassert(lfoXSmoothParam  != nullptr);
    jassert(lfoYSmoothParam  != nullptr);
    jassert(lfoZSmoothParam  != nullptr);
    jassert(lfoXTempoSyncParam != nullptr);
    jassert(lfoYTempoSyncParam != nullptr);
    jassert(lfoZTempoSyncParam != nullptr);
    jassert(lfoXBeatDivParam  != nullptr);
    jassert(lfoYBeatDivParam  != nullptr);
    jassert(lfoZBeatDivParam  != nullptr);

    lfoSpeedMulParam = apvts.getRawParameterValue(ParamID::LFO_SPEED_MUL);
    jassert(lfoSpeedMulParam != nullptr);

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
    orbitXYSmoothParam     = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_XY_SMOOTH);
    jassert(orbitXYSmoothParam     != nullptr);

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
    orbitXZSmoothParam     = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_XZ_SMOOTH);
    jassert(orbitXZSmoothParam     != nullptr);

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
    orbitYZSmoothParam     = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_YZ_SMOOTH);
    jassert(orbitYZSmoothParam     != nullptr);

    // Stereo orbit per-plane sync + shared speed
    orbitXYTempoSyncParam = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_XY_TEMPO_SYNC);
    orbitXZTempoSyncParam = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_XZ_TEMPO_SYNC);
    orbitYZTempoSyncParam = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_YZ_TEMPO_SYNC);
    orbitSpeedMulParam    = apvts.getRawParameterValue(ParamID::STEREO_ORBIT_SPEED_MUL);

    jassert(orbitXYTempoSyncParam != nullptr);
    jassert(orbitXZTempoSyncParam != nullptr);
    jassert(orbitYZTempoSyncParam != nullptr);
    jassert(orbitSpeedMulParam    != nullptr);

    // Listener head orientation
    listenerYawParam        = apvts.getRawParameterValue(ParamID::LISTENER_YAW);
    listenerPitchParam      = apvts.getRawParameterValue(ParamID::LISTENER_PITCH);
    listenerRollParam       = apvts.getRawParameterValue(ParamID::LISTENER_ROLL);
    headFollowsCameraParam  = apvts.getRawParameterValue(ParamID::HEAD_FOLLOWS_CAMERA);
    jassert(listenerYawParam        != nullptr);
    jassert(listenerPitchParam      != nullptr);
    jassert(listenerRollParam       != nullptr);
    jassert(headFollowsCameraParam  != nullptr);

    // Listener link + pilot
    listenerLinkParam  = apvts.getRawParameterValue(ParamID::LISTENER_LINK);
    listenerPilotParam = apvts.getRawParameterValue(ParamID::LISTENER_PILOT);
    jassert(listenerLinkParam  != nullptr);
    jassert(listenerPilotParam != nullptr);

    // Walker — movable listener position (always active)
    walkerXParam     = apvts.getRawParameterValue(ParamID::WALKER_X);
    walkerYParam     = apvts.getRawParameterValue(ParamID::WALKER_Y);
    walkerZParam     = apvts.getRawParameterValue(ParamID::WALKER_Z);
    wasdControlParam = apvts.getRawParameterValue(ParamID::WASD_CONTROL);
    jassert(walkerXParam     != nullptr);
    jassert(walkerYParam     != nullptr);
    jassert(walkerZParam     != nullptr);
    jassert(wasdControlParam != nullptr);

    // Register APVTS listeners for linked orientation + position broadcasting
    apvts.addParameterListener(ParamID::LISTENER_YAW,        this);
    apvts.addParameterListener(ParamID::LISTENER_PITCH,      this);
    apvts.addParameterListener(ParamID::LISTENER_ROLL,       this);
    apvts.addParameterListener(ParamID::HEAD_FOLLOWS_CAMERA, this);
    apvts.addParameterListener(ParamID::LISTENER_LINK,       this);
    apvts.addParameterListener(ParamID::LISTENER_PILOT,     this);
    apvts.addParameterListener(ParamID::WALKER_X,            this);
    apvts.addParameterListener(ParamID::WALKER_Y,            this);
    apvts.addParameterListener(ParamID::WALKER_Z,            this);

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

    // Dev panel: Pinna P1 fixed peak
    pinnaP1FreqHzParam = apvts.getRawParameterValue(ParamID::PINNA_P1_FREQ_HZ);
    pinnaP1GainDbParam = apvts.getRawParameterValue(ParamID::PINNA_P1_GAIN_DB);
    pinnaP1QParam      = apvts.getRawParameterValue(ParamID::PINNA_P1_Q);
    jassert(pinnaP1FreqHzParam != nullptr);
    jassert(pinnaP1GainDbParam != nullptr);
    jassert(pinnaP1QParam      != nullptr);

    // Dev panel: Pinna N2 secondary notch
    pinnaN2OffsetHzParam = apvts.getRawParameterValue(ParamID::PINNA_N2_OFFSET_HZ);
    pinnaN2GainDbParam   = apvts.getRawParameterValue(ParamID::PINNA_N2_GAIN_DB);
    pinnaN2QParam        = apvts.getRawParameterValue(ParamID::PINNA_N2_Q);
    jassert(pinnaN2OffsetHzParam != nullptr);
    jassert(pinnaN2GainDbParam   != nullptr);
    jassert(pinnaN2QParam        != nullptr);

    // Dev panel: Pinna N1 range limits
    pinnaN1MinHzParam = apvts.getRawParameterValue(ParamID::PINNA_N1_MIN_HZ);
    pinnaN1MaxHzParam = apvts.getRawParameterValue(ParamID::PINNA_N1_MAX_HZ);
    jassert(pinnaN1MinHzParam != nullptr);
    jassert(pinnaN1MaxHzParam != nullptr);

    // Dev panel: Floor bounce HF absorption
    floorAbsHzParam = apvts.getRawParameterValue(ParamID::FLOOR_ABS_HZ);
    jassert(floorAbsHzParam != nullptr);

    // Dev panel: Near-field LF boost
    nearFieldLFHzParam    = apvts.getRawParameterValue(ParamID::NEAR_FIELD_LF_HZ);
    nearFieldLFMaxDbParam = apvts.getRawParameterValue(ParamID::NEAR_FIELD_LF_MAX_DB);
    jassert(nearFieldLFHzParam    != nullptr);
    jassert(nearFieldLFMaxDbParam != nullptr);

    // Dev panel: Air absorption stage 2
    airAbs2MaxHzParam = apvts.getRawParameterValue(ParamID::AIR_ABS_2_MAX_HZ);
    airAbs2MinHzParam = apvts.getRawParameterValue(ParamID::AIR_ABS_2_MIN_HZ);
    jassert(airAbs2MaxHzParam != nullptr);
    jassert(airAbs2MinHzParam != nullptr);

    // Dev panel: Distance gain law
    distGainFloorDbParam = apvts.getRawParameterValue(ParamID::DIST_GAIN_FLOOR_DB);
    distGainMaxParam     = apvts.getRawParameterValue(ParamID::DIST_GAIN_MAX);
    jassert(distGainFloorDbParam != nullptr);
    jassert(distGainMaxParam     != nullptr);

    // Dev panel: Head shadow fully-open cap
    headShadowFullOpenHzParam = apvts.getRawParameterValue(ParamID::HEAD_SHADOW_FULL_OPEN_HZ);
    jassert(headShadowFullOpenHzParam != nullptr);

    // Input gain
    inputGainDbParam = apvts.getRawParameterValue(ParamID::INPUT_GAIN_DB);
    jassert(inputGainDbParam != nullptr);

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

    // Dev panel: Expanded pinna EQ (P5)
    conchaNotchFreqParam  = apvts.getRawParameterValue(ParamID::CONCHA_NOTCH_FREQ_HZ);
    conchaNotchQParam     = apvts.getRawParameterValue(ParamID::CONCHA_NOTCH_Q);
    conchaNotchMaxDbParam = apvts.getRawParameterValue(ParamID::CONCHA_NOTCH_MAX_DB);
    upperPinnaFreqParam   = apvts.getRawParameterValue(ParamID::UPPER_PINNA_FREQ_HZ);
    upperPinnaQParam      = apvts.getRawParameterValue(ParamID::UPPER_PINNA_Q);
    upperPinnaMinDbParam  = apvts.getRawParameterValue(ParamID::UPPER_PINNA_MIN_DB);
    upperPinnaMaxDbParam  = apvts.getRawParameterValue(ParamID::UPPER_PINNA_MAX_DB);
    shoulderPeakFreqParam = apvts.getRawParameterValue(ParamID::SHOULDER_PEAK_FREQ_HZ);
    shoulderPeakQParam    = apvts.getRawParameterValue(ParamID::SHOULDER_PEAK_Q);
    shoulderPeakMaxDbParam= apvts.getRawParameterValue(ParamID::SHOULDER_PEAK_MAX_DB);
    tragusNotchFreqParam  = apvts.getRawParameterValue(ParamID::TRAGUS_NOTCH_FREQ_HZ);
    tragusNotchQParam     = apvts.getRawParameterValue(ParamID::TRAGUS_NOTCH_Q);
    tragusNotchMaxDbParam = apvts.getRawParameterValue(ParamID::TRAGUS_NOTCH_MAX_DB);
    bypassExpandedPinnaParam = apvts.getRawParameterValue(ParamID::BYPASS_EXPANDED_PINNA);

    jassert(conchaNotchFreqParam  != nullptr);
    jassert(conchaNotchQParam     != nullptr);
    jassert(conchaNotchMaxDbParam != nullptr);
    jassert(upperPinnaFreqParam   != nullptr);
    jassert(upperPinnaQParam      != nullptr);
    jassert(upperPinnaMinDbParam  != nullptr);
    jassert(upperPinnaMaxDbParam  != nullptr);
    jassert(shoulderPeakFreqParam != nullptr);
    jassert(shoulderPeakQParam    != nullptr);
    jassert(shoulderPeakMaxDbParam!= nullptr);
    jassert(tragusNotchFreqParam  != nullptr);
    jassert(tragusNotchQParam     != nullptr);
    jassert(tragusNotchMaxDbParam != nullptr);
    jassert(bypassExpandedPinnaParam != nullptr);

    // Early Reflections (Image Source Method)
    erEnabledParam    = apvts.getRawParameterValue(ParamID::ER_ENABLED);
    erRoomSizeParam   = apvts.getRawParameterValue(ParamID::ER_ROOM_SIZE);
    erDampingParam    = apvts.getRawParameterValue(ParamID::ER_DAMPING);
    erLevelParam      = apvts.getRawParameterValue(ParamID::ER_LEVEL);
    erReverbSendParam = apvts.getRawParameterValue(ParamID::ER_REVERB_SEND);
    erGainDbParam     = apvts.getRawParameterValue(ParamID::ER_GAIN_DB);
    bypassERParam     = apvts.getRawParameterValue(ParamID::BYPASS_ER);

    jassert(erEnabledParam    != nullptr);
    jassert(erRoomSizeParam   != nullptr);
    jassert(erDampingParam    != nullptr);
    jassert(erLevelParam      != nullptr);
    jassert(erReverbSendParam != nullptr);
    jassert(erGainDbParam     != nullptr);
    jassert(bypassERParam     != nullptr);

    // Binaural toggle (user-facing)
    binauralEnabledParam = apvts.getRawParameterValue(ParamID::BINAURAL_ENABLED);
    jassert(binauralEnabledParam != nullptr);

    // Dev panel: Per-feature bypass toggles
    bypassITDParam        = apvts.getRawParameterValue(ParamID::BYPASS_ITD);
    bypassHeadShadowParam = apvts.getRawParameterValue(ParamID::BYPASS_HEAD_SHADOW);
    bypassILDParam        = apvts.getRawParameterValue(ParamID::BYPASS_ILD);
    bypassNearFieldParam  = apvts.getRawParameterValue(ParamID::BYPASS_NEAR_FIELD);
    bypassRearShadowParam = apvts.getRawParameterValue(ParamID::BYPASS_REAR_SHADOW);
    bypassPinnaEQParam    = apvts.getRawParameterValue(ParamID::BYPASS_PINNA_EQ);
    bypassCombParam       = apvts.getRawParameterValue(ParamID::BYPASS_COMB);
    bypassChestParam      = apvts.getRawParameterValue(ParamID::BYPASS_CHEST);
    bypassFloorParam      = apvts.getRawParameterValue(ParamID::BYPASS_FLOOR);
    bypassDistGainParam   = apvts.getRawParameterValue(ParamID::BYPASS_DIST_GAIN);
    bypassDopplerParam    = apvts.getRawParameterValue(ParamID::BYPASS_DOPPLER);
    bypassAirAbsParam     = apvts.getRawParameterValue(ParamID::BYPASS_AIR_ABS);
    bypassReverbParam     = apvts.getRawParameterValue(ParamID::BYPASS_REVERB);

    jassert(bypassITDParam        != nullptr);
    jassert(bypassHeadShadowParam != nullptr);
    jassert(bypassILDParam        != nullptr);
    jassert(bypassNearFieldParam  != nullptr);
    jassert(bypassRearShadowParam != nullptr);
    jassert(bypassPinnaEQParam    != nullptr);
    jassert(bypassCombParam       != nullptr);
    jassert(bypassChestParam      != nullptr);
    jassert(bypassFloorParam      != nullptr);
    jassert(bypassDistGainParam   != nullptr);
    jassert(bypassDopplerParam    != nullptr);
    jassert(bypassAirAbsParam     != nullptr);
    jassert(bypassReverbParam     != nullptr);

    // Start 30Hz timer for collecting foreign source positions
    startTimerHz(30);
}

XYZPanProcessor::~XYZPanProcessor() {
    // Stop the timer FIRST to prevent timerCallback from firing during teardown.
    // JUCE timers fire on the message thread; since we're already on the message
    // thread, this won't race — but it prevents any pending timer from firing
    // after we begin destruction.
    stopTimer();

    // Remove APVTS listeners BEFORE detaching from the hub. If we detach first,
    // the removal callbacks can trigger APVTS operations on surviving instances
    // that might broadcast orientation back to us while we still have listeners
    // registered — leading to parameterChanged calls on a half-destroyed object.
    apvts.removeParameterListener(ParamID::LISTENER_YAW,        this);
    apvts.removeParameterListener(ParamID::LISTENER_PITCH,      this);
    apvts.removeParameterListener(ParamID::LISTENER_ROLL,       this);
    apvts.removeParameterListener(ParamID::HEAD_FOLLOWS_CAMERA, this);
    apvts.removeParameterListener(ParamID::LISTENER_LINK,       this);
    apvts.removeParameterListener(ParamID::LISTENER_PILOT,     this);
    apvts.removeParameterListener(ParamID::WALKER_X,            this);
    apvts.removeParameterListener(ParamID::WALKER_Y,            this);
    apvts.removeParameterListener(ParamID::WALKER_Z,            this);

    // Now detach from the hub — mark dead, remove from linked list, fire
    // removal callbacks (which tell surviving editors to unbind from us).
    listenerHub_->detachInstance(this);
}

void XYZPanProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    jassert(sampleRate > 0.0 && sampleRate <= 192000.0);
    jassert(samplesPerBlock > 0 && samplesPerBlock <= 8192);
    DBG("XYZPan prepareToPlay: sr=" << sampleRate << " block=" << samplesPerBlock);
    engine.prepare(sampleRate, samplesPerBlock);

    // Phase 6: prepare R smoother — 20ms matches engine's internal position smoothing window
    rSmooth_.prepare(20.0f, static_cast<float>(sampleRate));
    rSmooth_.reset(rParam != nullptr ? rParam->load() : 1.0f);

    // Walker position smoothers — 5ms: just enough to avoid zipper noise
    walkerXSmooth_.prepare(5.0f, static_cast<float>(sampleRate));
    walkerYSmooth_.prepare(5.0f, static_cast<float>(sampleRate));
    walkerZSmooth_.prepare(5.0f, static_cast<float>(sampleRate));
    walkerXSmooth_.reset(walkerXParam != nullptr ? walkerXParam->load() : 0.0f);
    walkerYSmooth_.reset(walkerYParam != nullptr ? walkerYParam->load() : 0.0f);
    walkerZSmooth_.reset(walkerZParam != nullptr ? walkerZParam->load() : 0.0f);
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

    // Walker listener position — when linked, read from shared hub atomics
    // so all instances use the exact same values (single source of truth).
    const bool linked = listenerLinkParam->load() >= 0.5f;
    const float wx = linked ? listenerHub_->sharedWalkerX.load(std::memory_order_acquire)
                            : walkerXParam->load();
    const float wy = linked ? listenerHub_->sharedWalkerY.load(std::memory_order_acquire)
                            : walkerYParam->load();
    const float wz = linked ? listenerHub_->sharedWalkerZ.load(std::memory_order_acquire)
                            : walkerZParam->load();
    params.listenerX = walkerXSmooth_.process(wx) * r;
    params.listenerY = walkerYSmooth_.process(wy) * r;
    params.listenerZ = walkerZSmooth_.process(wz) * r;

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
    params.chestHPFHz       = chestHPFHzParam->load();
    params.chestLPHz        = chestLPHzParam->load();
    params.floorDelayMaxMs  = floorDelayMsParam->load();
    params.floorGainDb      = floorGainDbParam->load();

    // Dev panel: Distance processing (Phase 4)
    params.distDelayMaxMs = distDelayMaxMsParam->load();
    params.distSmoothMs   = distSmoothMsParam->load();
    params.dopplerEnabled = params.distDelayMaxMs > 0.0f;
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
    params.lfoXSmooth    = lfoXSmoothParam->load();
    params.lfoYSmooth    = lfoYSmoothParam->load();
    params.lfoZSmooth    = lfoZSmoothParam->load();
    params.lfoXTempoSync = lfoXTempoSyncParam->load() >= 0.5f;
    params.lfoYTempoSync = lfoYTempoSyncParam->load() >= 0.5f;
    params.lfoZTempoSync = lfoZTempoSyncParam->load() >= 0.5f;

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
    params.lfoSpeedMul   = lfoSpeedMulParam->load();

    // Momentary phase resets from UI buttons
    if (resetXYZLfoPhases.exchange(false)) {
        params.lfoXResetPhase = true;
        params.lfoYResetPhase = true;
        params.lfoZResetPhase = true;
    }

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
    params.stereoOrbitXYSmooth     = orbitXYSmoothParam->load();

    // Stereo orbit LFOs — XZ
    params.stereoOrbitXZWaveform   = static_cast<int>(std::round(orbitXZWaveformParam->load()));
    params.stereoOrbitXZRate       = orbitXZRateParam->load();
    params.stereoOrbitXZBeatDiv    = beatDivFromChoice(orbitXZBeatDivParam->load());
    params.stereoOrbitXZPhase      = orbitXZPhaseParam->load();
    params.stereoOrbitXZResetPhase = orbitXZResetPhaseParam->load() >= 0.5f;
    params.stereoOrbitXZDepth      = orbitXZDepthParam->load();
    params.stereoOrbitXZSmooth     = orbitXZSmoothParam->load();

    // Stereo orbit LFOs — YZ
    params.stereoOrbitYZWaveform   = static_cast<int>(std::round(orbitYZWaveformParam->load()));
    params.stereoOrbitYZRate       = orbitYZRateParam->load();
    params.stereoOrbitYZBeatDiv    = beatDivFromChoice(orbitYZBeatDivParam->load());
    params.stereoOrbitYZPhase      = orbitYZPhaseParam->load();
    params.stereoOrbitYZResetPhase = orbitYZResetPhaseParam->load() >= 0.5f;
    params.stereoOrbitYZDepth      = orbitYZDepthParam->load();
    params.stereoOrbitYZSmooth     = orbitYZSmoothParam->load();

    // Stereo orbit per-plane sync + shared speed
    params.stereoOrbitXYTempoSync = orbitXYTempoSyncParam->load() >= 0.5f;
    params.stereoOrbitXZTempoSync = orbitXZTempoSyncParam->load() >= 0.5f;
    params.stereoOrbitYZTempoSync = orbitYZTempoSyncParam->load() >= 0.5f;
    params.stereoOrbitSpeedMul    = orbitSpeedMulParam->load();

    // Momentary orbit phase reset from UI button
    if (resetOrbitLfoPhases.exchange(false)) {
        params.stereoOrbitXYResetPhase = true;
        params.stereoOrbitXZResetPhase = true;
        params.stereoOrbitYZResetPhase = true;
    }

    // Dev panel: Presence shelf
    params.presenceShelfFreqHz = presenceShelfFreqParam->load();
    params.presenceShelfMaxDb  = presenceShelfMaxDbParam->load();

    // Dev panel: Ear canal resonance
    params.earCanalFreqHz = earCanalFreqParam->load();
    params.earCanalQ      = earCanalQParam->load();
    params.earCanalMaxDb  = earCanalMaxDbParam->load();

    // Dev panel: Aux send
    params.auxSendGainMaxDb = auxSendGainMaxDbParam->load();

    // Dev panel: Pinna P1 fixed peak
    params.pinnaP1FreqHz  = pinnaP1FreqHzParam->load();
    params.pinnaP1GainDb  = pinnaP1GainDbParam->load();
    params.pinnaP1Q       = pinnaP1QParam->load();

    // Dev panel: Pinna N2 secondary notch
    params.pinnaN2OffsetHz = pinnaN2OffsetHzParam->load();
    params.pinnaN2GainDb   = pinnaN2GainDbParam->load();
    params.pinnaN2Q        = pinnaN2QParam->load();

    // Dev panel: Pinna N1 range limits
    params.pinnaN1MinHz = pinnaN1MinHzParam->load();
    params.pinnaN1MaxHz = pinnaN1MaxHzParam->load();

    // Dev panel: Floor bounce HF absorption
    params.floorAbsHz = floorAbsHzParam->load();

    // Dev panel: Near-field LF boost
    params.nearFieldLFHz    = nearFieldLFHzParam->load();
    params.nearFieldLFMaxDb = nearFieldLFMaxDbParam->load();

    // Dev panel: Air absorption stage 2
    params.airAbs2MaxHz = airAbs2MaxHzParam->load();
    params.airAbs2MinHz = airAbs2MinHzParam->load();

    // Dev panel: Distance gain law
    params.distGainFloorDb = distGainFloorDbParam->load();
    params.distGainMax     = distGainMaxParam->load();

    // Dev panel: Head shadow fully-open cap
    params.headShadowFullOpenHz = headShadowFullOpenHzParam->load();

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

    // Dev panel: Expanded pinna EQ (P5)
    params.conchaNotchFreqHz  = conchaNotchFreqParam->load();
    params.conchaNotchQ       = conchaNotchQParam->load();
    params.conchaNotchMaxDb   = conchaNotchMaxDbParam->load();
    params.upperPinnaFreqHz   = upperPinnaFreqParam->load();
    params.upperPinnaQ        = upperPinnaQParam->load();
    params.upperPinnaMinDb    = upperPinnaMinDbParam->load();
    params.upperPinnaMaxDb    = upperPinnaMaxDbParam->load();
    params.shoulderPeakFreqHz = shoulderPeakFreqParam->load();
    params.shoulderPeakQ      = shoulderPeakQParam->load();
    params.shoulderPeakMaxDb  = shoulderPeakMaxDbParam->load();
    params.tragusNotchFreqHz  = tragusNotchFreqParam->load();
    params.tragusNotchQ       = tragusNotchQParam->load();
    params.tragusNotchMaxDb   = tragusNotchMaxDbParam->load();

    // Early Reflections (Image Source Method)
    params.erEnabled     = erEnabledParam->load() >= 0.5f;
    params.erRoomSize    = erRoomSizeParam->load();
    params.erDamping     = erDampingParam->load();
    params.erLevel       = erLevelParam->load() * std::pow(10.0f, erGainDbParam->load() / 20.0f);
    params.erReverbSend  = erReverbSendParam->load();
    params.bypassER      = bypassERParam->load() >= 0.5f;

    // Dev panel: Per-feature bypass toggles
    params.bypassExpandedPinna = bypassExpandedPinnaParam->load() >= 0.5f;
    params.bypassITD        = bypassITDParam->load()        >= 0.5f;
    params.bypassHeadShadow = bypassHeadShadowParam->load() >= 0.5f;
    params.bypassILD        = bypassILDParam->load()        >= 0.5f;
    params.bypassNearField  = bypassNearFieldParam->load()  >= 0.5f;
    params.bypassRearShadow = bypassRearShadowParam->load() >= 0.5f;
    params.bypassPinnaEQ    = bypassPinnaEQParam->load()    >= 0.5f;
    params.bypassComb       = bypassCombParam->load()       >= 0.5f;
    params.bypassChest      = bypassChestParam->load()      >= 0.5f;
    params.bypassFloor      = bypassFloorParam->load()      >= 0.5f;
    params.bypassDistGain   = bypassDistGainParam->load()   >= 0.5f;
    params.bypassDoppler    = bypassDopplerParam->load()    >= 0.5f;
    params.bypassAirAbs     = bypassAirAbsParam->load()     >= 0.5f;
    params.bypassReverb     = bypassReverbParam->load()     >= 0.5f;

    // Listener head orientation (degrees in APVTS → radians for engine)
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
    params.listenerYaw   = (linked ? listenerHub_->sharedYaw.load(std::memory_order_acquire)
                                   : listenerYawParam->load()) * kDegToRad;
    params.listenerPitch = (linked ? listenerHub_->sharedPitch.load(std::memory_order_acquire)
                                   : listenerPitchParam->load()) * kDegToRad;
    params.listenerRoll  = (linked ? listenerHub_->sharedRoll.load(std::memory_order_acquire)
                                   : listenerRollParam->load()) * kDegToRad;

    // Binaural toggle (user-facing)
    params.binauralEnabled = binauralEnabledParam->load() >= 0.5f;

    // Phase 5: Read host BPM for LFO tempo sync (LFO-05)
    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            if (pos->getBpm().hasValue())
                params.hostBpm = static_cast<float>(*pos->getBpm());
            // else: hostBpm stays at its EngineParams default (120.0f)
        }
    }

    // Apply input gain (0–12 dB) to input channels before engine processing
    {
        const float gainDb = inputGainDbParam->load();
        if (gainDb > 0.001f) {
            const float gainLin = std::pow(10.0f, gainDb / 20.0f);
            const int numInCh = getTotalNumInputChannels();
            for (int ch = 0; ch < numInCh; ++ch)
                buffer.applyGain(ch, 0, buffer.getNumSamples(), gainLin);
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

    // Input RMS for GL sound wave visualization (computed before engine.process
    // in case input/output buffers alias on mono-in/stereo-out layouts)
    float inputRmsValue = 0.0f;
    {
        const int n = buffer.getNumSamples();
        float sum = 0.0f;
        for (int ch = 0; ch < numIn; ++ch) {
            const float* in = inputs[ch];
            if (!in) continue;
            for (int i = 0; i < n; ++i)
                sum += in[i] * in[i];
        }
        if (numIn > 0 && n > 0)
            inputRmsValue = std::sqrt(sum / static_cast<float>(n * numIn));
    }

    // Output channel pointers (buffer has stereo output per bus layout)
    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getWritePointer(1);

    engine.setParams(params);
    engine.process(inputs, numIn, outL, outR, nullptr, nullptr, buffer.getNumSamples());

    // Output RMS for UI meter
    {
        const int n = buffer.getNumSamples();
        float sumL = 0.f, sumR = 0.f;
        for (int i = 0; i < n; ++i) {
            sumL += outL[i] * outL[i];
            sumR += outR[i] * outR[i];
        }
        outputRmsL.store(std::sqrt(sumL / static_cast<float>(n)), std::memory_order_relaxed);
        outputRmsR.store(std::sqrt(sumR / static_cast<float>(n)), std::memory_order_relaxed);
    }

    // Phase 6: update PositionBridge with last modulated position (UI-07)
    // Written here on audio thread; XYZPanGLView reads it on GL thread via bridge_.read()
    auto mp = engine.getLastModulatedPosition();
    xyzpan::SourcePositionSnapshot snap;
    snap.x = mp.x;
    snap.y = mp.y;
    snap.z = mp.z;
    {
        const float dx = mp.x - params.listenerX;
        const float dy = mp.y - params.listenerY;
        const float dz = mp.z - params.listenerZ;
        snap.distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    // Stereo node positions for GL rendering
    auto sn = engine.getLastStereoNodes();
    snap.lNodeX = sn.lx;  snap.lNodeY = sn.ly;  snap.lNodeZ = sn.lz;
    snap.rNodeX = sn.rx;  snap.rNodeY = sn.ry;  snap.rNodeZ = sn.rz;
    snap.stereoWidth = sn.width;

    snap.listenerYaw   = params.listenerYaw;
    snap.listenerPitch = params.listenerPitch;
    snap.listenerRoll  = params.listenerRoll;

    snap.listenerPosX = params.listenerX;
    snap.listenerPosY = params.listenerY;
    snap.listenerPosZ = params.listenerZ;

    snap.sphereRadius = sphereRadiusParam->load();
    // Estimate dry (pre-distance) RMS: use input RMS for external audio,
    // and output RMS / distance gain for internal test tone.
    // This ensures waves reflect source loudness regardless of distance.
    {
        auto dspState = engine.getLastDSPState();
        const float distGain = std::max(dspState.distGainLinear, 0.001f);
        const float outputRms = std::max(outputRmsL.load(std::memory_order_relaxed),
                                         outputRmsR.load(std::memory_order_relaxed));
        const float dryFromOutput = outputRms / distGain;
        snap.inputRms = std::max(inputRmsValue, dryFromOutput);
    }

    positionBridge.write(snap);

    // Export source position for linked-instance visualization
    sourceExport.write(snap.x, snap.y, snap.z, snap.distance,
                       snap.stereoWidth,
                       snap.lNodeX, snap.lNodeY, snap.lNodeZ,
                       snap.rNodeX, snap.rNodeY, snap.rNodeZ,
                       snap.sphereRadius, snap.inputRms);

    // DSP state bridge for dev panel readouts
    dspStateBridge.write(engine.getLastDSPState());

    // LFO output values for UI waveform displays
    auto lfoOut = engine.getLastLFOOutputs();
    lfoOutputX.store(lfoOut.x, std::memory_order_relaxed);
    lfoOutputY.store(lfoOut.y, std::memory_order_relaxed);
    lfoOutputZ.store(lfoOut.z, std::memory_order_relaxed);
    lfoOutputOrbitXY.store(lfoOut.orbitXY, std::memory_order_relaxed);
    lfoOutputOrbitXZ.store(lfoOut.orbitXZ, std::memory_order_relaxed);
    lfoOutputOrbitYZ.store(lfoOut.orbitYZ, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* XYZPanProcessor::createEditor() {
    return new XYZPanEditor(*this);
}

// ---------------------------------------------------------------------------
// 30Hz timer — collect foreign source positions for GL view
// ---------------------------------------------------------------------------
void XYZPanProcessor::timerCallback() {
    if (listenerLinkParam->load() < 0.5f) {
        xyzpan::ForeignSourceBridge::Payload empty;
        foreignSourceBridge.write(empty);
        return;
    }
    xyzpan::ForeignSourceBridge::Payload payload;
    payload.count = listenerHub_->getLinkedSources(
        this, payload.sources, xyzpan::kMaxLinkedSources);
    foreignSourceBridge.write(payload);
}

// ---------------------------------------------------------------------------
// Linked listener orientation — APVTS parameter listener
// ---------------------------------------------------------------------------
void XYZPanProcessor::parameterChanged(const juce::String& parameterID, float newValue) {
    if (restoringState_)
        return;

    if (parameterID == ParamID::LISTENER_LINK) {
        if (newValue >= 0.5f) {
            listenerHub_->addLinkedInstance(this);
            // Adopt group orientation if others are already linked
            float yaw, pitch, roll;
            bool headFollows;
            if (listenerHub_->getCachedOrientation(yaw, pitch, roll, headFollows)) {
                receivingBroadcast_->store(true, std::memory_order_relaxed);
                if (auto* p = apvts.getParameter(ParamID::LISTENER_YAW))
                    p->setValue(p->convertTo0to1(yaw));
                if (auto* p = apvts.getParameter(ParamID::LISTENER_PITCH))
                    p->setValue(p->convertTo0to1(pitch));
                if (auto* p = apvts.getParameter(ParamID::LISTENER_ROLL))
                    p->setValue(p->convertTo0to1(roll));
                if (auto* p = apvts.getParameter(ParamID::HEAD_FOLLOWS_CAMERA))
                    p->setValue(headFollows ? 1.0f : 0.0f);
                receivingBroadcast_->store(false, std::memory_order_relaxed);
            } else {
                // First instance to link — seed shared atomics + auto-claim pilot
                listenerHub_->sharedYaw.store(listenerYawParam->load(), std::memory_order_release);
                listenerHub_->sharedPitch.store(listenerPitchParam->load(), std::memory_order_release);
                listenerHub_->sharedRoll.store(listenerRollParam->load(), std::memory_order_release);
                listenerHub_->claimPilot(this);
            }
            // Adopt group walker position
            float wx, wy, wz;
            if (listenerHub_->getCachedPosition(wx, wy, wz)) {
                receivingBroadcast_->store(true, std::memory_order_relaxed);
                if (auto* p = apvts.getParameter(ParamID::WALKER_X))
                    p->setValue(p->convertTo0to1(wx));
                if (auto* p = apvts.getParameter(ParamID::WALKER_Y))
                    p->setValue(p->convertTo0to1(wy));
                if (auto* p = apvts.getParameter(ParamID::WALKER_Z))
                    p->setValue(p->convertTo0to1(wz));
                receivingBroadcast_->store(false, std::memory_order_relaxed);
            } else {
                listenerHub_->sharedWalkerX.store(walkerXParam->load(), std::memory_order_release);
                listenerHub_->sharedWalkerY.store(walkerYParam->load(), std::memory_order_release);
                listenerHub_->sharedWalkerZ.store(walkerZParam->load(), std::memory_order_release);
            }
            // If restoring from saved state with pilot enabled, reclaim
            if (listenerPilotParam->load() >= 0.5f)
                listenerHub_->claimPilot(this);
        } else {
            listenerHub_->releasePilot(this);
            if (auto* p = apvts.getParameter(ParamID::LISTENER_PILOT))
                p->setValue(0.0f);
            listenerHub_->removeLinkedInstance(this);
        }
        return;
    }

    // Pilot toggle — claim or release pilot role
    if (parameterID == ParamID::LISTENER_PILOT) {
        if (newValue >= 0.5f) {
            if (listenerLinkParam->load() >= 0.5f)
                listenerHub_->claimPilot(this);
        } else {
            listenerHub_->releasePilot(this);
        }
        return;
    }

    // For orientation/walker params: broadcast if linked, pilot, and not receiving
    if (receivingBroadcast_->load(std::memory_order_relaxed))
        return;
    if (listenerLinkParam->load() < 0.5f)
        return;
    if (!listenerHub_->isPilot(this))
        return;

    // Broadcast orientation for yaw/pitch/roll/headFollows changes
    if (parameterID == ParamID::LISTENER_YAW || parameterID == ParamID::LISTENER_PITCH ||
        parameterID == ParamID::LISTENER_ROLL || parameterID == ParamID::HEAD_FOLLOWS_CAMERA) {
        listenerHub_->broadcastOrientation(
            this,
            listenerYawParam->load(),
            listenerPitchParam->load(),
            listenerRollParam->load(),
            headFollowsCameraParam->load() >= 0.5f);
    }

    // Broadcast walker position for walker_x/y/z changes
    if (parameterID == ParamID::WALKER_X || parameterID == ParamID::WALKER_Y ||
        parameterID == ParamID::WALKER_Z) {
        listenerHub_->broadcastPosition(
            this,
            walkerXParam->load(),
            walkerYParam->load(),
            walkerZParam->load());
    }
}

void XYZPanProcessor::listenerOrientationChanged(float yaw, float pitch, float roll,
                                                   bool headFollows) {
    // Use setValue (not setValueNotifyingHost) — receivers should NOT generate
    // host automation callbacks. setValue updates the internal atomic and fires
    // APVTS parameterChanged listeners (for GL head-follows sync) but skips the
    // host notification that would flood Ableton with N² automation writes.
    receivingBroadcast_->store(true, std::memory_order_relaxed);
    if (auto* p = apvts.getParameter(ParamID::LISTENER_YAW))
        p->setValue(p->convertTo0to1(yaw));
    if (auto* p = apvts.getParameter(ParamID::LISTENER_PITCH))
        p->setValue(p->convertTo0to1(pitch));
    if (auto* p = apvts.getParameter(ParamID::LISTENER_ROLL))
        p->setValue(p->convertTo0to1(roll));
    if (auto* p = apvts.getParameter(ParamID::HEAD_FOLLOWS_CAMERA))
        p->setValue(headFollows ? 1.0f : 0.0f);
    receivingBroadcast_->store(false, std::memory_order_relaxed);
}

void XYZPanProcessor::listenerPositionChanged(float x, float y, float z) {
    receivingBroadcast_->store(true, std::memory_order_relaxed);
    if (auto* p = apvts.getParameter(ParamID::WALKER_X))
        p->setValue(p->convertTo0to1(x));
    if (auto* p = apvts.getParameter(ParamID::WALKER_Y))
        p->setValue(p->convertTo0to1(y));
    if (auto* p = apvts.getParameter(ParamID::WALKER_Z))
        p->setValue(p->convertTo0to1(z));
    receivingBroadcast_->store(false, std::memory_order_relaxed);
}

void XYZPanProcessor::pilotStatusChanged(bool isPilot) {
    if (auto* p = apvts.getParameter(ParamID::LISTENER_PILOT))
        p->setValue(isPilot ? 1.0f : 0.0f);
}

bool XYZPanProcessor::isLinkedPilot() const {
    return listenerLinkParam->load() >= 0.5f && listenerHub_->isPilot(this);
}

bool XYZPanProcessor::isLinkedNonPilot() const {
    return listenerLinkParam->load() >= 0.5f && !listenerHub_->isPilot(this);
}

juce::String XYZPanProcessor::getPilotName() const {
    return listenerHub_->getPilotName();
}

// User-facing params saved in DAW state / user presets.
// Dev-panel tuning params are intentionally excluded — they keep
// defaults on load so hand-tuned values aren't overwritten.

void XYZPanProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    // Snap boolean parameter values — AudioParameterBool accepts any float via
    // setValue() but only stores true/false internally.  The APVTS ValueTree may
    // retain the raw float, causing state-restore round-trip mismatches.
    for (int i = 0; i < xml->getNumChildElements(); ++i) {
        auto* child = xml->getChildElement(i);
        if (child->hasTagName("PARAM")) {
            auto id = child->getStringAttribute("id");
            if (auto* p = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(id))) {
                child->setAttribute("value", p->get() ? 1.0 : 0.0);
            }
        }
    }

    // Persist instance name
    if (instanceName_.isNotEmpty()) {
        xml->setAttribute("instanceName", instanceName_);
        xml->setAttribute("nameManuallySet", nameManuallySet_ ? 1 : 0);
    }

    xml->setAttribute("stateVersion", 2);
    xml->setAttribute("currentProgram", currentPresetIndex_);
    copyXmlToBinary(*xml, destData);
}

void XYZPanProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType())) {
        restoringState_ = true;

        // Restore instance name and program index before replacing state
        instanceName_     = xml->getStringAttribute("instanceName", "");
        nameManuallySet_  = xml->getIntAttribute("nameManuallySet", 0) != 0;
        currentPresetIndex_ = xml->getIntAttribute("currentProgram", 0);

        // Migrate v1 presets: yaw/pitch/roll were 0–360, now -180–180.
        // APVTS stores normalized 0–1 values. Old: norm = deg/360.
        // Convert: oldDeg = norm * 360; if > 180 then oldDeg -= 360;
        // newNorm = (oldDeg + 180) / 360.
        if (xml->getIntAttribute("stateVersion", 1) < 2) {
            for (int i = 0; i < xml->getNumChildElements(); ++i) {
                auto* child = xml->getChildElement(i);
                if (child->hasTagName("PARAM")) {
                    auto id = child->getStringAttribute("id");
                    if (id == "listener_yaw" || id == "listener_pitch" || id == "listener_roll") {
                        float norm = static_cast<float>(child->getDoubleAttribute("value", 0.0));
                        float oldDeg = norm * 360.0f;           // old range 0–360
                        if (oldDeg > 180.0f) oldDeg -= 360.0f;  // now -180–180
                        float newNorm = (oldDeg + 180.0f) / 360.0f;
                        child->setAttribute("value", static_cast<double>(newNorm));
                    }
                }
            }
        }

        apvts.replaceState(juce::ValueTree::fromXml(*xml));

        // Force-snap bool params after state replacement — JUCE's ValueTree may
        // retain raw floats that AudioParameterBool doesn't snap on replaceState.
        for (auto* p : getParameters()) {
            if (auto* bp = dynamic_cast<juce::AudioParameterBool*>(p))
                bp->setValueNotifyingHost(bp->get() ? 1.0f : 0.0f);
        }

        restoringState_ = false;

        // Re-apply listener link state now that all params are fully restored.
        // During replaceState the parameterChanged callback was suppressed to
        // prevent the hub from overwriting just-restored orientation values.
        if (listenerLinkParam->load() >= 0.5f) {
            listenerHub_->addLinkedInstance(this);
            if (listenerPilotParam->load() >= 0.5f)
                listenerHub_->claimPilot(this);
        }
    }
}

void XYZPanProcessor::updateTrackProperties(const TrackProperties& properties) {
    if (properties.name.has_value()) {
        trackName_ = *properties.name;
        if (!nameManuallySet_)
            instanceName_ = trackName_;
    }
}

void XYZPanProcessor::setInstanceName(const juce::String& name) {
    if (name.isEmpty()) {
        // Clearing custom name — revert to DAW track name
        nameManuallySet_ = false;
        instanceName_ = trackName_;
    } else {
        instanceName_ = name;
        nameManuallySet_ = true;
    }
}

// ---------------------------------------------------------------------------
// Factory preset program methods (PARAM-05)
// setCurrentProgram() is called from the message thread — NEVER from processBlock.
// replaceState() uses internal APVTS locks and is NOT realtime-safe; this is
// correct because processBlock reads only pre-cached std::atomic<float>* pointers,
// never the ValueTree itself (INFRA-04).
// ---------------------------------------------------------------------------

int XYZPanProcessor::getNumPrograms() {
    return XYZPresets::kNumPresets;
}

int XYZPanProcessor::getCurrentProgram() {
    return currentPresetIndex_;
}

const juce::String XYZPanProcessor::getProgramName(int index) {
    if (index >= 0 && index < XYZPresets::kNumPresets)
        return XYZPresets::kFactoryPresets[index].name;
    return "Unknown";
}

void XYZPanProcessor::setCurrentProgram(int index) {
    if (index < 0 || index >= XYZPresets::kNumPresets)
        return;

    currentPresetIndex_ = index;

    // Parse XML and replace APVTS state.
    // replaceState() is thread-safe (uses internal locks) but NOT realtime-safe.
    // setCurrentProgram is called from the message thread by all major DAWs.
    auto xml = juce::parseXML(XYZPresets::kFactoryPresets[index].xml);
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

// Plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new XYZPanProcessor();
}
