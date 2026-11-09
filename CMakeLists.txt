cmake_minimum_required(VERSION 3.22)

# Binaire universel (Apple Silicon + Intel), macOS 11 minimum.
# Ces lignes doivent venir AVANT project().
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Architectures")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Version minimale de macOS")

project(BBDResV2 VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Récupération de JUCE (version figée)
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.14
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# V2 : identité distincte (nom et code de plug-in) pour cohabiter avec la V1 dans Live
juce_add_plugin(BBDResV2
    COMPANY_NAME "Perso"
    PLUGIN_MANUFACTURER_CODE Perx
    PLUGIN_CODE BbV2
    FORMATS AU VST3
    PRODUCT_NAME "BBDRes V2"
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(BBDResV2 PRIVATE
    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/UI/BBDLookAndFeel.h
    Source/UI/Widgets.h
    Source/UI/Views.h
    Source/Dsp/Response.h
    Source/Params.h
    Source/Dsp/Resonator.h
    Source/Dsp/Pitch.h
    Source/Dsp/Modulation.h
    Source/PluginProcessor.cpp)

target_compile_definitions(BBDResV2 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BBDResV2
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
