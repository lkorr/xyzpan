#pragma once

// APVTS parameter ID strings for XYZPan.
// Use these constants everywhere parameter IDs are referenced to avoid
// stringly-typed bugs between registration and lookup.
namespace ParamID {
    // Spatial position (Phase 1)
    constexpr const char* X = "x";
    constexpr const char* Y = "y";
    constexpr const char* Z = "z";

    // Phase 6: R scale/radius — multiplies XYZ before engine (PARAM-01)
    constexpr const char* R = "r";

    // Dev panel: binaural panning tuning (Phase 2)
    constexpr const char* ITD_MAX_MS       = "itd_max_ms";
    constexpr const char* HEAD_SHADOW_HZ   = "head_shadow_hz";
    constexpr const char* ILD_MAX_DB       = "ild_max_db";
    constexpr const char* REAR_SHADOW_HZ   = "rear_shadow_hz";
    constexpr const char* SMOOTH_ITD_MS    = "smooth_itd_ms";
    constexpr const char* SMOOTH_FILTER_MS = "smooth_filter_ms";
    constexpr const char* SMOOTH_GAIN_MS   = "smooth_gain_ms";

    // Dev panel: Comb filter bank (Phase 3) — per-filter parameters
    // Access via ParamID::COMB_DELAY[i] and ParamID::COMB_FB[i]
    constexpr const char* COMB_DELAY[10] = {
        "comb_delay_0", "comb_delay_1", "comb_delay_2", "comb_delay_3", "comb_delay_4",
        "comb_delay_5", "comb_delay_6", "comb_delay_7", "comb_delay_8", "comb_delay_9"
    };
    constexpr const char* COMB_FB[10] = {
        "comb_fb_0", "comb_fb_1", "comb_fb_2", "comb_fb_3", "comb_fb_4",
        "comb_fb_5", "comb_fb_6", "comb_fb_7", "comb_fb_8", "comb_fb_9"
    };
    constexpr const char* COMB_WET_MAX   = "comb_wet_max";

    // Dev panel: Elevation filter tuning (Phase 3)
    constexpr const char* PINNA_NOTCH_HZ = "pinna_notch_hz";
    constexpr const char* PINNA_NOTCH_Q  = "pinna_notch_q";
    constexpr const char* PINNA_SHELF_HZ = "pinna_shelf_hz";
    constexpr const char* CHEST_DELAY_MS = "chest_delay_ms";
    constexpr const char* CHEST_GAIN_DB  = "chest_gain_db";
    constexpr const char* FLOOR_DELAY_MS = "floor_delay_ms";
    constexpr const char* FLOOR_GAIN_DB  = "floor_gain_db";

    // Dev panel: Distance processing tuning (Phase 4)
    constexpr const char* DIST_DELAY_MAX_MS = "dist_delay_max_ms";
    constexpr const char* DIST_SMOOTH_MS    = "dist_smooth_ms";
    constexpr const char* DOPPLER_ENABLED   = "doppler_enabled";
    constexpr const char* AIR_ABS_MAX_HZ    = "air_abs_max_hz";
    constexpr const char* AIR_ABS_MIN_HZ    = "air_abs_min_hz";

    // Phase 5: Reverb (VERB-03)
    constexpr const char* VERB_SIZE        = "verb_size";
    constexpr const char* VERB_DECAY       = "verb_decay";
    constexpr const char* VERB_DAMPING     = "verb_damping";
    constexpr const char* VERB_WET         = "verb_wet";
    constexpr const char* VERB_PRE_DELAY   = "verb_pre_delay";

    // Phase 5: LFO — per axis (LFO-01 through LFO-05)
    constexpr const char* LFO_X_RATE      = "lfo_x_rate";
    constexpr const char* LFO_X_DEPTH     = "lfo_x_depth";
    constexpr const char* LFO_X_PHASE     = "lfo_x_phase";
    constexpr const char* LFO_X_WAVEFORM  = "lfo_x_waveform";   // float 0-3, step 1
    constexpr const char* LFO_Y_RATE      = "lfo_y_rate";
    constexpr const char* LFO_Y_DEPTH     = "lfo_y_depth";
    constexpr const char* LFO_Y_PHASE     = "lfo_y_phase";
    constexpr const char* LFO_Y_WAVEFORM  = "lfo_y_waveform";
    constexpr const char* LFO_Z_RATE      = "lfo_z_rate";
    constexpr const char* LFO_Z_DEPTH     = "lfo_z_depth";
    constexpr const char* LFO_Z_PHASE     = "lfo_z_phase";
    constexpr const char* LFO_Z_WAVEFORM  = "lfo_z_waveform";
    // Tempo sync (shared)
    constexpr const char* LFO_TEMPO_SYNC  = "lfo_tempo_sync";   // AudioParameterBool
    // Beat division per axis (float multiplier: 0.25=1/4, 0.5=1/2, 1.0=quarter, 2.0=half, 4.0=whole)
    constexpr const char* LFO_X_BEAT_DIV  = "lfo_x_beat_div";
    constexpr const char* LFO_Y_BEAT_DIV  = "lfo_y_beat_div";
    constexpr const char* LFO_Z_BEAT_DIV  = "lfo_z_beat_div";

