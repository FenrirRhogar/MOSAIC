cmake_minimum_required(VERSION 3.22)

project(MOSAIC VERSION 1.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MOSAIC_LTO "Link-time optimisation (slow links; turn off for development)" ON)
set(JUCE_DIR "" CACHE PATH "Local JUCE 8 checkout; fetched from GitHub when empty")

# ──────────────────────────────────────────────────────────────────────────────
# JUCE framework: a local checkout if given, otherwise the pinned release.
# ──────────────────────────────────────────────────────────────────────────────
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
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
        $<$<PLATFORM_ID:Linux>:JUCE_JACK=1>   # Linux Standalone: JACK / PipeWire-JACK for low latency (loaded at run time)
        JUCE_USE_CURL=1                       # update check (UpdateCheck.h); on Linux libcurl is
        JUCE_LOAD_CURL_SYMBOLS_LAZILY=1       # loaded at run time, so it is not a hard dependency
        FENRIKOS_UPDATE_REPO="MOSAIC"
        FENRIKOS_UPDATE_NAME="MOSAIC"
        FENRIKOS_PRODUCT_URL="https://fenrikos.gumroad.com/l/mosaic"
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
)

# ──────────────────────────────────────────────────────────────────────────────
# Linked JUCE modules — juce_audio_utils provides AudioThumbnail for waveforms.
# ──────────────────────────────────────────────────────────────────────────────
# Panel fonts (SIL OFL), compiled in so legends look the same on every platform.
juce_add_binary_data(RackFonts
    HEADER_NAME "RackFonts.h"
    NAMESPACE RackFonts
    SOURCES Source/fonts/BarlowCondensed-SemiBold.ttf Source/fonts/BarlowCondensed-Bold.ttf
            Source/fonts/Barlow-Regular.ttf Source/fonts/Barlow-Medium.ttf
            Source/fonts/JetBrainsMono-Medium.ttf Source/fonts/JetBrainsMono-Bold.ttf)
set_target_properties(RackFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(MOSAIC
    PUBLIC
        RackFonts
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
        juce::juce_recommended_warning_flags
)

if(MOSAIC_LTO)
    target_link_libraries(MOSAIC PUBLIC juce::juce_recommended_lto_flags)
endif()

# ──────────────────────────────────────────────────────────────────────────────
# Unit tests (run the MOSAICTests executable). Links the plugin's shared code.
# ──────────────────────────────────────────────────────────────────────────────
option(MOSAIC_TESTS "Build the unit tests" ON)
if(MOSAIC_TESTS)
    add_executable(MOSAICTests tests/ProcessorTests.cpp)
    target_include_directories(MOSAICTests PRIVATE $<TARGET_PROPERTY:MOSAIC,INCLUDE_DIRECTORIES>)
    target_compile_definitions(MOSAICTests PRIVATE $<TARGET_PROPERTY:MOSAIC,COMPILE_DEFINITIONS>)
    target_link_libraries(MOSAICTests PRIVATE MOSAIC)
endif()

# ──────────────────────────────────────────────────────────────────────────────
# Optional: MOSAIC_snapshot renders the editor to PNG (store artwork, visual checks)
# ──────────────────────────────────────────────────────────────────────────────
option(MOSAIC_SNAPSHOT "Build the editor snapshot tool" OFF)
if(MOSAIC_SNAPSHOT)
    add_executable(MOSAIC_snapshot tools/Snapshot.cpp)
    target_include_directories(MOSAIC_snapshot PRIVATE $<TARGET_PROPERTY:MOSAIC,INCLUDE_DIRECTORIES>)
    target_compile_definitions(MOSAIC_snapshot PRIVATE $<TARGET_PROPERTY:MOSAIC,COMPILE_DEFINITIONS>)
    target_link_libraries(MOSAIC_snapshot PRIVATE MOSAIC)
endif()
