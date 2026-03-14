add_library(xyzpan_engine STATIC
    src/Engine.cpp
    src/Coordinates.cpp
    src/FDNReverb.cpp
    src/LFO.cpp
)

target_include_directories(xyzpan_engine
    PUBLIC include
)

target_compile_features(xyzpan_engine PUBLIC cxx_std_20)

# Release optimization: full optimization, AVX2 SIMD, relaxed float for auto-vectorization
if(MSVC)
    target_compile_options(xyzpan_engine PRIVATE
        $<$<CONFIG:Release>:/O2 /arch:AVX2 /fp:fast>
    )
endif()

# NO juce:: link here — intentional architectural invariant.
# Engine must compile and link with zero JUCE headers.
