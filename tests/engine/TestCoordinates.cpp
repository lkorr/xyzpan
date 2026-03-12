#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "xyzpan/Coordinates.h"
#include "xyzpan/Constants.h"
#include <cmath>

using namespace xyzpan;
using Catch::Matchers::WithinAbs;

static constexpr float kPi = 3.14159265f;

// ---------------------------------------------------------------------------
// Cardinal directions
// ---------------------------------------------------------------------------
TEST_CASE("Coordinate conversion - cardinal directions", "[coordinates]") {
    SECTION("Front (Y=1): azimuth=0, elevation=0, distance=1") {
        auto s = toSpherical(0.0f, 1.0f, 0.0f);
        REQUIRE_THAT(s.azimuth,   WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(s.elevation, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(s.distance,  WithinAbs(1.0f, 0.001f));
    }

    SECTION("Right (X=1): azimuth=PI/2, elevation=0") {
        auto s = toSpherical(1.0f, 0.0f, 0.0f);
        REQUIRE_THAT(s.azimuth,   WithinAbs(kPi / 2.0f, 0.001f));
        REQUIRE_THAT(s.elevation, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(s.distance,  WithinAbs(1.0f, 0.001f));
    }

    SECTION("Left (X=-1): azimuth=-PI/2, elevation=0") {
        auto s = toSpherical(-1.0f, 0.0f, 0.0f);
        REQUIRE_THAT(s.azimuth,   WithinAbs(-kPi / 2.0f, 0.001f));
        REQUIRE_THAT(s.elevation, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(s.distance,  WithinAbs(1.0f, 0.001f));
    }

    SECTION("Behind (Y=-1): azimuth=PI (or -PI), elevation=0") {
        auto s = toSpherical(0.0f, -1.0f, 0.0f);
        // atan2(0, -1) = PI; both PI and -PI represent the same direction
        REQUIRE_THAT(std::abs(s.azimuth), WithinAbs(kPi, 0.001f));
        REQUIRE_THAT(s.elevation, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(s.distance,  WithinAbs(1.0f, 0.001f));
    }

    SECTION("Above (Z=1): elevation=PI/2") {
        auto s = toSpherical(0.0f, 0.0f, 1.0f);
        REQUIRE_THAT(s.elevation, WithinAbs(kPi / 2.0f, 0.001f));
        REQUIRE_THAT(s.distance,  WithinAbs(1.0f, 0.001f));
    }

    SECTION("Below (Z=-1): elevation=-PI/2") {
        auto s = toSpherical(0.0f, 0.0f, -1.0f);
        REQUIRE_THAT(s.elevation, WithinAbs(-kPi / 2.0f, 0.001f));
        REQUIRE_THAT(s.distance,  WithinAbs(1.0f, 0.001f));
    }
}

// ---------------------------------------------------------------------------
// Origin and minimum distance clamping
// ---------------------------------------------------------------------------
TEST_CASE("Coordinate conversion - origin and minimum distance", "[coordinates]") {
    SECTION("Exact origin (0,0,0) is clamped to kMinDistance") {
        auto s = toSpherical(0.0f, 0.0f, 0.0f);
        REQUIRE(s.distance >= kMinDistance);
        // Convention: at origin, azimuth = 0 (front)
        REQUIRE_THAT(s.azimuth, WithinAbs(0.0f, 0.001f));
    }

    SECTION("Near-origin values are also clamped to kMinDistance") {
        auto s = toSpherical(0.001f, 0.001f, 0.001f);
        REQUIRE(s.distance >= kMinDistance);
    }

    SECTION("Distance is always at least kMinDistance for any input") {
        REQUIRE(computeDistance(0.0f, 0.0f, 0.0f)   >= kMinDistance);
        REQUIRE(computeDistance(0.05f, 0.0f, 0.0f)  >= kMinDistance);
        REQUIRE(computeDistance(-0.05f, 0.0f, 0.0f) >= kMinDistance);
    }
}

// ---------------------------------------------------------------------------
// Boundary clamping (out-of-range inputs)
// ---------------------------------------------------------------------------
TEST_CASE("Coordinate conversion - boundary clamping", "[coordinates]") {
    SECTION("Values above +1.0 are clamped to +1.0 result") {
        auto sOver  = toSpherical(2.0f, 0.0f, 0.0f);
        auto sExact = toSpherical(1.0f, 0.0f, 0.0f);
        REQUIRE_THAT(sOver.azimuth,   WithinAbs(sExact.azimuth,   0.001f));
        REQUIRE_THAT(sOver.distance,  WithinAbs(sExact.distance,  0.001f));
        REQUIRE_THAT(sOver.elevation, WithinAbs(sExact.elevation, 0.001f));
    }

    SECTION("Values below -1.0 are clamped to -1.0 result") {
        auto sOver  = toSpherical(-2.0f, 0.0f, 0.0f);
        auto sExact = toSpherical(-1.0f, 0.0f, 0.0f);
        REQUIRE_THAT(sOver.azimuth,   WithinAbs(sExact.azimuth,   0.001f));
        REQUIRE_THAT(sOver.elevation, WithinAbs(sExact.elevation, 0.001f));
    }

    SECTION("Extreme values on all axes are clamped") {
        auto s = toSpherical(99.0f, 99.0f, 99.0f);
        auto sExpected = toSpherical(1.0f, 1.0f, 1.0f);
        REQUIRE_THAT(s.azimuth,   WithinAbs(sExpected.azimuth,   0.001f));
        REQUIRE_THAT(s.elevation, WithinAbs(sExpected.elevation, 0.001f));
        REQUIRE_THAT(s.distance,  WithinAbs(sExpected.distance,  0.001f));
    }

    SECTION("Y-axis overrange (0,5,0) clamps to same as (0,1,0)") {
        auto sOver  = toSpherical(0.0f, 5.0f, 0.0f);
        auto sExact = toSpherical(0.0f, 1.0f, 0.0f);
        REQUIRE_THAT(sOver.azimuth,   WithinAbs(sExact.azimuth,   0.001f));
        REQUIRE_THAT(sOver.elevation, WithinAbs(sExact.elevation, 0.001f));
        REQUIRE_THAT(sOver.distance,  WithinAbs(sExact.distance,  0.001f));
    }
}

// ---------------------------------------------------------------------------
// Diagonal positions
// ---------------------------------------------------------------------------
TEST_CASE("Coordinate conversion - diagonal positions", "[coordinates]") {
    SECTION("Front-right (X=1, Y=1): azimuth=PI/4, elevation=0") {
        auto s = toSpherical(1.0f, 1.0f, 0.0f);
        REQUIRE_THAT(s.azimuth,   WithinAbs(kPi / 4.0f, 0.001f));
        REQUIRE_THAT(s.elevation, WithinAbs(0.0f, 0.001f));
    }

    SECTION("Front-left (X=-1, Y=1): azimuth=-PI/4, elevation=0") {
        auto s = toSpherical(-1.0f, 1.0f, 0.0f);
        REQUIRE_THAT(s.azimuth,   WithinAbs(-kPi / 4.0f, 0.001f));
        REQUIRE_THAT(s.elevation, WithinAbs(0.0f, 0.001f));
    }

    SECTION("Front-above (Y=1, Z=1): azimuth=0, elevation=PI/4") {
        auto s = toSpherical(0.0f, 1.0f, 1.0f);
        REQUIRE_THAT(s.azimuth,   WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(s.elevation, WithinAbs(kPi / 4.0f, 0.001f));
    }

    SECTION("Right-above (X=1, Z=1): azimuth=PI/2, elevation=PI/4") {
        auto s = toSpherical(1.0f, 0.0f, 1.0f);
        REQUIRE_THAT(s.azimuth,   WithinAbs(kPi / 2.0f, 0.001f));
        REQUIRE_THAT(s.elevation, WithinAbs(kPi / 4.0f, 0.001f));
    }

    SECTION("Full corner (1,1,1): azimuth=PI/4, elevation=atan2(1,sqrt(2))") {
        // atan2(X, Y) = atan2(1, 1) = PI/4
        // elevation = atan2(Z, sqrt(X^2+Y^2)) = atan2(1, sqrt(2))
        auto s = toSpherical(1.0f, 1.0f, 1.0f);
        REQUIRE_THAT(s.azimuth,   WithinAbs(kPi / 4.0f, 0.001f));
        REQUIRE_THAT(s.elevation, WithinAbs(std::atan2(1.0f, std::sqrt(2.0f)), 0.001f));
        REQUIRE_THAT(s.distance,  WithinAbs(std::sqrt(3.0f), 0.001f));
    }
}

// ---------------------------------------------------------------------------
// Distance computation
// ---------------------------------------------------------------------------
TEST_CASE("Distance computation", "[coordinates]") {
    SECTION("computeDistance is sample-rate independent (pure math, no state)") {
        float d1 = computeDistance(0.5f, 0.5f, 0.5f);
        float d2 = computeDistance(0.5f, 0.5f, 0.5f);
        REQUIRE_THAT(d1, WithinAbs(d2, 1e-6f));
        REQUIRE(d1 >= kMinDistance);
    }

    SECTION("Unit sphere corner (1,1,1) gives sqrt(3)") {
        float d = computeDistance(1.0f, 1.0f, 1.0f);
        REQUIRE_THAT(d, WithinAbs(std::sqrt(3.0f), 0.001f));
    }

    SECTION("Unit axis (1,0,0) gives distance=1") {
        REQUIRE_THAT(computeDistance(1.0f, 0.0f, 0.0f), WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(computeDistance(0.0f, 1.0f, 0.0f), WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(computeDistance(0.0f, 0.0f, 1.0f), WithinAbs(1.0f, 0.001f));
    }

    SECTION("computeDistance clamps out-of-range inputs") {
        float dOver  = computeDistance(2.0f, 0.0f, 0.0f);
        float dExact = computeDistance(1.0f, 0.0f, 0.0f);
        REQUIRE_THAT(dOver, WithinAbs(dExact, 0.001f));
    }
}
