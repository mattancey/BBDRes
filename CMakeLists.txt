cmake_minimum_required(VERSION 3.22)
 
# Binaire universel (Apple Silicon + Intel), macOS 11 minimum.
# Ces lignes doivent venir AVANT project().
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Architectures")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Version minimale de macOS")
 
project(BBDRes VERSION 0.1.0)
 
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
 
juce_add_plugin(BBDRes
    COMPANY_NAME "Perso"
    PLUGIN_MANUFACTURER_CODE Perx
    PLUGIN_CODE Bbdr
    FORMATS AU VST3
    PRODUCT_NAME "BBDRes"
    COPY_PLUGIN_AFTER_BUILD FALSE)
 
target_sources(BBDRes PRIVATE
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp)
 
target_compile_definitions(BBDRes PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
 
target_link_libraries(BBDRes
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
 
