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
    kGroundCartesianGrid  = 10
};
inline constexpr int kNumGroundTypes = 11;

// POD struct for scene environment settings (skybox + ground plane).
// Trivially copyable under SpinLock alongside ColorTheme/AvatarParams.
struct SceneParams {
    int   skyType      = kSkyNone;
    int   groundType   = kGroundNone;
    float groundHeight = 0.0f;   // 0.0 = default (-4.5), 1.0 = 2x lower (-9.0)
    float groundHills  = 0.0f;   // 0.0 = flat, 1.0 = maximum terrain displacement
    bool  swapPanels   = false;  // swap listener and stereo orbit panel positions
    bool  showLabels   = true;   // show billboard object name labels in GL view
};

} // namespace xyzpan
