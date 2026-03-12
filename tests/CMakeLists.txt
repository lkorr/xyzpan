add_executable(XYZPanTests
    engine/TestCoordinates.cpp
)

target_link_libraries(XYZPanTests
    PRIVATE
        xyzpan_engine
        Catch2::Catch2WithMain
    # NO juce:: targets here — this enforces the engine isolation invariant (SETUP-02)
)

# Enable CTest and Catch2 test discovery
# Catch2 extras (CatchAddTests, Catch cmake modules) are in the Catch2 source extras dir
list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
include(CTest)
include(Catch)
catch_discover_tests(XYZPanTests)
