#include "PluginEditor.h"

// ─────────────────────────────────────────────
// SynthPanel — renders all controls from definition
// ─────────────────────────────────────────────

void SynthPanel::loadDefinition(const juce::String& jsonStr)
{
    groups.clear();
    auto root = juce::JSON::parse(jsonStr);
    if (!root.isObject()) return;

    auto enums = root.getProperty("enums", {});
    auto groupsArr = *root.getProperty("groups", {}).getArray();

    for (auto& g : groupsArr)
    {
        auto* group = new SynthGroup();
        group->name = g.getProperty("name", "").toString();

        auto paramsArr = g.getProperty("parameters", {});
        if (!paramsArr.isArray()) continue;

        for (auto& p : *paramsArr.getArray())
        {
            auto* ctrl = new SynthControl();
            ctrl->id = p.getProperty("id", "").toString();
            ctrl->name = p.getProperty("name", "").toString();
            ctrl->type = p.getProperty("type", "knob").toString();
            ctrl->cc = (int)p.getProperty("cc", -1);
            ctrl->min = (int)p.getProperty("min", 0);
            ctrl->max = (int)p.getProperty("max", 127);
            ctrl->value = (int)p.getProperty("default", 0);

            // Resolve enum values
            auto enumRef = p.getProperty("enumRef", "").toString();
            if (enumRef.isNotEmpty() && enums.isObject())
            {
                auto enumArr = enums.getProperty(enumRef, {});
                if (enumArr.isArray())
                    for (auto& v : *enumArr.getArray())
                        ctrl->values.add(v.toString());
            }
            auto valuesArr = p.getProperty("values", {});
            if (valuesArr.isArray())
                for (auto& v : *valuesArr.getArray())
                    ctrl->values.add(v.toString());

            group->controls.add(ctrl);
        }

        if (group->controls.size() > 0)
            groups.add(group);
        else
            delete group;
    }

    // Calculate total size needed
    int totalHeight = 0;
    for (auto* g : groups)
    {
        int cols = std::max(1, getWidth() / CONTROL_W);
        int rows = ((int)g->controls.size() + cols - 1) / cols;
        totalHeight += GROUP_HEADER + rows * CONTROL_H + GROUP_PAD;
    }
    setSize(std::max(600, getWidth()), std::max(totalHeight, 400));
    repaint();
}

void SynthPanel::setParamValue(int cc, int value)
{
    for (auto* g : groups)
        for (auto* c : g->controls)
            if (c->cc == cc)
            {
                c->value = value;
                repaint();
                return;
            }
}

juce::Rectangle<int> SynthPanel::getControlBounds(int groupIdx, int ctrlIdx)
{
    int cols = std::max(1, getWidth() / CONTROL_W);
    int y = 0;
    for (int gi = 0; gi < groups.size(); gi++)
    {
        int numCtrls = groups[gi]->controls.size();
        int rows = (numCtrls + cols - 1) / cols;
        if (gi == groupIdx)
        {
            int row = ctrlIdx / cols;
            int col = ctrlIdx % cols;
            return { col * CONTROL_W, y + GROUP_HEADER + row * CONTROL_H, CONTROL_W, CONTROL_H };
        }
        y += GROUP_HEADER + rows * CONTROL_H + GROUP_PAD;
    }
    return {};
}

SynthControl* SynthPanel::getControlAt(juce::Point<float> pos)
{
    for (int gi = 0; gi < groups.size(); gi++)
        for (int ci = 0; ci < groups[gi]->controls.size(); ci++)
            if (getControlBounds(gi, ci).toFloat().contains(pos))
                return groups[gi]->controls[ci];
    return nullptr;
}

void SynthPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a0a0a));

    int cols = std::max(1, getWidth() / CONTROL_W);
    int y = 0;

    for (int gi = 0; gi < groups.size(); gi++)
    {
        auto* group = groups[gi];
        int numCtrls = group->controls.size();
        int rows = (numCtrls + cols - 1) / cols;

        // Group background
        int groupH = GROUP_HEADER + rows * CONTROL_H;
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(0, (float)y, (float)getWidth(), (float)groupH, 4);

        // Group title
        g.setColour(juce::Colour(0xff6366f1));
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.drawText(group->name, 6, y + 2, getWidth() - 12, 14, juce::Justification::centredLeft);

        // Controls
        for (int ci = 0; ci < numCtrls; ci++)
        {
            auto bounds = getControlBounds(gi, ci);
            auto* ctrl = group->controls[ci];

            if (ctrl->type == "selector")
                drawSelector(g, bounds, ctrl);
            else if (ctrl->type == "toggle")
                drawToggle(g, bounds, ctrl);
            else
                drawKnob(g, bounds, ctrl);
        }

        y += groupH + GROUP_PAD;
    }
}

void SynthPanel::drawKnob(juce::Graphics& g, juce::Rectangle<int> bounds, SynthControl* ctrl)
{
    float cx = bounds.getCentreX();
    float cy = bounds.getY() + 28.0f;
    float r = 18.0f;

    float startAng = juce::MathConstants<float>::pi * 1.25f;
    float sweep = juce::MathConstants<float>::pi * 1.5f;
    float norm = ctrl->max > 0 ? (float)ctrl->value / (float)ctrl->max : 0;
    float valAng = norm * sweep;

    // Track
    juce::Path track;
    track.addCentredArc(cx, cy, r, r, 0, -startAng, -startAng + sweep, true);
    g.setColour(juce::Colour(0xff333333));
    g.strokePath(track, juce::PathStrokeType(2.0f));

    // Value arc
    juce::Path arc;
    arc.addCentredArc(cx, cy, r, r, 0, -startAng, -startAng + valAng, true);
    g.setColour(juce::Colour(0xff6366f1));
    g.strokePath(arc, juce::PathStrokeType(2.5f));

    // Body
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillEllipse(cx - r + 4, cy - r + 4, (r - 4) * 2, (r - 4) * 2);

    // Indicator line
    float ang = -startAng + valAng;
    float il = r - 5;
    g.setColour(juce::Colours::white);
    g.drawLine(cx, cy, cx + std::cos(ang) * il, cy + std::sin(ang) * il, 2.0f);

    // Value text
    g.setColour(juce::Colour(0xff777777));
    g.setFont(juce::FontOptions(8.0f));
    g.drawText(juce::String(ctrl->value), (int)(cx - 12), (int)(cy - 5), 24, 10, juce::Justification::centred);

    // Label
    g.setColour(juce::Colour(0xff999999));
    g.setFont(juce::FontOptions(8.0f));
    g.drawText(ctrl->name, bounds.getX(), bounds.getBottom() - 14, bounds.getWidth(), 12, juce::Justification::centred);
}

void SynthPanel::drawSelector(juce::Graphics& g, juce::Rectangle<int> bounds, SynthControl* ctrl)
{
    float cx = bounds.getCentreX();

    // Label
    g.setColour(juce::Colour(0xff999999));
    g.setFont(juce::FontOptions(8.0f));
    g.drawText(ctrl->name, bounds.getX(), bounds.getY() + 8, bounds.getWidth(), 12, juce::Justification::centred);

    // Value
    juce::String valText = (ctrl->value >= 0 && ctrl->value < ctrl->values.size())
                             ? ctrl->values[ctrl->value] : juce::String(ctrl->value);
    g.setColour(juce::Colour(0xff6366f1));
    g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
    g.drawText(valText, bounds.getX(), bounds.getY() + 26, bounds.getWidth(), 12, juce::Justification::centred);

    // Bottom label
    g.setColour(juce::Colour(0xff999999));
    g.setFont(juce::FontOptions(8.0f));
    g.drawText(ctrl->name, bounds.getX(), bounds.getBottom() - 14, bounds.getWidth(), 12, juce::Justification::centred);
}

