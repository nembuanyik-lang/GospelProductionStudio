# =============================================================================
#  Gospel Production Studio
#  Stage 1 / Step 3 - Song Library
# =============================================================================
cmake_minimum_required(VERSION 3.22)

project(GospelProductionStudio VERSION 0.2.0 LANGUAGES C CXX)

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

# Tracktion Engine is downloaded in Stage 0 and linked from Stage 1
# (timeline, tracks and recording are built on it).

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
    Source/SongEditor.cpp)

target_include_directories(GospelProductionStudio PRIVATE Source)

target_compile_definitions(GospelProductionStudio PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
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

target_link_libraries(GospelProductionStudio
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
