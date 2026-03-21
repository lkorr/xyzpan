#include "Camera.h"
#include <cmath>
#include <algorithm>

namespace xyzpan {

glm::mat4 Camera::getViewMatrix() const
{
    // Convert yaw/pitch from degrees to radians
    const float yawRad   = yaw   * (3.14159265f / 180.0f);
    const float pitchRad = pitch * (3.14159265f / 180.0f);

    // Compute eye position in spherical coordinates around origin
    const float cosP = std::cos(pitchRad);
    const float sinP = std::sin(pitchRad);
    const float cosY = std::cos(yawRad);
    const float sinY = std::sin(yawRad);

    const glm::vec3 eye(
        dist * cosP * sinY,   // X
        dist * sinP,          // Y (elevation)
        dist * cosP * cosY    // Z
    );

    const glm::vec3 target(0.0f, 0.0f, 0.0f);
    const glm::vec3 up(0.0f, 1.0f, 0.0f);

    return glm::lookAt(eye, target, up);
}

void Camera::setSnapTopDown()
{
    // Save current orbit in case user calls setOrbit()
    if (activeSnap == SnapView::Orbit) {
        savedYaw_   = yaw;
        savedPitch_ = pitch;
    }
    // Top-down: looking straight down along -Y axis.
    // Pitch=89.9 (near vertical) to avoid gimbal lock at exactly 90.
    yaw         = 0.0f;
    pitch       = 89.9f;
    dist        = 3.5f;
    orthoSnap   = true;
    activeSnap  = SnapView::TopDown;
}

void Camera::setSnapSide()
{
    if (activeSnap == SnapView::Orbit) {
        savedYaw_   = yaw;
        savedPitch_ = pitch;
    }
    // Side (XZ plane): looking from the positive X side, pitch=0, yaw=90
    yaw        = 90.0f;
    pitch      = 0.0f;
    dist       = 3.5f;
    orthoSnap  = true;
    activeSnap = SnapView::Side;
}

void Camera::setSnapFront()
{
    if (activeSnap == SnapView::Orbit) {
        savedYaw_   = yaw;
        savedPitch_ = pitch;
    }
    // Front (YZ plane): looking from the positive Z side, pitch=0, yaw=0
    yaw        = 0.0f;
    pitch      = 0.0f;
    dist       = 3.5f;
    orthoSnap  = true;
    activeSnap = SnapView::Front;
}

void Camera::setOrbit()
{
    yaw        = savedYaw_;
    pitch      = savedPitch_;
    orthoSnap  = false;
    activeSnap = SnapView::Orbit;
}

void Camera::applyMouseDrag(float dx, float dy)
{
    // If in a snap view, transition to free orbit starting from the current snap angles
    if (activeSnap != SnapView::Orbit) {
        orthoSnap  = false;
        activeSnap = SnapView::Orbit;
    }

    constexpr float kSensitivity = 0.4f;
    yaw   += dx * kSensitivity;
    pitch -= dy * kSensitivity;  // subtract: drag up -> camera tilts up (pitch increases)

    // Clamp pitch to avoid flipping over the poles
    pitch = std::clamp(pitch, -89.0f, 89.0f);
}

} // namespace xyzpan