void SynthPanel::drawToggle(juce::Graphics& g, juce::Rectangle<int> bounds, SynthControl* ctrl)
{
    float cx = bounds.getCentreX();
    bool on = ctrl->value > 0;

    // Button
    auto btnBounds = juce::Rectangle<float>(cx - 16, bounds.getY() + 22.0f, 32, 16);
    g.setColour(on ? juce::Colour(0xff6366f1) : juce::Colour(0xff333333));
    g.fillRoundedRectangle(btnBounds, 3);

    juce::String label = (ctrl->values.size() > 1) ? ctrl->values[on ? 1 : 0] : (on ? "ON" : "OFF");
    g.setColour(on ? juce::Colours::white : juce::Colour(0xff888888));
    g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
    g.drawText(label, btnBounds.toNearestInt(), juce::Justification::centred);

    // Label
    g.setColour(juce::Colour(0xff999999));
    g.setFont(juce::FontOptions(8.0f));
    g.drawText(ctrl->name, bounds.getX(), bounds.getBottom() - 14, bounds.getWidth(), 12, juce::Justification::centred);
}

void SynthPanel::mouseDown(const juce::MouseEvent& e)
{
    dragControl = getControlAt(e.position);
    if (!dragControl) return;

    if (dragControl->type == "toggle")
    {
        dragControl->value = dragControl->value > 0 ? 0 : 1;
        if (onParamChange && dragControl->cc >= 0)
            onParamChange(dragControl->cc, dragControl->value);
        repaint();
        dragControl = nullptr;
        return;
    }

    if (dragControl->type == "selector")
    {
        // Click cycles through values
        int numVals = dragControl->values.size();
        if (numVals == 0) numVals = dragControl->max + 1;
        dragControl->value = (dragControl->value + 1) % numVals;
        if (onParamChange && dragControl->cc >= 0)
        {
            int ccVal = dragControl->max > 127
                ? juce::roundToInt(dragControl->value * 127.0 / dragControl->max)
                : dragControl->value;
            onParamChange(dragControl->cc, ccVal);
        }
        repaint();
        dragControl = nullptr;
        return;
    }

    dragStartY = e.position.y;
    dragStartValue = dragControl->value;
}

void SynthPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (!dragControl || dragControl->type != "knob") return;
    float dy = dragStartY - e.position.y;
    int newVal = juce::jlimit(dragControl->min, dragControl->max,
                              dragStartValue + (int)(dy * 0.5f));
    if (newVal != dragControl->value)
    {
        dragControl->value = newVal;
        if (onParamChange && dragControl->cc >= 0)
        {
            int ccVal = dragControl->max > 127
                ? juce::roundToInt(dragControl->value * 127.0 / dragControl->max)
                : dragControl->value;
            onParamChange(dragControl->cc, ccVal);
        }
        repaint();
    }
}

void SynthPanel::mouseUp(const juce::MouseEvent&)
{
    dragControl = nullptr;
}

void SynthPanel::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    auto* ctrl = getControlAt(e.position);
    if (!ctrl || ctrl->type != "knob") return;

    int delta = wheel.deltaY > 0 ? 1 : -1;
    ctrl->value = juce::jlimit(ctrl->min, ctrl->max, ctrl->value + delta);
    if (onParamChange && ctrl->cc >= 0)
    {
        int ccVal = ctrl->max > 127
            ? juce::roundToInt(ctrl->value * 127.0 / ctrl->max)
            : ctrl->value;
        onParamChange(ctrl->cc, ccVal);
    }
    repaint();
}

// ─────────────────────────────────────────────
// AnarackEditor
// ─────────────────────────────────────────────

