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
- Stylised synth panel with real-time visual feedback (see section 8 — Synth Panel UI)
- MIDI controller mapping status — shows detected controller and active page
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

#### Stereo Handling: Mono Stream + Stereo Capture

Most synths in the catalogue output stereo (Rev2, Prophet 5/10, OB-6, Moog One, Summit, Hydrasynth). Stereo doubles the audio bandwidth per synth (~2.3 Mbit/s vs ~1.15 Mbit/s at 48kHz/24-bit). While this is trivial for good connections, it can matter on marginal ones.

**Solution: stream mono for live monitoring, capture stereo locally.**

- **Live stream to user (mono):** The server sums left+right to mono and streams a single channel back to the user in real time. For sound design and playing, mono is perfectly fine — you're tweaking parameters, not mixing in stereo.
- **Server-side capture (stereo):** The server simultaneously records the full stereo output to disk as 48kHz/24-bit WAV. When the session ends, the user downloads or renders the stereo recording. No latency constraint on this — it's just a file.
- **Optional stereo streaming:** User setting to stream in full stereo for people with great connections. The plugin shows which mode it's in.

This gives the user the best of both worlds: lowest-latency monitoring while playing, full stereo quality in the final recording.

#### Audio Interface

**Prototype:** Focusrite Scarlett 18i20 (4th gen) — USB class-compliant, works on Pi/Linux without drivers, separate power supply, 18 inputs = 9 stereo pairs.

**Production:** MOTU 24Ai + 24Ao — 24 channels in each direction, enough for 10+ stereo synths. Only one synth is active per user session (unless multi-synth on Professional tier), so the server routes the active synth's stereo pair — not all synths simultaneously.

**Channel allocation:**
- Mono synths (Sub37, Minitaur): 1 input each
- Stereo synths (Rev2, Prophet 5/10, OB-6, Moog One, Summit, Hydrasynth, Juno): 2 inputs each (stereo pair)
- On the 18i20: ~8 stereo synths + a couple of mono = fits the 10 headliners
- On the MOTU 24Ai: room for the full catalogue with headroom

**Recording:**
- JACK capture to disk per-session — always stereo, always 48kHz/24-bit WAV
- Available for download via web app as a backup — but the primary workflow is bouncing in the DAW
- Server-side recording is the safety net and the source for the lossless download

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

### 7. MIDI Controller Mapping

The playability gap between "clicking virtual knobs with a mouse" and "turning real knobs on a real synth" is the biggest UX challenge. MIDI controller mapping closes most of that gap — producers are already used to controlling synths via MIDI controllers, they just need it to work instantly.

#### Two Approaches (Both Active Simultaneously)

**1. Pre-built profiles — works out of the box**

Synth profiles + controller profiles combine automatically. It's M + N definitions, not M × N mappings:

- **Synth parameter maps** (one per synth in the catalogue) — defines which CC/NRPN controls which parameter, priority-ranked by importance. Built by reading each synth's MIDI implementation chart.
- **Controller input maps** (one per supported controller) — defines which physical knobs/faders/buttons send which CCs. Built by reading each controller's documentation.
- **The bridge** — software routing that connects them automatically. When a user connects to a Moog One with an Arturia KeyLab, the system maps KeyLab knob 1 → Moog One filter cutoff, knob 2 → resonance, etc. Zero setup.

Auto-detection: the system identifies the connected controller via its MIDI device name and loads the right profile automatically.

**Priority controllers to support at launch:**
- Arturia KeyLab (49/61/88) — huge market share among bedroom producers
- Novation LaunchKey / SL MkIII — popular, good knob count
- Native Instruments Komplete Kontrol — common in pro setups
- Akai MPK series — ubiquitous
- Roland A-series
- Generic fallback — any controller with knobs sending CCs 1-8 gets a basic mapping

**2. Manual mapping — double-click and twiddle**

For anything the profiles don't cover, or personal preference overrides:

1. User double-clicks a virtual knob on the synth panel (e.g. filter cutoff) — it highlights, waiting for input
2. User twiddles any knob on their MIDI controller
3. System captures the incoming CC and maps it instantly — done
4. Mapping is saved per-synth, per-user — persists across sessions

This is the same "MIDI learn" pattern used by every DAW, so producers already understand it. No menus, no dropdowns, no MIDI CC numbers to look up.

**Combined flow:**
- User plugs in controller → auto-detected, pre-built mapping loaded
- Everything works immediately for the most important parameters
- User wants filter cutoff on a different knob → double-click, twiddle, done
- Custom overrides are saved and layered on top of the profile

#### Page System for Limited Knobs

Most controllers have 8 knobs. Most synths have 50+ parameters. Solution: parameter pages.

- **Page 1:** Filter + Amp (cutoff, resonance, envelope ADSR, volume)
- **Page 2:** Oscillators (waveform, pitch, detune, mix)
- **Page 3:** Modulation (LFO rate, depth, mod wheel assignment)
- **Page 4:** Effects (delay, reverb, chorus — where applicable)

Pages are switched via buttons on the controller (if available) or via the UI. The current page is shown prominently in the synth panel so the user always knows which parameters their knobs control.

**Soft-takeover / pickup mode:** When switching pages, knob positions on the controller won't match the parameter values. Two options:
- **Pickup mode** — knob does nothing until it passes through the current parameter value, then takes over. Prevents value jumps.
- **Relative mode** — if the controller supports endless encoders, movement is always relative. No jump problem.

