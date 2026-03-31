// TestOrientationMath.cpp — Regression tests for mouse-drag Jacobian and WASD
// basis vectors at cardinal roll angles (0, 90, 180, -90).
// Pure math — no JUCE or Engine dependency.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <algorithm>

static constexpr float kPi     = 3.14159265358979323846f;
static constexpr float kDeg2Rad = kPi / 180.0f;

// ---------------------------------------------------------------------------
// Extracted from XYZPanGLView.cpp:955-966 / Camera.cpp:114-123
// Raw Jacobian inverse: screen delta -> Euler increments (before sensitivity).
// ---------------------------------------------------------------------------
struct JacobianResult { float dYaw; float dPitch; };

static JacobianResult computeMouseJacobian(float rollDeg, float pitchDeg,
                                           float dx, float dy)
{
    const float rollRad  = rollDeg  * kDeg2Rad;
    const float pitchRad = pitchDeg * kDeg2Rad;
    const float cosR = std::cos(rollRad);
    const float sinR = std::sin(rollRad);
    const float rawCosP = std::cos(pitchRad);
    const float cosP = std::max(std::abs(rawCosP), 0.1f)
                     * (rawCosP >= 0.0f ? 1.0f : -1.0f);

    return { (dx * cosR - dy * sinR) / cosP,
              dx * sinR + dy * cosR };
}

// ---------------------------------------------------------------------------
// Extracted from PluginEditor.cpp:1915-1943
// R = Rz(yaw) * Rx(pitch) * Ry(roll)
// Returns world-space position delta (before speed scaling).
// ---------------------------------------------------------------------------
struct WASDDelta { float dx; float dy; float dz; };

static WASDDelta computeWASDDelta(float yawRad, float pitchRad, float rollRad,
                                  float fwd, float strafe, float vert)
{
    const float cosY = std::cos(yawRad);
    const float sinY = std::sin(yawRad);
    const float cosP = std::cos(pitchRad);
    const float sinP = std::sin(pitchRad);
    const float cosR = std::cos(rollRad);
    const float sinR = std::sin(rollRad);

    // Forward (Y-axis column — roll doesn't change gaze direction)
    const float fwdX = -sinY * cosP;
    const float fwdY =  cosY * cosP;
    const float fwdZ =  sinP;

    // Right (X-axis column)
    const float rightX =  cosY * cosR - sinY * sinP * sinR;
    const float rightY =  sinY * cosR + cosY * sinP * sinR;
    const float rightZ = -cosP * sinR;

    // Up (Z-axis column)
    const float upX = cosY * sinR + sinY * sinP * cosR;
    const float upY = sinY * sinR - cosY * sinP * cosR;
    const float upZ = cosP * cosR;

    return { fwd * fwdX + strafe * rightX + vert * upX,
             fwd * fwdY + strafe * rightY + vert * upY,
             fwd * fwdZ + strafe * rightZ + vert * upZ };
}

// ===== Mouse Jacobian tests =================================================
// Tests the raw dYaw/dPitch output at pitch=0 (cosP=1).
// Application convention (not tested here):
//   Head-follows: newYaw = oldYaw - dYaw*0.4   (XYZPanGLView.cpp:968)
//   Camera orbit: yaw   = yaw   + dYaw*0.4     (Camera.cpp:125)

