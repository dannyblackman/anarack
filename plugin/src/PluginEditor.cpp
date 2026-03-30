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
        // CC changes from knobs/buttons
        .withEventListener("ccChange", [this](const juce::var& payload)
        {
            int cc = (int)payload.getProperty("cc", -1);
            int value = (int)payload.getProperty("value", 0);
            if (cc >= 0 && cc <= 127 && value >= 0 && value <= 127)
            {
                const uint8_t msg[3] = { 0xB0, (uint8_t)cc, (uint8_t)value };
                processor.getTransport().sendMidi(msg, 3);
                processor.ccValues[cc] = value; // keep in sync for relative controllers
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
                auto& t = processor.getTransport();
                if (lan)
                    t.connect(host);
                else
                {
                    auto ep = processor.wgEndpoint + ":" + juce::String(processor.wgPort);
                    t.connectWireGuard(ep, processor.wgServerPubkey);
                }
            }
        })
        // Disconnect button
        .withEventListener("doDisconnect", [this](const juce::var&)
        {
            processor.getTransport().disconnect();
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

    // Send initConfig once WebView is ready (slight delay for page load)
    juce::Timer::callAfterDelay(500, [this]()
    {
        if (webView)
        {
            auto obj = juce::DynamicObject::Ptr(new juce::DynamicObject());
            obj->setProperty("host", processor.serverHost);
            obj->setProperty("lan", !processor.useWireGuard);
            webView->emitEventIfBrowserIsVisible("initConfig", juce::var(obj.get()));
        }
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
        state->setProperty("mode", c ? juce::String(t.isWireGuard() ? "WireGuard" : "LAN") : juce::String());
        state->setProperty("rtt", c ? t.getEstimatedRtt() : 0);
        state->setProperty("bufferMs", c ? (float)t.getBufferLevel() / 48.0f : 0.0f);
        state->setProperty("midiIn", processor.midiInCount.load(std::memory_order_relaxed));
        state->setProperty("mappedSends", processor.mappedSendCount.load(std::memory_order_relaxed));
        state->setProperty("learning", processor.learnTargetCC.load() >= 0);
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
