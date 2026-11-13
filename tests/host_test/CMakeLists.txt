# Dev-only headless host used to smoke-test the built plugin and grab a
# screenshot of the editor. Not part of the shipped product - only built
# when -DSKATETUNE_BUILD_HOST_TEST=ON is passed to CMake.
add_executable(SkateTuneHostTest host_test.cpp)

target_compile_definitions(SkateTuneHostTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_ALSA=0
    JUCE_USE_XRANDR=0
    JUCE_USE_XINERAMA=0
    JUCE_USE_XCURSOR=0
    JUCE_USE_XRENDER=0
    JUCE_USE_XINPUT=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(SkateTuneHostTest PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
)
