#pragma once
// ListenerQuatAccumulator — quaternion-based head orientation accumulator.
// Decouples mouse/key delta accumulation (quaternion space) from the
// DAW-automatable RPY knob parameters (Euler space).
//
// All methods are called on the message thread (mouse via callAsync,
// Q/E via editor timer, parameterChanged from JUCE). No mutex needed.

#include "QuatMath.h"
#include <atomic>
#include <memory>

namespace xyzpan {

class ListenerQuatAccumulator {
public:
    // Path A: mouse drag — fully body-frame.
    // dx = screen-space horizontal pixels, dy = screen-space vertical pixels.
    void applyMouseDelta(float screenDx, float screenDy, float sensitivity)
    {
        // Body-local axes
        const Vec3 up    = quatRotateVec(q_, 0.0f, 0.0f, 1.0f);  // body Z = up
        const Vec3 right = quatRotateVec(q_, 1.0f, 0.0f, 0.0f);  // body X = right

        // Delta quaternions around body axes (expressed in world coords)
        const Quat qYaw   = quatFromAxisAngle(up.x,    up.y,    up.z,    -screenDx * sensitivity);
        const Quat qPitch = quatFromAxisAngle(right.x, right.y, right.z, -screenDy * sensitivity);

        // Left-multiply: axes are in world coords, so delta applies in world frame
        q_ = quatNormalize(quatMul(quatMul(qPitch, qYaw), q_));
        prevRPY_ = quatToRPY(q_, prevRPY_);
    }

    // Path A: Q/E roll around body-forward axis.
    void applyRollDelta(float rollDeltaDeg)
    {
        constexpr float kDeg2Rad = 3.14159265358979323846f / 180.0f;
        const Vec3 fwd = quatRotateVec(q_, 0.0f, 1.0f, 0.0f);  // body Y = forward
        const Quat qRoll = quatFromAxisAngle(fwd.x, fwd.y, fwd.z, rollDeltaDeg * kDeg2Rad);
        q_ = quatNormalize(quatMul(qRoll, q_));
        prevRPY_ = quatToRPY(q_, prevRPY_);
    }

    // Path B: DAW automation or knob edit wrote RPY externally.
    void syncFromRPY(float yawDeg, float pitchDeg, float rollDeg)
    {
        RPY rpy{yawDeg, pitchDeg, rollDeg};
        q_ = rpyToQuat(rpy);
        prevRPY_ = rpy;
    }

    // Read the last baked RPY.
    RPY bakeRPY() const { return prevRPY_; }

    // Current quaternion (for testing / inspection).
    Quat currentQuat() const { return q_; }

    // Set true before writing APVTS from Path A, false after.
    // parameterChanged checks this to skip RPY->quat sync for Path A writes.
    std::shared_ptr<std::atomic<bool>> drivingFromInput =
        std::make_shared<std::atomic<bool>>(false);

private:
    Quat q_{1.0f, 0.0f, 0.0f, 0.0f};
    RPY  prevRPY_{0.0f, 0.0f, 0.0f};
};

} // namespace xyzpan
