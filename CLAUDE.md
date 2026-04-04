# Anarack

Remote hardware synth studio — producers control real synths over the internet, hear near-real-time audio, download lossless recordings.

## Project Phase

**Phase 2 — P2P direct connection working.** The AU/VST3 plugin connects directly to the Pi via Session API with ephemeral WireGuard keys. ASRC drift correction produces click-free audio. Tested stable at 80ms buffer on LAN, 200ms on WireGuard.

## Current Version

**v0.3.12 (build 48)** — Single source of truth: `plugin/CMakeLists.txt` (`VERSION` + `ANARACK_BUILD_NUMBER`). Piped to C++ via compile defs, then to WebView UI via `initConfig`.

## Tech Stack

- **Server (Pi):** Python 3.11+, JACK2, rtmidi, asyncio, WireGuard (kernel module)
- **Session API:** Python asyncio HTTP + WebSocket (coordinates P2P connections, manages WG peers)
- **Pi Agent:** Python service that registers Pi with Session API, manages WG peers dynamically
- **Plugin (DAW):** JUCE C++ — builds as AU (Logic) + VST3 (Ableton) from one codebase
- **Networking:** Embedded WireGuard (boringtun static lib in the plugin). No user-visible VPN.
- **VPS Relay:** Vultr Manchester, `66.245.195.65` — fallback relay when P2P fails
- **Audio:** JACK at 128 samples / 48kHz, raw PCM over UDP (int16 mono, 128-sample packets with 12-byte header)
- **Legacy client (Web):** Vanilla HTML/JS demo UI, WebSocket for MIDI (still works for quick testing)

## Development Workflow

### Branching Policy

- **Direct to main** for small, safe changes: config, docs, version bumps, one-file fixes, changelog updates
- **Feature branch + worktree** for anything risky or multi-file: audio path, protocol changes, networking, anything that could break the build

### Branch Naming

- `feat/description` — new features
- `fix/description` — bug fixes
- `chore/description` — maintenance, refactoring

### Worktrees

Use git worktrees for feature branches and parallel agent work. Multiple agents must NEVER work on the same worktree or branch simultaneously.

```bash
# Create a worktree for a feature branch
git worktree add worktrees/feat-my-feature -b feat/my-feature

# When done and merged, clean up
git worktree remove worktrees/feat-my-feature
```

### Versioning

- Version and build number defined in `plugin/CMakeLists.txt` (single source of truth)
- `project(AnarackRev2 VERSION X.Y.Z)` — semantic version
- `set(ANARACK_BUILD_NUMBER N)` — monotonic integer, never resets
- Piped to C++ as `ANARACK_VERSION` / `ANARACK_BUILD_NUMBER` compile definitions
- WebView UI receives version via `initConfig` event — never hardcode in HTML

### Changelog

Every build gets a `CHANGELOG.md` entry. Format:

```
## vX.Y.Z (build N) — Description (YYYY-MM-DD HH:MM)
```

- Log what changed AND what didn't work
- Failed attempts are valid: "Attempted X, caused Y — reverted"
- Never skip a version or silently remove an entry

## Project Structure

```
plugin/              — JUCE AU/VST3 plugin (C++, builds for Logic + Ableton)
  src/               — Plugin source (PluginProcessor, PluginEditor, NetworkTransport, WgTunnel, SessionClient, JitterBuffer)
  ui/rev2-panel.html — Full Rev2 front panel UI (HTML/JS, served via WebBrowserComponent)
  lib/               — Static libraries (libboringtun.a)
server/              — Runs on Raspberry Pi 5
  midi_router.py     — MIDI routing, audio streaming, JACK interface
  session_api.py     — Session coordination API (HTTP + WebSocket + STUN)
  pi_agent.py        — Pi registration, WG peer management
client/              — Browser-based demo UI (virtual keyboard + CC sliders)
scripts/             — Setup and utility scripts (JACK config, Pi setup, dev server)
docs/strategy/       — Business plan & strategy docs
docs/plans/          — Implementation plans and specs
```

## Audio Streaming Architecture

### Packet Format
- **12-byte header:** sequence (u32) + timestamp (u32) + flags (u16) + checksum (u16)
- **Payload:** 128 × int16 samples = 256 bytes
- **Total:** 268 bytes per packet, 375 packets/sec at 48kHz
- **Server-side packet duplication:** each packet sent twice (5ms delayed) for loss recovery

