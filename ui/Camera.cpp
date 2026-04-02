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

    constexpr float kSensitivity = 0.007f; // radians per pixel (~0.4 deg/px)

    // Accumulate drag rotation in quaternion space to avoid Euler cross-coupling
    // drift at non-cardinal roll angles (45°, 135°, etc.).
    //
    // Yaw: world-up Y axis — horizontal drag always orbits around "up".
    // Pitch: camera-local right axis — vertical drag always tilts the view.
    glm::quat qYaw = glm::angleAxis(-dx * kSensitivity, glm::vec3(0.0f, 1.0f, 0.0f));

    glm::vec3 right = orientation_ * glm::vec3(1.0f, 0.0f, 0.0f);
    glm::quat qPitch = glm::angleAxis(-dy * kSensitivity, right);

    orientation_ = glm::normalize(qYaw * qPitch * orientation_);

    // Extract Euler angles for parameters/display.
    syncEulerFromQuat();
}

// ---------------------------------------------------------------------------
// Quaternion ↔ Euler synchronisation
// Rotation order: Ry(yaw) · Rx(pitch) · Rz(-roll)   (matches getViewMatrix)
// ---------------------------------------------------------------------------
void Camera::syncQuatFromEuler()
{
    glm::quat qY = glm::angleAxis(glm::radians(yaw),   glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat qP = glm::angleAxis(glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat qR = glm::angleAxis(glm::radians(-roll), glm::vec3(0.0f, 0.0f, 1.0f));
    orientation_ = qY * qP * qR;
}

void Camera::syncEulerFromQuat()
{
    // Decompose R = Ry(yaw) · Rx(pitch) · Rz(-roll)
    // GLM column-major: m[col][row]
    //   m[2][1] = -sin(pitch)
    //   m[2][0] =  sin(yaw)*cos(pitch)     m[2][2] = cos(yaw)*cos(pitch)
    //   m[1][0] =  cos(pitch)*sin(-roll)    m[1][1] = cos(pitch)*cos(-roll)
    glm::mat3 m = glm::mat3_cast(orientation_);

    float sinP = glm::clamp(-m[2][1], -1.0f, 1.0f);
    float newPitch = glm::degrees(std::asin(sinP));

    float newYaw, newRoll;
    if (std::abs(sinP) < 0.9999f) {
        newYaw  =  glm::degrees(std::atan2(m[2][0], m[2][2]));
        newRoll = -glm::degrees(std::atan2(m[0][1], m[1][1]));
    } else {
        // Gimbal lock — pitch ≈ ±90°. Roll is indeterminate; keep previous value.
        newRoll = roll;
        newYaw  = glm::degrees(std::atan2(-m[0][2], m[0][0]));
    }

    // Unwrap each angle to stay closest to the previous value (prevents 180° knob jumps).
    auto unwrap = [](float nv, float ov) {
        float d = std::fmod(std::fmod(nv - ov + 180.0f, 360.0f) + 360.0f, 360.0f) - 180.0f;
        if (d < -180.0f) d += 360.0f;
        return ov + d;
    };

    yaw   = unwrap(newYaw,   yaw);
    pitch = unwrap(newPitch, pitch);
    roll  = unwrap(newRoll,  roll);
}

} // namespace xyzpan
