# Software

## System Architecture

Two interfaces, two purposes:

1. **DAW plugin** (VST3/AU) — the production interface. Producers never leave Ableton or Logic. One install, self-contained, no additional software required.
2. **Web app** — marketing, booking, account management, and a **browser demo** for try-before-you-install conversion.

```
┌─────────────────────────────────────────────────────────────┐
│                    Producer's DAW                             │
│                                                               │
│  ┌─────────────────────────────────────┐                     │
│  │       Anarack Plugin (VST3/AU)      │                     │
│  │                                       │                     │
│  │  - Appears as a virtual instrument   │                     │
│  │  - Receives MIDI from DAW            │◄── DAW MIDI track  │
│  │  - Returns audio to DAW              │──► DAW audio bus    │
│  │  - Synth-specific parameter panel    │                     │
│  │  - All DAW automation works          │                     │
│  │  - Record/bounce as normal           │                     │
│  │  - Embedded WireGuard (boringtun)    │                     │
│  │  - No additional software to install │                     │
│  └──────────────┬───────────────────────┘                     │
└─────────────────┼────────────────────────────────────────────┘
                  │ Embedded WireGuard tunnel (UDP)
                  │ MIDI out + PCM audio return
                  │
┌─────────────────┴────────────────────────────────────────────┐
│                  Session Server (Mac Mini)                     │
│                                                               │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────────┐       │
│  │ API Server  │  │ Audio Engine │  │ MIDI Router   │       │
│  │ (sessions,  │  │ (CoreAudio,  │  │ (assigns user │       │
│  │  auth)      │  │  netjack2,   │  │  to synth     │       │
│  │             │  │  recording)  │  │  MIDI port)   │       │
│  └─────────────┘  └──────────────┘  └───────────────┘       │
│                                                               │
│  ┌─────────────────────────────────────────────────┐         │
│  │ Session Manager                                  │         │
│  │ - Spins up audio + MIDI route per session       │         │
│  │ - Starts/stops WAV recording                    │         │
│  │ - Issues ephemeral WireGuard keypairs            │         │
│  │ - Monitors synth health (MIDI heartbeat)        │         │
│  └─────────────────────────────────────────────────┘         │
└───────────────────────────────────────────────────────────────┘


┌─────────────────────────────────────────────────────────────┐
│                    Browser (try before install)               │
│                                                               │
│  ┌─────────────────────────────────────┐                     │
│  │       Web Demo                       │                     │
│  │                                       │                     │
│  │  - Virtual keyboard + CC sliders    │                     │
│  │  - WebRTC audio (higher latency)    │                     │
│  │  - 5-min free trial per synth       │                     │
│  │  - "Download the plugin" CTA        │                     │
│  └──────────────┬───────────────────────┘                     │
└─────────────────┼────────────────────────────────────────────┘
                  │ WebRTC (browser-native, ~100-150ms)
                  │ WebSocket MIDI
                  └──► Same Session Server
```

## Why a DAW Plugin Changes Everything

- **MIDI routing is free** — the DAW handles it. User draws notes in the piano roll, plays their MIDI keyboard, uses arpeggiators — it all just works. No custom MIDI UI needed.
- **Audio return lands on a DAW track** — producers can mix, EQ, add effects, bounce to audio, exactly like any other instrument. No separate recording/download step.
- **Automation works** — map any synth parameter to a DAW automation lane. Draw filter sweeps, LFO rate changes, etc. in the timeline. This is huge.
- **Familiar workflow** — zero learning curve. It's just another instrument plugin.
- **Preset recall** — save/load synth patches from the plugin UI, stored in your Anarack account.

## Shared UI — One Codebase, Two Surfaces

The plugin UI and the web app share a single React component library. This is the biggest architectural win for maintainability.

```
anarack/
├── packages/
│   ├── ui/                    ← Shared React component library
│   │   ├── SynthBrowser/      ← Grid of synths, live status, queue, booking
│   │   ├── ParameterPanel/    ← Knobs/sliders, per-synth layouts from JSON config
│   │   ├── SessionView/       ← Timer, preset browser, connection status
│   │   ├── PresetBrowser/     ← Save/load/browse patches
│   │   └── api/               ← API client (auth, sessions, availability, Stripe)
│   │
│   ├── plugin/                ← JUCE C++ (audio, MIDI, WireGuard, WebView host)
│   │   └── loads packages/ui in a WebView
│   │   └── JS bridge: plugin ↔ React (send MIDI, receive audio status, etc.)
│   │
│   └── web/                   ← Next.js app
│       ├── marketing pages
│       ├── browser demo       ← uses packages/ui + WebRTC for audio
│       └── account/billing    ← uses packages/ui for session history etc.
```

