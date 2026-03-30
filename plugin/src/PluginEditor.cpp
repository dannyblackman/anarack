#include "PluginEditor.h"

// ═══════════════════════════════════════════════
// SynthPanel — hard-coded Rev2 front panel layout
// ═══════════════════════════════════════════════

void SynthPanel::loadDefinition(const juce::String& jsonStr)
{
    allControls.clear();
    positioned.clear();
    labels.clear();

    auto root = juce::JSON::parse(jsonStr);
    if (!root.isObject()) return;

    auto enums = root.getProperty("enums", {});
    auto groupsArr = root.getProperty("groups", {});
    if (!groupsArr.isArray()) return;

    for (auto& g : *groupsArr.getArray())
    {
        auto paramsArr = g.getProperty("parameters", {});
        if (!paramsArr.isArray()) continue;
        for (auto& p : *paramsArr.getArray())
        {
            auto* c = new SynthControl();
            c->id = p.getProperty("id", "").toString();
            c->name = p.getProperty("name", "").toString();
            c->type = p.getProperty("type", "knob").toString();
            c->cc = (int)p.getProperty("cc", -1);
            c->min = (int)p.getProperty("min", 0);
            c->max = (int)p.getProperty("max", 127);
            c->value = (int)p.getProperty("default", 0);
            auto enumRef = p.getProperty("enumRef", "").toString();
            if (enumRef.isNotEmpty() && enums.isObject())
            {
                auto ea = enums.getProperty(enumRef, {});
                if (ea.isArray()) for (auto& v : *ea.getArray()) c->values.add(v.toString());
            }
            auto va = p.getProperty("values", {});
            if (va.isArray()) for (auto& v : *va.getArray()) c->values.add(v.toString());
            allControls.add(c);
        }
    }

    auto lay = root.getProperty("layout", {});
    if (lay.isObject())
    {
        auto cs = lay.getProperty("accentColor", "").toString();
        if (cs.isNotEmpty()) accent = juce::Colour::fromString("FF" + cs.trimCharactersAtStart("#"));
    }

    buildRev2Layout();
    repaint();
}

SynthControl* SynthPanel::findByName(const juce::String& n)
{
    for (auto* c : allControls) if (c->name == n) return c;
    return nullptr;
}
SynthControl* SynthPanel::findByCC(int cc)
{
    for (auto* c : allControls) if (c->cc == cc) return c;
    return nullptr;
}
void SynthPanel::addCtrl(int x, int y, SynthControl* c) { if (c) positioned.push_back({c, x, y}); }
void SynthPanel::addLabel(const juce::String& n, int x, int y, int w) { labels.push_back({n, x, y, w}); }

SynthControl* SynthPanel::findById(const juce::String& id)
{
    for (auto* c : allControls) if (c->id == id) return c;
    return nullptr;
}

