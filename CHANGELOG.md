# Changelog

## v0.5.1 (build 80) — Two dropdown styles: button + endless encoder (2026-04-06)

- `dropdownBtn` — button-style dropdown matching `.btn` look, shows selected label
- `dropdownKnob` — endless-encoder-style, canvas drawn like a knob with arc
  indicating position, invisible `<select>` overlay opens native dropdown on click
- Applied:
  - **Modulation Source + Destination** → dropdownBtn (new MOD_SOURCES enum)
  - **Sequencer Mode + Destination** → dropdownBtn; removed unused Value knob
  - **Arp Mode** → dropdownBtn (was generic dropdown)
  - **Sequencer Type** → dropdownBtn
  - **FX Select** → dropdownKnob (endless encoder style)
  - **Aux Env Destination** → dropdownKnob
  - **LFO Destination** → dropdownKnob

## v0.5.0 (build 79) — Dropdowns for enum parameters (2026-04-06)

Replace knobs with native `<select>` dropdowns for parameters that have
enumerated values (where scrubbing through a knob is unusable):

- **FX Select** (CC 3) — 14 FX types: Delay Mono, DDL Stereo, BBD Delay,
  Chorus, Phaser High/Low/Mst, Flanger 1/2, Reverb, Ring Mod, Distortion, HP Filter
- **Arp Mode** (CC 34) — Up / Down / Up/Down / Random / Assign
- **Aux Env Destination** (CC 85) — 53 modulation destinations
- **LFO Destination** (NRPN 40/45/50/55) — 53 modulation destinations
- **Sequencer Type** (CC 19) — Gated / Poly (replaces two separate buttons)

New `dropdown(parent, label, value, options, opts)` helper with matching
knob-style visuals. Inbound paramUpdate snaps dropdowns instead of animating.

## v0.4.11 (build 77) — Logic Latch automation + row 2 alignment (2026-04-06)

- **Latch/Write automation now records knob movements**: wrap setValueNotifyingHost
  in beginChangeGesture/endChangeGesture so Logic detects the parameter as touched
- LPF section gets min-width 560px so AMP right edge aligns with AUX ENV (row 1)
- Reverted programme section padding change (didn't actually help alignment)

## v0.4.9 (build 75) — Drop resize, fix programme section padding (2026-04-06)

- Drop drag-to-resize (setResizable false) — janky with WebBrowserComponent
- Auto-fit to 98% of screen on open is enough for all screen sizes
- Programme section now has reduced padding (12px 4px) since it has no
  border/label, so the default 22px top padding was wasted space

## v0.4.8 (build 74) — Offline state with Connect retry button (2026-04-06)

- New ConnState::offline (value 3) for when autoConnect times out without audio
- autoConnect disconnects the transport on timeout so reconnect() works cleanly
- Panel shows "Offline" with a "Connect" button when connState === 3
- Screen text shows "Synth Offline" when in offline state
- Clicking Connect triggers reconnect() which restarts the connect thread

## v0.4.7 (build 73) — CSS centering, fix false "connected" state (2026-04-06)

- scalePanel uses CSS transform with translate(-50%, -50%) centering instead
  of JS-computed left/top. GPU-accelerated, way smoother during resize.
- Fix false "connected" state: the WireGuard tunnel reporting "up" doesn't
  mean the Pi is actually responding. autoConnect now stays in "connecting"
  if the 15s audio timeout fires, instead of flipping to "connected".
- Editor's status display now checks processor.connectionState (which
  requires real audio flow) not just transport.isConnected() (socket only).

## v0.4.6 (build 72) — Smoother resize, auto-size sentinel (2026-04-06)

- editorWidth/Height default to 0 (sentinel) so auto-size always runs on first launch
- Auto-size now 98% of screen width (was 90%)
- scalePanel: cache panel's natural size, only measure once instead of every resize
- Throttle resize via requestAnimationFrame to reduce jank during drag
- Removed `transform = 'none'` flash that caused jumpy redraws

## v0.4.5 (build 71) — Auto-size fix (skipped, html not included)

## v0.4.4 (build 70) — Auto-fit screen + draggable corner resizer (2026-04-06)

