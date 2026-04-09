if(APPLE)
    set(XYZPAN_FORMATS VST3 AU Standalone)
else()
    set(XYZPAN_FORMATS VST3 Standalone)
endif()

juce_add_plugin(XYZPan
    COMPANY_NAME "XYZAudio"
    PLUGIN_MANUFACTURER_CODE Xyza
    PLUGIN_CODE Xzpn
    FORMATS ${XYZPAN_FORMATS}
    PRODUCT_NAME "XYZPan"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    NEEDS_WEB_BROWSER FALSE
)

# Factory presets embedded as binary data (fallback when not installed)
juce_add_binary_data(XYZPanPresets
    HEADER_NAME "FactoryPresetData.h"
    NAMESPACE FactoryPresetData
    SOURCES
        ${CMAKE_SOURCE_DIR}/presets/factory/Default.xml
        ${CMAKE_SOURCE_DIR}/presets/factory/Orbit_XY.xml
        ${CMAKE_SOURCE_DIR}/presets/factory/Slow_Drift.xml
        ${CMAKE_SOURCE_DIR}/presets/factory/Behind_You.xml
        ${CMAKE_SOURCE_DIR}/presets/factory/Fly_Around.xml
        ${CMAKE_SOURCE_DIR}/presets/factory/Overhead.xml
        ${CMAKE_SOURCE_DIR}/presets/factory/Near_Whisper.xml
)

target_sources(XYZPan
    PRIVATE
        PluginProcessor.cpp
        PluginEditor.cpp
        ParamLayout.cpp
        PresetManager.cpp
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

# Prevent D2D/OpenGL conflict on Windows NVIDIA (Phase 6)
if(WIN32)
    target_compile_definitions(XYZPan PUBLIC JUCE_DIRECT2D=0)
endif()

target_link_libraries(XYZPan
    PRIVATE
        XYZPanPresets
        xyzpan_engine
        xyzpan_ui           # Phase 6: OpenGL renderer and alchemy look and feel
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl   # Phase 6: OpenGL context and shader program
        juce::juce_gui_extra  # ColourSelector + CallOutBox for avatar color swatches
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
