#pragma once
// QuatMath.h — standalone quaternion math for Z-up listener orientation.
// Zero dependencies. All functions are inline for header-only use.
//
// Coordinate convention (engine):
//   X = right, Y = forward, Z = up
//   Rotation order: R = Rz(yaw) * Rx(pitch) * Ry(roll)
//   Yaw around Z, Pitch around X, Roll around Y (forward axis)

#include <cmath>
#include <algorithm>

namespace xyzpan {

struct Quat {
    float w = 1.0f, x = 0.0f, y = 0.0f, z = 0.0f;
};

struct RPY {
    float yawDeg = 0.0f, pitchDeg = 0.0f, rollDeg = 0.0f;
};

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

// Hamilton product: a * b
inline Quat quatMul(const Quat& a, const Quat& b)
{
    return {
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w
    };
}

// L2 normalize
inline Quat quatNormalize(const Quat& q)
{
    const float mag = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    if (mag < 1e-12f) return {1.0f, 0.0f, 0.0f, 0.0f};
    const float inv = 1.0f / mag;
    return {q.w * inv, q.x * inv, q.y * inv, q.z * inv};
}

// Quaternion from axis-angle. Axis need not be unit length (will be normalized).
inline Quat quatFromAxisAngle(float ax, float ay, float az, float angleRad)
{
    const float mag = std::sqrt(ax * ax + ay * ay + az * az);
    if (mag < 1e-12f) return {1.0f, 0.0f, 0.0f, 0.0f};
    const float inv = 1.0f / mag;
    const float ha = angleRad * 0.5f;
    const float s = std::sin(ha) * inv;
    return {std::cos(ha), ax * s, ay * s, az * s};
}

// Rotate vector v by quaternion q: q * [0,v] * q_conj
inline Vec3 quatRotateVec(const Quat& q, float vx, float vy, float vz)
{
    // Optimized form: v' = v + 2*w*(u x v) + 2*(u x (u x v))
    // where u = (q.x, q.y, q.z), w = q.w
    const float ux = q.x, uy = q.y, uz = q.z;
    // t = 2 * (u x v)
    const float tx = 2.0f * (uy * vz - uz * vy);
    const float ty = 2.0f * (uz * vx - ux * vz);
    const float tz = 2.0f * (ux * vy - uy * vx);
    return {
        vx + q.w * tx + (uy * tz - uz * ty),
        vy + q.w * ty + (uz * tx - ux * tz),
        vz + q.w * tz + (ux * ty - uy * tx)
    };
}

// RPY (degrees) -> quaternion.
// Rotation order: R = Rz(yaw) * Rx(pitch) * Ry(roll)
inline Quat rpyToQuat(const RPY& rpy)
{
    constexpr float kDeg2Rad = 3.14159265358979323846f / 180.0f;
    const Quat qYaw   = quatFromAxisAngle(0.0f, 0.0f, 1.0f, rpy.yawDeg   * kDeg2Rad);
    const Quat qPitch = quatFromAxisAngle(1.0f, 0.0f, 0.0f, rpy.pitchDeg * kDeg2Rad);
    const Quat qRoll  = quatFromAxisAngle(0.0f, 1.0f, 0.0f, rpy.rollDeg  * kDeg2Rad);
    return quatNormalize(quatMul(quatMul(qYaw, qPitch), qRoll));
}

// Wrap angle to [-180, 180] range.
inline float wrapDeg(float deg)
{
    deg = std::fmod(std::fmod(deg + 180.0f, 360.0f) + 360.0f, 360.0f) - 180.0f;
    return deg;
}

// Quaternion -> RPY (degrees).
// Decomposition of R = Rz(yaw) * Rx(pitch) * Ry(roll).
// prevRPY used for unwrap continuity and gimbal lock fallback.
//
// Matrix layout (row, col) from the WASD basis vectors in PluginEditor.cpp:
//   Column 0 (right/X):   ( cosY*cosR - sinY*sinP*sinR,  sinY*cosR + cosY*sinP*sinR, -cosP*sinR )
//   Column 1 (forward/Y): (-sinY*cosP,                    cosY*cosP,                    sinP     )
//   Column 2 (up/Z):      ( cosY*sinR + sinY*sinP*cosR,  sinY*sinR - cosY*sinP*cosR,  cosP*cosR )
//
// Extract:
//   sinP  = R[2][1]  (row 2, col 1)
//   yaw   = atan2(-R[0][1], R[1][1])
//   roll  = atan2(-R[2][0], R[2][2])
inline RPY quatToRPY(const Quat& q, const RPY& prevRPY = {})
{
    constexpr float kRad2Deg = 180.0f / 3.14159265358979323846f;

    // Convert quaternion to 3x3 rotation matrix (row-major access).
    // m[row][col]
    const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    const float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    const float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;

    // R[row][col] — derived from q for Rz*Rx*Ry convention:
    //   R = I + 2w*[u]x + 2*[u]x*[u]x  (Rodrigues form)
    // But easier to use the standard quat-to-matrix formula:
    //   R[0][0] = 1-2(yy+zz)   R[0][1] = 2(xy-wz)     R[0][2] = 2(xz+wy)
    //   R[1][0] = 2(xy+wz)     R[1][1] = 1-2(xx+zz)   R[1][2] = 2(yz-wx)
    //   R[2][0] = 2(xz-wy)     R[2][1] = 2(yz+wx)     R[2][2] = 1-2(xx+yy)
    const float r01 = 2.0f * (xy - wz);
    const float r11 = 1.0f - 2.0f * (xx + zz);
    const float r20 = 2.0f * (xz - wy);
    const float r21 = 2.0f * (yz + wx);
    const float r22 = 1.0f - 2.0f * (xx + yy);

    const float sinP = std::clamp(r21, -1.0f, 1.0f);
    float pitchDeg = std::asin(sinP) * kRad2Deg;

    float yawDeg, rollDeg;
    if (std::abs(sinP) < 0.9999f) {
        yawDeg  = std::atan2(-r01, r11) * kRad2Deg;
        rollDeg = std::atan2(-r20, r22) * kRad2Deg;
    } else {
        // Gimbal lock — pitch near ±90°. Roll is indeterminate; keep previous.
        rollDeg = prevRPY.rollDeg;
        // Use remaining elements to extract yaw.
        // R[0][0] = cosY*cosR - sinY*sinP*sinR
        // R[1][0] = sinY*cosR + cosY*sinP*sinR
        // At sinP=±1, cosP=0: R[0][0] ≈ cosY*cosR∓sinY*sinR = cos(Y±R)
        //                      R[1][0] ≈ sinY*cosR±cosY*sinR = sin(Y±R)
        // So atan2(R[1][0], R[0][0]) = yaw ± roll. Subtract prevRoll to isolate yaw.
        const float r00 = 1.0f - 2.0f * (yy + zz);
        const float r10 = 2.0f * (xy + wz);
        const float combined = std::atan2(r10, r00) * kRad2Deg;
        // combined = yaw + sign(sinP)*roll
        yawDeg = combined - (sinP > 0.0f ? 1.0f : -1.0f) * prevRPY.rollDeg;
    }

    yawDeg   = wrapDeg(yawDeg);
    pitchDeg = wrapDeg(pitchDeg);
    rollDeg  = wrapDeg(rollDeg);

    return {yawDeg, pitchDeg, rollDeg};
}

} // namespace xyzpan
