# Changelog

## v0.3.12 (build 48) — Centralize version/build number (2026-04-03)

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
