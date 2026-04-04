cmake_minimum_required(VERSION 3.22)
project(AnarackRev2 VERSION 0.4.0)

# Build number: monotonically increasing, increment on every build
set(ANARACK_BUILD_NUMBER 66)

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
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Anarack Rev2"
)

juce_add_binary_data(AnarackRev2Data
    SOURCES
        ui/rev2-panel.html
)

target_sources(AnarackRev2
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
        src/NetworkTransport.cpp
        src/SessionClient.cpp
        src/WgTunnel.cpp
)

target_include_directories(AnarackRev2
    PRIVATE
        src
        lib
)

set_target_properties(AnarackRev2 PROPERTIES
    JUCE_PLIST_ADDITIONS "<key>NSLocalNetworkUsageDescription</key><string>Anarack needs local network access to connect to the synth server on your Raspberry Pi.</string><key>NSBonjourServices</key><array><string>_udp._udp</string></array>"
)

target_compile_definitions(AnarackRev2
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        ANARACK_VERSION="${PROJECT_VERSION}"
        ANARACK_BUILD_NUMBER=${ANARACK_BUILD_NUMBER}
)

target_link_libraries(AnarackRev2
    PRIVATE
        AnarackRev2Data
        juce::juce_audio_utils
        juce::juce_gui_extra
        juce::juce_core
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/libboringtun.a
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
