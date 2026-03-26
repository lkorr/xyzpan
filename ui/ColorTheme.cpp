#include "ColorTheme.h"

namespace xyzpan {

// ---------------------------------------------------------------------------
// Theme 0: Alchemy Gold (default — matches current static constexpr palette)
// ---------------------------------------------------------------------------
static const ThemeEntry kAlchemyGold = {
    "Alchemy Gold",
    ColorTheme{} // all defaults match the original palette
};

// ---------------------------------------------------------------------------
// Theme 1: Obsidian Silver — cool silver/steel on deep charcoal
// ---------------------------------------------------------------------------
static const ThemeEntry kObsidianSilver = {
    "Obsidian Silver",
    {
        // JUCE panel colors
        0xFF0C0C0Eu, // background — near-black blue-grey
        0xFF1A1A1Eu, // darkIron — dark steel
        0xFF4A4A52u, // bronze — mid steel
        0xFFA0A0B0u, // warmGold — silver
        0xFFC0C0D0u, // brightGold — bright silver
        0xFFB0B0BAu, // parchment — light silver-grey
        0xFFD0D0DAu, // goldLeafPale — pale silver
        0xFF0C0C0Eu, // obsidian
        0xFF161618u, // obsidianLight
        0xFF2A2A30u, // darkParchmentMid
        0xFF1A1A1Eu, // darkParchment
        0xFFB0B0BAu, // agedPapyrus
        0xFF8A8A94u, // agedPapyrusDark
        0xFFD0D0DAu, // hoverGold
        // LFO / knob accent colors
        0xFF7A8EB0u, // lfoAccent — steel blue
        0xFF9AB0D0u, // lfoAccentBright — bright steel blue
        // GL scene colors
        toVec3(0xFF0C0C0Eu), // glBackground
        toVec3(0xFF8A8A94u), // glGrid
        toVec3(0xFFA0A0B0u), // glListenerHead
        toVec3(0xFFC0C0D0u), // glNose
        toVec3(0xFF808090u), // glEar
        toVec3(0xFF9090A0u), // glHat
        {0.90f, 0.90f, 0.92f}, // glEyeWhite — cool off-white
        toVec3(0xFFA0A0B0u), // glAudibleSphere
        toVec3(0xFFC0C0D0u), // glSourceNormal
        toVec3(0xFFD0D0DAu), // glSourceHover
        toVec3(0xFF5A8A9Eu), // glStereoL — cool teal
        toVec3(0xFF9A5A6Au), // glStereoR — muted rose
        toVec3(0xFFC0C0D0u), // glTrail
    }
};

// ---------------------------------------------------------------------------
// Theme 2: Verdant Copper — oxidized copper greens on dark earth
// ---------------------------------------------------------------------------
static const ThemeEntry kVerdantCopper = {
    "Verdant Copper",
    {
        // JUCE panel colors
        0xFF0A0E0Au, // background — dark forest
        0xFF141E14u, // darkIron — dark moss
        0xFF3A5A3Au, // bronze — copper patina
        0xFF6AAA6Au, // warmGold — verdant green
        0xFF8ACC8Au, // brightGold — bright patina
        0xFF9ABA8Au, // parchment — pale sage
        0xFFBADAAAu, // goldLeafPale — light green
        0xFF0A0E0Au, // obsidian
        0xFF121A12u, // obsidianLight
        0xFF243424u, // darkParchmentMid
        0xFF141E14u, // darkParchment
        0xFF9ABA8Au, // agedPapyrus
        0xFF7A9A6Au, // agedPapyrusDark
        0xFFBADAAAu, // hoverGold
        // LFO / knob accent colors
        0xFF4AAA8Au, // lfoAccent — teal copper patina
        0xFF6ACCA8u, // lfoAccentBright — bright patina
        // GL scene colors
        toVec3(0xFF0A0E0Au), // glBackground
        toVec3(0xFF7A9A6Au), // glGrid
        toVec3(0xFF6AAA6Au), // glListenerHead
        toVec3(0xFF8ACC8Au), // glNose
        toVec3(0xFF5A8A4Au), // glEar
        toVec3(0xFF4A7A3Au), // glHat
        {0.88f, 0.92f, 0.86f}, // glEyeWhite — greenish off-white
        toVec3(0xFF6AAA6Au), // glAudibleSphere
        toVec3(0xFF8ACC8Au), // glSourceNormal
        toVec3(0xFFBADAAAu), // glSourceHover
        toVec3(0xFF4A9A8Au), // glStereoL — teal-green
        toVec3(0xFFAA6A3Au), // glStereoR — warm copper
        toVec3(0xFF8ACC8Au), // glTrail
    }
};

// ---------------------------------------------------------------------------
// Theme 3: Cinnabar Red — deep crimson/scarlet on dark oxblood
// ---------------------------------------------------------------------------
static const ThemeEntry kCinnabarRed = {
    "Cinnabar Red",
    {
        // JUCE panel colors
        0xFF0E0808u, // background — dark oxblood
        0xFF1E1212u, // darkIron — deep maroon
        0xFF5A3232u, // bronze — dark crimson
        0xFFAA5A4Au, // warmGold — cinnabar red
        0xFFCC7A6Au, // brightGold — bright cinnabar
        0xFFBA8A7Au, // parchment — warm rose-grey
        0xFFDAAA9Au, // goldLeafPale — pale rose
        0xFF0E0808u, // obsidian
        0xFF181010u, // obsidianLight
        0xFF302020u, // darkParchmentMid
        0xFF1E1212u, // darkParchment
        0xFFBA8A7Au, // agedPapyrus
        0xFF9A6A5Au, // agedPapyrusDark
        0xFFDAAA9Au, // hoverGold
        // LFO / knob accent colors
        0xFFCC6A4Au, // lfoAccent — cinnabar-ember
        0xFFE88A6Au, // lfoAccentBright — bright ember
        // GL scene colors
        toVec3(0xFF0E0808u), // glBackground
        toVec3(0xFF9A6A5Au), // glGrid
        toVec3(0xFFAA5A4Au), // glListenerHead
        toVec3(0xFFCC7A6Au), // glNose
        toVec3(0xFF8A4A3Au), // glEar
        toVec3(0xFF7A3A2Au), // glHat
        {0.92f, 0.88f, 0.85f}, // glEyeWhite — warm pinkish off-white
        toVec3(0xFFAA5A4Au), // glAudibleSphere
        toVec3(0xFFCC7A6Au), // glSourceNormal
        toVec3(0xFFDAAA9Au), // glSourceHover
        toVec3(0xFF5A9A7Au), // glStereoL — contrasting teal
        toVec3(0xFFDA6A4Au), // glStereoR — bright cinnabar
        toVec3(0xFFCC7A6Au), // glTrail
    }
};

// ---------------------------------------------------------------------------
// Theme 4: Midnight Neon — deep navy with electric cyan/magenta LFO accents
// ---------------------------------------------------------------------------
static const ThemeEntry kMidnightNeon = {
    "Midnight Neon",
    {
        // JUCE panel colors
        0xFF08080Eu, // background — deep navy-black
        0xFF10101Au, // darkIron — dark indigo
        0xFF2A2A3Au, // bronze — muted slate-blue
        0xFF5A5A70u, // warmGold — dusty lavender
        0xFF7A7A90u, // brightGold — light slate
        0xFF808096u, // parchment — cool grey
        0xFFA0A0B6u, // goldLeafPale — pale lavender
        0xFF08080Eu, // obsidian
        0xFF0E0E16u, // obsidianLight
        0xFF202030u, // darkParchmentMid
        0xFF10101Au, // darkParchment
        0xFF8080A0u, // agedPapyrus
        0xFF606078u, // agedPapyrusDark
        0xFFA0A0C0u, // hoverGold
        // LFO / knob accent — electric cyan (high saturation, pops against muted navy)
        0xFF00D4FFu, // lfoAccent — electric cyan
        0xFF40E8FFu, // lfoAccentBright — bright cyan
        // GL scene colors
        toVec3(0xFF08080Eu), // glBackground
        toVec3(0xFF404060u), // glGrid
        toVec3(0xFF5A5A70u), // glListenerHead
        toVec3(0xFF7A7A90u), // glNose
        toVec3(0xFF4A4A60u), // glEar
        toVec3(0xFF3A3A50u), // glHat
        {0.88f, 0.88f, 0.94f}, // glEyeWhite
        toVec3(0xFF5A5A70u), // glAudibleSphere
        toVec3(0xFF7A7A90u), // glSourceNormal
        toVec3(0xFFA0A0B6u), // glSourceHover
        toVec3(0xFF00C8E0u), // glStereoL — cyan
        toVec3(0xFFE040A0u), // glStereoR — magenta
        toVec3(0xFF7A7A90u), // glTrail
    }
};

// ---------------------------------------------------------------------------
// Theme 5: Void Purple — dark charcoal with vivid violet/purple LFO accents
// ---------------------------------------------------------------------------
static const ThemeEntry kVoidPurple = {
    "Void Purple",
    {
        // JUCE panel colors
        0xFF0A080Cu, // background — near-black purple
        0xFF161218u, // darkIron — dark plum
        0xFF3A3040u, // bronze — dusty mauve
        0xFF6A5A74u, // warmGold — muted purple-grey
        0xFF8A7A94u, // brightGold — light mauve
        0xFF9A8AA4u, // parchment — lavender grey
        0xFFBAAAC4u, // goldLeafPale — pale lavender
        0xFF0A080Cu, // obsidian
        0xFF120E14u, // obsidianLight
        0xFF282030u, // darkParchmentMid
        0xFF161218u, // darkParchment
        0xFF9A8AA4u, // agedPapyrus
        0xFF7A6A84u, // agedPapyrusDark
        0xFFBAAAC4u, // hoverGold
        // LFO / knob accent — vivid electric purple (pops against muted mauve)
        0xFFB040FFu, // lfoAccent — electric violet
        0xFFCC70FFu, // lfoAccentBright — bright violet
        // GL scene colors
        toVec3(0xFF0A080Cu), // glBackground
        toVec3(0xFF5A4A64u), // glGrid
        toVec3(0xFF6A5A74u), // glListenerHead
        toVec3(0xFF8A7A94u), // glNose
        toVec3(0xFF5A4A64u), // glEar
        toVec3(0xFF4A3A54u), // glHat
        {0.90f, 0.88f, 0.94f}, // glEyeWhite
        toVec3(0xFF6A5A74u), // glAudibleSphere
        toVec3(0xFF8A7A94u), // glSourceNormal
        toVec3(0xFFBAAAC4u), // glSourceHover
        toVec3(0xFF7A50C0u), // glStereoL — deep purple
        toVec3(0xFFE06090u), // glStereoR — hot pink
        toVec3(0xFF8A7A94u), // glTrail
    }
};

// ---------------------------------------------------------------------------
// Theme 6: Arctic Frost — icy blue-white with hot orange LFO accents
// ---------------------------------------------------------------------------
static const ThemeEntry kArcticFrost = {
    "Arctic Frost",
    {
        // JUCE panel colors
        0xFF0A0C10u, // background — deep arctic blue
        0xFF141820u, // darkIron — dark slate blue
        0xFF3A4450u, // bronze — blue-grey
        0xFF6A7A8Au, // warmGold — slate
        0xFF8A9AAAu, // brightGold — light steel
        0xFFA0B0C0u, // parchment — pale blue-grey
        0xFFC0D0E0u, // goldLeafPale — ice blue
        0xFF0A0C10u, // obsidian
        0xFF101418u, // obsidianLight
        0xFF243040u, // darkParchmentMid
        0xFF141820u, // darkParchment
        0xFFA0B0C0u, // agedPapyrus
        0xFF7A8A9Au, // agedPapyrusDark
        0xFFC0D0E0u, // hoverGold
        // LFO / knob accent — hot orange (extreme contrast against cold blues)
        0xFFFF8020u, // lfoAccent — hot orange
        0xFFFFA050u, // lfoAccentBright — bright amber
        // GL scene colors
        toVec3(0xFF0A0C10u), // glBackground
        toVec3(0xFF5A6A7Au), // glGrid
        toVec3(0xFF6A7A8Au), // glListenerHead
        toVec3(0xFF8A9AAAu), // glNose
        toVec3(0xFF5A6A7Au), // glEar
        toVec3(0xFF4A5A6Au), // glHat
        {0.90f, 0.92f, 0.96f}, // glEyeWhite — cool blue-white
        toVec3(0xFF6A7A8Au), // glAudibleSphere
        toVec3(0xFF8A9AAAu), // glSourceNormal
        toVec3(0xFFC0D0E0u), // glSourceHover
        toVec3(0xFF40A0D0u), // glStereoL — ice blue
        toVec3(0xFFE07030u), // glStereoR — warm orange
        toVec3(0xFF8A9AAAu), // glTrail
    }
};

// ---------------------------------------------------------------------------
// Theme 7: Ember Ash — dark charcoal/ash with neon green LFO accents
// ---------------------------------------------------------------------------
static const ThemeEntry kEmberAsh = {
    "Ember Ash",
    {
        // JUCE panel colors
        0xFF0C0C0Au, // background — warm charcoal
        0xFF1A1816u, // darkIron — dark ash
        0xFF3A3834u, // bronze — mid ash
        0xFF6A6860u, // warmGold — warm grey
        0xFF8A8880u, // brightGold — light ash
        0xFF9A9890u, // parchment — pale ash
        0xFFBAB8B0u, // goldLeafPale — light warm grey
        0xFF0C0C0Au, // obsidian
        0xFF141412u, // obsidianLight
        0xFF2A2A26u, // darkParchmentMid
        0xFF1A1816u, // darkParchment
        0xFF9A9890u, // agedPapyrus
        0xFF7A7870u, // agedPapyrusDark
        0xFFBAB8B0u, // hoverGold
        // LFO / knob accent — neon green (extreme pop against neutral ash)
        0xFF30E060u, // lfoAccent — neon green
        0xFF60FF90u, // lfoAccentBright — bright neon green
        // GL scene colors
        toVec3(0xFF0C0C0Au), // glBackground
        toVec3(0xFF5A5850u), // glGrid
        toVec3(0xFF6A6860u), // glListenerHead
        toVec3(0xFF8A8880u), // glNose
        toVec3(0xFF5A5850u), // glEar
        toVec3(0xFF4A4840u), // glHat
        {0.92f, 0.92f, 0.88f}, // glEyeWhite — warm off-white
        toVec3(0xFF6A6860u), // glAudibleSphere
        toVec3(0xFF8A8880u), // glSourceNormal
        toVec3(0xFFBAB8B0u), // glSourceHover
        toVec3(0xFF20C050u), // glStereoL — green
        toVec3(0xFFD06030u), // glStereoR — warm ember
        toVec3(0xFF8A8880u), // glTrail
    }
};

// ---------------------------------------------------------------------------
// Theme 8: Solar Flare — deep brown-black with vivid gold-yellow LFO accents
// ---------------------------------------------------------------------------
static const ThemeEntry kSolarFlare = {
    "Solar Flare",
    {
        // JUCE panel colors
        0xFF0E0A06u, // background — deep warm brown
        0xFF1A1410u, // darkIron — dark umber
        0xFF3A3028u, // bronze — brown
        0xFF6A5A48u, // warmGold — dark tan
        0xFF8A7A68u, // brightGold — tan
        0xFF9A8A78u, // parchment — light brown
        0xFFBAA898u, // goldLeafPale — pale tan
        0xFF0E0A06u, // obsidian
        0xFF141008u, // obsidianLight
        0xFF2A2218u, // darkParchmentMid
        0xFF1A1410u, // darkParchment
        0xFF9A8A78u, // agedPapyrus
        0xFF7A6A58u, // agedPapyrusDark
        0xFFBAA898u, // hoverGold
        // LFO / knob accent — vivid solar yellow (blazing against dark earth tones)
        0xFFFFD020u, // lfoAccent — solar yellow
        0xFFFFE060u, // lfoAccentBright — bright solar
        // GL scene colors
        toVec3(0xFF0E0A06u), // glBackground
        toVec3(0xFF5A4A38u), // glGrid
        toVec3(0xFF6A5A48u), // glListenerHead
        toVec3(0xFF8A7A68u), // glNose
        toVec3(0xFF5A4A38u), // glEar
        toVec3(0xFF4A3A28u), // glHat
        {0.94f, 0.90f, 0.84f}, // glEyeWhite — warm amber-white
        toVec3(0xFF6A5A48u), // glAudibleSphere
        toVec3(0xFF8A7A68u), // glSourceNormal
        toVec3(0xFFBAA898u), // glSourceHover
        toVec3(0xFFE0A020u), // glStereoL — amber
        toVec3(0xFFE04020u), // glStereoR — solar red
        toVec3(0xFF8A7A68u), // glTrail
    }
};

// ---------------------------------------------------------------------------
// Theme 9: Deep Ocean — dark teal-black with hot pink/magenta LFO accents
// ---------------------------------------------------------------------------
static const ThemeEntry kDeepOcean = {
    "Deep Ocean",
    {
        // JUCE panel colors
        0xFF060A0Cu, // background — deep ocean black
        0xFF0E1418u, // darkIron — dark teal
        0xFF283840u, // bronze — dark sea slate
        0xFF4A6068u, // warmGold — sea grey
        0xFF6A8088u, // brightGold — light sea
        0xFF7A9098u, // parchment — pale sea
        0xFF9AB0B8u, // goldLeafPale — mist blue
        0xFF060A0Cu, // obsidian
        0xFF0C1216u, // obsidianLight
        0xFF1E2C34u, // darkParchmentMid
        0xFF0E1418u, // darkParchment
        0xFF7A9098u, // agedPapyrus
        0xFF5A7078u, // agedPapyrusDark
        0xFF9AB0B8u, // hoverGold
        // LFO / knob accent — hot magenta/pink (vivid pop against cool dark teal)
        0xFFFF3090u, // lfoAccent — hot magenta-pink
        0xFFFF60B0u, // lfoAccentBright — bright pink
        // GL scene colors
        toVec3(0xFF060A0Cu), // glBackground
        toVec3(0xFF3A5058u), // glGrid
        toVec3(0xFF4A6068u), // glListenerHead
        toVec3(0xFF6A8088u), // glNose
        toVec3(0xFF3A5058u), // glEar
        toVec3(0xFF2A4048u), // glHat
        {0.86f, 0.92f, 0.94f}, // glEyeWhite — cool aqua-white
        toVec3(0xFF4A6068u), // glAudibleSphere
        toVec3(0xFF6A8088u), // glSourceNormal
        toVec3(0xFF9AB0B8u), // glSourceHover
        toVec3(0xFF2090B0u), // glStereoL — ocean blue
        toVec3(0xFFE04080u), // glStereoR — magenta
        toVec3(0xFF6A8088u), // glTrail
    }
};

// ---------------------------------------------------------------------------
static const ThemeEntry* kThemes[kNumThemes] = {
    &kAlchemyGold,
    &kObsidianSilver,
    &kVerdantCopper,
    &kCinnabarRed,
    &kMidnightNeon,
    &kVoidPurple,
    &kArcticFrost,
    &kEmberAsh,
    &kSolarFlare,
    &kDeepOcean
};

const ThemeEntry& getThemeEntry(int index) {
    if (index < 0 || index >= kNumThemes)
        return kAlchemyGold;
    return *kThemes[index];
}

} // namespace xyzpan
