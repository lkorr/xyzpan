// TestQuatAccumulator.cpp — Tests for QuatMath.h and ListenerQuatAccumulator.h.
// Pure math — no JUCE or Engine dependency.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "QuatMath.h"
#include "ListenerQuatAccumulator.h"
#include <cmath>

using namespace xyzpan;
using Catch::Approx;

static constexpr float kPi     = 3.14159265358979323846f;
static constexpr float kDeg2Rad = kPi / 180.0f;

// ===== QuatMath basic operations =============================================

TEST_CASE("quatMul with identity", "[quatmath]")
{
    const Quat id{1, 0, 0, 0};
    const Quat q{0.707107f, 0.707107f, 0.0f, 0.0f};  // 90 deg around X

    auto r = quatMul(q, id);
    CHECK(r.w == Approx(q.w).margin(1e-5f));
    CHECK(r.x == Approx(q.x).margin(1e-5f));
    CHECK(r.y == Approx(q.y).margin(1e-5f));
    CHECK(r.z == Approx(q.z).margin(1e-5f));

    r = quatMul(id, q);
    CHECK(r.w == Approx(q.w).margin(1e-5f));
    CHECK(r.x == Approx(q.x).margin(1e-5f));
    CHECK(r.y == Approx(q.y).margin(1e-5f));
    CHECK(r.z == Approx(q.z).margin(1e-5f));
}

TEST_CASE("quatNormalize", "[quatmath]")
{
    Quat q{2, 0, 0, 0};
    auto n = quatNormalize(q);
    CHECK(n.w == Approx(1.0f).margin(1e-6f));
    CHECK(n.x == Approx(0.0f).margin(1e-6f));

    q = {1, 1, 1, 1};
    n = quatNormalize(q);
    float mag = std::sqrt(n.w*n.w + n.x*n.x + n.y*n.y + n.z*n.z);
    CHECK(mag == Approx(1.0f).margin(1e-6f));
}

TEST_CASE("quatFromAxisAngle", "[quatmath]")
{
    // 0 angle -> identity
    auto q = quatFromAxisAngle(1, 0, 0, 0.0f);
    CHECK(q.w == Approx(1.0f).margin(1e-6f));
    CHECK(q.x == Approx(0.0f).margin(1e-6f));

    // 90 deg around Z
    q = quatFromAxisAngle(0, 0, 1, kPi * 0.5f);
    CHECK(q.w == Approx(std::cos(kPi / 4.0f)).margin(1e-5f));
    CHECK(q.z == Approx(std::sin(kPi / 4.0f)).margin(1e-5f));
}

TEST_CASE("quatRotateVec", "[quatmath]")
{
    // 90 deg yaw around Z: X-right -> Y-forward
    auto q = quatFromAxisAngle(0, 0, 1, kPi * 0.5f);
    auto v = quatRotateVec(q, 1, 0, 0);
    CHECK(v.x == Approx(0.0f).margin(1e-5f));
    CHECK(v.y == Approx(1.0f).margin(1e-5f));
    CHECK(v.z == Approx(0.0f).margin(1e-5f));

    // Z axis should be unchanged by yaw
    v = quatRotateVec(q, 0, 0, 1);
    CHECK(v.x == Approx(0.0f).margin(1e-5f));
    CHECK(v.y == Approx(0.0f).margin(1e-5f));
    CHECK(v.z == Approx(1.0f).margin(1e-5f));
}

// ===== rpyToQuat / quatToRPY =================================================

TEST_CASE("rpyToQuat identity", "[quatmath]")
{
    auto q = rpyToQuat({0, 0, 0});
    CHECK(q.w == Approx(1.0f).margin(1e-6f));
    CHECK(q.x == Approx(0.0f).margin(1e-6f));
    CHECK(q.y == Approx(0.0f).margin(1e-6f));
    CHECK(q.z == Approx(0.0f).margin(1e-6f));
}

