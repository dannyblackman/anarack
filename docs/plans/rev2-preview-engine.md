# Rev2 Preview Engine — Implementation Plan

**Goal:** Add a local synth engine to the plugin that emulates the Rev2's signal path, giving users zero-latency sound design. When connected to hardware, the real Rev2 takes over for final recording.

**Workflow:** Load preset → Preview mode (local DSP, zero latency) → Connect → Hardware mode (real Rev2 audio) → Tweak → Record lossless

**Key decisions:**
- **Dual Layer (A+B) is Phase 1.** The Rev2's stacked layers are fundamental to its sound — many patches use both layers with different filter/detune/timbre settings. Without Layer B the preview sounds wrong.
- **Preset loading/saving is Phase 1.** SysEx patch import/export enables the core workflow: load a patch, preview locally, send to hardware, A/B, save. This is not optional.

## Architecture Overview

The preview engine slots into the existing `AnarackProcessor::processBlock()`. A mode toggle determines whether audio comes from the local synth engine or the network ring buffer. MIDI and parameter changes drive both paths — the same knob movements that control the Rev2 also control the emulator.

```
                    ┌─────────────────────────────┐
  MIDI / CCs ──────►│  AnarackProcessor            │
                    │                              │
                    │  mode == Preview?             │
                    │    YES → Rev2Engine::render() │──► Audio out (local DSP)
                    │    NO  → NetworkTransport     │──► Audio out (from Pi)
                    └─────────────────────────────┘
```

## Rev2 Signal Path to Model

```
Layer A (8 voices):                          Layer B (8 voices):
  OSC1 ─┐                                     OSC1 ─┐
        ├─► Mixer ─► LPF ─► VCA ──┐                 ├─► Mixer ─► LPF ─► VCA ──┐
  OSC2 ─┘  +Sub +Noise      ↑     │           OSC2 ─┘  +Sub +Noise      ↑     │
                          FilterEnv │                                 FilterEnv │
                          AmpEnv    │                                 AmpEnv    │
                                    ├──► Stereo Mix ──► Effects ──► Output
```

Each layer has 8 voices. Each voice has: 2 DCOs, mixer (osc balance + sub + noise), 4-pole LPF, filter envelope, amp envelope, aux envelope, 4 LFOs, mod matrix. The two layers can be stacked (both play same notes), split (keyboard zones), or independent.

**Layer modes:**
- **Stacked:** Both layers triggered by every note (16 voices = 8 per layer). This is the "thick Rev2 sound."
- **Split:** Layer A on lower keys, Layer B on upper keys, configurable split point.
- **Layer A only:** 16 voices, single layer (simpler patches).

## Components

### Phase 1 — Core Engine (makes sound)

#### 1.1 `Rev2Voice` — Single voice with full signal path

**Oscillators (2 per voice):**
- PolyBLEP anti-aliased waveforms: sawtooth, triangle, saw+tri, pulse
- Pulse width modulation (CC 30, 31)
- Frequency (CC 20, 24) — semitone-based, 0-120 range maps to MIDI note offset
- Fine tune (CC 21, 25) — ±50 cents
- Hard sync (osc2 synced to osc1)
- Glide/portamento (CC 23, 27 amount, CC 65 on/off)

**Mixer:**
- Osc balance (CC 28) — crossfade between osc1 and osc2
- Sub oscillator level (CC 8) — one octave below osc1
- Noise level (CC 29) — white noise
- Oscillator slop (CC 9) — random pitch drift per voice, updated per sample block

**Filter:**
- 4-pole resonant low-pass filter (SSI2144-style)
- Use Zavalishin's TPT (topology-preserving transform) ladder model — best balance of accuracy and CPU
- Cutoff (CC 102), Resonance (CC 103)
- Key tracking amount (CC 104)
- Audio mod amount (CC 105) — feed pre-filter signal back to modulate cutoff
- Filter envelope amount (CC 106) — bipolar (0-254, center=127)

**Filter Envelope (ADSR):**
- Delay (CC 108), Attack (CC 109), Decay (CC 110), Sustain (CC 111), Release (CC 112)
- Velocity amount (CC 107)
- Modulates filter cutoff

**Amp Envelope (ADSR):**
- Delay (CC 117), Attack (CC 118), Decay (CC 119), Sustain (CC 75), Release (CC 76)
- Velocity amount (CC 116)
- VCA Level (CC 113) as base amplitude

