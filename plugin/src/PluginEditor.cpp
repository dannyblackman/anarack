#include "PluginEditor.h"
#include "BinaryData.h"

// ═══════════════════════════════════════════════
// AnarackEditor — WebView-based synth panel
// ═══════════════════════════════════════════════

AnarackEditor::AnarackEditor(AnarackProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(1400, 600);
    setResizable(true, true);

    // ── Connection bar ──
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

    // ── WebView panel ──
    auto options = juce::WebBrowserComponent::Options{}
        .withNativeIntegrationEnabled()
        .withEventListener("ccChange", [this](const juce::var& payload)
        {
            // JS sends: { cc: N, value: V }
            int cc = (int)payload.getProperty("cc", -1);
            int value = (int)payload.getProperty("value", 0);
            if (cc >= 0 && cc <= 127 && value >= 0 && value <= 127)
            {
                const uint8_t msg[3] = { 0xB0, (uint8_t)cc, (uint8_t)value };
                processor.getTransport().sendMidi(msg, 3);
            }
        })
        .withResourceProvider([](const juce::String& url) -> std::optional<juce::WebBrowserComponent::Resource>
        {
            // Serve rev2-panel.html for root request
            if (url == "/" || url.isEmpty())
            {
                juce::WebBrowserComponent::Resource res;
                res.data = std::vector<std::byte>(
                    reinterpret_cast<const std::byte*>(BinaryData::rev2panel_html),
                    reinterpret_cast<const std::byte*>(BinaryData::rev2panel_html) + BinaryData::rev2panel_htmlSize
                );
                res.mimeType = "text/html";
                return res;
            }
            return std::nullopt;
        })
        .withBackend(juce::WebBrowserComponent::Options::Backend::defaultBackend);

    webView = std::make_unique<juce::WebBrowserComponent>(options);
    addAndMakeVisible(*webView);
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

    startTimerHz(10);
}

AnarackEditor::~AnarackEditor() { stopTimer(); }

void AnarackEditor::toggleConnection()
{
    auto& t = processor.getTransport();
    if (t.isConnected())
    {
        t.disconnect();
        connectButton.setButtonText("Connect");
    }
    else
    {
        processor.serverHost = hostInput.getText();
        processor.useWireGuard = wgToggle.getToggleState();
        if (processor.useWireGuard)
        {
            auto ep = processor.wgEndpoint + ":" + juce::String(processor.wgPort);
            t.connectWireGuard(ep, processor.wgServerPubkey);
        }
        else
            t.connect(processor.serverHost);
        connectButton.setButtonText("Disconnect");
    }
}

void AnarackEditor::timerCallback()
{
    auto& t = processor.getTransport();
    bool c = t.isConnected();
    auto s = juce::String(c ? (t.isWireGuard() ? "WG" : "LAN") : "Disconnected");
    if (c)
    {
        s += " | Buf: " + juce::String((float)t.getBufferLevel() / 48.0f, 0) + "ms";
        s += " | Pkts: " + juce::String(t.getPacketsReceived());
        int r = t.getEstimatedRtt();
        if (r > 0) s += " | RTT: " + juce::String(r) + "ms";
    }
    statusLabel.setText(s, juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, c ? juce::Colour(0xff4ade80) : juce::Colours::grey);
}

void AnarackEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a0a0a));
}

void AnarackEditor::resized()
{
    auto area = getLocalBounds();
    auto top = area.removeFromTop(26).reduced(4, 2);
    hostLabel.setBounds(top.removeFromLeft(50));
    connectButton.setBounds(top.removeFromRight(80));
    top.removeFromRight(4);
    wgToggle.setBounds(top.removeFromRight(40));
    top.removeFromRight(4);
    hostInput.setBounds(top.removeFromLeft(150));
    top.removeFromLeft(8);
    statusLabel.setBounds(top);
    webView->setBounds(area);
}