TEST_CASE("rpyToQuat 90-degree cardinals", "[quatmath]")
{
    // Yaw 90 around Z
    auto q = rpyToQuat({90, 0, 0});
    auto expected = quatFromAxisAngle(0, 0, 1, 90.0f * kDeg2Rad);
    CHECK(q.w == Approx(expected.w).margin(1e-5f));
    CHECK(q.x == Approx(expected.x).margin(1e-5f));
    CHECK(q.y == Approx(expected.y).margin(1e-5f));
    CHECK(q.z == Approx(expected.z).margin(1e-5f));

    // Pitch 90 around X
    q = rpyToQuat({0, 90, 0});
    expected = quatFromAxisAngle(1, 0, 0, 90.0f * kDeg2Rad);
    CHECK(q.w == Approx(expected.w).margin(1e-5f));
    CHECK(q.x == Approx(expected.x).margin(1e-5f));
    CHECK(q.y == Approx(expected.y).margin(1e-5f));
    CHECK(q.z == Approx(expected.z).margin(1e-5f));

    // Roll 90 around Y
    q = rpyToQuat({0, 0, 90});
    expected = quatFromAxisAngle(0, 1, 0, 90.0f * kDeg2Rad);
    CHECK(q.w == Approx(expected.w).margin(1e-5f));
    CHECK(q.x == Approx(expected.x).margin(1e-5f));
    CHECK(q.y == Approx(expected.y).margin(1e-5f));
    CHECK(q.z == Approx(expected.z).margin(1e-5f));
}

TEST_CASE("rpyToQuat/quatToRPY round-trip", "[quatmath]")
{
    constexpr float kEps = 0.01f;  // degrees

    SECTION("Identity") {
        RPY rpy{0, 0, 0};
        auto q = rpyToQuat(rpy);
        auto out = quatToRPY(q, rpy);
        CHECK(out.yawDeg   == Approx(0.0f).margin(kEps));
        CHECK(out.pitchDeg == Approx(0.0f).margin(kEps));
        CHECK(out.rollDeg  == Approx(0.0f).margin(kEps));
    }

    SECTION("Arbitrary angles") {
        RPY rpy{30.0f, -45.0f, 60.0f};
        auto q = rpyToQuat(rpy);
        auto out = quatToRPY(q, rpy);
        CHECK(out.yawDeg   == Approx(30.0f).margin(kEps));
        CHECK(out.pitchDeg == Approx(-45.0f).margin(kEps));
        CHECK(out.rollDeg  == Approx(60.0f).margin(kEps));
    }

    SECTION("Negative angles") {
        RPY rpy{-120.0f, 45.0f, -30.0f};
        auto q = rpyToQuat(rpy);
        auto out = quatToRPY(q, rpy);
        CHECK(out.yawDeg   == Approx(-120.0f).margin(kEps));
        CHECK(out.pitchDeg == Approx(45.0f).margin(kEps));
        CHECK(out.rollDeg  == Approx(-30.0f).margin(kEps));
    }

    SECTION("All 180") {
        RPY rpy{180.0f, 0.0f, 180.0f};
        auto q = rpyToQuat(rpy);
        auto out = quatToRPY(q, rpy);
        // ±180 are the same rotation; atan2 may return either sign
        CHECK(std::abs(out.yawDeg)   == Approx(180.0f).margin(kEps));
        CHECK(out.pitchDeg == Approx(0.0f).margin(kEps));
        CHECK(std::abs(out.rollDeg)  == Approx(180.0f).margin(kEps));
    }
}

// ===== Decomposition cross-check against WASD rotation matrix ================
// Verify that rpyToQuat produces the same rotation matrix as the explicit
// WASD code in PluginEditor.cpp:1992-2009.

