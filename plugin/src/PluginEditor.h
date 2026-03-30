#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// A single parameter control — renders as knob, selector, or toggle
struct SynthControl
{
    juce::String id;
    juce::String name;
    juce::String type; // "knob", "selector", "toggle"
    int cc = -1;
    int min = 0;
    int max = 127;
    int value = 0;
    juce::StringArray values; // for selector/toggle
};

// Group of controls
struct SynthGroup
{
    juce::String name;
    juce::OwnedArray<SynthControl> controls;
};

class SynthPanel : public juce::Component
{
public:
    void loadDefinition(const juce::String& jsonStr);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    std::function<void(int cc, int value)> onParamChange;
    void setParamValue(int cc, int value);

private:
    juce::OwnedArray<SynthGroup> groups;
    SynthControl* dragControl = nullptr;
    float dragStartY = 0;
    int dragStartValue = 0;

    SynthControl* getControlAt(juce::Point<float> pos);
    juce::Rectangle<int> getControlBounds(int groupIdx, int ctrlIdx);
    void drawKnob(juce::Graphics& g, juce::Rectangle<int> bounds, SynthControl* ctrl);
    void drawSelector(juce::Graphics& g, juce::Rectangle<int> bounds, SynthControl* ctrl);
    void drawToggle(juce::Graphics& g, juce::Rectangle<int> bounds, SynthControl* ctrl);

    static constexpr int GROUP_HEADER = 18;
    static constexpr int CONTROL_W = 56;
    static constexpr int CONTROL_H = 68;
    static constexpr int GROUP_PAD = 4;
};

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
    void fetchDefinition();

    AnarackProcessor& processor;

    juce::Label hostLabel;
    juce::TextEditor hostInput;
    juce::ToggleButton wgToggle;
    juce::TextButton connectButton;
    juce::Label statusLabel;

    SynthPanel synthPanel;
    juce::Viewport viewport;
    bool definitionLoaded = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnarackEditor)
};