AnarackEditor::AnarackEditor(AnarackProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(800, 600);
    setResizable(true, true);

    hostLabel.setText("Server:", juce::dontSendNotification);
    hostLabel.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(hostLabel);

    hostInput.setText(processor.serverHost);
    hostInput.setFont(juce::FontOptions(12.0f));
    hostInput.onReturnKey = [this] { toggleConnection(); };
    addAndMakeVisible(hostInput);

    wgToggle.setButtonText("WG");
    wgToggle.setToggleState(processor.useWireGuard, juce::dontSendNotification);
    addAndMakeVisible(wgToggle);

    connectButton.setButtonText("Connect");
    connectButton.onClick = [this] { toggleConnection(); };
    addAndMakeVisible(connectButton);

    statusLabel.setText("Disconnected", juce::dontSendNotification);
    statusLabel.setFont(juce::FontOptions(11.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(statusLabel);

    // Synth panel in a scrollable viewport
    synthPanel.onParamChange = [this](int cc, int value) {
        processor.getTransport().sendMidi(
            (const uint8_t[]){ 0xB0, (uint8_t)cc, (uint8_t)value }, 3);
    };
    viewport.setViewedComponent(&synthPanel, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    startTimerHz(10);
}

AnarackEditor::~AnarackEditor()
{
    stopTimer();
}

void AnarackEditor::toggleConnection()
{
    auto& transport = processor.getTransport();

    if (transport.isConnected())
    {
        transport.disconnect();
        connectButton.setButtonText("Connect");
        definitionLoaded = false;
    }
    else
    {
        processor.serverHost = hostInput.getText();
        processor.useWireGuard = wgToggle.getToggleState();

        if (processor.useWireGuard)
        {
            auto endpoint = processor.wgEndpoint + ":" + juce::String(processor.wgPort);
            transport.connectWireGuard(endpoint, processor.wgServerPubkey);
        }
        else
        {
            transport.connect(processor.serverHost);
        }
        connectButton.setButtonText("Disconnect");

        // Fetch the synth definition
        fetchDefinition();
    }
}

void AnarackEditor::fetchDefinition()
{
    // Fetch definition from server via a background thread
    auto host = processor.serverHost;
    juce::Thread::launch([this, host]() {
        auto url = juce::URL("http://" + host + ":8080/api/synth/definition");
        auto stream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                                .withConnectionTimeoutMs(3000));
        if (stream)
        {
            auto json = stream->readEntireStreamAsString();
            juce::MessageManager::callAsync([this, json]() {
                synthPanel.setSize(viewport.getWidth(), 2000);
                synthPanel.loadDefinition(json);
                definitionLoaded = true;
            });
        }
    });
}

void AnarackEditor::timerCallback()
{
    auto& transport = processor.getTransport();
    bool conn = transport.isConnected();

    auto statusText = juce::String("Disconnected");
    if (conn)
    {
        float bufMs = (float)transport.getBufferLevel() / 48.0f;
        int rtt = transport.getEstimatedRtt();
        statusText = (transport.isWireGuard() ? "WG" : "LAN");
        statusText += " | Buf: " + juce::String(bufMs, 0) + "ms";
        statusText += " | Pkts: " + juce::String(transport.getPacketsReceived());
        if (rtt > 0) statusText += " | RTT: " + juce::String(rtt) + "ms";
    }

    statusLabel.setText(statusText, juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId,
                          conn ? juce::Colour(0xff4ade80) : juce::Colours::grey);
}

void AnarackEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a0a0a));
}

void AnarackEditor::resized()
{
    auto area = getLocalBounds();

    // Connection bar at top (30px)
    auto topBar = area.removeFromTop(30).reduced(4, 2);
    hostLabel.setBounds(topBar.removeFromLeft(50));
    connectButton.setBounds(topBar.removeFromRight(80));
    topBar.removeFromRight(4);
    wgToggle.setBounds(topBar.removeFromRight(40));
    topBar.removeFromRight(4);
    hostInput.setBounds(topBar.removeFromLeft(150));
    topBar.removeFromLeft(8);
    statusLabel.setBounds(topBar);

    // Synth panel viewport fills the rest
    viewport.setBounds(area);
    synthPanel.setSize(area.getWidth(), std::max(synthPanel.getHeight(), area.getHeight()));
}
