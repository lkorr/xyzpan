#pragma once

// APVTS parameter ID strings for XYZPan.
// Use these constants everywhere parameter IDs are referenced to avoid
// stringly-typed bugs between registration and lookup.
namespace ParamID {
    // Spatial position (Phase 1)
    constexpr const char* X = "x";
    constexpr const char* Y = "y";
    constexpr const char* Z = "z";

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
} // namespace ParamID
