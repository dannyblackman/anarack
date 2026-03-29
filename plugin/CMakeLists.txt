cmake_minimum_required(VERSION 3.22)
project(AnarackRev2 VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(JUCE)

juce_add_plugin(AnarackRev2
    COMPANY_NAME "Anarack"
    BUNDLE_ID "com.anarack.rev2"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    PLUGIN_MANUFACTURER_CODE Anak
    PLUGIN_CODE Arv2
    FORMATS AU Standalone
    PRODUCT_NAME "Anarack Rev2"
)

target_sources(AnarackRev2
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
        src/NetworkTransport.cpp
)

target_include_directories(AnarackRev2
    PRIVATE
        src
)

set_target_properties(AnarackRev2 PROPERTIES
    JUCE_PLIST_ADDITIONS "<key>NSLocalNetworkUsageDescription</key><string>Anarack needs local network access to connect to the synth server on your Raspberry Pi.</string><key>NSBonjourServices</key><array><string>_udp._udp</string></array>"
)

target_compile_definitions(AnarackRev2
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(AnarackRev2
    PRIVATE
        juce::juce_audio_utils
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