### JitterBuffer (plugin/src/JitterBuffer.h)
- Timestamp-indexed ring buffer — packets placed by timestamp, not arrival order
- Packet loss concealment (PLC): crossfade from last good audio on gaps
- PLC gap→data transition: 64-sample crossfade from last PLC output to real audio
- 50% prebuffer target before playback starts
- Fill level: `samplesWritten - totalSamplesRead` (only counts unique samples, not duplicates)

### ASRC Drift Correction (plugin/src/PluginProcessor.cpp)
Pi and DAW clocks drift by ~10-100ppm. The ASRC system corrects this:
- **Smoothed fill error:** low-pass filter on buffer fill level vs target (block-size-independent alpha)
- **Drift accumulator:** accumulates fractional sample drift per block
- **Correction:** when accumulator reaches ±1, drop or duplicate 1 sample
- **Linear interpolation:** maps N±1 input samples across N output samples (no splice, no click)
- **Guard check:** only drop when buffer above target, only dup when below. Skip (don't reset) if guard blocks.
- **Block-size independent:** filter alpha = blockSize / (sampleRate × 5.0), drift clamp = 7.0 × blockSize / sampleRate
- **Startup delay:** 4 seconds (scaled by block size) before corrections begin

### Tested Buffer Sizes
| Connection | Min Stable Buffer | Notes |
|-----------|-------------------|-------|
| LAN (raw UDP) | 80ms | Zero encryption overhead |
| P2P (WireGuard, same LAN) | 200ms | boringtun userspace jitter ~50ms |
| Relay (WireGuard via VPS) | 300ms | Encryption + network jitter |

## Connection Modes

### Three modes:
1. **P2P** — Session API + ephemeral WireGuard keys, direct to Pi (preferred)
2. **Relay** — Static WireGuard keys via VPS relay (fallback)
3. **LAN** — Raw UDP, no encryption (development/testing)

### P2P Connection Flow
1. Plugin generates ephemeral X25519 keypair (`WgTunnel::generateKeypair()`)
2. Plugin calls `POST /sessions` on Session API with its pubkey
3. Session API notifies Pi Agent via WebSocket
4. Pi Agent runs `wg set wg0 peer <pubkey> allowed-ips 10.0.0.10/32`
5. API returns Pi's pubkey + WG listen port to plugin
6. Plugin connects boringtun to Pi's LAN IP + WG port
7. Audio streams directly, no VPS relay

### Network Topology
```
P2P (preferred):
Plugin (10.0.0.10) ◄──── WireGuard ────► Pi wg0 (10.0.0.2)
                     direct, ~2-5ms LAN

Relay (fallback):
Plugin (10.0.0.3) ──► VPS (10.0.0.1) ──► Pi (10.0.0.2)
                   66.245.195.65:51820

LAN (dev):
Plugin ◄──── raw UDP ────► Pi (192.168.1.131)
```

### Tunnel IPs
- `10.0.0.1` — VPS relay
- `10.0.0.2` — Pi (WireGuard interface)
- `10.0.0.3` — Plugin via VPS relay (static keys)
- `10.0.0.10` — Plugin via P2P direct (ephemeral keys)

### Session API (runs on Pi for now, move to VPS for production)
- HTTP `:8800` — `GET /pis`, `POST /sessions`, `DELETE /sessions/{id}`
- WebSocket `:8802` — Pi registration, heartbeat, session notifications
- STUN UDP `:8801` — endpoint discovery for NAT traversal

## Plugin Architecture

### Auto-connect
- `prepareToPlay` triggers `autoConnect()` on a background thread
- No need to open the editor — plugin connects when loaded by DAW
- Connection state: disconnected → connecting → connected
- UI stays yellow "Connecting" until JitterBuffer prebuffer fills (audio flowing)

### State Persistence
`getStateInformation` / `setStateInformation` saves:
- Server host, WireGuard mode, buffer size
- MIDI learn mappings (ccMap[128])
- Restored on DAW project reload

### WebView UI
- `plugin/ui/rev2-panel.html` — full Rev2 front panel
- Dev mode: loads from filesystem if file exists at hardcoded path (hot reload)
- Production: served via JUCE ResourceProvider
- Boot animation: knobs animate from zero to default values on connect
- ASRC/PLC diagnostics logged every 5 seconds in bottom-right panel

## Raspberry Pi Deployment

```bash
# Connection
ssh pi@anarack.local              # LAN: 192.168.1.131, WG tunnel: 10.0.0.2

# Deploy server files
scp server/midi_router.py pi@anarack.local:~/anarack/server/
scp server/session_api.py pi@anarack.local:~/anarack/server/
scp server/pi_agent.py pi@anarack.local:~/anarack/server/

# Restart midi_router
ssh pi@anarack.local "cd /home/pi/anarack && PYTHONUNBUFFERED=1 nohup venv/bin/python server/midi_router.py --midi-port 'Prophet Rev2' > /tmp/anarack.log 2>&1 &"

# Restart session API + Pi agent
ssh pi@anarack.local "cd /home/pi/anarack && PYTHONUNBUFFERED=1 nohup venv/bin/python server/session_api.py > /tmp/session_api.log 2>&1 &"
ssh pi@anarack.local "cd /home/pi/anarack && PYTHONUNBUFFERED=1 nohup venv/bin/python server/pi_agent.py > /tmp/pi_agent.log 2>&1 &"

# Check logs
ssh pi@anarack.local "cat /tmp/anarack.log"
ssh pi@anarack.local "cat /tmp/pi_agent.log"

# Check WireGuard peers (clean up stale ephemeral peers)
ssh pi@anarack.local "sudo wg show wg0"
```

**Important:**
- Python venv: `~/anarack/venv/` — always use `venv/bin/python`
- JACK: `jackd -R -d alsa -d hw:0 -r 48000 -p 128 -n 3`
- Scarlett 18i16 is `hw:0`, Rev2 MIDI is on USB
- Pi WireGuard listen port: **33933** (NOT 51820 — check with `sudo wg show wg0`)
- JACK xrun watchdog in midi_router.py auto-restarts JACK after 5 xruns in 60s

## Plugin Build

```bash
# Build (from plugin/ directory)
cd plugin
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Install AU for Logic
cp -R build/AnarackRev2_artefacts/Release/AU/"Anarack Rev2.component" ~/Library/Audio/Plug-Ins/Components/
```

### IMPORTANT: On every build
See **Development Workflow > Versioning & Changelog** above. Also update the HTML fallback version in `plugin/ui/rev2-panel.html` (shown before initConfig fires).

### Feature Branch Builds (Worktrees)
When building the plugin from a feature branch worktree, ALWAYS change these in the worktree's `plugin/CMakeLists.txt`:
1. `PRODUCT_NAME` → `"Anarack Rev2 [branch-name]"` (e.g., `"Anarack Rev2 [preset-browser]"`)
2. `BUNDLE_ID` → `"com.anarack.rev2.dev"`
3. `PLUGIN_CODE` → different 4-char code (e.g., `Adv2`)

This lets both main and feature builds coexist in Logic. When merging to main, revert these to production values. After merge, delete the dev component:
```bash
rm -rf ~/Library/Audio/Plug-Ins/Components/"Anarack Rev2 [branch-name].component"
```

## Key Decisions

- **Embedded WireGuard** via boringtun — no Tailscale, no VPN client, no user setup
- **Session API** for P2P — ephemeral keys per session, dynamic peer management
- **P2P first, relay fallback** — direct connection when possible, VPS relay when NAT blocks it
- **ASRC with linear interpolation** — not Lagrange resampler (caused jitter on int16 audio)
- **JitterBuffer with PLC** — timestamp-indexed, crossfade concealment, better than simple ring buffer
- **16-bit audio** — captures full Rev2 dynamic range (~90dB), lower bandwidth than 24-bit
- **Block-size-independent ASRC** — works correctly at 32, 128, or 2048 sample blocks (Logic uses 32)
- **Int16 PCM, not compressed** — lowest latency, acceptable bandwidth (~100KB/s mono)

## Known Issues / TODO

- **WireGuard jitter ~50ms** — boringtun userspace encryption adds jitter, limits P2P buffer to ~200ms
- **Auto buffer detection** — needs thread-safe JitterBuffer resize (can't reconfigure while streaming)
- **Bidirectional CC sync** — Rev2 can send CCs back (MIDI Param Xmit → CC), not yet wired to plugin
- **Stale WG peers** — cleaned up on new session, but manual cleanup may be needed: `ssh pi@anarack.local "sudo wg show wg0"`
- **VPS SSH** — SSH key not configured from dev machine, need to set up

## Adding a New Synth / Rack

When adding a new Pi rack or synth:

1. **Pi setup:** Install JACK, WireGuard, Python venv, deploy server files (see `scripts/pi-setup/`)
2. **WireGuard:** Configure wg0 with VPS as peer, note the listen port
3. **Session API:** Register Pi with unique `pi_id` and synth list
4. **Synth definition:** Add to `synths/` directory (JSON with CC mappings)
5. **UI panel:** Create HTML panel matching the synth's front panel layout
6. **Plugin:** Update synth parameter list in `PluginProcessor.cpp`
7. **Test:** LAN first (80ms), then WireGuard P2P (200ms), then remote via VPS (300ms)
