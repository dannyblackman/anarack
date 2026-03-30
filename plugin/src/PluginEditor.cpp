#include "PluginEditor.h"

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

    // WebView for the synth panel
    webView = std::make_unique<juce::WebBrowserComponent>();
    addAndMakeVisible(*webView);

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

        // Load the synth panel UI in the WebView
        // For now, load from the server's WebSocket endpoint
        auto isLocal = processor.serverHost.contains("192.168") || processor.serverHost.contains("localhost");
        auto proto = isLocal ? "ws" : "wss";
        // The WebView will load the bundled HTML which connects to the server
        auto htmlFile = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                            .getParentDirectory().getChildFile("ui").getChildFile("index.html");
        if (htmlFile.existsAsFile())
            webView->goToURL(htmlFile.getFullPathName());
    }
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

    // WebView fills the rest
    if (webView)
        webView->setBounds(area);
}
