#pragma once
#include <juce_core/juce_core.h>
#include "WgTunnel.h"

// Minimal HTTP client for the Anarack Session API.
// Fetches session info (Pi endpoint, pubkey) so the plugin can connect
// with ephemeral keys instead of hardcoded ones.

struct SessionInfo
{
    juce::String sessionId;
    juce::String piEndpoint;      // Pi's public IP:port (for hole punching)
    juce::String piPubkey;        // Pi's WireGuard public key
    juce::String piLocalIp;       // Pi's LAN IP (for same-network detection)
    int piWgPort = 51820;         // Pi's WireGuard listen port
    juce::String relayEndpoint;   // Fallback VPS relay
    bool valid = false;
};

class SessionClient
{
public:
    SessionClient() = default;

    // Set the session API base URL (e.g., "http://anarack.local:8800")
    void setApiUrl(const juce::String& url) { apiUrl = url; }

    // Request a new session. Generates ephemeral WireGuard keypair.
    // Returns session info including Pi's pubkey and endpoint.
    SessionInfo createSession(const juce::String& piId);

    // End a session (cleanup)
    void endSession(const juce::String& sessionId);

    // Get the ephemeral private key from the last createSession call
    juce::String getPrivateKey() const { return ephemeralPrivKey; }

private:
    // Simple synchronous HTTP request. Returns response body or empty on error.
    juce::String httpRequest(const juce::String& method, const juce::String& path,
                             const juce::String& body = {});

    juce::String apiUrl { "http://anarack.local:8800" };
    juce::String ephemeralPrivKey;
    juce::String ephemeralPubKey;
};
