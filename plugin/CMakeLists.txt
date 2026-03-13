juce_add_plugin(XYZPan
    COMPANY_NAME "XYZAudio"
    PLUGIN_MANUFACTURER_CODE Xyza
    PLUGIN_CODE Xzpn
    FORMATS VST3 Standalone
    PRODUCT_NAME "XYZPan"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(XYZPan
    PRIVATE
        PluginProcessor.cpp
        PluginEditor.cpp
        ParamLayout.cpp
)

target_include_directories(XYZPan
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(XYZPan
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_DIRECT2D=0     # Phase 6: prevent D2D/OpenGL conflict on Windows NVIDIA
)

target_link_libraries(XYZPan
    PRIVATE
        xyzpan_engine
        xyzpan_ui           # Phase 6: OpenGL renderer and alchemy look and feel
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl   # Phase 6: OpenGL context and shader program
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