TEST_CASE("Mouse Jacobian at cardinal rolls", "[orientation][jacobian]")
{
    constexpr float kEps = 1e-4f;

    SECTION("Roll=0, horizontal drag") {
        auto r = computeMouseJacobian(0.0f, 0.0f, 10.0f, 0.0f);
        CHECK(r.dYaw   == Catch::Approx(10.0f).margin(kEps));
        CHECK(r.dPitch == Catch::Approx(0.0f).margin(kEps));
    }
    SECTION("Roll=0, vertical drag") {
        auto r = computeMouseJacobian(0.0f, 0.0f, 0.0f, 10.0f);
        CHECK(r.dYaw   == Catch::Approx(0.0f).margin(kEps));
        CHECK(r.dPitch == Catch::Approx(10.0f).margin(kEps));
    }

    SECTION("Roll=90, horizontal drag") {
        auto r = computeMouseJacobian(90.0f, 0.0f, 10.0f, 0.0f);
        CHECK(r.dYaw   == Catch::Approx(0.0f).margin(kEps));
        CHECK(r.dPitch == Catch::Approx(10.0f).margin(kEps));
    }
    SECTION("Roll=90, vertical drag") {
        auto r = computeMouseJacobian(90.0f, 0.0f, 0.0f, 10.0f);
        CHECK(r.dYaw   == Catch::Approx(-10.0f).margin(kEps));
        CHECK(r.dPitch == Catch::Approx(0.0f).margin(kEps));
    }

    SECTION("Roll=180, horizontal drag") {
        auto r = computeMouseJacobian(180.0f, 0.0f, 10.0f, 0.0f);
        CHECK(r.dYaw   == Catch::Approx(-10.0f).margin(kEps));
        CHECK(r.dPitch == Catch::Approx(0.0f).margin(kEps));
    }
    SECTION("Roll=180, vertical drag") {
        auto r = computeMouseJacobian(180.0f, 0.0f, 0.0f, 10.0f);
        CHECK(r.dYaw   == Catch::Approx(0.0f).margin(kEps));
        CHECK(r.dPitch == Catch::Approx(-10.0f).margin(kEps));
    }

    SECTION("Roll=-90, horizontal drag") {
        auto r = computeMouseJacobian(-90.0f, 0.0f, 10.0f, 0.0f);
        CHECK(r.dYaw   == Catch::Approx(0.0f).margin(kEps));
        CHECK(r.dPitch == Catch::Approx(-10.0f).margin(kEps));
    }
    SECTION("Roll=-90, vertical drag") {
        auto r = computeMouseJacobian(-90.0f, 0.0f, 0.0f, 10.0f);
        CHECK(r.dYaw   == Catch::Approx(10.0f).margin(kEps));
        CHECK(r.dPitch == Catch::Approx(0.0f).margin(kEps));
    }
}

// ===== WASD basis vector tests ===============================================
// All at yaw=0, pitch=0. Tests world-space delta for each input axis.
// Engine coords: X=right, Y=forward, Z=up.

TEST_CASE("WASD basis vectors at cardinal rolls", "[orientation][wasd]")
{
    constexpr float kEps = 1e-4f;

    SECTION("Roll=0: W->+Y, D->+X, Space->+Z") {
        const float roll = 0.0f * kDeg2Rad;
        auto w = computeWASDDelta(0, 0, roll, 1, 0, 0);
        CHECK(w.dx == Catch::Approx(0.0f).margin(kEps));
        CHECK(w.dy == Catch::Approx(1.0f).margin(kEps));
        CHECK(w.dz == Catch::Approx(0.0f).margin(kEps));

        auto d = computeWASDDelta(0, 0, roll, 0, 1, 0);
        CHECK(d.dx == Catch::Approx(1.0f).margin(kEps));
        CHECK(d.dy == Catch::Approx(0.0f).margin(kEps));
        CHECK(d.dz == Catch::Approx(0.0f).margin(kEps));

        auto sp = computeWASDDelta(0, 0, roll, 0, 0, 1);
        CHECK(sp.dx == Catch::Approx(0.0f).margin(kEps));
        CHECK(sp.dy == Catch::Approx(0.0f).margin(kEps));
        CHECK(sp.dz == Catch::Approx(1.0f).margin(kEps));
    }

    SECTION("Roll=90: W->+Y, D->-Z, Space->+X") {
        const float roll = 90.0f * kDeg2Rad;
        auto w = computeWASDDelta(0, 0, roll, 1, 0, 0);
        CHECK(w.dx == Catch::Approx(0.0f).margin(kEps));
        CHECK(w.dy == Catch::Approx(1.0f).margin(kEps));
        CHECK(w.dz == Catch::Approx(0.0f).margin(kEps));

        auto d = computeWASDDelta(0, 0, roll, 0, 1, 0);
        CHECK(d.dx == Catch::Approx(0.0f).margin(kEps));
        CHECK(d.dy == Catch::Approx(0.0f).margin(kEps));
        CHECK(d.dz == Catch::Approx(-1.0f).margin(kEps));

        auto sp = computeWASDDelta(0, 0, roll, 0, 0, 1);
        CHECK(sp.dx == Catch::Approx(1.0f).margin(kEps));
        CHECK(sp.dy == Catch::Approx(0.0f).margin(kEps));
        CHECK(sp.dz == Catch::Approx(0.0f).margin(kEps));
    }

    SECTION("Roll=180: W->+Y, D->-X, Space->-Z") {
        const float roll = 180.0f * kDeg2Rad;
        auto w = computeWASDDelta(0, 0, roll, 1, 0, 0);
        CHECK(w.dx == Catch::Approx(0.0f).margin(kEps));
        CHECK(w.dy == Catch::Approx(1.0f).margin(kEps));
        CHECK(w.dz == Catch::Approx(0.0f).margin(kEps));

        auto d = computeWASDDelta(0, 0, roll, 0, 1, 0);
        CHECK(d.dx == Catch::Approx(-1.0f).margin(kEps));
        CHECK(d.dy == Catch::Approx(0.0f).margin(kEps));
        CHECK(d.dz == Catch::Approx(0.0f).margin(kEps));

        auto sp = computeWASDDelta(0, 0, roll, 0, 0, 1);
        CHECK(sp.dx == Catch::Approx(0.0f).margin(kEps));
        CHECK(sp.dy == Catch::Approx(0.0f).margin(kEps));
        CHECK(sp.dz == Catch::Approx(-1.0f).margin(kEps));
    }

    SECTION("Roll=-90: W->+Y, D->+Z, Space->-X") {
        const float roll = -90.0f * kDeg2Rad;
        auto w = computeWASDDelta(0, 0, roll, 1, 0, 0);
        CHECK(w.dx == Catch::Approx(0.0f).margin(kEps));
        CHECK(w.dy == Catch::Approx(1.0f).margin(kEps));
        CHECK(w.dz == Catch::Approx(0.0f).margin(kEps));

        auto d = computeWASDDelta(0, 0, roll, 0, 1, 0);
        CHECK(d.dx == Catch::Approx(0.0f).margin(kEps));
        CHECK(d.dy == Catch::Approx(0.0f).margin(kEps));
        CHECK(d.dz == Catch::Approx(1.0f).margin(kEps));

        auto sp = computeWASDDelta(0, 0, roll, 0, 0, 1);
        CHECK(sp.dx == Catch::Approx(-1.0f).margin(kEps));
        CHECK(sp.dy == Catch::Approx(0.0f).margin(kEps));
        CHECK(sp.dz == Catch::Approx(0.0f).margin(kEps));
    }
}