**What's shared:**
- All UI components (synth browser, parameter panels, presets, queue, session view)
- API client (auth, session management, availability, billing)
- Synth parameter definitions (JSON configs)
- Design system (colours, fonts, component styles)

**What's different per surface:**
- **Plugin:** JUCE C++ handles audio I/O, MIDI, and WireGuard. React talks to C++ via a JS bridge (`window.anarack.sendMidi()`, `window.anarack.getLatency()`, etc.)
- **Web demo:** Browser handles audio via WebRTC and MIDI via WebSocket. Same React components, different transport layer underneath.

**JS Bridge (plugin only):**
```typescript
// React components call these — in the plugin they bridge to C++,
// on the web they bridge to WebRTC/WebSocket
interface AnarackTransport {
  sendMidi(status: number, data1: number, data2: number): void
  getLatency(): number
  getConnectionStatus(): ConnectionStatus
  connect(synthId: string): Promise<void>
  disconnect(): Promise<void>
}
```

The transport interface is implemented differently in each environment but the UI code doesn't care — it just calls `transport.sendMidi()`.

## Component Breakdown

### 1. DAW Plugin (VST3 / AU)

The plugin the producer installs. It appears as a virtual instrument in their DAW.

**Framework:** JUCE (C++) for the audio/MIDI/networking engine. The entire UI is a WebView (`juce::WebBrowserComponent`) rendering the same React components used on the website. This means one UI codebase for both plugin and web app.

**Why WebView in a plugin?** Increasingly common (Arturia, Output, etc.). The audio thread stays in C++ — that's the performance-critical path. The UI (knobs, menus, status indicators) runs in the WebView and communicates with the C++ backend via a JavaScript bridge. Knob rendering at 60fps in a WebView is fine — it's not the audio thread.

JUCE builds VST3 + AU + AAX from one codebase. Supports macOS (Logic, Ableton) and Windows (Ableton, FL Studio, Bitwig).

**What it does:**
- Receives MIDI from the DAW's MIDI routing (notes, CCs, pitch bend, aftertouch)
- Sends MIDI over UDP to the session server
- Receives audio back over UDP from the session server
- Outputs audio to the DAW's audio bus (the plugin's audio output)
- Displays a synth-specific parameter panel with knobs/sliders mapped to CCs
- Exposes parameters to DAW automation (filter cutoff = CC 74, etc.)
- Handles session auth (login once, token stored locally)
- Shows connection status, latency indicator

**Plugin UI — the complete experience (no browser needed):**

The plugin has two views:

**Synth Browser view** (shown when not connected to a synth):
- Grid/list of all synths in the rack with photos and descriptions
- Each synth shows real-time status:
  - **Available** — green dot, "Connect now" button → instant session start
  - **In use** — red dot, shows estimated time remaining, "Join queue" button
  - **Queued** — amber dot, shows your position, "Leave queue" button
  - **Offline/maintenance** — grey dot
- Queue system: join a queue for a busy synth, get a notification when it's your turn ("Moog Sub37 is ready — connect now?"). Your turn is held for 2 minutes.
- Schedule a slot: for users who want guaranteed time, book a specific slot from a calendar view within the plugin
- Account/subscription status shown in a corner

**Session view** (shown when connected to a synth):
- Synth-specific parameter panel (knobs, sliders, grouped by section)
- Connection quality indicator (latency ms, jitter, packet loss)
- Session timer (remaining time / pay-as-you-go running total)
- Preset browser (save/load patches via SysEx)
- "Disconnect" button → returns to synth browser

**Audio format:**
- Plugin requests audio from the DAW at whatever sample rate/buffer the user has set
- Network transport runs at 48kHz/24-bit — plugin resamples if user's session is 44.1kHz or 96kHz
- Latency is reported to the DAW via plugin latency compensation — so bounced audio aligns perfectly

