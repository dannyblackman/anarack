#include "SessionClient.h"
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

SessionInfo SessionClient::createSession(const juce::String& piId)
{
    SessionInfo info;

    // Generate ephemeral WireGuard keypair
    auto [priv, pub] = WgTunnel::generateKeypair();
    if (priv.isEmpty() || pub.isEmpty())
    {
        DBG("SessionClient: failed to generate keypair");
        return info;
    }
    ephemeralPrivKey = priv;
    ephemeralPubKey = pub;

    DBG("SessionClient: generated ephemeral keypair, pub=" + pub.substring(0, 16) + "...");

    // POST /sessions
    juce::String body = "{\"pi_id\":\"" + piId + "\",\"plugin_pubkey\":\"" + pub + "\"}";
    auto response = httpRequest("POST", "/sessions", body);

    if (response.isEmpty())
    {
        DBG("SessionClient: no response from API");
        return info;
    }

    // Parse JSON response
    auto json = juce::JSON::parse(response);
    if (json.isVoid())
    {
        DBG("SessionClient: invalid JSON: " + response);
        return info;
    }

    info.sessionId = json.getProperty("session_id", "").toString();
    info.piEndpoint = json.getProperty("pi_endpoint", "").toString();
    info.piPubkey = json.getProperty("pi_pubkey", "").toString();
    info.piLocalIp = json.getProperty("pi_local_ip", "").toString();
    info.piWgPort = (int)json.getProperty("pi_wg_port", 51820);
    info.relayEndpoint = json.getProperty("relay_endpoint", "").toString();
    info.valid = info.piPubkey.isNotEmpty();

    DBG("SessionAPI: session=" + info.sessionId
        + " piEndpoint=" + info.piEndpoint
        + " piPubkey=" + info.piPubkey.substring(0, 16) + "..."
        + " piLocalIp=" + info.piLocalIp
        + " piWgPort=" + juce::String(info.piWgPort));

    return info;
}

void SessionClient::endSession(const juce::String& sessionId)
{
    if (sessionId.isNotEmpty())
        httpRequest("DELETE", "/sessions/" + sessionId);
}

juce::String SessionClient::httpRequest(const juce::String& method,
                                         const juce::String& path,
                                         const juce::String& body)
{
    // Parse host and port from apiUrl
    auto url = apiUrl;
    if (url.startsWith("http://"))
        url = url.substring(7);

    auto host = url.upToFirstOccurrenceOf(":", false, false);
    auto portStr = url.fromFirstOccurrenceOf(":", false, false);
    int port = portStr.isNotEmpty() ? portStr.getIntValue() : 80;

    if (host.isEmpty()) return {};

    // Resolve hostname
    struct addrinfo hints = {}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int err = getaddrinfo(host.toRawUTF8(), nullptr, &hints, &res);
    if (err != 0 || res == nullptr) return {};

    auto* addr = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
    addr->sin_port = htons((uint16_t)port);

    // Connect TCP socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { freeaddrinfo(res); return {}; }

    // Set timeout
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (::connect(fd, (struct sockaddr*)addr, sizeof(*addr)) < 0)
    {
        freeaddrinfo(res);
        ::close(fd);
        return {};
    }
    freeaddrinfo(res);

    // Build HTTP request
    juce::String request = method + " " + path + " HTTP/1.1\r\n"
        + "Host: " + host + "\r\n"
        + "Connection: close\r\n";

    if (body.isNotEmpty())
    {
        request += juce::String("Content-Type: application/json\r\n")
            + "Content-Length: " + juce::String(body.length()) + "\r\n";
    }
    request += "\r\n";
    if (body.isNotEmpty())
        request += body;

    // Send
    auto reqBytes = request.toRawUTF8();
    ::send(fd, reqBytes, (int)std::strlen(reqBytes), 0);

    // Read response
    juce::String response;
    char buf[4096];
    int n;
    while ((n = (int)::recv(fd, buf, sizeof(buf) - 1, 0)) > 0)
    {
        buf[n] = 0;
        response += buf;
    }
    ::close(fd);

    // Extract body (after \r\n\r\n)
    auto bodyStart = response.indexOf("\r\n\r\n");
    if (bodyStart < 0) return {};

    return response.substring(bodyStart + 4);
}