**Aux Envelope (Env 3):**
- Delay (CC 88), Attack (CC 89), Decay (CC 90), Sustain (CC 77), Release (CC 78)
- Destination select (CC 85) — routes to various parameters
- Amount (CC 86) — bipolar
- Velocity amount (CC 87)

**LFOs (4):**
- Multiple shapes: triangle, sawtooth, reverse saw, square, random
- Rate, amount, destination
- Note: LFO CCs are mostly NRPNs on the real Rev2, not simple CCs. For Phase 1, implement LFOs internally but defer full NRPN mapping to Phase 2.

#### 1.2 `Rev2Engine` — Dual-layer polyphonic voice manager

- **Two independent layers (A + B)**, each with its own parameter set
- 16 voices total: 8 per layer in stacked/split mode, or 16 for single-layer
- Layer modes: Stacked, Split (configurable split point), Layer A only
- Voice allocation per layer: lowest-available, with voice stealing (oldest note) when full
- Unison mode: stack multiple voices on one note with configurable detune
- Pan spread (CC 114): distribute voices across stereo field
- Global pitch bend, mod wheel handling
- Renders both layers and sums to stereo output

#### 1.3 `Rev2Preset` — SysEx patch import/export

The Rev2's SysEx format is documented in the manual. Each patch is a structured dump of all parameters for both layers.

**SysEx Patch Format (Rev2):**
- Header: `F0 01 2F` (Sequential, Rev2)
- Program dump: `02` (single program)
- Data: ~400 parameters, packed with the Rev2's nybble encoding (each byte split into two nybbles for 7-bit MIDI safety)
- Footer: checksum + `F7`

**Features:**
- `loadFromSysEx(const uint8_t* data, int size)` — Parse SysEx dump, set all engine parameters for both layers
- `saveToSysEx()` — Export current engine state as a valid Rev2 SysEx program dump
- `sendToHardware()` — Send SysEx dump to the real Rev2 via the transport layer
- `requestFromHardware()` — Request current patch from Rev2, load into engine
- File I/O: load/save `.syx` files for patch library management

**Workflow integration:**
1. User loads a `.syx` file or requests current patch from hardware
2. Engine parameters set instantly — preview plays the patch locally
3. User tweaks in preview mode (zero latency)
4. "Send to Hardware" pushes the patch to the real Rev2
5. User can save modified patch back to `.syx`

This enables round-tripping: hardware → emulator → tweak → hardware → record.

#### 1.4 Parameter bridge

The existing `SynthParam` array and `paramByCC[]` lookup already maps CCs to parameter values. The preview engine reads from the same `ccValues[128]` array that currently drives the hardware.

New code needed:
- `Rev2Engine::setCC(int cc, int value)` — updates the engine's internal parameter state
- Called from the same place that currently calls `transport.sendMidi()` in processBlock
- In preview mode, CC changes go to engine instead of (or in addition to) network

### Phase 2 — Effects

The Rev2 has a single effects bus with selectable type (CC 3):

| FX Type | CC 3 Value | Parameters |
|---------|-----------|------------|
| Off | 0 | — |
| Delay (mono) | 1 | Time, Feedback |
| Delay (stereo) | 2 | Time, Feedback |
| BBD Delay | 3 | Time, Feedback |
| Chorus | 4 | Rate, Depth |
| Phaser | 5 | Rate, Depth |
| Flanger | 6 | Rate, Depth |
| Distortion | 7 | Amount, Tone |
| Ring Mod | 8 | Freq, Depth |
| Reverb | 9 | Time, Damping |
| Various combos | 10-13 | ... |

- FX on/off (CC 16), FX mix (CC 17)
- FX Param 1 (CC 12), FX Param 2 (CC 13)
- Implement using JUCE's built-in DSP: `dsp::DelayLine`, `dsp::Chorus`, `dsp::Reverb`
- Distortion: simple waveshaper (tanh soft clip)
- Phaser/Flanger: modulated allpass chains

### Phase 3 — NRPN Parameters and Mod Matrix

The Rev2's deeper parameters (individual LFO rates/destinations, mod matrix slots, unison detune, etc.) use NRPNs rather than CCs. This phase adds:

- NRPN message parsing (CC 99/98 for parameter number, CC 6/38 for value)
- Full mod matrix: 8 slots, each with source → destination → amount
- Per-LFO: rate, shape, amount, destination, sync
- Unison detune amount
- Voice mode (poly, unison, mono, etc.)

