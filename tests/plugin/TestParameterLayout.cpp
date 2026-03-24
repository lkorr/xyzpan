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

    SECTION("Early Reflection parameters are registered") {
        REQUIRE(proc.apvts.getParameter(ParamID::ER_ENABLED)     != nullptr);
        REQUIRE(proc.apvts.getParameter(ParamID::ER_ROOM_SIZE)   != nullptr);
        REQUIRE(proc.apvts.getParameter(ParamID::ER_DAMPING)     != nullptr);
        REQUIRE(proc.apvts.getParameter(ParamID::ER_LEVEL)       != nullptr);
        REQUIRE(proc.apvts.getParameter(ParamID::ER_REVERB_SEND) != nullptr);
        REQUIRE(proc.apvts.getParameter(ParamID::BYPASS_ER)      != nullptr);
    }

    SECTION("ER Room Size range is 1-30") {
        auto range = proc.apvts.getParameterRange(ParamID::ER_ROOM_SIZE);
        REQUIRE(range.start == Catch::Approx(1.0f));
        REQUIRE(range.end   == Catch::Approx(30.0f));
    }

    SECTION("ER defaults match constants") {
        auto erRoomRange = proc.apvts.getParameterRange(ParamID::ER_ROOM_SIZE);
        float erRoomDefault = erRoomRange.convertFrom0to1(
            proc.apvts.getParameter(ParamID::ER_ROOM_SIZE)->getDefaultValue());
        REQUIRE(erRoomDefault == Catch::Approx(5.0f).margin(0.2f));
    }

    SECTION("Chest bounce filter parameters are registered") {
        auto* hpf = proc.apvts.getParameter(ParamID::CHEST_HPF_HZ);
        auto* lp  = proc.apvts.getParameter(ParamID::CHEST_LP_HZ);
        REQUIRE(hpf != nullptr);
        REQUIRE(lp  != nullptr);

        // Verify defaults
        auto hpfRange = proc.apvts.getParameterRange(ParamID::CHEST_HPF_HZ);
        float hpfDefault = hpfRange.convertFrom0to1(hpf->getDefaultValue());
        REQUIRE(hpfDefault == Catch::Approx(700.0f).margin(5.0f));

        auto lpRange = proc.apvts.getParameterRange(ParamID::CHEST_LP_HZ);
        float lpDefault = lpRange.convertFrom0to1(lp->getDefaultValue());
        REQUIRE(lpDefault == Catch::Approx(1000.0f).margin(5.0f));
    }

    SECTION("Binaural Enabled parameter registered with default true") {
        auto* param = proc.apvts.getParameter(ParamID::BINAURAL_ENABLED);
        REQUIRE(param != nullptr);
        // Bool param: default value in normalized space is 1.0 for true
        REQUIRE(param->getDefaultValue() == Catch::Approx(1.0f));
    }
}
