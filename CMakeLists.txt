cmake_minimum_required(VERSION 3.22)

project(ChromaPulse VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
endif()

# ------------------------------------------------------------------------------
# JUCE (fetched automatically; swap for add_subdirectory(JUCE) if you vendor it)
# ------------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

# ------------------------------------------------------------------------------
# Plugin target
# ------------------------------------------------------------------------------
set(CP_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND CP_FORMATS AU)
endif()

juce_add_plugin(ChromaPulse
    COMPANY_NAME                "Him'z DSP"
    PRODUCT_NAME                "Chroma-Pulse"
    PLUGIN_MANUFACTURER_CODE    Himz
    PLUGIN_CODE                 Chpl
    FORMATS                     ${CP_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Distortion
    AU_MAIN_TYPE                kAudioUnitType_Effect
    BUNDLE_ID                   com.himzdsp.chromapulse)

target_sources(ChromaPulse PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h)

target_compile_definitions(ChromaPulse PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ChromaPulse
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
