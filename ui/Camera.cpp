#include "Camera.h"
#include <cmath>

namespace xyzpan {

Camera::Camera() { syncQuatFromEuler(); }

glm::mat4 Camera::getViewMatrix(const glm::vec3& target) const
{
    glm::mat4 rot = glm::mat4_cast(orientation_);

    // Extract camera axes from rotation matrix
    glm::vec3 forward = glm::vec3(rot * glm::vec4(0, 0, -1, 0));
    glm::vec3 up      = glm::vec3(rot * glm::vec4(0, 1,  0, 0));

    // Eye position: orbit at `dist` behind the target along -forward
    const glm::vec3 eye = target - forward * dist;

    // Build view matrix manually (inverse of camera transform)
    glm::vec3 right = glm::normalize(glm::cross(forward, up));
    up = glm::cross(right, forward);

    glm::mat4 view(1.0f);
    view[0][0] = right.x;    view[1][0] = right.y;    view[2][0] = right.z;
    view[0][1] = up.x;       view[1][1] = up.y;       view[2][1] = up.z;
    view[0][2] = -forward.x; view[1][2] = -forward.y; view[2][2] = -forward.z;
    view[3][0] = -glm::dot(right, eye);
    view[3][1] = -glm::dot(up, eye);
    view[3][2] =  glm::dot(forward, eye);

    return view;
}

void Camera::setSnapTopDown()
{
    // Save current orbit in case user calls setOrbit()
    if (activeSnap == SnapView::Orbit) {
        savedYaw_   = yaw;
        savedPitch_ = pitch;
        savedRoll_  = roll;
    }
    // Top-down: looking straight down along -Y axis.
    yaw         = 0.0f;
    pitch       = 90.0f;
    roll        = 0.0f;
    dist        = 3.5f;
    orthoSnap   = true;
    activeSnap  = SnapView::TopDown;
    syncQuatFromEuler();
}

void Camera::setSnapSide()
{
    if (activeSnap == SnapView::Orbit) {
        savedYaw_   = yaw;
        savedPitch_ = pitch;
        savedRoll_  = roll;
    }
    // Side (XZ plane): looking from the positive X side, pitch=0, yaw=90
    yaw        = 90.0f;
    pitch      = 0.0f;
    roll       = 0.0f;
    dist       = 3.5f;
    orthoSnap  = true;
    activeSnap = SnapView::Side;
    syncQuatFromEuler();
}

void Camera::setSnapFront()
{
    if (activeSnap == SnapView::Orbit) {
        savedYaw_   = yaw;
        savedPitch_ = pitch;
        savedRoll_  = roll;
    }
    // Front (YZ plane): looking from the positive Z side, pitch=0, yaw=0
    yaw        = 0.0f;
    pitch      = 0.0f;
    roll       = 0.0f;
    dist       = 3.5f;
    orthoSnap  = true;
    activeSnap = SnapView::Front;
    syncQuatFromEuler();
}

void Camera::setOrbit()
{
    yaw        = savedYaw_;
    pitch      = savedPitch_;
    roll       = savedRoll_;
    orthoSnap  = false;
    activeSnap = SnapView::Orbit;
    syncQuatFromEuler();
}

void Camera::applyMouseDrag(float dx, float dy)
{
    // If in a snap view, transition to free orbit starting from the current snap angles
    if (activeSnap != SnapView::Orbit) {
        orthoSnap  = false;
        activeSnap = SnapView::Orbit;
    }

    constexpr float kSensitivity = 0.4f;
    const float yawAngle   = glm::radians(-dx * kSensitivity);
    const float pitchAngle = glm::radians(-dy * kSensitivity);

    // Right vector from CURRENT quaternion (no Euler rebuild)
    glm::vec3 right = glm::mat3_cast(orientation_)[0];

    // Horizontal → world Y spin;  Vertical → camera-right backflip
    glm::quat qYaw   = glm::angleAxis(yawAngle,   glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat qPitch = glm::angleAxis(pitchAngle, right);

    // Accumulate into member quaternion — NO Euler round-trip
    orientation_ = glm::normalize(qPitch * qYaw * orientation_);

    // Extract Euler for knobs (output-only, not fed back into orientation_)
    syncEulerFromQuat();
}

// ---------------------------------------------------------------------------
// Quaternion ↔ Euler synchronisation
// Rotation order: Ry(yaw) · Rz(-roll) · Rx(pitch)
// Roll is the middle axis → ±90° singularity on roll (head sideways).
// Yaw and pitch get full ±180° range.
// ---------------------------------------------------------------------------
void Camera::syncQuatFromEuler()
{
    glm::quat qY = glm::angleAxis(glm::radians(yaw),   glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat qR = glm::angleAxis(glm::radians(-roll), glm::vec3(0.0f, 0.0f, 1.0f));
    glm::quat qP = glm::angleAxis(glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
    orientation_ = qY * qR * qP;
}

void Camera::syncEulerFromQuat()
{
    // Decompose R = Ry(yaw) · Rz(-roll) · Rx(pitch)
    // Roll (middle axis) limited to ±90°. Yaw and pitch get full ±180°.
    glm::mat3 m = glm::mat3_cast(orientation_);

    // sin(-roll) = m[0][1]
    float sinR = glm::clamp(m[0][1], -1.0f, 1.0f);
    float newRoll = -glm::degrees(std::asin(sinR));

    float newYaw, newPitch;
    if (std::abs(sinR) < 0.9999f) {
        newYaw   = glm::degrees(std::atan2(-m[0][2], m[0][0]));
        newPitch = glm::degrees(std::atan2(-m[2][1], m[1][1]));
    } else {
        // Gimbal lock — roll ≈ ±90°. Yaw indeterminate; keep previous.
        newYaw   = yaw;
        newPitch = glm::degrees(std::atan2(m[1][2], m[2][2]));
    }

    auto wrap180 = [](float v) {
        return std::fmod(std::fmod(v + 180.0f, 360.0f) + 360.0f, 360.0f) - 180.0f;
    };

    yaw   = wrap180(newYaw);
    pitch = wrap180(newPitch);
    roll  = wrap180(newRoll);
}

} // namespace xyzpan
