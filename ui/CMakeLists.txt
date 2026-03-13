# xyzpan_ui: STATIC library (Phase 06-02)
# Contains OpenGL renderer, camera, mesh geometry, and alchemy LookAndFeel.
# PositionBridge.h remains header-only; exposed via PUBLIC include directory.
add_library(xyzpan_ui STATIC
    Camera.cpp
    Mesh.cpp
    AlchemyLookAndFeel.cpp
    XYZPanGLView.cpp
    LFOWaveformButton.cpp   # Phase 6-03
    LFOStrip.cpp            # Phase 6-03
    DevPanelComponent.cpp   # Phase 6-03
)

target_include_directories(xyzpan_ui
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(xyzpan_ui
    PUBLIC
        juce::juce_opengl
        juce::juce_audio_processors
        juce::juce_gui_basics
        glm::glm
)
