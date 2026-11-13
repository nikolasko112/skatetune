cmake_minimum_required(VERSION 3.22)

project(SkateTune VERSION 1.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.5
    GIT_SHALLOW TRUE
)
# Dev tip: during local iteration you can skip re-cloning JUCE on every
# configure by pointing CMake at an existing checkout instead:
#   cmake -B build -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
FetchContent_MakeAvailable(JUCE)

# Logic Pro (macOS) only loads Audio Units, so AU is only buildable - and
# only requested - when building on a Mac. Everywhere else (including this
# project's own CI's Linux-free pipeline, or a developer's Linux machine)
# we build VST3 only.
if (APPLE)
    set(ST_FORMATS VST3 AU)
else()
    set(ST_FORMATS VST3)
endif()

juce_add_plugin(SkateTune
    COMPANY_NAME                "SkateTune"
    BUNDLE_ID                   "com.skatetune.skatetune"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    PLUGIN_MANUFACTURER_CODE    Sk8r
    PLUGIN_CODE                 Skt1
    FORMATS                     ${ST_FORMATS}
    PRODUCT_NAME                "SkateTune"
    VST3_CATEGORIES             "Fx"
    AU_MAIN_TYPE                "kAudioUnitType_Effect"
)

target_sources(SkateTune PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/gui/SkaterView.cpp
)

target_include_directories(SkateTune PRIVATE Source)

target_compile_definitions(SkateTune PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_ALSA=0
    # The next five only matter on Linux, and only for local dev/test builds -
    # the shipped product is macOS VST3+AU, which never touches X11 at all.
    # They let this project configure on a Linux box that is missing the
    # libxrandr-dev / libxinerama-dev / libxcursor-dev / libxrender-dev /
    # libxi-dev packages, without changing anything about the macOS build.
    JUCE_USE_XRANDR=0
    JUCE_USE_XINERAMA=0
    JUCE_USE_XCURSOR=0
    JUCE_USE_XRENDER=0
    JUCE_USE_XINPUT=0
)

target_link_libraries(SkateTune PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
)

option(SKATETUNE_BUILD_HOST_TEST "Build a small headless host that loads the built plugin and smoke-tests it (dev only, not shipped)" OFF)
if (SKATETUNE_BUILD_HOST_TEST)
    add_subdirectory(tests/host_test)
endif()
