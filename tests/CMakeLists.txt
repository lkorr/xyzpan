add_executable(XYZPanTests
    engine/TestCoordinates.cpp
    engine/TestBinauralPanning.cpp
    engine/TestDepthAndElevation.cpp
    engine/TestDistanceProcessing.cpp
    engine/TestCreativeTools.cpp
    engine/TestAuxSend.cpp
    engine/TestStereoSplit.cpp
    engine/TestSampleRateAdaptation.cpp
)

target_link_libraries(XYZPanTests
    PRIVATE
        xyzpan_engine
        Catch2::Catch2WithMain
    # NO juce:: targets here — this enforces the engine isolation invariant (SETUP-02)
)

# Catch2 test discovery via CTest
# enable_testing() is called at the root CMakeLists.txt; include(CTest) not needed here.
# Catch2 extras (Catch.cmake module with catch_discover_tests) are in the source extras dir.
list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
include(Catch)
catch_discover_tests(XYZPanTests)

# ---------------------------------------------------------------------------
# XYZPanPluginTests — plugin-layer unit tests (require JUCE)
# Links XYZPan_SharedCode, which is the JUCE-generated shared code target
# created by juce_add_plugin(XYZPan ...) in plugin/CMakeLists.txt.
# Separate from XYZPanTests to preserve SETUP-02 engine isolation invariant.
# ---------------------------------------------------------------------------
add_executable(XYZPanPluginTests
    plugin/TestParameterLayout.cpp
    plugin/TestPositionBridge.cpp
    plugin/TestPresets.cpp
)

target_link_libraries(XYZPanPluginTests
    PRIVATE
        XYZPan
        xyzpan_ui
        xyzpan_engine
        juce::juce_audio_utils
        Catch2::Catch2WithMain
)

target_include_directories(XYZPanPluginTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/plugin
)

catch_discover_tests(XYZPanPluginTests)