The UI shows the current parameter value and the physical knob position as two indicators, so the user can see when they'll "catch" the value.

#### Controller Mapping Config

```json
{
  "controller": "arturia-keylab-61-mkii",
  "name": "Arturia KeyLab 61 MkII",
  "knobs": [
    { "cc": 74, "label": "Knob 1" },
    { "cc": 71, "label": "Knob 2" },
    { "cc": 76, "label": "Knob 3" },
    { "cc": 77, "label": "Knob 4" },
    { "cc": 93, "label": "Knob 5" },
    { "cc": 73, "label": "Knob 6" },
    { "cc": 75, "label": "Knob 7" },
    { "cc": 72, "label": "Knob 8" }
  ],
  "faders": [
    { "cc": 52, "label": "Fader 1" },
    { "cc": 53, "label": "Fader 2" }
  ],
  "page_buttons": {
    "next": { "cc": 100 },
    "prev": { "cc": 101 }
  }
}
```

#### Synth-to-Controller Mapping (Auto-Generated)

```json
{
  "synth": "moog-one",
  "pages": [
    {
      "name": "Filter / Amp",
      "mappings": [
        { "param": "filter_cutoff", "knob_index": 0 },
        { "param": "filter_resonance", "knob_index": 1 },
        { "param": "filter_eg_amount", "knob_index": 2 },
        { "param": "amp_attack", "knob_index": 3 },
        { "param": "amp_decay", "knob_index": 4 },
        { "param": "amp_sustain", "knob_index": 5 },
        { "param": "amp_release", "knob_index": 6 },
        { "param": "master_volume", "knob_index": 7 }
      ]
    },
    {
      "name": "Oscillators",
      "mappings": [
        { "param": "osc1_waveform", "knob_index": 0 },
        { "param": "osc1_pitch", "knob_index": 1 },
        { "param": "osc2_waveform", "knob_index": 2 },
        { "param": "osc2_pitch", "knob_index": 3 },
        { "param": "osc3_waveform", "knob_index": 4 },
        { "param": "osc3_pitch", "knob_index": 5 },
        { "param": "osc_mix", "knob_index": 6 },
        { "param": "noise_level", "knob_index": 7 }
      ]
    }
  ]
}
```

### 8. Synth Panel UI

The visual representation of the synth is critical for bridging the gap between remote control and the feeling of "being in front of the synth." The panel needs to provide instant visual feedback so the user's brain connects their physical knob movement to the sound change.

#### Design Approach: Stylised Synth Panels

Each synth gets a custom-designed panel in the plugin/web UI that reflects the character and layout of the real synth, without being a pixel-perfect photo recreation.

**Why stylised, not photographic:**
- Photos don't scale well across screen sizes and DPI
- Interactive elements need to be clearly clickable/draggable
- Easier to maintain and update
- Avoids trademark/IP issues with manufacturer imagery
- Can be more functional than the real panel (grouping, labelling, tooltips)

**Why not a generic/standardised layout:**
- Half the appeal is "I'm playing a Moog One" — it should *look* like a Moog One
- Each synth has a different parameter set and signal flow — a generic layout obscures that
- Recognisable panel design builds emotional connection to the instrument

#### Panel Requirements

- **Visual knobs/sliders that move in real time** — when the user turns a physical controller knob, the on-screen knob mirrors the movement instantly (no waiting for server round-trip — update locally on MIDI send, before the sound changes)
- **Parameter value display** — hover or always-visible numerical value next to each knob (e.g. "Cutoff: 6.4kHz" not just "Cutoff: 87")
- **Section grouping** — parameters grouped by function (Oscillators, Filter, Amp, Modulation, Effects) matching the real synth's panel layout where practical
- **Active parameter highlight** — when a knob is being moved (physically or via mouse), it visually highlights and shows the value prominently. Helps the user see which parameter they're changing.
- **MIDI learn indicator** — when in learn mode (double-click), the knob pulses or glows to show it's waiting for controller input
- **Controller mapping overlay** — toggle to show which physical knob/fader is mapped to each on-screen parameter ("Knob 3 → Filter Cutoff"). Helps users orient themselves.
- **Page indicator** — when using paged controller mapping, show which page is active and which parameters are currently mapped to physical controls vs only available via mouse

#### Interaction Modes

1. **Physical controller** (primary) — turn real knobs, on-screen knobs follow, sound changes. Best experience.
2. **Mouse drag** — click and drag on-screen knobs directly. Works but less tactile. Essential for users without controllers and for the web demo.
3. **DAW automation** — parameter changes from automation lanes show on the panel in real time during playback. The panel becomes a visual monitor of what's happening.
4. **Scroll wheel** — hover over a knob, scroll to adjust. Fine-tuning mode.

#### Web Demo Considerations

The browser demo uses the same synth panels but without controller support (Web MIDI API is available but not reliably supported across browsers). The demo relies on mouse interaction + virtual keyboard. This is intentionally a degraded experience compared to the plugin — it's meant to hook people, not replace the full product.

### 9. Networking Layer

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

### 10. Payments & Sessions

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
- [ ] Mono stream for live monitoring (halves audio bandwidth, reduces latency on marginal connections)
- [ ] Full stereo capture to disk server-side (always, regardless of stream mode)

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
