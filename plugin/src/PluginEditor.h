#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
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
    juce::Label hostLabel;
    juce::TextEditor hostInput;
    juce::ToggleButton wgToggle;
    juce::TextButton connectButton;
    juce::Label statusLabel;

    std::unique_ptr<juce::WebBrowserComponent> webView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnarackEditor)
};