**Key JUCE advantage:** plugin latency compensation. JUCE lets you declare the plugin's latency (network round trip), and the DAW automatically compensates when bouncing/rendering. So even though monitoring has ~40ms delay, the final rendered audio is sample-accurate.

### 2. Audio Engine (Server)

The core — lowest latency path possible.

**Stack:** JACK2 on macOS (runs on CoreAudio) + netjack2 for network audio

**Configuration:**
- Sample rate: 48kHz (96kHz optional for premium)
- Buffer size: 64 samples (1.3ms)
- Network buffer: 2-3 packets (~4-6ms) — absorbs single packet loss
- Format: 24-bit PCM over UDP (no codec)

**Recording:**
- JACK capture to disk per-session (server-side backup)
- 48kHz/24-bit WAV
- Available for download via web app as a backup — but the primary workflow is bouncing in the DAW

**Alternative to netjack2:** Since the plugin handles audio I/O natively (it's a JUCE audio plugin), we could skip netjack2 entirely and use a custom UDP audio protocol between the plugin and server. This is simpler — no JACK install on the client side. The plugin sends MIDI packets and receives raw PCM audio packets directly. The server side still uses JACK/CoreAudio for the synth I/O.

### 3. MIDI Router (Server)

**Stack:** Python or Node.js service using `rtmidi`

**Responsibilities:**
- Receives MIDI from plugin via UDP (3 bytes per message, sub-ms)
- Routes to the correct synth's MIDI port based on session assignment
- Handles MIDI channel mapping (plugin sends on channel 1, router maps to synth's channel)
- SysEx support for preset save/recall
- Rate limiting — prevent MIDI floods from crashing a synth

### 4. Session Manager (Server)

**Stack:** Node.js or Python service

**Responsibilities:**
- Allocates synth + audio channels + MIDI port when a session starts
- Spins up audio stream + MIDI route per active session
- Generates WireGuard/Tailscale peer config for the plugin to connect
- Starts/stops server-side WAV recording
- Monitors synth health via periodic MIDI identity request (SysEx)
- Hard timeout on sessions (prevents runaway bookings)

**Session lifecycle:**
```
BOOKED → CONNECTING → ACTIVE → ENDING → COMPLETE
```

### 5. Web App

**Stack:** Next.js or similar — deliberately lightweight. The plugin is the product, the web app supports it.

**Pages:**
- **Landing/marketing** — what it is, synth catalogue with audio samples, pricing, "Download plugin" CTA
- **Browser demo** — the conversion engine. Pick a synth, get 5 minutes free. Virtual keyboard + CC sliders + WebRTC audio. No sign-up required. Big "Download the plugin for the full experience" CTA after the demo. This is how people discover the product — hear a real Moog in 10 seconds, zero install.
- **Account** — sign up, login, manage subscription, payment (Stripe), session history, download server-side backup recordings
- **Plugin download** — macOS (AU + VST3), Windows (VST3), install instructions

**Not on the web app:** booking and synth selection happen entirely in the plugin. Producers shouldn't have to leave their DAW to start a session.

### 6. Synth Parameter Definitions

Each synth gets a JSON config defining its parameters. The plugin downloads these from the server and renders the appropriate control panel.

```json
{
  "id": "moog-sub37",
  "name": "Moog Sub37",
  "midi_channel": 1,
  "image": "sub37.png",
  "parameters": [
    {
      "label": "Filter Cutoff",
      "cc": 19,
      "min": 0,
      "max": 127,
      "default": 64,
      "automatable": true,
      "group": "Filter"
    },
    {
      "label": "Filter Resonance",
      "cc": 21,
      "min": 0,
      "max": 127,
      "default": 0,
      "automatable": true,
      "group": "Filter"
    },
    {
      "label": "Osc 1 Waveform",
      "cc": 70,
      "min": 0,
      "max": 127,
      "default": 0,
      "automatable": true,
      "group": "Oscillator"
    }
  ],
  "presets": {
    "supports_sysex": true,
    "sysex_dump_request": [240, 4, 37, ...],
    "max_presets": 256
  }
}
```

### 7. Networking Layer

**Principle: one install, zero configuration.** The user downloads the plugin and that's it. No VPN clients, no Tailscale, no port forwarding.

**Plugin (production path):** Embedded WireGuard via boringtun (Cloudflare's Rust userspace WireGuard)
- Compiled into the JUCE plugin as a static library — no separate install
- Session start flow:
  1. Plugin calls API: "start session for user X on synth Y"
  2. Server generates ephemeral WireGuard keypair (lives only for this session)
  3. Server returns: public key, endpoint, allowed IPs
  4. Plugin establishes WireGuard tunnel from within its own process
  5. MIDI and PCM audio flow over the encrypted UDP tunnel
  6. Session ends → keys expire, tunnel tears down
- NAT traversal: server has a public IP (or port-forwarded), so the plugin always initiates outbound UDP — works through nearly all NATs/firewalls without STUN/TURN
- Latency: identical to raw WireGuard (~10-20ms between UK residential connections)
- Security: each session gets unique keys, no persistent VPN, no access to anything except the assigned synth's audio/MIDI streams

**Browser demo (conversion path):** WebRTC
- Browser-native, zero install
- Higher latency (~100-150ms) — acceptable for a 5-minute demo
- Opus audio codec (browser doesn't support raw PCM over WebRTC)
- WebSocket for MIDI (browser can't send raw UDP)
- Purpose: let someone hear a real Moog in 10 seconds, then convert to plugin download

**Prototype (Phase 0):** Tailscale between Pi and laptop
- Quick to set up, proves the concept
- Tailscale is only used during prototyping, never in production
- Replaced by embedded boringtun when building the JUCE plugin

### 8. Payments & Sessions

- **Stripe** for payments (subscriptions + per-session)
- **Real-time availability** — plugin polls the API for synth status (available/in-use/offline)
- **Instant connect** — if a synth is free, one click to start. No pre-booking required.
- **Queue system** — if a synth is busy, join the queue. Server sends push notification (via plugin polling or WebSocket) when it's your turn. 2-minute hold window.
- **Advance booking** — optional, for users who want guaranteed time. Calendar view in plugin UI. 30/60 min slots.
- **Session metering** — pay-as-you-go sessions billed per minute. Running total shown in plugin UI. Subscription users just see remaining hours.
- Auto-charge on session end, refund if technical issues

## DAW Compatibility

### Priority 1 (Launch)
- **Ableton Live** (macOS + Windows) — VST3 + AU
- **Logic Pro** (macOS) — AU

### Priority 2 (Post-launch)
- **FL Studio** (Windows) — VST3
- **Bitwig** (macOS + Windows + Linux) — VST3
- **Pro Tools** — AAX (JUCE supports this)

JUCE builds all formats from one codebase, so supporting multiple DAWs is primarily a testing effort, not a development effort.

## Latency Optimisation Checklist

### Server side
- [ ] JACK at 64 samples / 48kHz (1.3ms buffer)
- [ ] macOS real-time thread priority
- [ ] CPU governor locked to `performance` (no frequency scaling)
- [ ] USB audio on dedicated bus (not shared with MIDI)
- [ ] Wired ethernet (no WiFi on server side)
- [ ] Network buffer: 2-3 packets (~4-6ms)
- [ ] Raw PCM transport (no codec)
- [ ] UDP for both MIDI and audio (no TCP)

### Client side (plugin)
- [ ] Report accurate latency to DAW for compensation
- [ ] Minimal receive buffer (configurable in plugin settings)
- [ ] Embedded boringtun — direct WireGuard tunnel, no relay
- [ ] Show connection quality indicator in plugin UI (latency + jitter)
- [ ] Recommend wired ethernet in plugin UI when jitter is high

## Prototype Scope (Phase 0)

For the prototype, skip JUCE and test the raw audio/MIDI path:

1. Python script on Raspberry Pi 5
2. JACK + netjack2 (64 samples)
3. USB MIDI to Prophet Rev2
4. Scarlett 2i2 for audio I/O
5. Simple Python MIDI sender on laptop (or pipe Ableton MIDI out via a virtual MIDI port + UDP bridge script)
6. Tailscale for networking
7. JACK capture for recording

**Phase 0 success criteria:** route Ableton's MIDI output through the network to the Rev2, hear audio back in Ableton's input, with <50ms latency. If this works, the JUCE plugin is just packaging the same data path into a proper plugin format.
