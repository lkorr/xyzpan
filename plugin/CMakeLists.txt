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
)

target_link_libraries(XYZPan
    PRIVATE
        xyzpan_engine
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