    // Stereo source node splitting
    constexpr const char* STEREO_WIDTH         = "stereo_width";
    constexpr const char* STEREO_FACE_LISTENER = "stereo_face_listener";
    constexpr const char* STEREO_ORBIT_PHASE   = "stereo_orbit_phase";
    constexpr const char* STEREO_ORBIT_OFFSET  = "stereo_orbit_offset";

    // Stereo orbit LFOs — XY plane
    constexpr const char* STEREO_ORBIT_XY_WAVEFORM    = "stereo_orbit_xy_waveform";
    constexpr const char* STEREO_ORBIT_XY_RATE         = "stereo_orbit_xy_rate";
    constexpr const char* STEREO_ORBIT_XY_BEAT_DIV     = "stereo_orbit_xy_beat_div";
    constexpr const char* STEREO_ORBIT_XY_PHASE        = "stereo_orbit_xy_phase";
    constexpr const char* STEREO_ORBIT_XY_RESET_PHASE  = "stereo_orbit_xy_reset_phase";
    constexpr const char* STEREO_ORBIT_XY_DEPTH        = "stereo_orbit_xy_depth";

    // Stereo orbit LFOs — XZ plane
    constexpr const char* STEREO_ORBIT_XZ_WAVEFORM    = "stereo_orbit_xz_waveform";
    constexpr const char* STEREO_ORBIT_XZ_RATE         = "stereo_orbit_xz_rate";
    constexpr const char* STEREO_ORBIT_XZ_BEAT_DIV     = "stereo_orbit_xz_beat_div";
    constexpr const char* STEREO_ORBIT_XZ_PHASE        = "stereo_orbit_xz_phase";
    constexpr const char* STEREO_ORBIT_XZ_RESET_PHASE  = "stereo_orbit_xz_reset_phase";
    constexpr const char* STEREO_ORBIT_XZ_DEPTH        = "stereo_orbit_xz_depth";

    // Stereo orbit LFOs — YZ plane
    constexpr const char* STEREO_ORBIT_YZ_WAVEFORM    = "stereo_orbit_yz_waveform";
    constexpr const char* STEREO_ORBIT_YZ_RATE         = "stereo_orbit_yz_rate";
    constexpr const char* STEREO_ORBIT_YZ_BEAT_DIV     = "stereo_orbit_yz_beat_div";
    constexpr const char* STEREO_ORBIT_YZ_PHASE        = "stereo_orbit_yz_phase";
    constexpr const char* STEREO_ORBIT_YZ_RESET_PHASE  = "stereo_orbit_yz_reset_phase";
    constexpr const char* STEREO_ORBIT_YZ_DEPTH        = "stereo_orbit_yz_depth";

    // Stereo orbit shared
    constexpr const char* STEREO_ORBIT_TEMPO_SYNC = "stereo_orbit_tempo_sync";
    constexpr const char* STEREO_ORBIT_SPEED_MUL  = "stereo_orbit_speed_mul";

    // Dev panel: Presence shelf
    constexpr const char* PRESENCE_SHELF_FREQ_HZ = "presence_shelf_freq_hz";
    constexpr const char* PRESENCE_SHELF_MAX_DB  = "presence_shelf_max_db";

    // Dev panel: Ear canal resonance
    constexpr const char* EAR_CANAL_FREQ_HZ = "ear_canal_freq_hz";
    constexpr const char* EAR_CANAL_Q       = "ear_canal_q";
    constexpr const char* EAR_CANAL_MAX_DB  = "ear_canal_max_db";

    // Dev panel: Aux reverb send
    constexpr const char* AUX_SEND_GAIN_MAX_DB = "aux_send_gain_max_db";

    // Dev panel: Geometry
    constexpr const char* SPHERE_RADIUS             = "sphere_radius";
    constexpr const char* VERT_MONO_CYLINDER_RADIUS = "vert_mono_cyl_radius";

    // Dev panel: Test tone oscillator
    constexpr const char* TEST_TONE_ENABLED  = "test_tone_enabled";
    constexpr const char* TEST_TONE_GAIN_DB  = "test_tone_gain_db";
    constexpr const char* TEST_TONE_PITCH_HZ = "test_tone_pitch_hz";
    constexpr const char* TEST_TONE_PULSE_HZ = "test_tone_pulse_hz";
    constexpr const char* TEST_TONE_WAVEFORM = "test_tone_waveform";
} // namespace ParamID
