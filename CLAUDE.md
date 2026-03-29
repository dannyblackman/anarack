# Anarack

Remote hardware synth studio — producers control real synths over the internet, hear near-real-time audio, download lossless recordings.

## Project Phase

**Phase 1 — Production plugin working.** The AU/VST3 plugin connects to the Pi over the internet via an embedded WireGuard tunnel, with a VPS relay handling NAT traversal. Tested at 18ms RTT on WiFi, ~80ms on 4G.

## Tech Stack

- **Server (Pi):** Python 3.11+, JACK2, rtmidi, asyncio, WireGuard (kernel module)
- **Plugin (DAW):** JUCE C++ — builds as AU (Logic) + VST3 (Ableton) from one codebase
- **Networking:** Embedded WireGuard (boringtun static lib in the plugin). No user-visible VPN — one plugin, click connect, it works.
- **VPS Relay:** Vultr London, `66.245.195.65` — forwards encrypted UDP between plugin and Pi
- **Audio:** JACK at 64 samples / 48kHz, raw PCM over UDP (int16 mono, 128-sample packets)
- **Legacy client (Web):** Vanilla HTML/JS demo UI, WebSocket for MIDI (still works for quick testing)

## Project Structure

```
plugin/         — JUCE AU/VST3 plugin (C++, builds for Logic + Ableton)
server/         — Runs on Raspberry Pi 5 (MIDI routing, audio engine, session management)
client/         — Browser-based demo UI (virtual keyboard + CC sliders)
scripts/        — Setup and utility scripts (JACK config, Pi setup, dev server)
docs/strategy/  — Business plan & strategy docs (overview, financials, timelines, etc.)
docs/plans/     — Implementation plans and specs
worktrees/      — Git worktrees for feature branches (gitignored)
```

## Development Workflow

### Branches

- `main` is the default branch — always deployable
- Feature branches: `feat/description`
- Bug fixes: `fix/description`
- Chores/maintenance: `chore/description`

### Worktrees

Use git worktrees for parallel work on feature branches:

```bash
# Create a worktree for a new feature branch
git worktree add worktrees/feat-my-feature -b feat/my-feature

# Clean up when done (after merging)
git worktree remove worktrees/feat-my-feature
```

### Commands

```bash
# Local dev (serves client UI)
./scripts/dev.sh                   # http://localhost:8080

# Server (on Pi)
cd server && pip install -r requirements.txt
python midi_router.py              # Start MIDI routing server
```

## Network Topology

```
Plugin (DAW)                    VPS Relay (Vultr London)           Pi (home)
10.0.0.3  ──── WireGuard ────▶ 66.245.195.65 (10.0.0.1) ────▶ 192.168.1.131 (10.0.0.2)
                                forwards encrypted UDP             WireGuard kernel module
```

- **Plugin tunnel IP:** `10.0.0.3`
- **VPS tunnel IP:** `10.0.0.1` (relay at `66.245.195.65`)
- **Pi tunnel IP:** `10.0.0.2` (LAN: `192.168.1.131`, hostname: `pi@anarack.local`)
- **Two connection modes:** Raw UDP for LAN testing, WireGuard for internet. The plugin picks the right mode based on the target address.
- **Adaptive pre-buffer:** 15ms on LAN, scales with measured RTT on internet connections.
- **Sample rate resampling:** Server always sends 48kHz. Plugin resamples to match whatever rate the DAW host is running.

## VPS Relay

The VPS at `66.245.195.65` (Vultr London) runs WireGuard and forwards encrypted UDP between the plugin and the Pi. It solves NAT traversal — neither the plugin nor the Pi need open ports.

```bash
# SSH to the VPS
ssh root@66.245.195.65

# WireGuard config is at /etc/wireguard/wg0.conf
# Check tunnel status
wg show
```

## Raspberry Pi Deployment

The server runs on a Raspberry Pi 5. Deploy directly via SSH — never ask the user to SSH manually.

```bash
# Connection
ssh pi@anarack.local              # LAN: 192.168.1.131, WireGuard tunnel: 10.0.0.2

# Deploy server
scp server/midi_router.py pi@anarack.local:~/anarack/server/midi_router.py

# Restart server (use venv Python, PYTHONUNBUFFERED for log visibility)
ssh pi@anarack.local "cd /home/pi/anarack && PYTHONUNBUFFERED=1 nohup venv/bin/python server/midi_router.py --midi-port 'Prophet Rev2' > /tmp/anarack.log 2>&1 &"

# Check logs
ssh pi@anarack.local "cat /tmp/anarack.log"
```

**Important:**
- Python venv is at `~/anarack/venv/` — always use `venv/bin/python`
- JACK runs as a separate process (`jackd -R -d alsa -d hw:0 -r 48000 -p 128 -n 3`)
- Scarlett 18i16 is `hw:0`, Rev2 MIDI is on USB
- WireGuard kernel module is configured on the Pi (`/etc/wireguard/wg0.conf`)

## Plugin Build

The plugin builds AU + VST3 + Standalone from a single JUCE codebase. boringtun (Cloudflare's Rust WireGuard implementation) is compiled as a static library and linked into the plugin — no user-visible VPN, no separate app to install.

```bash
# Build the plugin (from plugin/ directory)
cmake -B build -G Xcode
cmake --build build --config Release

# Outputs:
#   Anarack Rev2.component  → ~/Library/Audio/Plug-Ins/Components/  (Logic)
#   Anarack Rev2.vst3       → ~/Library/Audio/Plug-Ins/VST3/        (Ableton, Bitwig)
#   Anarack Rev2.app        → Standalone for testing

# Rebuild boringtun static lib (if needed)
cd plugin/lib/boringtun/boringtun
cargo build --lib --no-default-features --features ffi-bindings --release --target aarch64-apple-darwin
# Then copy the .a to plugin/lib/libboringtun.a
```

## Key Decisions

- **Embedded WireGuard** via boringtun (Cloudflare's Rust implementation) compiled as a static lib inside the plugin. No Tailscale, no VPN client, no user setup.
- **VPS relay** for NAT traversal — the Pi stays behind home NAT, the VPS has a public IP, encrypted UDP flows through it.
- JACK2 for audio capture on the Pi, raw PCM over UDP (no netjack2 in production path)
- Python for the server; may move audio path to Rust later
- Single JUCE codebase produces AU + VST3 — same source, different wrappers