// ===== Forward-vector invariant ==============================================
// Roll must never change the gaze (forward) direction.
// Forward = (-sinY*cosP, cosY*cosP, sinP) — no roll terms.

TEST_CASE("Forward vector is unaffected by roll", "[orientation][invariant]")
{
    constexpr float kEps = 1e-4f;

    SECTION("yaw=0, pitch=0: forward is (0,1,0) for all rolls") {
        for (float rollDeg : {0.0f, 30.0f, 45.0f, 60.0f, 90.0f,
                              120.0f, 135.0f, 150.0f, 180.0f,
                              -30.0f, -45.0f, -60.0f, -90.0f,
                              -120.0f, -135.0f, -150.0f, -180.0f}) {
            INFO("roll = " << rollDeg);
            auto r = computeWASDDelta(0, 0, rollDeg * kDeg2Rad, 1, 0, 0);
            CHECK(r.dx == Catch::Approx(0.0f).margin(kEps));
            CHECK(r.dy == Catch::Approx(1.0f).margin(kEps));
            CHECK(r.dz == Catch::Approx(0.0f).margin(kEps));
        }
    }

    SECTION("yaw=45, pitch=30: forward is constant across all rolls") {
        const float yaw   = 45.0f * kDeg2Rad;
        const float pitch = 30.0f * kDeg2Rad;
        const float expectX = -std::sin(yaw) * std::cos(pitch);
        const float expectY =  std::cos(yaw) * std::cos(pitch);
        const float expectZ =  std::sin(pitch);

        for (float rollDeg : {0.0f, 30.0f, 45.0f, 60.0f, 90.0f,
                              120.0f, 135.0f, 150.0f, 180.0f,
                              -30.0f, -45.0f, -60.0f, -90.0f,
                              -120.0f, -135.0f, -150.0f, -180.0f}) {
            INFO("roll = " << rollDeg);
            auto r = computeWASDDelta(yaw, pitch, rollDeg * kDeg2Rad, 1, 0, 0);
            CHECK(r.dx == Catch::Approx(expectX).margin(kEps));
            CHECK(r.dy == Catch::Approx(expectY).margin(kEps));
            CHECK(r.dz == Catch::Approx(expectZ).margin(kEps));
        }
    }
}
