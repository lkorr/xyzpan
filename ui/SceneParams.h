#pragma once

namespace xyzpan {

enum SkyType : int {
    kSkyNone      = 0,
    kSkyDayClouds = 1,
    kSkyNightSky  = 2,
    kSkySunset    = 3,
    kSkyOvercast  = 4,
    kSkyAurora    = 5,
    kSkyContours  = 6,
    kSkyVoronoi   = 7,
    kSkyWireframe = 8,
    kSkyNoise     = 9
};
inline constexpr int kNumSkyTypes = 10;

enum GroundType : int {
    kGroundNone       = 0,
    kGroundGrass      = 1,
    kGroundSandDunes  = 2,
    kGroundCity       = 3,
    kGroundSnow       = 4,
    kGroundOcean      = 5,
    kGroundPolarGrid  = 6,
    kGroundContourMap = 7,
    kGroundVoronoi    = 8,
    kGroundTerraces       = 9,
    kGroundCartesianGrid  = 10,
    kGroundPailiaq        = 11
};
inline constexpr int kNumGroundTypes = 12;

// Source node visual shape (selectable in Customize tab).
enum SourceShape : int {
    kShapeSphere          = 0,
    kShapePyramid         = 1,   // single rotating pyramid
    kShapeCube            = 2,   // rotating cube
    kShapeOctahedron      = 3,   // rotating octahedron (diamond)
    kShapeRing            = 4,   // wireframe torus ring
    kShapeClusterSpheres  = 5,   // chaotic cluster of small spheres
    kShapeClusterPyramids = 6,   // chaotic cluster of small pyramids
    kShapeClusterCubes    = 7,   // chaotic cluster of small cubes
    kShapeClusterOctas    = 8,   // chaotic cluster of small octahedrons
    kShapeClusterRings    = 9    // chaotic cluster of small torus rings
};
inline constexpr int kNumSourceShapes = 10;

// OpenGL blend mode for sound wave rendering.
// 0-7: single modes applied uniformly to all waves.
// 8-15: compound modes that cycle blend state per-wave for layered effects.
enum WaveBlendMode : int {
    // --- Single modes ---
    kBlendNormal     = 0,
    kBlendAdditive   = 1,
    kBlendDifference = 2,
    kBlendMin        = 3,   // Darken
    kBlendMax        = 4,   // Brighten
    kBlendMultiply   = 5,
    kBlendScreen     = 6,   // Inverse multiply — brightens without blowing out whites
    kBlendInvert     = 7,   // Color inversion via subtract-from-white
    // --- Compound modes (cycle per-wave) ---
    kBlendPulse      = 8,   // Additive → Normal alternating
    kBlendStrobe     = 9,   // Additive → Difference alternating
    kBlendBreath     = 10,  // Normal → Max → Normal → Min cycling
    kBlendPrism      = 11,  // Additive → Difference → Screen cycling
    kBlendHaze       = 12,  // Multiply → Additive → Max cycling
    kBlendShatter    = 13,  // Difference → Min → Additive → Max → Invert cycling
    kBlendNebula     = 14,  // Screen → Additive → Max → Normal cycling
    kBlendVoid       = 15   // Invert → Difference → Min → Multiply → Additive cycling
};
inline constexpr int kNumWaveBlendModes = 16;

// POD struct for scene environment settings (skybox + ground plane).
// Trivially copyable under SpinLock alongside ColorTheme/AvatarParams.
struct SceneParams {
    int   skyType      = kSkyNone;
    int   groundType   = kGroundNone;
    float groundHeight = 0.0f;   // 0.0 = default (-4.5), 1.0 = 10x lower (-45.0)
    float groundHills  = 0.0f;   // 0.0 = flat, 1.0 = maximum terrain displacement
    float groundRipple = 0.0f;  // 0.0 = static hills, 1.0 = max animation speed
    float groundFog    = 1.0f;  // 0.0 = no fog, 1.0 = full fog
    bool  showLabels   = true;   // show billboard object name labels in GL view
    bool  showArrow    = true;   // show direction arrow emanating from observer head
    int   sourceShape    = kShapeSphere;  // visual shape for source nodes
    int   clusterCount   = 7;             // number of objects in cluster shapes (1–7)
};

} // namespace xyzpan