TEST_CASE("rpyToQuat matches WASD rotation matrix", "[quatmath]")
{
    constexpr float kEps = 1e-4f;

    auto testCase = [&](float yawDeg, float pitchDeg, float rollDeg) {
        INFO("yaw=" << yawDeg << " pitch=" << pitchDeg << " roll=" << rollDeg);

        const float cosY = std::cos(yawDeg * kDeg2Rad), sinY = std::sin(yawDeg * kDeg2Rad);
        const float cosP = std::cos(pitchDeg * kDeg2Rad), sinP = std::sin(pitchDeg * kDeg2Rad);
        const float cosR = std::cos(rollDeg * kDeg2Rad), sinR = std::sin(rollDeg * kDeg2Rad);

        // Forward (Y column) from WASD code
        const float fwdX = -sinY * cosP;
        const float fwdY =  cosY * cosP;
        const float fwdZ =  sinP;
        // Right (X column)
        const float rightX =  cosY * cosR - sinY * sinP * sinR;
        const float rightY =  sinY * cosR + cosY * sinP * sinR;
        const float rightZ = -cosP * sinR;
        // Up (Z column)
        const float upX = cosY * sinR + sinY * sinP * cosR;
        const float upY = sinY * sinR - cosY * sinP * cosR;
        const float upZ = cosP * cosR;

        auto q = rpyToQuat({yawDeg, pitchDeg, rollDeg});

        // Quat-rotated basis vectors should match
        auto qRight = quatRotateVec(q, 1, 0, 0);
        CHECK(qRight.x == Approx(rightX).margin(kEps));
        CHECK(qRight.y == Approx(rightY).margin(kEps));
        CHECK(qRight.z == Approx(rightZ).margin(kEps));

        auto qFwd = quatRotateVec(q, 0, 1, 0);
        CHECK(qFwd.x == Approx(fwdX).margin(kEps));
        CHECK(qFwd.y == Approx(fwdY).margin(kEps));
        CHECK(qFwd.z == Approx(fwdZ).margin(kEps));

        auto qUp = quatRotateVec(q, 0, 0, 1);
        CHECK(qUp.x == Approx(upX).margin(kEps));
        CHECK(qUp.y == Approx(upY).margin(kEps));
        CHECK(qUp.z == Approx(upZ).margin(kEps));
    };

    testCase(0, 0, 0);
    testCase(90, 0, 0);
    testCase(0, 90, 0);
    testCase(0, 0, 90);
    testCase(45, 30, -60);
    testCase(-120, 45, 135);
    testCase(180, 0, 180);
}

// ===== Loop-de-loop tests ====================================================

TEST_CASE("Loop-de-loop at roll=0", "[accumulator]")
{
    ListenerQuatAccumulator accum;
    accum.syncFromRPY(0, 0, 0);

    // 360 steps of 1-degree pitch (mouse dy)
    constexpr float kSensitivity = 1.0f;  // 1 radian per unit for easy math
    const float stepRad = 1.0f * kDeg2Rad;

    for (int i = 0; i < 360; ++i) {
        accum.applyMouseDelta(0.0f, -stepRad / kSensitivity, kSensitivity);
    }

    auto rpy = accum.bakeRPY();
    // Should be back near identity (full loop)
    CHECK(std::fmod(std::fmod(rpy.yawDeg + 180.0f, 360.0f) + 360.0f, 360.0f) - 180.0f
          == Approx(0.0f).margin(1.0f));
    CHECK(std::fmod(std::fmod(rpy.pitchDeg + 180.0f, 360.0f) + 360.0f, 360.0f) - 180.0f
          == Approx(0.0f).margin(1.0f));
    CHECK(std::fmod(std::fmod(rpy.rollDeg + 180.0f, 360.0f) + 360.0f, 360.0f) - 180.0f
          == Approx(0.0f).margin(1.0f));
}

TEST_CASE("Loop-de-loop at roll=45", "[accumulator]")
{
    ListenerQuatAccumulator accum;
    accum.syncFromRPY(0, 0, 45);

    constexpr float kSensitivity = 1.0f;
    const float stepRad = 1.0f * kDeg2Rad;

    for (int i = 0; i < 360; ++i) {
        accum.applyMouseDelta(0.0f, -stepRad / kSensitivity, kSensitivity);
    }

    auto rpy = accum.bakeRPY();
    // Should return to approximately (0, 0, 45)
    CHECK(std::fmod(std::fmod(rpy.yawDeg + 180.0f, 360.0f) + 360.0f, 360.0f) - 180.0f
          == Approx(0.0f).margin(1.0f));
    CHECK(std::fmod(std::fmod(rpy.pitchDeg + 180.0f, 360.0f) + 360.0f, 360.0f) - 180.0f
          == Approx(0.0f).margin(1.0f));
    CHECK(std::fmod(std::fmod(rpy.rollDeg + 180.0f, 360.0f) + 360.0f, 360.0f) - 180.0f
          == Approx(45.0f).margin(1.0f));
}

// ===== Roll accumulation =====================================================

