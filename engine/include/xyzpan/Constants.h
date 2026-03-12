#pragma once

namespace xyzpan {

// Minimum distance from the origin to prevent division-by-zero and undefined
// azimuth/elevation when the source is placed at or near the listener position.
constexpr float kMinDistance = 0.1f;

// Hard clamp limit for all XYZ inputs. Values outside this range are clamped
// before processing. LFO overshoot beyond ±1 is the caller's responsibility.
constexpr float kMaxInputXYZ = 1.0f;

} // namespace xyzpan
