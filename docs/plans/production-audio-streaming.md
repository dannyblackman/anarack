# Production Audio Streaming — Implementation Plan

**Goal:** Rock-solid, glitch-free audio streaming from the Pi to the DAW plugin with fixed, known latency that the DAW compensates for. The audio must be absolutely clean — no crackles, clicks, or dropouts. Latency of 150-250ms is acceptable because the DAW's Plugin Delay Compensation (PDC) makes it transparent during playback.

**Key insight:** We don't need low latency. We need *fixed, reliable* latency. The DAW handles the rest.

## How DAW Latency Compensation Works (for context)

When a plugin reports `setLatencySamples(N)`, the DAW shifts all MIDI sent to that plugin forward by N samples. During playback, the user hears the hardware synth perfectly in time with every other track. This only works if:
1. The reported latency is accurate
2. The reported latency never changes during a session
3. Audio flows continuously (including silence) so the latency is always in the chain

**Important limitation:** PDC only works during playback of recorded/sequenced MIDI. When the user plays live, they hear the real round-trip latency. The intended workflow is: design sounds in preview mode (emulator, zero latency) → record MIDI → play back through hardware (PDC-compensated, perfect sync).

## Current State (what's wrong)

- Raw PCM over UDP, 128-sample packets (~2.7ms each), no metadata
- Adaptive pre-buffer that changes size based on RTT
- Packet loss = audible click (no concealment)
- Jitter tolerance is low (~20ms of buffer)
- `setLatencySamples()` changes during playback → DAW compensation is wrong
- Server only sends audio when there's sound → first-note clipping risk

## Architecture

### Packet Format

```
Current:   [128 × int16 samples] = 256 bytes, no metadata

Proposed:  [Header: 12 bytes][Payload: 256 bytes] = 268 bytes

Header:
  sequence  (uint32)  — monotonic counter, never resets during session
  timestamp (uint32)  — sample position in stream (for buffer placement)
  flags     (uint16)  — bit 0: FEC packet, bit 1: silence, bit 2: session-start
  checksum  (uint16)  — CRC-16 of header + payload (catch corruption)
```

Packets stay well under MTU limits. WireGuard adds ~60 bytes overhead → total ~328 bytes per packet, well under the ~1400 byte fragmentation threshold.

### Jitter Buffer (replaces AudioRingBuffer for network audio)

The core of the reliability improvement. A timestamp-indexed ring buffer that:

1. **Places packets by timestamp, not arrival order.** A late packet goes to its correct position, not the end.
2. **Has a fixed size determined at connection time.** Measured RTT × 2 + 50ms margin, locked for the session.
3. **Detects gaps** via sequence numbers (missing = lost packet).
4. **Deduplicates** redundant copies of the same packet (from packet duplication, see below).
5. **Reports its fixed size to the DAW** via `setLatencySamples()` once at connection. Never changes mid-session.
6. **Always contains audio** — filled with silence when no notes are playing, so the latency chain is never broken.

```cpp
class JitterBuffer
{
public:
    // Set once at connection, never changes
    void configure(int fixedSizeSamples, double sampleRate);
    int getFixedLatencySamples() const;

    // Called by network receive thread
    void writePacket(uint32_t sequence, uint32_t timestamp,
                     const int16_t* samples, int numSamples);

    // Called by audio thread
    void read(float* output, int numSamples);

    // Stats
    int getFillLevel() const;
    int getPacketsLost() const;
    int getPacketsRecovered() const;

private:
    // Timestamp-indexed ring buffer
    std::vector<float> buffer;
    int writePos = 0;    // next expected timestamp position
    int readPos = 0;     // current read position

    // Gap tracking
    uint32_t expectedSequence = 0;
    int packetsLost = 0;
    int packetsRecovered = 0;

    // Packet loss concealment
    void concealGap(int gapStartSample, int gapLengthSamples);
};
```

### Packet Loss Resilience — Packet Duplication

After evaluating FEC options:

