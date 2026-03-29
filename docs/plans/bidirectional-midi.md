# Plan: Bidirectional MIDI — Read Parameter Values from Rev2

**Status:** Implemented (2026-03-29)

## Context

Currently MIDI flows one way: **browser → server → Rev2**. When a user changes program on the Rev2, the on-screen knobs don't update to reflect the actual patch values. This makes the UI feel disconnected from the real synth.

**Goal:** When a program changes (or any parameter changes on the synth), the on-screen knobs should snap to the actual values. This creates the feeling of "I'm looking at the real synth's front panel."

## Approach: CC Transmit from Rev2

The Rev2 has a Global setting called **"MIDI Param Xmit"** — when set to **"CC"**, the Rev2 broadcasts all parameter values as CC messages whenever:
- A program is loaded
- A knob is turned on the synth itself
- A parameter changes for any reason

This is simpler than SysEx dumps — we just listen for incoming CCs and update the UI.

**Prerequisite:** User must set Rev2 Global setting "MIDI Param Xmit" → "CC". We'll add a note about this in the UI.

## Previous Architecture

```
Browser/App  →  WebSocket  →  Server (midi_router.py)  →  rtmidi.MidiOut  →  Rev2
                                                                                 ↓
                                                           (audio only)    ←  Scarlett
```

MIDI was output-only. `rtmidi.MidiOut` sent TO the Rev2 but nothing read FROM it.

## New Architecture

```
Browser/App  →  WebSocket  →  Server  →  rtmidi.MidiOut  →  Rev2
Browser/App  ←  WebSocket  ←  Server  ←  rtmidi.MidiIn   ←  Rev2
                                                                ↓
                                          (audio)         ←  Scarlett
```

## Files Modified

| File | Changes |
|---|---|
| `server/midi_router.py` | Added `rtmidi.MidiIn`, forward incoming CCs to WebSocket clients, echo suppression |
| `client/index.html` | Handle incoming CC messages from server, update knob positions |

## Implementation Details

### Server — `midi_router.py`

1. **`open_midi_input()`** — mirrors `open_midi_port()` but for `rtmidi.MidiIn`, same substring matching logic

2. **`MidiRouter` additions:**
   - `midi_ws_clients: set` — tracks connected MIDI WebSocket clients
   - `_last_sent: dict[int, float]` — `{cc: timestamp}` for echo suppression
   - `_loop` — reference to asyncio event loop (needed because rtmidi callback runs on a separate thread)
   - `on_synth_message()` — rtmidi callback, filters to CC messages, checks echo suppression, broadcasts
   - `_broadcast_cc()` — JSON-encodes CC and sends to all connected MIDI WS clients via `asyncio.run_coroutine_threadsafe()`
   - `_is_echo()` — returns True if we sent the same CC within 50ms (prevents feedback loops)

3. **Echo suppression:** `send()` now timestamps outgoing CCs in `_last_sent`. When a CC arrives from the synth within 50ms of one we sent, it's treated as an echo and dropped.

4. **`ws_handler`:** MIDI clients are added to `router.midi_ws_clients` on connect, removed on disconnect.

5. **`main()`:** Opens MIDI input alongside output, sets the callback. Gracefully degrades if no input port available.

### Browser — `index.html`

**Message format (server → browser):**
```json
{"type": "cc", "cc": 102, "value": 87}
```

This is different from the existing browser→server format (`{"status", "data1", "data2"}`) so the browser can distinguish incoming synth state from its own outgoing messages.

`ws.onmessage` handler parses incoming JSON, finds the matching knob by CC number, and calls `setKnobValue(knob, value, false)` — `false` means don't send it back to the server (avoids feedback loop).

## Verification

1. Set Rev2 Global → MIDI Param Xmit → CC
2. Connect browser client, play a note (confirm basic MIDI works)
3. Change program via the UI buttons → all knobs should snap to new values
4. Turn a knob on the Rev2 itself → on-screen knob should follow
5. Drag an on-screen knob → Rev2 should respond WITHOUT the knob bouncing (echo suppression working)
6. Test with both browser client and native app