void SynthPanel::buildRev2Layout()
{
    // Matching the Rev2 front panel diagram
    // Top row: LFOs | Effects | Aux Envelope
    // Bottom row: OSC1 | OSC2 | Mixer | Filter | Filter Env | Amp | Amp Env

    int x, y;
    const int S = 12; // section gap
    auto c = [&](int dx, int dy, const char* id) { addCtrl(x+dx*CW, y+dy, findById(id)); };

    // ═══ TOP ROW ═══
    x = 0; y = 0;
    addLabel("LFO 1", x, y, 3*CW); y=14;
    c(0,y,"lfo1Freq"); c(1,y,"lfo1Shape"); c(2,y,"lfo1Amt");
    x += 3*CW+S; y=0;

    addLabel("LFO 2", x, y, 3*CW); y=14;
    c(0,y,"lfo2Freq"); c(1,y,"lfo2Shape"); c(2,y,"lfo2Amt");
    x += 3*CW+S; y=0;

    addLabel("LFO 3", x, y, 3*CW); y=14;
    c(0,y,"lfo3Freq"); c(1,y,"lfo3Shape"); c(2,y,"lfo3Amt");
    x += 3*CW+S; y=0;

    addLabel("LFO 4", x, y, 3*CW); y=14;
    c(0,y,"lfo4Freq"); c(1,y,"lfo4Shape"); c(2,y,"lfo4Amt");
    x += 3*CW+S; y=0;

    addLabel("EFFECTS", x, y, 5*CW); y=14;
    c(0,y,"fxType"); c(1,y,"fxOnOff"); c(2,y,"fxMix"); c(3,y,"fxParam1"); c(4,y,"fxParam2");
    x += 5*CW+S; y=0;

    addLabel("AUX ENVELOPE", x, y, 7*CW); y=14;
    c(0,y,"env3Dest"); c(1,y,"env3Amt"); c(2,y,"env3Vel");
    c(3,y,"env3Delay"); c(4,y,"env3Attack"); c(5,y,"env3Decay"); c(6,y,"env3Sustain");
    int topW = x + 7*CW;

    // ═══ BOTTOM ROW ═══
    x = 0; y = CH+28;
    int y0 = y;

    addLabel("OSC 1", x, y, 5*CW); y=y0+14;
    c(0,y,"osc1Freq"); c(1,y,"osc1FineTune"); c(2,y,"osc1Shape"); c(3,y,"osc1PulseWidth"); c(4,y,"osc1Glide");
    x += 5*CW+S; y=y0;

    addLabel("OSC 2", x, y, 5*CW); y=y0+14;
    c(0,y,"osc2Freq"); c(1,y,"osc2FineTune"); c(2,y,"osc2Shape"); c(3,y,"osc2PulseWidth"); c(4,y,"osc2Glide");
    x += 5*CW+S; y=y0;

    addLabel("MIXER", x, y, 4*CW); y=y0+14;
    c(0,y,"oscMix"); c(1,y,"subOsc"); c(2,y,"noiseLevel"); c(3,y,"oscSlop");
    x += 4*CW+S; y=y0;

    addLabel("LOW-PASS FILTER", x, y, 4*CW); y=y0+14;
    c(0,y,"filterFreq"); c(1,y,"filterRes"); c(2,y,"filterKeyAmt"); c(3,y,"filterAudioMod");
    x += 4*CW+S; y=y0;

    addLabel("FILTER ENVELOPE", x, y, 7*CW); y=y0+14;
    c(0,y,"filterEnvAmt"); c(1,y,"filterEnvVel"); c(2,y,"filterEnvDelay");
    c(3,y,"filterEnvAttack"); c(4,y,"filterEnvDecay"); c(5,y,"filterEnvSustain"); c(6,y,"filterEnvRelease");
    x += 7*CW+S; y=y0;

    addLabel("AMPLIFIER", x, y, 3*CW); y=y0+14;
    c(0,y,"vcaLevel"); c(1,y,"voiceVolume"); c(2,y,"panSpread");
    x += 3*CW+S; y=y0;

    addLabel("AMP ENVELOPE", x, y, 7*CW); y=y0+14;
    c(0,y,"ampEnvAmt"); c(1,y,"ampEnvVel"); c(2,y,"ampEnvDelay");
    c(3,y,"ampEnvAttack"); c(4,y,"ampEnvDecay"); c(5,y,"ampEnvSustain"); c(6,y,"ampEnvRelease");
    int botW = x + 7*CW;

    setSize(std::max(topW, botW) + 20, 2*(CH+28) + 10);
}

void SynthPanel::setParamValue(int cc, int value)
{
    for (auto& pc : positioned)
        if (pc.ctrl && pc.ctrl->cc == cc) { pc.ctrl->value = value; repaint(); return; }
}

SynthControl* SynthPanel::getControlAt(juce::Point<float> pos)
{
    float scale = std::min((float)getParentWidth() / (float)getWidth(), 1.0f);
    if (scale <= 0) scale = 1.0f;
    auto np = pos / scale;
    for (auto& pc : positioned)
        if (pc.ctrl && juce::Rectangle<int>(pc.x, pc.y, CW, CH).toFloat().contains(np))
            return pc.ctrl;
    return nullptr;
}

void SynthPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a0a0a));

    for (auto& sl : labels)
    {
        g.setColour(accent);
        g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
        g.drawText(sl.name, sl.x, sl.y, sl.w, 12, juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff2a2a2a));
        g.drawHorizontalLine(sl.y + 12, (float)sl.x, (float)(sl.x + sl.w));
    }

    for (auto& pc : positioned)
    {
        if (!pc.ctrl) continue;
        if (pc.ctrl->type == "selector") drawSelector(g, pc.x, pc.y, pc.ctrl);
        else if (pc.ctrl->type == "toggle") drawToggle(g, pc.x, pc.y, pc.ctrl);
        else drawKnob(g, pc.x, pc.y, pc.ctrl);
    }
}