- **XOR-based FEC** recovers single random losses but fails on burst loss (common on WiFi).
- **Reed-Solomon** handles bursts but adds complexity.
- **Packet duplication is simpler, handles bursts, and bandwidth cost is trivial.**

**Approach:** The Pi sends every audio packet twice, with the duplicate delayed by ~10ms (4-5 packets later). If the original is lost, the delayed copy arrives in time for the jitter buffer to place it correctly (we have 150ms+ of buffer runway). Both copies carry the same sequence number, so the jitter buffer deduplicates trivially.

```
Timeline:  Pkt1  Pkt2  Pkt3  Pkt4  Pkt1'  Pkt5  Pkt2'  Pkt6  Pkt3'  ...
                                     ↑              ↑              ↑
                               duplicate of 1   dup of 2      dup of 3
```

Bandwidth: ~192 KB/s × 2 = ~384 KB/s. Still trivial for any internet connection.

For both copies of a packet to be lost, the network would need to drop packets continuously for 10ms+ — at that point you have a genuine outage, not normal packet loss.

### Packet Loss Concealment (for unrecoverable losses)

When both copies of a packet are lost (rare but possible):

1. **Short gap (1-2 packets, ~5ms):** Crossfade interpolation between the last good sample and the next good sample. Nearly inaudible on sustained synth sounds.
2. **Medium gap (3-5 packets, ~13ms):** Repeat the last good packet with a gentle fade. Works well for synth pads and sustained tones.
3. **Long gap (>5 packets, >13ms):** Fade to silence over ~5ms. Don't hard-cut.

Never output a click. The worst case should be a brief, smooth fade — not an artifact.

### Clock Drift Compensation

The Pi's 48kHz clock and the DAW's clock drift apart by a few samples per minute. Left unchecked, the jitter buffer gradually overfills or empties.

**Keep the existing adaptive resampling approach**, but decouple it from latency reporting:
- `setLatencySamples()` reports the fixed jitter buffer size (never changes)
- Resampling ratio nudges gently (±0.2%) to keep the buffer fill level at target
- This is invisible to the user — no audio artifacts from sub-0.2% ratio changes

### Connection Lifecycle

```
1. CONNECT
   - Establish UDP/WireGuard connection
   - Exchange handshake packets for 2 seconds
   - Measure RTT statistics (mean, p95, max)
   - Calculate buffer size: max(RTT_p95 × 2 + 50ms, 100ms), capped at 300ms
   - Report setLatencySamples() — locked for session
   - Server begins streaming silence packets immediately

2. RUNNING
   - Jitter buffer receives and reorders packets
   - Audio thread reads from buffer at fixed rate
   - Drift compensation via gentle resampling
   - Stats monitored: fill level, packet loss rate, recovered packets

3. DEGRADED (sustained underrun)
   - If buffer empties for >500ms: mute output, flush buffer
   - Display connection warning in UI
   - Continue accepting packets — re-prebuffer when they resume
   - Do NOT change setLatencySamples()

4. DISCONNECT
   - Clean shutdown, drain buffer
   - On reconnect: full re-handshake, new RTT measurement, new buffer size
   - setLatencySamples() may change between sessions (that's fine — not mid-session)
```

### Server-Side Changes (midi_router.py)

The Pi server needs corresponding changes:

1. **Packet headers:** Add sequence number (uint32), timestamp (uint32), flags (uint16), checksum (uint16) to every audio packet.
2. **Packet duplication:** After sending each packet, queue a delayed duplicate (~10ms later). Simple: maintain a small ring buffer of recent packets and re-send from it.
3. **Continuous streaming:** Always send audio packets, even during silence. The server must never stop the packet stream — silence packets keep the jitter buffer filled and the latency chain intact.
4. **Handshake:** Respond to RTT measurement pings during connection setup.
5. **Late MIDI handling:** If a MIDI message arrives more than 500ms after the expected time (detected via gaps in the MIDI stream during a connection outage), discard it — it's stale.

### Plugin-Side Changes