TEST_CASE("Full roll cycle returns to start", "[accumulator]")
{
    ListenerQuatAccumulator accum;
    accum.syncFromRPY(30, 15, 0);

    // Apply roll in 2.8-degree increments for a full 360
    const int steps = static_cast<int>(std::round(360.0f / 2.8f));
    for (int i = 0; i < steps; ++i) {
        accum.applyRollDelta(2.8f);
    }

    // Residual from non-integer division
    float residual = 360.0f - steps * 2.8f;
    if (std::abs(residual) > 0.001f)
        accum.applyRollDelta(residual);

    auto rpy = accum.bakeRPY();
    CHECK(std::fmod(std::fmod(rpy.yawDeg + 180.0f, 360.0f) + 360.0f, 360.0f) - 180.0f
          == Approx(30.0f).margin(1.0f));
    CHECK(std::fmod(std::fmod(rpy.pitchDeg + 180.0f, 360.0f) + 360.0f, 360.0f) - 180.0f
          == Approx(15.0f).margin(1.0f));
    CHECK(std::fmod(std::fmod(rpy.rollDeg + 180.0f, 360.0f) + 360.0f, 360.0f) - 180.0f
          == Approx(0.0f).margin(1.0f));
}

// ===== Path B sync ===========================================================

TEST_CASE("syncFromRPY overrides accumulator state", "[accumulator]")
{
    ListenerQuatAccumulator accum;
    accum.applyMouseDelta(10.0f, 5.0f, 0.01f);  // move somewhere

    accum.syncFromRPY(45.0f, -30.0f, 10.0f);
    auto rpy = accum.bakeRPY();
    CHECK(rpy.yawDeg   == Approx(45.0f).margin(0.01f));
    CHECK(rpy.pitchDeg == Approx(-30.0f).margin(0.01f));
    CHECK(rpy.rollDeg  == Approx(10.0f).margin(0.01f));
}

// ===== Gimbal lock ===========================================================

TEST_CASE("Gimbal lock preserves previous roll", "[quatmath]")
{
    RPY prev{0, 0, 45};
    auto q = rpyToQuat({0, 90, 45});
    auto out = quatToRPY(q, prev);
    CHECK(out.pitchDeg == Approx(90.0f).margin(0.1f));
    CHECK(out.rollDeg  == Approx(45.0f).margin(0.1f));
}

// ===== wrapDeg ===============================================================

TEST_CASE("wrapDeg keeps values in [-180, 180]", "[quatmath]")
{
    CHECK(wrapDeg(0.0f)    == Approx(0.0f).margin(0.01f));
    CHECK(wrapDeg(179.0f)  == Approx(179.0f).margin(0.01f));
    CHECK(wrapDeg(-179.0f) == Approx(-179.0f).margin(0.01f));
    CHECK(wrapDeg(181.0f)  == Approx(-179.0f).margin(0.01f));
    CHECK(wrapDeg(-181.0f) == Approx(179.0f).margin(0.01f));
    CHECK(wrapDeg(360.0f)  == Approx(0.0f).margin(0.01f));
    CHECK(wrapDeg(540.0f)  == Approx(-180.0f).margin(0.01f));
}

// ===== Mouse wrapping — RPY stays in [-180, 180] ============================

TEST_CASE("Yaw wraps through +180/-180 via mouse", "[accumulator]")
{
    ListenerQuatAccumulator accum;
    accum.syncFromRPY(170, 0, 0);

    constexpr float kSensitivity = 1.0f;
    const float stepRad = 1.0f * kDeg2Rad;

    // Drag left (positive yaw) past +180
    for (int i = 0; i < 20; ++i) {
        accum.applyMouseDelta(-stepRad / kSensitivity, 0.0f, kSensitivity);
        auto rpy = accum.bakeRPY();
        CHECK(rpy.yawDeg >= -180.0f);
        CHECK(rpy.yawDeg <= 180.0f);
    }

    // After 20 degrees past 170, should have wrapped to negative side
    auto rpy = accum.bakeRPY();
    CHECK(rpy.yawDeg < 0.0f);  // wrapped past +180 to negative
    CHECK(rpy.yawDeg == Approx(-170.0f).margin(2.0f));
}

