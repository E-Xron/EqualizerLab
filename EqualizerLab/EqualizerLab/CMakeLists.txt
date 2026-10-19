cmake_minimum_required(VERSION 3.22)
project(EqualizerLab VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
option(COPY_AFTER_BUILD "Copy VST3 to the system folder after building" ON)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(EqualizerLab
    COMPANY_NAME "YourName"
    PLUGIN_MANUFACTURER_CODE Yrnm
    PLUGIN_CODE Eql1
    FORMATS VST3 AU Standalone                 # AU (Logic, GarageBand) is built on macOS only
    PRODUCT_NAME "EqualizerLab"
    BUNDLE_ID com.retunelab.equalizerlab
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Fx EQ
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD ${COPY_AFTER_BUILD})

target_sources(EqualizerLab PRIVATE
    Source/FilterDesign.h Source/EqEngine.h Source/EqEngine.cpp Source/Analysis.h Source/Presets.h Source/Look.h
    Source/PluginProcessor.h Source/PluginProcessor.cpp Source/EqDisplay.h Source/EqDisplay.cpp
    Source/PluginEditor.h Source/PluginEditor.cpp)
target_compile_definitions(EqualizerLab PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(EqualizerLab PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
