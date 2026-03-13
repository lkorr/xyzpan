#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "PositionBridge.h"

using namespace xyzpan;

TEST_CASE("PositionBridge default read returns front-center snapshot", "[bridge][UI-07]") {
    PositionBridge bridge;
    auto snap = bridge.read();
    REQUIRE(snap.x        == Catch::Approx(0.0f));
    REQUIRE(snap.y        == Catch::Approx(1.0f));
    REQUIRE(snap.z        == Catch::Approx(0.0f));
    REQUIRE(snap.distance == Catch::Approx(1.0f));
}

TEST_CASE("PositionBridge write/read round-trip", "[bridge][UI-07]") {
    PositionBridge bridge;

    SECTION("Written values are readable") {
        SourcePositionSnapshot s;
        s.x = -0.5f; s.y = 0.7f; s.z = 0.3f; s.distance = 0.85f;
        bridge.write(s);
        auto result = bridge.read();
        REQUIRE(result.x        == Catch::Approx(-0.5f));
        REQUIRE(result.y        == Catch::Approx(0.7f));
        REQUIRE(result.z        == Catch::Approx(0.3f));
        REQUIRE(result.distance == Catch::Approx(0.85f));
    }

    SECTION("Multiple writes: last write wins") {
        SourcePositionSnapshot a, b;
        a.x = 0.1f; b.x = 0.9f;
        bridge.write(a);
        bridge.write(b);
        REQUIRE(bridge.read().x == Catch::Approx(0.9f));
    }
}
