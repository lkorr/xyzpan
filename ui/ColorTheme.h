#pragma once
#include <cstdint>
#include <glm/glm.hpp>

namespace xyzpan {

// Convert a 0xFFRRGGBB uint32_t to a normalized glm::vec3 {r,g,b}.
inline glm::vec3 toVec3(uint32_t argb) {
    return {
        static_cast<float>((argb >> 16) & 0xFF) / 255.0f,
        static_cast<float>((argb >>  8) & 0xFF) / 255.0f,
        static_cast<float>( argb        & 0xFF) / 255.0f
    };
}

// Runtime color theme — replaces the static constexpr palette in AlchemyLookAndFeel.
// JUCE panel colors (uint32_t, 0xAARRGGBB format) are fed to setColour()/Colour().
// GL colors (glm::vec3) are fed directly to shader uniforms.
struct ColorTheme {
    // ===== JUCE panel colors (used by AlchemyLookAndFeel draw methods) =====
    uint32_t background       = 0xFF0F0D0Au; // kBackground / kObsidian
    uint32_t darkIron          = 0xFF1E1A14u; // kDarkIron / kDarkParchment
    uint32_t bronze            = 0xFF554A37u; // kBronze / kVerdigris
    uint32_t warmGold          = 0xFFC9A84Cu; // kWarmGold / kGoldLeaf
    uint32_t brightGold        = 0xFFD9BE6Eu; // kBrightGold / kGoldLeafLight
    uint32_t parchment         = 0xFFC8B88Au; // kParchment / kAgedPapyrus
    uint32_t goldLeafPale      = 0xFFE8D49Au; // kGoldLeafPale — hero label text
    uint32_t obsidian          = 0xFF0F0D0Au; // kObsidian — shadows
    uint32_t obsidianLight     = 0xFF1A1611u; // kObsidianLight — knob body gradient
    uint32_t darkParchmentMid  = 0xFF362E21u; // kDarkParchmentMid — knob body gradient
    uint32_t darkParchment     = 0xFF1E1A14u; // kDarkParchment — knob body gradient
    uint32_t agedPapyrus       = 0xFFC8B88Au; // kAgedPapyrus — specular/indicator
    uint32_t agedPapyrusDark   = 0xFFA89A70u; // kAgedPapyrusDark — sheen
    uint32_t hoverGold         = 0xFFE8D49Au; // kHoverGold

    // ===== LFO / knob accent colors (used by LFO strips + non-XYZ knobs) =====
    uint32_t lfoAccent         = 0xFFC9A84Cu; // kWarmGold — LFO waveform, shape borders, knob arcs
    uint32_t lfoAccentBright   = 0xFFD9BE6Eu; // kBrightGold — selected shape line, waveform dot

    // ===== GL scene colors =====
    glm::vec3 glBackground     = toVec3(0xFF0F0D0Au);
    glm::vec3 glGrid           = toVec3(0xFFA89A70u); // Aged Papyrus dark
    glm::vec3 glListenerHead   = toVec3(0xFFC9A84Cu); // Gold Leaf
    glm::vec3 glNose          = toVec3(0xFFD9BE6Eu); // Gold Leaf light
    glm::vec3 glEar            = toVec3(0xFFA68B3Au); // Gold Leaf dark
    glm::vec3 glHat            = toVec3(0xFFB8962Au); // Hat default (warm gold-brown)
    glm::vec3 glEyeWhite       = {0.92f, 0.90f, 0.85f}; // Default eye sclera
    glm::vec3 glAudibleSphere  = toVec3(0xFFC9A84Cu); // Gold Leaf
    glm::vec3 glSourceNormal   = toVec3(0xFFD9BE6Eu); // Gold Leaf light
    glm::vec3 glSourceHover    = toVec3(0xFFE8D49Au); // Gold Leaf pale
    glm::vec3 glStereoL        = toVec3(0xFF5A9E8Fu); // Verdigris patina green
    glm::vec3 glStereoR        = toVec3(0xFFB85A3Au); // Cinnabar copper
    glm::vec3 glTrail          = toVec3(0xFFD9BE6Eu); // Gold Leaf light
};

// Theme name + data for UI display.
struct ThemeEntry {
    const char* name;
    ColorTheme  theme;
};

// Number of built-in themes.
inline constexpr int kNumThemes = 10;

// Get theme by index. Out-of-range returns Alchemy Gold.
const ThemeEntry& getThemeEntry(int index);

} // namespace xyzpan
