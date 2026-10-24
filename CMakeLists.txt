cmake_minimum_required(VERSION 3.22)

project(MOSAIC VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ──────────────────────────────────────────────────────────────────────────────
# JUCE framework — reuse the local checkout two levels up (this repo is JUCE).
# Falls back to fetching a matching tag if built standalone elsewhere.
# ──────────────────────────────────────────────────────────────────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.13
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# ──────────────────────────────────────────────────────────────────────────────
# Signalsmith Stretch (high-quality pitch/time for the "Studio" pad mode).
# Header-only; depends on Signalsmith's "linear" lib. SOURCE_SUBDIR points at a
# non-existent dir so FetchContent only populates the headers (no add_subdirectory).
# ──────────────────────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(signalsmith_linear
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/linear.git
    GIT_TAG 7bb1c8f36794d6980080ce817c91f8da8fe56a66
    SOURCE_SUBDIR  do-not-build)
FetchContent_Declare(signalsmith_stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG 88061fd4eda56a2353c5d65c74aa3f4ccf76f9da
    SOURCE_SUBDIR  do-not-build)
FetchContent_MakeAvailable(signalsmith_linear signalsmith_stretch)

# ──────────────────────────────────────────────────────────────────────────────
# Plugin target — instrument (synth) with MIDI input and multiple stereo outs.
# ──────────────────────────────────────────────────────────────────────────────
juce_add_plugin(MOSAIC
    COMPANY_NAME                "fenrikos"
    BUNDLE_ID                   "com.fenrikos.MOSAIC"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE    Fnrk
    PLUGIN_CODE                 Dmsp
    FORMATS                     VST3 AU Standalone
    VST3_CATEGORIES             Instrument Sampler
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    PRODUCT_NAME                "MOSAIC"
    COPY_PLUGIN_AFTER_BUILD     FALSE
)

juce_generate_juce_header(MOSAIC)

# ──────────────────────────────────────────────────────────────────────────────
# Source files
# ──────────────────────────────────────────────────────────────────────────────
target_sources(MOSAIC PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SamplerEngine.cpp
    Source/PadComponent.cpp
)

target_include_directories(MOSAIC
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
        ${signalsmith_stretch_SOURCE_DIR}
        ${signalsmith_linear_SOURCE_DIR}/include
)

# ──────────────────────────────────────────────────────────────────────────────
# Compile definitions
# ──────────────────────────────────────────────────────────────────────────────
target_compile_definitions(MOSAIC
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
)

# ──────────────────────────────────────────────────────────────────────────────
# Linked JUCE modules — juce_audio_utils provides AudioThumbnail for waveforms.
# ──────────────────────────────────────────────────────────────────────────────
target_link_libraries(MOSAIC
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
