# =============================================================================
#  Gospel Production Studio
#  Stage 1 / Step 5 - Tracks and timeline, automatic updates
# =============================================================================
cmake_minimum_required(VERSION 3.22)

project(GospelProductionStudio VERSION 0.8.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Static C++ runtime: the .exe runs on any Windows PC without extra installs.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# -----------------------------------------------------------------------------
#  Libraries
#  JUCE is taken from inside Tracktion Engine so both always match.
#  (Fallback: a plain JUCE clone in libs/JUCE.)
# -----------------------------------------------------------------------------
set(TE_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/libs/tracktion_engine")
set(JUCE_DIR "${TE_DIR}/modules/juce")

if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/JUCE")
endif()

if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "\nJUCE / Tracktion Engine not found.\n"
        "Run setup.bat first (see README.md, step 4).\n")
endif()

add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/juce")

# Tracktion Engine: the professional DAW engine (transport, tracks,
# recording, mixing, plugins).
juce_add_modules(ALIAS_NAMESPACE tracktion
    "${TE_DIR}/modules/tracktion_core"
    "${TE_DIR}/modules/tracktion_graph"
    "${TE_DIR}/modules/tracktion_engine")

# -----------------------------------------------------------------------------
#  Application
# -----------------------------------------------------------------------------
juce_add_gui_app(GospelProductionStudio
    PRODUCT_NAME  "Gospel Production Studio"
    COMPANY_NAME  "Minister Kemamah"
    BUNDLE_ID     "com.ministerkemamah.gospelproductionstudio"
    VERSION       ${PROJECT_VERSION})

target_sources(GospelProductionStudio PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/Song.cpp
    Source/SongLibrary.cpp
    Source/MemoRecorder.cpp
    Source/SongListPanel.cpp
    Source/SongEditor.cpp
    Source/ClickSounds.cpp
    Source/StudioEngine.cpp
    Source/StudioPage.cpp
    Source/Timeline.cpp
    Source/Updater.cpp
    Source/Theory.cpp
    Source/ChordTrack.cpp
    Source/ChordPanel.cpp
    Source/PianoRoll.cpp
    Source/Sfz.cpp
    Source/Instruments.cpp
    Source/SamplerPlugin.cpp
    Source/OrganPlugin.cpp
    Source/Grooves.cpp
    Source/RenderTest.cpp
    Source/SoundLibrary.cpp
    Source/InstrumentPanel.cpp
    Source/Takes.cpp)

target_include_directories(GospelProductionStudio PRIVATE Source)

target_compile_definitions(GospelProductionStudio PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:GospelProductionStudio,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:GospelProductionStudio,JUCE_VERSION>")

# -----------------------------------------------------------------------------
#  ASIO (lowest-latency driver for audio interfaces on Windows)
#  Optional now; switched on in Stage 2 once the Steinberg ASIO SDK
#  (free download) is placed in libs/asiosdk.
# -----------------------------------------------------------------------------
option(GPS_ENABLE_ASIO "Enable ASIO driver support (needs libs/asiosdk)" OFF)

if(GPS_ENABLE_ASIO)
    target_compile_definitions(GospelProductionStudio PRIVATE JUCE_ASIO=1)
    target_include_directories(GospelProductionStudio PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/asiosdk/common")
endif()

# Automatic updates: the GitHub build passes its run number and repository.
set(GPS_BUILD_NUMBER "0" CACHE STRING "Build number (set by the GitHub workflow)")
set(GPS_UPDATE_REPO "nembuanyik-lang/GospelProductionStudio" CACHE STRING "GitHub owner/repo for updates")
target_compile_definitions(GospelProductionStudio PRIVATE
    GPS_BUILD_NUMBER=${GPS_BUILD_NUMBER}
    GPS_UPDATE_REPO="${GPS_UPDATE_REPO}")

# Linux is only used for Claude's pre-checks before each GitHub build.
target_compile_definitions(GospelProductionStudio PRIVATE
    $<$<PLATFORM_ID:Linux>:JUCE_ALSA=0>
    $<$<PLATFORM_ID:Linux>:JUCE_JACK=0>
    $<$<PLATFORM_ID:Linux>:JUCE_USE_XRANDR=0>
    $<$<PLATFORM_ID:Linux>:JUCE_USE_XINERAMA=0>
    $<$<PLATFORM_ID:Linux>:JUCE_USE_XCURSOR=0>
    $<$<PLATFORM_ID:Linux>:JUCE_USE_XINPUT=0>)

if(MSVC)
    target_compile_options(GospelProductionStudio PRIVATE /bigobj /utf-8)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(GospelProductionStudio PRIVATE "-latomic")
endif()

target_link_libraries(GospelProductionStudio
    PRIVATE
        tracktion::tracktion_core
        tracktion::tracktion_graph
        tracktion::tracktion_engine
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_dsp
        juce::juce_cryptography
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
