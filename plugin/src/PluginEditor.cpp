#include "PluginEditor.h"
#include "BinaryData.h"

// ═══════════════════════════════════════════════
// AnarackEditor — WebView-based synth panel
// ═══════════════════════════════════════════════

AnarackEditor::AnarackEditor(AnarackProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    // ── WebView with event wiring ──
    auto options = juce::WebBrowserComponent::Options{}
        .withNativeIntegrationEnabled()
        // NRPN changes (LFOs, filter poles, sync etc.)
        .withEventListener("nrpnChange", [this](const juce::var& payload)
        {
            int nrpn = (int)payload.getProperty("nrpn", -1);
            int value = (int)payload.getProperty("value", 0);
            if (nrpn >= 0 && nrpn < 16384 && value >= 0 && value <= 127)
            {
                auto& t = processor.getTransport();
                // NRPN = 4 CC messages: CC99 (MSB), CC98 (LSB), CC6 (value MSB), CC38 (value LSB)
                const uint8_t nrpnMsb[3] = { 0xB0, 99, (uint8_t)(nrpn >> 7) };
                const uint8_t nrpnLsb[3] = { 0xB0, 98, (uint8_t)(nrpn & 0x7F) };
                const uint8_t dataMsb[3] = { 0xB0, 6, (uint8_t)value };
                const uint8_t dataLsb[3] = { 0xB0, 38, 0 };
                t.sendMidi(nrpnMsb, 3);
                t.sendMidi(nrpnLsb, 3);
                t.sendMidi(dataMsb, 3);
                t.sendMidi(dataLsb, 3);
            }
        })
        // CC changes from knobs/buttons
        .withEventListener("ccChange", [this](const juce::var& payload)
        {
            int cc = (int)payload.getProperty("cc", -1);
            int value = (int)payload.getProperty("value", 0);
            if (cc >= 0 && cc <= 127 && value >= 0 && value <= 127)
            {
                if (cc == 120 || cc == 121)
                {
                    // Program/Bank change: CC 120 = program, CC 121 = bank
                    // Store values and send Bank Select + Program Change
                    static int curBank = 0, curProg = 0;
                    if (cc == 120) curProg = value;
                    if (cc == 121) curBank = value;
                    const uint8_t bankMsg[3] = { 0xB0, 0x00, (uint8_t)curBank };
                    processor.getTransport().sendMidi(bankMsg, 3);
                    const uint8_t pgmMsg[2] = { 0xC0, (uint8_t)curProg };
                    processor.getTransport().sendMidi(pgmMsg, 2);
                }
                else
                {
                    const uint8_t msg[3] = { 0xB0, (uint8_t)cc, (uint8_t)value };
                    processor.getTransport().sendMidi(msg, 3);
                }
                processor.ccValues[cc] = value;
                // Sync DAW parameter so automation records correctly
                if (auto* p = processor.paramByCC[cc])
                {
                    p->setValueNotifyingHost(p->convertTo0to1((float)value));
                    processor.lastAutomationVal[cc] = value; // prevent automation fighting back
                }
            }
        })
        // Connect button
        .withEventListener("doConnect", [this](const juce::var& payload)
        {
            auto host = payload.getProperty("host", "").toString();
            bool lan = (bool)payload.getProperty("lan", false);
            if (host.isNotEmpty())
            {
                processor.serverHost = host;
                processor.useWireGuard = !lan;
                processor.reconnect();
            }
        })
        .withEventListener("doDisconnect", [this](const juce::var&)
        {
            processor.disconnectAndCleanup();
        })
        // MIDI Learn: JS sends synthCC to learn
        .withEventListener("startLearn", [this](const juce::var& payload)
        {
            int synthCC = (int)payload.getProperty("cc", -1);
            if (synthCC >= 0) processor.startLearn(synthCC);
        })
        .withEventListener("cancelLearn", [this](const juce::var&)
        {
            processor.clearLearn();
        })
        // Fixed buffer size
        .withEventListener("setBuffer", [this](const juce::var& payload)
        {
            int ms = (int)payload.getProperty("ms", 0);
            processor.setFixedBuffer(ms);
        })
        // Encoder sensitivity
        .withEventListener("setSensitivity", [this](const juce::var& payload)
        {
            int val = (int)payload.getProperty("value", 3);
            processor.encoderSensitivity.store(juce::jlimit(1, 10, val));
        })
        // Open direct MIDI input device
        .withEventListener("openMidiInput", [this](const juce::var& payload)
        {
            auto name = payload.getProperty("name", "").toString();
            processor.openDirectMidiInput(name);
        })
        // Ping button
        .withEventListener("doPing", [this](const juce::var&)
        {
            // RTT is already measured by the transport; just log it
            auto rtt = processor.getTransport().getEstimatedRtt();
            if (webView)
            {
                auto obj = juce::DynamicObject::Ptr(new juce::DynamicObject());
                obj->setProperty("msg", "RTT: " + juce::String(rtt) + "ms");
                webView->emitEventIfBrowserIsVisible("logMessage", juce::var(obj.get()));
            }
        })
        // Serve the HTML panel
        .withResourceProvider([](const juce::String& url) -> std::optional<juce::WebBrowserComponent::Resource>
        {
            if (url == "/" || url.isEmpty())
            {
                auto* data = BinaryData::rev2panel_html;
                auto size = BinaryData::rev2panel_htmlSize;
                return juce::WebBrowserComponent::Resource{
                    { reinterpret_cast<const std::byte*>(data),
                      reinterpret_cast<const std::byte*>(data) + size },
                    "text/html"
                };
            }
            return std::nullopt;
        })
        // Send initial config to JS after page loads
        .withInitialisationData("config", [this]()
        {
            auto obj = juce::DynamicObject::Ptr(new juce::DynamicObject());
            obj->setProperty("host", processor.serverHost);
            obj->setProperty("lan", !processor.useWireGuard);
            return juce::var(obj.get());
        }());

    webView = std::make_unique<juce::WebBrowserComponent>(options);
    addAndMakeVisible(*webView);
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

    // Send initConfig and auto-connect once WebView is ready
    juce::Timer::callAfterDelay(500, [this]()
    {
        if (webView)
        {
            auto obj = juce::DynamicObject::Ptr(new juce::DynamicObject());
            obj->setProperty("host", processor.serverHost);
            obj->setProperty("lan", !processor.useWireGuard);
            juce::Array<juce::var> devs;
            for (auto& name : processor.getAvailableMidiInputs())
                devs.add(name);
            obj->setProperty("midiInputs", devs);
            webView->emitEventIfBrowserIsVisible("initConfig", juce::var(obj.get()));
        }

        // Connection is handled by PluginProcessor::autoConnect (background thread)
        // No auto-connect here — the processor connects on prepareToPlay
    });

    setSize(1900, 516);
    setResizable(true, true);
    startTimerHz(10);
}

