# =============================================================================
#  Gospel Production Studio
#  Stage 1 / Step 5 - Tracks and timeline, automatic updates
# =============================================================================
cmake_minimum_required(VERSION 3.22)

project(GospelProductionStudio VERSION 1.3.1 LANGUAGES C CXX)

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
    ICON_BIG      "${CMAKE_CURRENT_SOURCE_DIR}/Branding/icon-512.png"
    ICON_SMALL    "${CMAKE_CURRENT_SOURCE_DIR}/Branding/icon-64.png"
    VERSION       ${PROJECT_VERSION})

# The logo, built into the app (header and window icon)
juce_add_binary_data(GpsBranding SOURCES
    Branding/logo-icon.svg
    Branding/icon-256.png)

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
    Source/DrumGrid.cpp
    Source/MySounds.cpp
    Source/Loops.cpp
    Source/DeEsserPlugin.cpp
    Source/Timing.cpp
    Source/Harmony.cpp
    Source/TempoFollow.cpp
    Source/BackgroundWork.cpp
    Source/RealFeel.cpp
    Source/SongFiles.cpp
    Source/LatencyCheck.cpp
    Source/SongSafety.cpp
    Source/Backing.cpp
    Source/GuitarPlugin.cpp
    Source/Percussion.cpp
    Source/ChordDetect.cpp
    Source/Stems.cpp
    Source/Help.cpp
    Source/Learn.cpp
    Source/LearnCourse.cpp
    Source/Analysis.cpp
    Source/AnalyserPlugin.cpp
    Source/AnalyserPage.cpp
    Source/Release.cpp
    Source/ReleasePage.cpp
    Source/RenderTest.cpp
    Source/SoundLibrary.cpp
    Source/InstrumentPanel.cpp
    Source/Takes.cpp
    Source/AudioEdit.cpp
    Source/CompEditor.cpp
    Source/PluginHost.cpp
    Source/Mixing.cpp
    Source/MixerView.cpp
    Source/Automation.cpp
    Source/AutomationLane.cpp
    Source/Mp3Encoder.cpp
    Source/Exporter.cpp
    Source/ExportPage.cpp
    Source/MasterPlugin.cpp
    Source/MasteringPage.cpp
    Source/VocalTunePlugin.cpp)

target_include_directories(GospelProductionStudio PRIVATE Source)

target_compile_definitions(GospelProductionStudio PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1
    TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:GospelProductionStudio,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:GospelProductionStudio,JUCE_VERSION>")

# -----------------------------------------------------------------------------
#  ASIO (lowest-latency driver for audio interfaces on Windows), Stage 2.
#  The GitHub build downloads Steinberg's free ASIO SDK into libs/asiosdk;
#  ASIO is switched on whenever it is there.
# -----------------------------------------------------------------------------
set(GPS_ASIO_DEFAULT OFF)
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/asiosdk/common/iasiodrv.h")
    set(GPS_ASIO_DEFAULT ON)
endif()
option(GPS_ENABLE_ASIO "Enable ASIO driver support (needs libs/asiosdk)" ${GPS_ASIO_DEFAULT})
message(STATUS "ASIO support: ${GPS_ENABLE_ASIO}")

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

# MP3 export (Step 14): the LAME encoder, if the build machine has it.
# GitHub's Windows build installs it with vcpkg and passes GPS_LAME_ROOT.
set(GPS_LAME_ROOT "" CACHE PATH "Folder with include/lame/lame.h and the LAME library")
find_path(LAME_INCLUDE_DIR lame/lame.h HINTS "${GPS_LAME_ROOT}/include")
find_library(LAME_LIBRARY NAMES mp3lame libmp3lame libmp3lame-static mp3lame-static HINTS "${GPS_LAME_ROOT}/lib")
find_library(MPGHIP_LIBRARY NAMES mpghip libmpghip libmpghip-static mpghip-static HINTS "${GPS_LAME_ROOT}/lib")
set(GPS_LAME_OK OFF)
if(LAME_INCLUDE_DIR AND LAME_LIBRARY)
    # Make sure it really links here (otherwise build without MP3 rather than fail)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_INCLUDES "${LAME_INCLUDE_DIR}")
    set(CMAKE_REQUIRED_LIBRARIES "${LAME_LIBRARY}")
    if(MPGHIP_LIBRARY)
        list(APPEND CMAKE_REQUIRED_LIBRARIES "${MPGHIP_LIBRARY}")
    endif()
    check_cxx_source_compiles("
        #include <lame/lame.h>
        int main() { lame_t l = lame_init(); float a[1] = {0}; unsigned char b[8192];
                     lame_encode_buffer_ieee_float (l, a, a, 1, b, 8192); id3tag_init (l); lame_close (l); return 0; }"
        GPS_LAME_LINKS)
    unset(CMAKE_REQUIRED_INCLUDES)
    unset(CMAKE_REQUIRED_LIBRARIES)
    set(GPS_LAME_OK ${GPS_LAME_LINKS})
endif()

if(GPS_LAME_OK)
    message(STATUS "MP3 export: LAME found (${LAME_LIBRARY})")
    target_include_directories(GospelProductionStudio PRIVATE "${LAME_INCLUDE_DIR}")
    target_link_libraries(GospelProductionStudio PRIVATE "${LAME_LIBRARY}")
    if(MPGHIP_LIBRARY)
        target_link_libraries(GospelProductionStudio PRIVATE "${MPGHIP_LIBRARY}")
    endif()
    target_compile_definitions(GospelProductionStudio PRIVATE GPS_HAVE_LAME=1)
else()
    message(STATUS "MP3 export: LAME not found, WAV only")
    target_compile_definitions(GospelProductionStudio PRIVATE GPS_HAVE_LAME=0)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(GospelProductionStudio PRIVATE "-latomic")
endif()

target_link_libraries(GospelProductionStudio
    PRIVATE
        GpsBranding
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
