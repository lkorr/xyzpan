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
    constexpr const char* ITD_MAX_MS      = "itd_max_ms";
    constexpr const char* HEAD_SHADOW_HZ  = "head_shadow_hz";
    constexpr const char* ILD_MAX_DB      = "ild_max_db";
    constexpr const char* REAR_SHADOW_HZ  = "rear_shadow_hz";
    constexpr const char* SMOOTH_ITD_MS   = "smooth_itd_ms";
    constexpr const char* SMOOTH_FILTER_MS = "smooth_filter_ms";
    constexpr const char* SMOOTH_GAIN_MS  = "smooth_gain_ms";
} // namespace ParamID