TEST_CASE("Yaw wraps through -180/+180 via mouse", "[accumulator]")
{
    ListenerQuatAccumulator accum;
    accum.syncFromRPY(-170, 0, 0);

    constexpr float kSensitivity = 1.0f;
    const float stepRad = 1.0f * kDeg2Rad;

    // Drag right (negative yaw) past -180
    for (int i = 0; i < 20; ++i) {
        accum.applyMouseDelta(stepRad / kSensitivity, 0.0f, kSensitivity);
        auto rpy = accum.bakeRPY();
        CHECK(rpy.yawDeg >= -180.0f);
        CHECK(rpy.yawDeg <= 180.0f);
    }

    auto rpy = accum.bakeRPY();
    CHECK(rpy.yawDeg > 0.0f);  // wrapped past -180 to positive
    CHECK(rpy.yawDeg == Approx(170.0f).margin(2.0f));
}

TEST_CASE("Pitch wraps through +180/-180 via mouse at roll=0", "[accumulator]")
{
    ListenerQuatAccumulator accum;
    accum.syncFromRPY(0, 80, 0);

    constexpr float kSensitivity = 1.0f;
    const float stepRad = 1.0f * kDeg2Rad;

    // Continuous upward pitch past 90 and onward — RPY decomposition will
    // flip yaw/roll at the gimbal boundary but values must stay in range.
    for (int i = 0; i < 200; ++i) {
        accum.applyMouseDelta(0.0f, -stepRad / kSensitivity, kSensitivity);
        auto rpy = accum.bakeRPY();
        CHECK(rpy.yawDeg   >= -180.0f);
        CHECK(rpy.yawDeg   <= 180.0f);
        CHECK(rpy.pitchDeg >= -180.0f);
        CHECK(rpy.pitchDeg <= 180.0f);
        CHECK(rpy.rollDeg  >= -180.0f);
        CHECK(rpy.rollDeg  <= 180.0f);
    }
}

TEST_CASE("Roll wraps through +180/-180 via Q/E", "[accumulator]")
{
    ListenerQuatAccumulator accum;
    accum.syncFromRPY(0, 0, 170);

    // Roll past +180
    for (int i = 0; i < 10; ++i) {
        accum.applyRollDelta(2.8f);
        auto rpy = accum.bakeRPY();
        CHECK(rpy.rollDeg >= -180.0f);
        CHECK(rpy.rollDeg <= 180.0f);
    }

    // Should have wrapped to negative side (170 + 28 = 198 -> wraps to ~-162)
    auto rpy = accum.bakeRPY();
    CHECK(rpy.rollDeg < 0.0f);
}

TEST_CASE("All RPY stay in range during combined mouse movement", "[accumulator]")
{
    ListenerQuatAccumulator accum;
    accum.syncFromRPY(0, 0, 45);

    constexpr float kSensitivity = 1.0f;
    const float stepRad = 2.0f * kDeg2Rad;

    // Diagonal drag at roll=45 for a large number of steps
    for (int i = 0; i < 500; ++i) {
        accum.applyMouseDelta(stepRad / kSensitivity,
                              -stepRad / kSensitivity,
                              kSensitivity);
        auto rpy = accum.bakeRPY();
        INFO("step " << i);
        CHECK(rpy.yawDeg   >= -180.0f);
        CHECK(rpy.yawDeg   <= 180.0f);
        CHECK(rpy.pitchDeg >= -180.0f);
        CHECK(rpy.pitchDeg <= 180.0f);
        CHECK(rpy.rollDeg  >= -180.0f);
        CHECK(rpy.rollDeg  <= 180.0f);
    }
}

// ===== Quaternion continuity through full pitch loop =========================

TEST_CASE("Quaternion is continuous through full pitch loop", "[accumulator]")
{
    ListenerQuatAccumulator accum;
    accum.syncFromRPY(0, 0, 0);

    constexpr float kSensitivity = 1.0f;
    const float stepRad = 1.0f * kDeg2Rad;
    Quat prevQ = accum.currentQuat();
    bool hadDiscontinuity = false;

    for (int i = 0; i < 360; ++i) {
        accum.applyMouseDelta(0.0f, -stepRad / kSensitivity, kSensitivity);
        Quat q = accum.currentQuat();
        // Quaternion dot product should be close to 1 for small steps
        float dot = prevQ.w * q.w + prevQ.x * q.x + prevQ.y * q.y + prevQ.z * q.z;
        if (std::abs(dot) < 0.99f) hadDiscontinuity = true;
        prevQ = q;
    }

    CHECK_FALSE(hadDiscontinuity);
}
