#pragma once

namespace xyzpan {

// Parameters passed to the engine each processBlock.
// Grows per phase as DSP features are added.
struct EngineParams {
    float x = 0.0f;  // [-1, 1]: X=1 = right, X=-1 = left
    float y = 1.0f;  // [-1, 1]: Y=1 = front, Y=-1 = behind
    float z = 0.0f;  // [-1, 1]: Z=1 = above, Z=-1 = below
};

// Result of XYZ-to-spherical coordinate conversion.
// Y-forward convention: azimuth=0 means directly in front (Y=1).
struct SphericalCoord {
    float azimuth;    // radians: 0 = front, +PI/2 = right (X=1), clockwise positive
    float elevation;  // radians: +PI/2 = directly above, -PI/2 = directly below
    float distance;   // normalized Euclidean distance, clamped to [kMinDistance, sqrt(3)]
};

} // namespace xyzpan
