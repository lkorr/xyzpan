#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "PluginProcessor.h"
#include "ParamIDs.h"

TEST_CASE("Parameter layout contains R parameter", "[param][PARAM-01]") {
    XYZPanProcessor proc;

    SECTION("R parameter is registered in APVTS") {
        auto* param = proc.apvts.getParameter(ParamID::R);
        REQUIRE(param != nullptr);
    }

    SECTION("R parameter default is 1.0") {
        auto range = proc.apvts.getParameterRange(ParamID::R);
        float defaultNorm = proc.apvts.getParameter(ParamID::R)->getDefaultValue();
        float defaultVal  = range.convertFrom0to1(defaultNorm);
        REQUIRE(defaultVal == Catch::Approx(1.0f).margin(0.01f));
    }

    SECTION("R parameter range is 0.0 to 2.0") {
        auto range = proc.apvts.getParameterRange(ParamID::R);
        REQUIRE(range.start == Catch::Approx(0.0f));
        REQUIRE(range.end   == Catch::Approx(2.0f));
    }

    SECTION("Core spatial parameters are registered") {
        REQUIRE(proc.apvts.getParameter(ParamID::X) != nullptr);
        REQUIRE(proc.apvts.getParameter(ParamID::Y) != nullptr);
        REQUIRE(proc.apvts.getParameter(ParamID::Z) != nullptr);
    }

    SECTION("LFO and reverb parameters are registered") {
        REQUIRE(proc.apvts.getParameter(ParamID::LFO_X_RATE) != nullptr);
        REQUIRE(proc.apvts.getParameter(ParamID::VERB_WET)   != nullptr);
    }
}
