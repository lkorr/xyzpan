#pragma once

// APVTS parameter ID strings for XYZPan.
// Use these constants everywhere parameter IDs are referenced to avoid
// stringly-typed bugs between registration and lookup.
namespace ParamID {
    constexpr const char* X = "x";
    constexpr const char* Y = "y";
    constexpr const char* Z = "z";
} // namespace ParamID
