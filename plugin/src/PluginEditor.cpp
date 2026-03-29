#include "PluginEditor.h"

AnarackEditor::AnarackEditor(AnarackProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(340, 200);

    titleLabel.setText("ANARACK REV2", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff6366f1));
    addAndMakeVisible(titleLabel);

    hostLabel.setText("Server:", juce::dontSendNotification);
    hostLabel.setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(hostLabel);

    hostInput.setText(processor.serverHost);
    hostInput.setFont(juce::FontOptions(13.0f));
    hostInput.onReturnKey = [this] { toggleConnection(); };
    addAndMakeVisible(hostInput);

    connectButton.setButtonText("Connect");
    connectButton.onClick = [this] { toggleConnection(); };
    addAndMakeVisible(connectButton);

    statusLabel.setText("Disconnected", juce::dontSendNotification);
    statusLabel.setFont(juce::FontOptions(13.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(statusLabel);

    bufferLabel.setText("Buffer: —", juce::dontSendNotification);
    bufferLabel.setFont(juce::FontOptions(12.0f));
    bufferLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(bufferLabel);

    packetLabel.setText("Packets: 0", juce::dontSendNotification);
    packetLabel.setFont(juce::FontOptions(12.0f));
    packetLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(packetLabel);

    startTimerHz(10); // Update UI 10 times per second
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
        transport.connect(processor.serverHost);
        connectButton.setButtonText("Disconnect");
    }
}

void AnarackEditor::timerCallback()
{
    auto& transport = processor.getTransport();
    bool conn = transport.isConnected();

    statusLabel.setText(conn ? "Connected" : "Disconnected", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId,
                          conn ? juce::Colour(0xff4ade80) : juce::Colours::grey);

    if (conn)
    {
        float bufMs = (float)transport.getBufferLevel() / 48.0f; // ms at 48kHz
        bufferLabel.setText("Buffer: " + juce::String(bufMs, 1) + " ms", juce::dontSendNotification);
        packetLabel.setText("Packets: " + juce::String(transport.getPacketsReceived()), juce::dontSendNotification);
    }
}

void AnarackEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a0a0a));
}

void AnarackEditor::resized()
{
    auto area = getLocalBounds().reduced(16);

    titleLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(8);

    auto row = area.removeFromTop(28);
    hostLabel.setBounds(row.removeFromLeft(55));
    connectButton.setBounds(row.removeFromRight(90));
    row.removeFromRight(8);
    hostInput.setBounds(row);

    area.removeFromTop(12);
    statusLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(4);
    bufferLabel.setBounds(area.removeFromTop(18));
    packetLabel.setBounds(area.removeFromTop(18));
}
