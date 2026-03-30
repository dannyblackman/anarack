#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

struct SynthControl
{
    juce::String id, name, type;
    int cc = -1, min = 0, max = 127, value = 0;
    juce::StringArray values;
};

// Renders the synth panel as two horizontal rows of controls with section dividers
// Layout matches the real Rev2 front panel exactly
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

    juce::Colour accent { 0xffE53935 };

private:
    // A positioned control
    struct PosCtrl { SynthControl* ctrl; int x; int y; };
    // A section label
    struct SectionLabel { juce::String name; int x; int y; int w; };

    juce::OwnedArray<SynthControl> allControls;
    std::vector<PosCtrl> positioned;
    std::vector<SectionLabel> labels;

    void buildRev2Layout();
    SynthControl* findByName(const juce::String& name);
    SynthControl* findByCC(int cc);
    SynthControl* findById(const juce::String& id);
    void addCtrl(int x, int y, SynthControl* c);
    void addLabel(const juce::String& name, int x, int y, int w);

    SynthControl* getControlAt(juce::Point<float> pos);
    void drawKnob(juce::Graphics& g, int x, int y, SynthControl* c);
    void drawSelector(juce::Graphics& g, int x, int y, SynthControl* c);
    void drawToggle(juce::Graphics& g, int x, int y, SynthControl* c);

    SynthControl* dragCtrl = nullptr;
    float dragStartY = 0;
    int dragStartVal = 0;
    static constexpr int CW = 55; // control width
    static constexpr int CH = 62; // control height
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
    bool defLoaded = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnarackEditor)
};
