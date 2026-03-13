add_executable(XYZPanTests
    engine/TestCoordinates.cpp
    engine/TestBinauralPanning.cpp
    engine/TestDepthAndElevation.cpp
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