- Editor opens at 90% of screen width on first launch (sized to fit your display)
- Inset WebView 16px at the bottom so the corner resizer is clickable
  (WebBrowserComponent is a heavyweight native view that intercepts mouse events
  in its area, hiding JUCE's lightweight resize grabber)
- ResizableCornerComponent now positioned in the uncovered bottom-right corner

## v0.4.3 (build 69) — Resizable editor with persisted size (2026-04-06)

- Editor now opens at 1400×380 (fits 16" MacBook screen) instead of 1900×516
- setResizeLimits: 600×200 min, 2400×800 max — drag any corner to resize
- Editor size persists in plugin state (saved per project, restored on reload)
- The HTML panel's existing scalePanel() handles inner content scaling automatically

## v0.4.2 (build 68) — Header cleanup, latency button (2026-04-06)

- Latency is now a proper "Test Latency" button next to Connect (not a clickable text label)
- Removed RTT, buffer, MIDI in, mapped from header (cluttered, mostly debug info)
- Latency value pill remains in header to show last test result
- Reduces header to: status • mode • version • latency • [Test latency] [Connect] [⚙]

## v0.4.1 (build 67) — End-to-end latency test (2026-04-06)

- Click "ms latency" pill in panel header to run a 5x latency test
- Sends note-on, scans incoming audio packets for peak above threshold (800)
- Measures real perceptual round-trip: keypress → MIDI → synth → audio capture → packet → audio thread
- Reports avg/min/max in the pill (hover for details, log shows full breakdown)
- Implementation: NetworkTransport scans int16 payloads when latencyTestActive, fires onAudioPeak callback to PluginProcessor; processor schedules note-on/off via Timer::callAfterDelay, handles 5 iterations with 350ms gaps and 1s per-test timeout
- Untested with hardware (built away from rig)

## Browser demo — Public Rev2 demo at rev2.anarack.com (2026-04-06)

- **Browser demo** — same Rev2 panel HTML runs in plugin AND browser via JS shim that replaces JUCE WebView bridge with WebSocket
- **VPS WebSocket relay** (`server/ws_relay.py`) — bridges public WSS to Pi's WebSocket over WireGuard, with 20-connection cap
- **GitHub Pages deploy** at https://rev2.anarack.com via `gh-pages` branch + Caddy + Let's Encrypt on VPS
- 5-octave keyboard injected into the panel for browser play (touch + computer keys A-L)
- AudioContext created on user gesture in parent page for iOS Safari
- Audio: 50ms scheduling buffer, generation counter prevents stale callback races, ArrayBuffer validation
- Connection states: connecting → live (on first audio/CC) → offline (after 10s timeout or WS error)
- "Powered off" visual: panel desaturates and dims when synth offline, boots back up on connect
- Centered status overlay (scaled-up panel pill) for connecting/offline states
- Mobile: skip keyboard, scroll the panel instead of scaling tiny
- Open Graph + Twitter Card meta tags for link previews
- Hide plugin-only controls (server, buffer, sensitivity, MIDI input, ping) in browser mode

## v0.3.20 (build 56) — Boot with real patch values, reconnect fix (2026-04-03)

- Knobs + patch name load from Rev2 on every connect (first load, reconnect, re-add plugin)
- Edit buffer request fires on every registration, throttled to 1 per 3 seconds
- Fixed: edit buffer CCs were sent to wrong port (MIDI source port instead of audio port 9999)
- Removed hardcoded "REV 2 Massive Pad" default — screen blank until real name arrives
- Removed "Connected" text — screen shows real patch name directly
- showConnectingState only fires if not already connected (was overwriting patch name)
- Known: program/bank pot position not synced from SysEx (not in parameter mapping)

## v0.3.19 (build 55) — Fix "Connecting..." stuck on screen (2026-04-03)

- showConnectingState was overwriting "Connected" text after auto-connect

## v0.3.18 (build 54) — UI polish: patch name transitions, disconnect state (2026-04-03)

- Screen keeps old patch name until new one arrives (no more '...' flash on program change)
- Disconnect properly resets screen to "Connecting..."
- Reconnect clears stale patch name state

## v0.3.17 (build 53) — Fix critical audio packet loss bug (2026-04-03)

- **Root cause found:** JSON detection `packetBuf[0] == '{'` false-matched audio packets when the sequence number's low byte was 0x7B (123 = ASCII `{`). This silently dropped 1 audio packet every 256 packets = 7 clicks per 5 seconds.
- Fix: check `bytesRead != 268` first — audio packets are always exactly 268 bytes, JSON never is.
- Also: increased UDP receive buffer to 1MB, single UDP audio client, JitterBuffer sequence tracking reorder, debug diagnostics (recv/dup/gap). These didn't fix the issue but are retained.
- Builds 50-52 were debugging iterations that didn't fix the clicking (attempted: disable duplication, disable MIDI poll, increase recv buffer, reorder sequence tracking).

## v0.3.14 (build 50) — Fix patch name broadcast, increase edit buffer delay (2026-04-03)

- Fix: patch name broadcast was short-circuiting when no WebSocket clients connected (early return before UDP send)
- Patch name now broadcasts via UDP socket to plugin clients
- Increase edit buffer request delay to 2 seconds (was 0.5s — CCs arrived before UI was ready)
- Known issue: relay mode has ~1.4 packets/sec loss on WiFi, causing clicks. Packet duplication (5ms) doesn't cover WiFi burst losses.

## v0.3.13 (build 49) — Boot animation with real patch values (2026-04-03)

- On connect, knobs stay at zero until real CC values arrive from Rev2's edit buffer dump
- Server requests edit buffer when plugin first registers — Rev2 sends all param values
- Knobs animate from zero to actual patch positions (not defaults)
- Fix CC broadcast: use MIDI UDP transport instead of audio_streamer socket (works for WireGuard)
- Default to WireGuard/P2P mode (not LAN)
- Fixed setStateInformation default for useWireGuard

## v0.3.12 (build 48) — Centralize version/build number (2026-04-03 11:10)

- **Version single source of truth** — version and build number now defined in `plugin/CMakeLists.txt` only
- CMake passes `ANARACK_VERSION` and `ANARACK_BUILD_NUMBER` as compile definitions
- Plugin sends version + build to WebView UI via `initConfig` event
- HTML status bar displays version from C++ (no more hardcoded version in HTML)
- Build number is a monotonic integer — increment on every build, never reset
- Status bar now shows `v0.3.12 (48)` format

### Missing from changelog: v0.3.11

v0.3.11 was released without a changelog entry. Changes unknown — included here for completeness.

## v0.3.10 — Fix CC feedback loop, auto-detect Scarlett (2026-04-03)

- Prevent bidirectional CC feedback loop: update lastAutomationVal when receiving CCs from synth
- start-all.sh auto-detects Scarlett card number (was hardcoded hw:0)

## v0.3.9 — Bidirectional MIDI via raw device (2026-04-03)

- Fix: rtmidi can't receive on same ALSA port open for output. Switch MIDI input to raw /dev/snd/midiCxDy with asyncio polling
- UDP program changes now trigger edit buffer request (was WebSocket only)
- Rev2 USB MIDI output may need global settings toggled off/on after Pi reboot

## v0.3.8 — Bidirectional MIDI: Rev2 → plugin UI (2026-04-02)

- Server broadcasts CCs to UDP plugin clients (was WebSocket only)
- Plugin parses JSON CC packets, updates UI knobs and DAW parameters
- Patch name displayed on OLED screen
- Transport.onSynthCC / onPatchName callbacks

## v0.3.7 — Fix LAN garbled audio (2026-04-02)

- Configure JitterBuffer for ALL modes (LAN + P2P). Server packet duplication was writing every packet twice to AudioRingBuffer (no dedup)
- Default to LAN mode

## v0.3.4-v0.3.6 — Reconnect fixes, diagnostics (2026-04-02)

- Reset resampler on disconnect
- Diagnostics always log at least once after connect
- Various prepareToPlay fixes

## v0.3.3 — Reconnect fix, LAN stability (2026-04-02)

- Fix reconnect race condition — stop old connect thread before resetting JitterBuffer
- Fix LAN garbled audio — don't resize AudioRingBuffer while streaming
- WgTunnel recv timeout 50ms→1ms, tick 100ms→10ms for less WireGuard jitter
- JitterBuffer reset on disconnect prevents ghost PLC between sessions

## v0.3.2 — Persist settings, P2P mode indicator (2026-04-02)

- Buffer size, WireGuard mode, server host persisted in plugin state across DAW restarts
- Buffer dropdown reflects saved value on editor open
- Connection mode shows P2P / Relay / LAN in status bar

## v0.3.1 — Auto-connect, connecting UI, P2P (2026-04-02)

- P2P direct WireGuard connection via Session API with ephemeral keys
- Auto-connect on plugin load (background thread from prepareToPlay)
- Connecting state: yellow pulsing dot, stays until audio flows
- Boot animation: knobs animate from zero on connect
- Session API + Pi Agent for P2P connection coordination
- Pi agent cleans up previous sessions on new connection

## v0.3.0 — Stable ASRC Streaming (2026-04-02)

Drift correction that actually works. Zero clicks in extended recordings.

- **ASRC crossfade drift correction** — replaces Lagrange resampler with drift-accumulating approach. Reads samples directly, tracks buffer fill drift, applies ±1 sample corrections via linear interpolation at zero crossings. Inaudible corrections every ~0.2s.
- **Block-size-independent ASRC** — filter alpha, drift clamp, and startup delay scale with block size. Works correctly at 32, 128, or 2048 sample blocks.
- **Buffer-level guard** — prevents ASRC from over-correcting when buffer is at/past target.
- **JitterBuffer fill level fix** — `samplesWritten` was double-counting duplicate packets, making fill appear to grow infinitely. ASRC dropped at max rate until buffer drained. Fixed to count only unique samples placed.
- **PLC crossfade on gap→data transition** — crossfades from last PLC output to real audio over up to 64 samples, eliminating hard transitions.
- **Buffer reconnect** — changing buffer size dropdown and reconnecting now reconfigures the JitterBuffer.
- **ASRC/PLC diagnostics** — log panel shows drop/dup counts, PLC samples, buffer fill, lost packets, block size.
- **Linear interpolation for small blocks** — at 32-sample blocks, the splice+crossfade approach didn't fit. Linear interpolation maps N±1 input samples smoothly across N output samples.

**Test results:** m19 — 0 discontinuities in 122 seconds (was 134 in m13).

### Commits
- `315764f` PLC crossfade on gap→data transition, buffer reconnect fix
- `9888283` ASRC: block-size-independent filter and drift clamp
- `662d8ba` Revert guard removal — keep buffer-level checks on ASRC corrections
- `dddd882` ASRC: linear interpolation for small blocks (replaces broken crossfade)
- `6b8cbfa` Fix JitterBuffer fill level double-counting duplicate packets
- `c69898c` ASRC zero-crossing detection — corrections at signal nulls
- `6602bf3` Widen ASRC crossfade from 8 to 32 samples (~0.67ms)
- `6fe286c` ASRC crossfade drift correction — replaces Lagrange resampler

## v0.2.2 — Stable Streaming Baseline (2026-04-01)

First clean extended recording (m12, 136 seconds).

- **JACK xrun watchdog** — auto-restart JACK when 5+ xruns accumulate in 60 seconds.
- Confirmed: fresh JACK restart produces clean audio; degradation is from accumulated JACK session timing issues.

### Commits
- `74aef66` JACK xrun watchdog — auto-restart when audio degrades
- `bd01309` v0.2.2 — Stable streaming baseline (m12 confirmed smooth)

## v0.2.1 — Fix ASRC Buffer Drain (2026-04-01)

- Fix buffer drain when ASRC ratio ~1.0 — direct read instead of resampler.

### Commits
- `a21ff19` v0.2.1 — Fix ASRC buffer drain: direct read when ratio ~1.0

## v0.2.0 — Complete Audio Streaming Overhaul (2026-04-01)

JitterBuffer, packet headers, packet duplication, FEC groundwork.

- **JitterBuffer** — timestamp-indexed ring buffer with packet loss concealment (PLC).
- **12-byte packet headers** — sequence number, timestamp, flags, checksum.
- **Server-side packet duplication** — 5ms delayed resend via asyncio.call_later.
- **Fixed buffer mode** — user-selectable buffer size with DAW PDC (Plugin Delay Compensation).

### Commits
- `cbfb3b2` v0.2.0 — Complete audio streaming overhaul

## v0.1.x — Initial Plugin Development (2026-03)

Rev2 front panel UI, MIDI CC control, MIDI Learn, DAW automation, WireGuard tunnel, basic audio streaming.
