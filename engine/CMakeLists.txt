add_library(xyzpan_engine STATIC
    src/Engine.cpp
    src/Coordinates.cpp
    src/FDNReverb.cpp
)

target_include_directories(xyzpan_engine
    PUBLIC include
)

target_compile_features(xyzpan_engine PUBLIC cxx_std_20)

# NO juce:: link here — intentional architectural invariant.
# Engine must compile and link with zero JUCE headers.