AnarackEditor::~AnarackEditor() { stopTimer(); }

void AnarackEditor::timerCallback()
{
    auto& t = processor.getTransport();
    bool c = t.isConnected();

    if (webView)
    {
        auto state = juce::DynamicObject::Ptr(new juce::DynamicObject());
        state->setProperty("connected", c);
        juce::String modeStr;
        if (c)
        {
            if (!t.isWireGuard())
                modeStr = "LAN";
            else if (processor.currentSessionId.isNotEmpty())
                modeStr = "P2P";
            else
                modeStr = "Relay";
        }
        state->setProperty("mode", modeStr);
        state->setProperty("pktSize", t.getLastPacketSize());
        state->setProperty("rtt", c ? t.getEstimatedRtt() : 0);
        int fillSamples = processor.jitterBuffer.isConfigured()
                        ? processor.jitterBuffer.getFillLevel()
                        : t.getBufferLevel();
        state->setProperty("bufferMs", c ? (float)fillSamples / 48.0f : 0.0f);
        int fixedMs = processor.fixedBufferMs.load();
        state->setProperty("bufferTarget", fixedMs > 0 ? fixedMs : -1);
        state->setProperty("totalLatency", c ? (int)(processor.getLatencySamples() * 1000.0 / 48000.0) : 0);
        state->setProperty("midiIn", processor.midiInCount.load(std::memory_order_relaxed));
        state->setProperty("mappedSends", processor.mappedSendCount.load(std::memory_order_relaxed));
        state->setProperty("learning", processor.learnTargetCC.load() >= 0);
        state->setProperty("connState", processor.connectionState.load(std::memory_order_relaxed));
        state->setProperty("autoBuffer", processor.autoDetectedBufferMs.load(std::memory_order_relaxed));
        state->setProperty("blockSize", processor.lastBlockSize.load(std::memory_order_relaxed));
        state->setProperty("asrcDrops", processor.asrcDropCount.load(std::memory_order_relaxed));
        state->setProperty("asrcDups", processor.asrcDupCount.load(std::memory_order_relaxed));
        state->setProperty("plcSamples", processor.jitterBuffer.getPlcSamples());
        state->setProperty("pktLost", processor.jitterBuffer.getPacketsLost());
        int learnFrom = processor.lastLearnedFrom.exchange(-1);
        int learnTo = processor.lastLearnedTo.exchange(-1);
        if (learnFrom >= 0 && learnTo >= 0)
        {
            state->setProperty("learnedFrom", learnFrom);
            state->setProperty("learnedTo", learnTo);
        }
        webView->emitEventIfBrowserIsVisible("connectionStatus", juce::var(state.get()));

        // Drain mapped CC ring buffer and push to JS for knob updates
        int r = processor.ccRingRead.load(std::memory_order_relaxed);
        int w = processor.ccRingWrite.load(std::memory_order_acquire);
        while (r < w)
        {
            auto& ev = processor.ccRing[r % AnarackProcessor::CC_RING_SIZE];
            auto ccObj = juce::DynamicObject::Ptr(new juce::DynamicObject());
            ccObj->setProperty("cc", (int)ev.cc);
            ccObj->setProperty("value", (int)ev.val);
            webView->emitEventIfBrowserIsVisible("paramUpdate", juce::var(ccObj.get()));
            r++;
        }
        processor.ccRingRead.store(r, std::memory_order_relaxed);
    }
}

void AnarackEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff111111));
}

void AnarackEditor::resized()
{
    if (webView) webView->setBounds(getLocalBounds());
}