#### PluginProcessor.h
```cpp
// Replace:
AudioRingBuffer audioRingBuffer;
// With:
JitterBuffer jitterBuffer;

// Add:
enum class StreamState { Connecting, Running, Degraded, Disconnected };
std::atomic<int> streamState { (int)StreamState::Disconnected };
```

#### PluginProcessor::prepareToPlay()
```cpp
// Remove adaptive prebuffer logic
// Jitter buffer size is set at connection time, not prepareToPlay
// Just configure resampling here
```

#### PluginProcessor::processBlock()
```cpp
// In hardware mode:
// Read from jitterBuffer instead of audioRingBuffer
// No prebuffering logic — the jitter buffer handles it
// setLatencySamples already set at connection, never changes here
```

#### NetworkTransport
```cpp
// Receive thread:
// Parse packet headers
// Pass sequence + timestamp + samples to jitterBuffer.writePacket()
// Discard duplicates (jitter buffer handles via sequence number)

// Connection setup:
// RTT measurement loop (2 seconds of pings)
// Calculate and lock buffer size
// Signal processor to call setLatencySamples()
```

## File Structure

```
plugin/src/
  JitterBuffer.h / .cpp      — NEW: timestamp-indexed buffer, PLC, dedup
  AudioRingBuffer.h           — KEEP: still used for CC ring buffer etc.
  NetworkTransport.h / .cpp   — MODIFY: packet headers, RTT handshake, duplication handling
  PluginProcessor.h / .cpp    — MODIFY: use JitterBuffer, fixed latency reporting
  ...existing files...

server/
  midi_router.py              — MODIFY: packet headers, duplication, continuous streaming
```

## Implementation Order

1. **Packet header (server)** — Add sequence/timestamp/flags/checksum to audio packets in midi_router.py. Backwards-compatible: old plugin ignores unknown bytes, new plugin detects header presence.
2. **JitterBuffer (plugin)** — New class, timestamp-indexed placement, gap detection.
3. **Wire into NetworkTransport** — Parse headers, feed JitterBuffer.
4. **Wire into PluginProcessor** — Replace AudioRingBuffer reads with JitterBuffer reads. Fixed `setLatencySamples()`.
5. **RTT handshake** — Connection-time measurement, buffer size calculation.
6. **Packet duplication (server)** — Send each packet twice with 10ms delay.
7. **Packet loss concealment (plugin)** — Interpolation/fade for unrecoverable gaps.
8. **Continuous silence streaming (server)** — Never stop sending packets.
9. **Connection state machine** — Connecting/Running/Degraded/Disconnected states, UI indicators.
10. **Testing** — Simulate packet loss and jitter to verify resilience.

## Metrics to Monitor (exposed to UI)

- **Buffer fill level** — should hover around target, not swing wildly
- **Packet loss rate** — percentage of packets lost (before recovery)
- **Packets recovered** — via duplication (should account for most losses)
- **Unrecoverable losses** — packets where both copies were lost (should be near zero)
- **Clock drift** — current resampling ratio deviation from 1.0
- **Connection state** — Running / Degraded / Disconnected

## Gotchas and Mitigations

| Gotcha | Mitigation |
|--------|-----------|
| DAW reads `setLatencySamples()` at different times | Set it once at connection, before any audio flows. Test in Logic + Ableton. |
| WiFi burst loss defeats single-copy FEC | Packet duplication with 10ms offset handles bursts up to 10ms. |
| First note clipped if buffer is empty | Server streams silence continuously — buffer is always full. |
| Network dies mid-session | Mute + flush + re-prebuffer on resume. Never change reported latency. |
| WireGuard adds its own jitter | RTT measured through the actual tunnel, not raw network. |
| UDP packet fragmentation | Packets are 268 bytes — well under 1400 byte threshold even with WG overhead. |
| User expects live monitoring to be in sync | Document clearly: PDC works during playback only. Live playing has real latency. Design sounds in preview mode. |
| Server MIDI pileup during outage | Discard MIDI arriving >500ms late. |
