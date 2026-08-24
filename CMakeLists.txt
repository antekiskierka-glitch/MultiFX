cmake_minimum_required(VERSION 3.22)
project(MultiFX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MultiFX
    COMPANY_NAME "YourCompany"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Ycmp
    PLUGIN_CODE Mfx1
    FORMATS VST3 Standalone
    PRODUCT_NAME "MultiFX"
)

juce_generate_juce_header(MultiFX)

juce_add_binary_data(MultiFXAssets
    NAMESPACE BinaryData
    SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Assets/face1.png
    ${CMAKE_CURRENT_SOURCE_DIR}/Assets/face2.png
    ${CMAKE_CURRENT_SOURCE_DIR}/Assets/face3.png
    ${CMAKE_CURRENT_SOURCE_DIR}/Assets/face4.png
    ${CMAKE_CURRENT_SOURCE_DIR}/Assets/face5.png
    ${CMAKE_CURRENT_SOURCE_DIR}/Assets/background.png
    ${CMAKE_CURRENT_SOURCE_DIR}/Assets/knob_01.png
    ${CMAKE_CURRENT_SOURCE_DIR}/Assets/knob_02.png
    ${CMAKE_CURRENT_SOURCE_DIR}/Assets/knob_03.png
    ${CMAKE_CURRENT_SOURCE_DIR}/Assets/knob_04.png
    ${CMAKE_CURRENT_SOURCE_DIR}/Assets/knob_05.png
    ${CMAKE_CURRENT_SOURCE_DIR}/Assets/knob_06.png
    ${CMAKE_CURRENT_SOURCE_DIR}/Assets/knob_07.png
    ${CMAKE_CURRENT_SOURCE_DIR}/Assets/knob_08.png
    ${CMAKE_CURRENT_SOURCE_DIR}/Assets/knob_09.png
)

target_sources(MultiFX
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_compile_definitions(MultiFX
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(MultiFX
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        MultiFXAssets
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
