#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "xyzpan/dsp/SineLUT.h"
#include <cmath>

using namespace xyzpan::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("SineLUT accuracy within 0.001 across full range", "[SineLUT]") {
    constexpr int kSteps = 10000;
    float maxError = 0.0f;
    for (int i = 0; i < kSteps; ++i) {
        const float phase = static_cast<float>(i) / kSteps;
        const float lut = SineLUT::lookup(phase);
        const float ref = std::sin(phase * 6.28318530f);
        const float err = std::abs(lut - ref);
        if (err > maxError) maxError = err;
    }
    INFO("Max SineLUT error: " << maxError);
    CHECK(maxError < 0.001f);
}

TEST_CASE("SineLUT cosLookup matches std::cos within 0.001", "[SineLUT]") {
    constexpr int kSteps = 10000;
    float maxError = 0.0f;
    for (int i = 0; i < kSteps; ++i) {
        const float phase = static_cast<float>(i) / kSteps;
        const float lut = SineLUT::cosLookup(phase);
        const float ref = std::cos(phase * 6.28318530f);
        const float err = std::abs(lut - ref);
        if (err > maxError) maxError = err;
    }
    INFO("Max cosLookup error: " << maxError);
    CHECK(maxError < 0.001f);
}

TEST_CASE("SineLUT boundary wrapping correct at 0 and 1", "[SineLUT]") {
    // Phase 0 = sin(0) = 0
    CHECK_THAT(SineLUT::lookup(0.0f), WithinAbs(0.0f, 0.001f));
    // Phase 0.25 = sin(PI/2) = 1
    CHECK_THAT(SineLUT::lookup(0.25f), WithinAbs(1.0f, 0.001f));
    // Phase 0.5 = sin(PI) = 0
    CHECK_THAT(SineLUT::lookup(0.5f), WithinAbs(0.0f, 0.001f));
    // Phase 0.75 = sin(3PI/2) = -1
    CHECK_THAT(SineLUT::lookup(0.75f), WithinAbs(-1.0f, 0.001f));
}

TEST_CASE("SineLUT lookupAngle handles negative angles", "[SineLUT]") {
    // sin(-PI/2) = -1
    CHECK_THAT(SineLUT::lookupAngle(-3.14159265f / 2.0f), WithinAbs(-1.0f, 0.002f));
    // cos(0) = 1
    CHECK_THAT(SineLUT::cosLookupAngle(0.0f), WithinAbs(1.0f, 0.001f));
    // cos(PI) = -1
    CHECK_THAT(SineLUT::cosLookupAngle(3.14159265f), WithinAbs(-1.0f, 0.002f));
}
