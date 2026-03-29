#pragma once
#include <juce_core/juce_core.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <atomic>
#include <thread>

extern "C" {
#include "wireguard_ffi.h"
}

// Wraps boringtun's C FFI into a simple send/recv interface.
// Manages the WireGuard tunnel, the real UDP socket, and the tick timer.
// All crypto happens on background threads — safe to call send() from any thread.
class WgTunnel
{
public:
    WgTunnel();
    ~WgTunnel();

    // Connect to a WireGuard endpoint.
    // myPrivkeyB64: our base64 private key
    // serverPubkeyB64: server's base64 public key
    // serverEndpoint: "ip:port" of the server's WireGuard listener
    // tunnelLocalIp: our IP inside the tunnel (e.g., "10.0.0.2")
    // tunnelRemoteIp: server's IP inside the tunnel (e.g., "10.0.0.1")
    bool connect(const juce::String& myPrivkeyB64,
                 const juce::String& serverPubkeyB64,
                 const juce::String& serverEndpoint,
                 const juce::String& tunnelLocalIp = "10.0.0.2",
                 const juce::String& tunnelRemoteIp = "10.0.0.1");

    void disconnect();
    bool isConnected() const { return connected.load(); }

    // Send raw application data through the tunnel.
    // dstPort: the destination port inside the tunnel (e.g., 5555 for MIDI)
    // srcPort: source port (can be anything, e.g., 0)
    int send(const void* data, int len, uint16_t dstPort, uint16_t srcPort = 12345);

    // Receive raw application data from the tunnel.
    // Returns bytes received, or -1 if nothing available.
    // srcPort is set to the source port from the inner UDP header.
    int recv(void* buf, int maxLen, uint16_t& srcPort);

    // Get WireGuard stats
    struct stats getStats() const;

    // Generate a new keypair. Returns base64 private key and public key.
    static std::pair<juce::String, juce::String> generateKeypair();

private:
    void recvLoop();       // Background thread: reads from real UDP socket, decrypts
    void tickLoop();       // Background thread: calls wireguard_tick every 100ms
    void sendToNetwork(const uint8_t* data, size_t len);
    void processWgResult(struct wireguard_result result, uint8_t* buf);

    // Build a minimal IP + UDP packet wrapping application data
    int buildIpUdpPacket(uint8_t* out, int maxLen,
                         const void* payload, int payloadLen,
                         uint16_t srcPort, uint16_t dstPort);

    // Extract payload from an IP + UDP packet
    int extractPayload(const uint8_t* ipPacket, int ipLen,
                       void* payload, int maxLen, uint16_t& srcPort);

    struct wireguard_tunnel* tunnel = nullptr;
    int udpFd = -1;                          // Real UDP socket to server
    struct sockaddr_in serverAddr {};

    uint32_t localIp = 0;   // Network byte order
    uint32_t remoteIp = 0;  // Network byte order

    std::atomic<bool> connected { false };
    std::unique_ptr<std::thread> recvThread;
    std::unique_ptr<std::thread> tickThread;

    // Lock-free queue for received decrypted payloads
    // Simple ring buffer: recv thread writes, app reads
    static constexpr int RECV_QUEUE_SIZE = 512;
    static constexpr int RECV_BUF_SIZE = 2048;
    struct RecvPacket {
        uint8_t data[RECV_BUF_SIZE];
        int len = 0;
        uint16_t srcPort = 0;
        std::atomic<bool> ready { false };
    };
    RecvPacket recvQueue[RECV_QUEUE_SIZE];
    std::atomic<int> recvWriteIdx { 0 };
    std::atomic<int> recvReadIdx { 0 };

    // Mutex for sendToNetwork (called from multiple threads: send + tick + recv)
    std::mutex sendMutex;

    // Reusable buffers (not thread-safe — only used under sendMutex or in single-threaded context)
    uint8_t wgSendBuf[MAX_WIREGUARD_PACKET_SIZE];
    uint8_t wgRecvBuf[MAX_WIREGUARD_PACKET_SIZE];
    uint8_t ipBuildBuf[2048];
};