### Phase 4 — Tuning and Character

This is the open-ended "make it sound more like a Rev2" phase:

- **Filter tuning:** A/B the emulator vs hardware at various cutoff/resonance settings, adjust the filter model's nonlinearity and saturation curves
- **Oscillator character:** Tune the slop algorithm, adjust waveform shapes to match Rev2's specific DCO behavior
- **Envelope curves:** The Rev2 uses exponential curves — tune the curvature to match
- **Gain staging:** Add subtle soft-clipping at mixer and filter stages

## File Structure

```
plugin/src/
  engine/
    Rev2Engine.h / .cpp      — Dual-layer voice manager, polyphony, stereo summing
    Rev2Voice.h / .cpp        — Single voice: oscs, filter, envs, LFOs
    Rev2Preset.h / .cpp       — SysEx parsing/generation, .syx file I/O, preset round-tripping
    Oscillator.h / .cpp       — PolyBLEP oscillator (saw, tri, pulse, sync)
    LadderFilter.h / .cpp     — TPT 4-pole LPF (SSI2144-style)
    Envelope.h / .cpp         — DAHDSR envelope with configurable curves
    LFO.h / .cpp              — Multi-shape LFO
    Effects.h / .cpp          — FX chain (delay, chorus, phaser, etc.)
  PluginProcessor.h / .cpp    — Modified: mode toggle, routes to engine or network
  PluginEditor.h / .cpp       — Modified: Preview/Hardware toggle in UI
  ...existing files...
```

## Changes to Existing Code

### PluginProcessor.h
```cpp
// New members:
#include "engine/Rev2Engine.h"

enum class PlayMode { Preview, Hardware };
std::atomic<int> playMode { (int)PlayMode::Preview };  // atomic for thread safety
Rev2Engine previewEngine;
```

### PluginProcessor::processBlock()
```
// After MIDI processing (which already extracts CCs and notes):
if (playMode == Preview)
{
    // Feed MIDI events to preview engine
    previewEngine.processBlock(buffer, midiEvents, numSamples);
    // Still send CCs to hardware so it stays in sync (optional)
}
else
{
    // Existing network audio path (unchanged)
    ...read from audioRingBuffer...
}
```

### UI Changes
- Add a toggle button/switch: "Preview" / "Hardware"
- Preview mode available even when disconnected (the whole point)
- Visual indicator showing which mode is active
- When switching Preview → Hardware, the sound changes character but the patch translates

## Implementation Order

1. **Oscillator.h/cpp** — PolyBLEP saw, pulse, triangle. Test with a simple sine first.
2. **Envelope.h/cpp** — ADSR with delay stage and velocity scaling.
3. **LadderFilter.h/cpp** — TPT ladder, 4-pole, with resonance and saturation.
4. **Rev2Voice.h/cpp** — Wire up: oscs → mixer → filter → amp. One voice, monophonic.
5. **Rev2Engine.h/cpp** — Dual-layer polyphony, voice allocation, stereo output.
6. **Rev2Preset.h/cpp** — SysEx parsing/generation, parameter mapping for both layers.
7. **Wire into PluginProcessor** — Mode toggle, parameter bridge, preset load/save.
8. **Test and A/B** — Load real Rev2 patches via SysEx, compare against hardware.
9. **LFO.h/cpp** — Multi-shape, routable to pitch/filter/amp.
10. **Effects.h/cpp** — Delay, chorus, distortion at minimum.
11. **Mod matrix and NRPNs** — Deep parameter control.
12. **Preset browser UI** — Load/save .syx files, request/send patches to hardware.

## Open Questions

- **CPU budget:** 16 voices × 2 layers × (2 oscs + filter + 3 envs + 4 LFOs) at 48kHz. Should be fine on any modern Mac but worth monitoring — worst case is 16 stacked voices where both layers render per note.
- **Web UI integration:** The current UI is a WebView. The Preview/Hardware toggle and preset browser need to be exposed to the web UI via the existing JS bridge.
- **Rev2 SysEx documentation:** Need to verify the exact nybble packing format and parameter order from the Rev2 manual. The MIDI implementation chart documents all parameters but the SysEx dump format has some quirks (packed nybbles, checksum).
- **Patch bank management:** Individual patch load/save only for now. No need to mirror the Rev2's full 8×128 bank structure — just recall a sound and apply it.