void SynthPanel::drawKnob(juce::Graphics& g, int x, int y, SynthControl* c)
{
    float cx = x + CW/2.0f, cy = y + 24.0f, r = 18.0f;
    float sa = juce::MathConstants<float>::pi * 1.25f;
    float sw = juce::MathConstants<float>::pi * 1.5f;
    float norm = c->max > 0 ? (float)c->value / (float)c->max : 0;

    juce::Path track; track.addCentredArc(cx, cy, r, r, 0, -sa, -sa + sw, true);
    g.setColour(juce::Colour(0xff333333)); g.strokePath(track, juce::PathStrokeType(2.0f));

    juce::Path arc; arc.addCentredArc(cx, cy, r, r, 0, -sa, -sa + norm*sw, true);
    g.setColour(accent); g.strokePath(arc, juce::PathStrokeType(2.5f));

    g.setColour(juce::Colour(0xff222222)); g.fillEllipse(cx-r+4, cy-r+4, (r-4)*2, (r-4)*2);
    g.setColour(juce::Colour(0xff3a3a3a)); g.drawEllipse(cx-r+4, cy-r+4, (r-4)*2, (r-4)*2, 0.5f);

    float ang = -sa + norm*sw;
    g.setColour(juce::Colours::white); g.drawLine(cx, cy, cx+std::cos(ang)*(r-5), cy+std::sin(ang)*(r-5), 2.0f);

    g.setColour(juce::Colour(0xff666666)); g.setFont(juce::FontOptions(7.5f));
    g.drawText(juce::String(c->value), (int)(cx-12), (int)(cy-4), 24, 10, juce::Justification::centred);

    g.setColour(juce::Colour(0xff888888)); g.setFont(juce::FontOptions(7.0f));
    g.drawText(c->name, x, y+CH-12, CW, 10, juce::Justification::centred);
}

void SynthPanel::drawSelector(juce::Graphics& g, int x, int y, SynthControl* c)
{
    g.setColour(juce::Colour(0xff888888)); g.setFont(juce::FontOptions(7.0f));
    g.drawText(c->name, x, y+4, CW, 10, juce::Justification::centred);
    juce::String val = (c->value >= 0 && c->value < c->values.size()) ? c->values[c->value] : juce::String(c->value);
    g.setColour(accent); g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
    g.drawText(val, x, y+22, CW, 12, juce::Justification::centred);
    g.setColour(juce::Colour(0xff888888)); g.setFont(juce::FontOptions(7.0f));
    g.drawText(c->name, x, y+CH-12, CW, 10, juce::Justification::centred);
}

void SynthPanel::drawToggle(juce::Graphics& g, int x, int y, SynthControl* c)
{
    bool on = c->value > 0;
    auto btn = juce::Rectangle<float>(x+CW/2.0f-14, y+18.0f, 28, 14);
    g.setColour(on ? accent : juce::Colour(0xff333333)); g.fillRoundedRectangle(btn, 3);
    g.setColour(on ? juce::Colours::white : juce::Colour(0xff888888));
    g.setFont(juce::FontOptions(7.5f, juce::Font::bold));
    g.drawText((c->values.size()>1 ? c->values[on?1:0] : (on?"ON":"OFF")), btn.toNearestInt(), juce::Justification::centred);
    g.setColour(juce::Colour(0xff888888)); g.setFont(juce::FontOptions(7.0f));
    g.drawText(c->name, x, y+CH-12, CW, 10, juce::Justification::centred);
}

void SynthPanel::mouseDown(const juce::MouseEvent& e)
{
    dragCtrl = getControlAt(e.position);
    if (!dragCtrl) return;
    if (dragCtrl->type == "toggle") {
        dragCtrl->value = dragCtrl->value > 0 ? 0 : 1;
        if (onParamChange && dragCtrl->cc >= 0) onParamChange(dragCtrl->cc, dragCtrl->value);
        repaint(); dragCtrl = nullptr; return;
    }
    if (dragCtrl->type == "selector") {
        int n = dragCtrl->values.size(); if (!n) n = dragCtrl->max+1;
        dragCtrl->value = (dragCtrl->value+1) % n;
        if (onParamChange && dragCtrl->cc >= 0) {
            int cv = dragCtrl->max>127 ? juce::roundToInt(dragCtrl->value*127.0/dragCtrl->max) : dragCtrl->value;
            onParamChange(dragCtrl->cc, cv);
        }
        repaint(); dragCtrl = nullptr; return;
    }
    dragStartY = e.position.y; dragStartVal = dragCtrl->value;
}

void SynthPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (!dragCtrl || dragCtrl->type != "knob") return;
    float dy = dragStartY - e.position.y;
    int nv = juce::jlimit(dragCtrl->min, dragCtrl->max, dragStartVal + (int)(dy * 0.7f));
    if (nv != dragCtrl->value) {
        dragCtrl->value = nv;
        if (onParamChange && dragCtrl->cc >= 0) {
            int cv = dragCtrl->max>127 ? juce::roundToInt(dragCtrl->value*127.0/dragCtrl->max) : dragCtrl->value;
            onParamChange(dragCtrl->cc, cv);
        }
        repaint();
    }
}

void SynthPanel::mouseUp(const juce::MouseEvent&) { dragCtrl = nullptr; }

void SynthPanel::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    auto* c = getControlAt(e.position);
    if (!c || c->type != "knob") return;
    c->value = juce::jlimit(c->min, c->max, c->value + (w.deltaY > 0 ? 1 : -1));
    if (onParamChange && c->cc >= 0) {
        int cv = c->max>127 ? juce::roundToInt(c->value*127.0/c->max) : c->value;
        onParamChange(c->cc, cv);
    }
    repaint();
}

// ═══════════════════════════════════════════════
// AnarackEditor
// ═══════════════════════════════════════════════

AnarackEditor::AnarackEditor(AnarackProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(1200, 220);
    setResizable(true, true);

    hostLabel.setText("Server:", juce::dontSendNotification);
    hostLabel.setFont(juce::FontOptions(12.0f)); addAndMakeVisible(hostLabel);
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

    synthPanel.onParamChange = [this](int cc, int value) {
        processor.getTransport().sendMidi((const uint8_t[]){0xB0,(uint8_t)cc,(uint8_t)value}, 3);
    };
    viewport.setViewedComponent(&synthPanel, false);
    viewport.setScrollBarsShown(false, true);
    addAndMakeVisible(viewport);
    startTimerHz(10);
}

AnarackEditor::~AnarackEditor() { stopTimer(); }

void AnarackEditor::toggleConnection()
{
    auto& t = processor.getTransport();
    if (t.isConnected()) { t.disconnect(); connectButton.setButtonText("Connect"); defLoaded=false; }
    else {
        processor.serverHost = hostInput.getText();
        processor.useWireGuard = wgToggle.getToggleState();
        if (processor.useWireGuard) {
            auto ep = processor.wgEndpoint + ":" + juce::String(processor.wgPort);
            t.connectWireGuard(ep, processor.wgServerPubkey);
        } else t.connect(processor.serverHost);
        connectButton.setButtonText("Disconnect");
        fetchDefinition();
    }
}

void AnarackEditor::fetchDefinition()
{
    auto host = processor.serverHost;
    juce::Thread::launch([this, host]() {
        auto url = juce::URL("http://" + host + ":8080/api/synth/definition");
        auto stream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs(3000));
        if (stream) {
            auto json = stream->readEntireStreamAsString();
            juce::MessageManager::callAsync([this, json]() { synthPanel.loadDefinition(json); defLoaded=true; resized(); });
        }
    });
}

void AnarackEditor::timerCallback()
{
    auto& t = processor.getTransport();
    bool c = t.isConnected();
    auto s = juce::String(c ? (t.isWireGuard() ? "WG" : "LAN") : "Disconnected");
    if (c) {
        s += " | Buf: " + juce::String((float)t.getBufferLevel()/48.0f,0) + "ms";
        s += " | Pkts: " + juce::String(t.getPacketsReceived());
        int r = t.getEstimatedRtt(); if (r>0) s += " | RTT: " + juce::String(r) + "ms";
    }
    statusLabel.setText(s, juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, c ? juce::Colour(0xff4ade80) : juce::Colours::grey);
}

void AnarackEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xff0a0a0a)); }

void AnarackEditor::resized()
{
    auto area = getLocalBounds();
    auto top = area.removeFromTop(26).reduced(4, 2);
    hostLabel.setBounds(top.removeFromLeft(50));
    connectButton.setBounds(top.removeFromRight(80));
    top.removeFromRight(4); wgToggle.setBounds(top.removeFromRight(40));
    top.removeFromRight(4); hostInput.setBounds(top.removeFromLeft(150));
    top.removeFromLeft(8); statusLabel.setBounds(top);
    viewport.setBounds(area);
    synthPanel.setSize(std::max(synthPanel.getWidth(), area.getWidth()), area.getHeight());
}
