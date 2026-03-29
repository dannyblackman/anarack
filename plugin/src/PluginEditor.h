#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class AnarackEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    AnarackEditor(AnarackProcessor&);
    ~AnarackEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void toggleConnection();

    AnarackProcessor& processor;

    juce::Label titleLabel;
    juce::Label hostLabel;
    juce::TextEditor hostInput;
    juce::ToggleButton wgToggle;
    juce::TextButton connectButton;
    juce::Label statusLabel;
    juce::Label bufferLabel;
    juce::Label packetLabel;
    juce::Label rttLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnarackEditor)
};
