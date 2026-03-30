# Anarack UX Workflow & Synth Definition System

**Status:** Planned
**Date:** 2026-03-30
**Source:** Stakeholder interview

## Core Product Experience

Anarack is a remote hardware instrument. The latency reality is managed through a two-phase workflow: **sound design** (live, interactive, latency-tolerant) and **bounce** (offline-quality recording, zero-latency audio capture).

## The Two-Phase Workflow

### Phase 1: Sound Design (Live)

The user explores and creates their sound in real time.

```
User plays keys / turns knobs
        ↓
MIDI sent over WireGuard tunnel (~18ms)
        ↓
Pi receives MIDI, sends to synth (<1ms)
        ↓
Synth produces audio → JACK captures
        ↓
Audio streamed back over tunnel (~18ms)
        ↓
User hears the synth (~40ms total round trip)
```

**Session start:** Synth loads Programme 1, Bank 1. User can browse presets or start designing from there.

**What the user does:**
- Plays keys on their MIDI controller to audition the sound
- Turns knobs (hardware controller or on-screen) to shape the sound
- Browses factory presets for starting points
- Saves their sound as an Anarack preset for recall later

**Latency is fine here** — they're designing, not performing to a click track. 40ms feels like playing through a slightly distant amp.

### Phase 2: Bounce (Offline Quality)

The user has their sound. Now they want a perfect recording with their composed MIDI part.

```
User clicks "Bounce" in the Anarack plugin
        ↓
Plugin sends the ENTIRE MIDI file to the server
(all notes + CC automation, as a complete file, not real-time)
        ↓
Server plays the MIDI locally to the synth (<1ms, zero network latency)
        ↓
Server records lossless 24-bit WAV simultaneously
        ↓
Audio also streamed back to user for real-time monitoring
(user hears it playing with latency — that's fine, it's just a preview)
        ↓
Recording finishes → server sends WAV back to plugin
        ↓
WAV automatically placed on an audio track in the DAW
(perfectly aligned to the timeline — zero latency artefacts)
        ↓
WAV also available for manual download from the plugin UI
```

**Key insight:** The bounce audio has ZERO network latency because the server plays the MIDI locally. The recording is bit-perfect. The user monitors it in real-time (with latency) but the actual WAV is clean.

**Why this matters:** Producers don't care about live latency for the final recording. They care that the WAV they get back is perfectly timed and studio quality. This workflow gives them both: interactive sound design AND perfect recordings.

### What the user needs to understand

The plugin should make the two phases clear:
- **"Design" mode** — you're hearing the synth live, there's ~40ms delay, that's normal
- **"Bounce" mode** — send your MIDI, get a perfect WAV back, no timing issues
- The monitoring audio during bounce has latency but the downloaded WAV doesn't

## Synth Definition System

Every synth needs a complete parameter definition that drives:
1. The UI (what knobs/switches/selectors to show)
2. The MIDI mapping (which CC/NRPN/SysEx controls each parameter)
3. The controller mapping (which hardware knob controls which parameter)
4. The bounce engine (what automation data to include in the MIDI file)

### Definition Format

```json
{
  "id": "sequential-prophet-rev2",
  "name": "Sequential Prophet Rev2",
  "manufacturer": "Sequential",
  "midi_channel": 1,
  "image": "rev2-hero.png",

  "sysex": {
    "manufacturer_id": [0x01],
    "device_id": 0x2F,
    "edit_buffer_request": [0xF0, 0x01, 0x2F, 0x06, 0xF7],
    "edit_buffer_response_cmd": 0x03,
    "packing": "dsi_7bit"
  },

  "groups": [
    {
      "id": "osc1",
      "name": "OSC 1",
      "order": 1,
      "parameters": [
        {
          "id": "osc1_freq",
          "label": "Frequency",
          "type": "knob",
          "cc": 20,
          "min": 0,
          "max": 120,
          "default": 24,
          "sysex_offset": 0,
          "display": "note_name"
        },
        {
          "id": "osc1_shape",
          "label": "Shape",
          "type": "selector",
          "cc": 22,
          "min": 0,
          "max": 4,
          "default": 1,
          "sysex_offset": 4,
          "values": ["Off", "Saw", "Saw+Tri", "Triangle", "Pulse"]
        },
        {
          "id": "osc1_sync",
          "label": "Sync",
          "type": "toggle",
          "cc": 17,
          "values": ["Off", "On"],
          "default": 0
        }
      ]
    },
    {
      "id": "filter",
      "name": "FILTER",
      "order": 4,
      "parameters": [
        {
          "id": "filter_cutoff",
          "label": "Cutoff",
          "type": "knob",
          "cc": 102,
          "min": 0,
          "max": 164,
          "default": 100,
          "sysex_offset": 22
        },
        {
          "id": "filter_poles",
          "label": "Poles",
          "type": "selector",
          "cc": 19,
          "values": ["2-pole", "4-pole"],
          "default": 1
        }
      ]
    },
    {
      "id": "effects",
      "name": "EFFECTS",
      "order": 8,
      "parameters": [
        {
          "id": "fx_type",
          "label": "Type",
          "type": "selector",
          "cc": 3,
          "values": ["Off", "Delay M", "Delay S", "BBD Dly", "Chorus",
                     "Phas Hi", "Phas Lo", "Phas Ms", "Flang 1", "Flang 2",
                     "Reverb", "RingMod", "Distort", "HP Filt"],
          "default": 0,
          "dynamic_labels": {
            "fx_param1": {
              "0": "—", "1": "Time", "2": "Time", "3": "Time",
              "4": "Rate", "5": "Rate", "6": "Rate", "7": "Rate",
              "8": "Rate", "9": "Rate", "10": "Time", "11": "Tune",
              "12": "Gain", "13": "Cutoff"
            },
            "fx_param2": {
              "0": "—", "1": "Feedbk", "2": "Feedbk", "3": "Feedbk",
              "4": "Depth", "5": "Depth", "6": "Depth", "7": "Depth",
              "8": "Depth", "9": "Depth", "10": "Color", "11": "Track",
              "12": "Tone", "13": "Reso"
            }
          }
        }
      ]
    }
  ],

  "presets": {
    "factory_banks": ["A", "B", "C", "D", "E", "F", "G", "H"],
    "programs_per_bank": 128,
    "bank_select_cc": 0,
    "program_change": true
  },

  "controller_profiles": {
    "launchkey_mk3": {
      "name": "Novation Launchkey MK3",
      "encoding": "relative",
      "default_mapping": {
        "21": "filter_cutoff",
        "22": "filter_reso",
        "23": "osc1_shape",
        "24": "osc1_freq"
      }
    }
  }
}
```

### Control Types

| Type | Widget | Use case |
|------|--------|----------|
| `knob` | Continuous rotary (0-127 or custom range) | Filter cutoff, volume, envelope times |
| `selector` | Button group or dropdown with named values | Osc shape, FX type, filter poles |
| `toggle` | On/off switch | Sync, unison, FX on/off |
| `display` | Read-only value | Patch name, current program number |

### Data Sources for Synth Definitions

1. **Bootstrap from open-source editors** — Edisyn, KnobKraft-orm, Ctrlr all have complete parameter maps for hundreds of synths. Parse their source code to generate our JSON definitions.
2. **Validate against MIDI implementation charts** — cross-reference with each synth's official MIDI spec.
3. **Community contributions** — eventually let users submit corrections and new synth definitions.

### UI Generation

The UI is **generated from the definition file**, not hand-coded per synth:
- Groups become collapsible sections or tabs
- Parameters render as the appropriate widget based on `type`
- Layout follows group `order`
- Anarack's design language (colours, fonts, knob style) is consistent across all synths
- Dynamic labels (like FX param names changing based on FX type) are driven by the definition

### Preset Save/Recall

- Users can save their current sound as an Anarack preset (stores all parameter values)
- Presets are per-user, per-synth
- Stored server-side (so they persist across sessions)
- Can be recalled in future sessions on the same synth
- Factory presets browsable via the definition's `presets` config

### MIDI Controller Mapping

- Auto-detection of known controllers (Launchkey, KeyLab, etc.)
- Manual learn mode: double-click a UI knob → turn hardware knob → mapped
- Mappings stored per-user, per-synth, per-controller
- Soft-takeover: when switching parameter pages, knobs don't jump
- Controller profiles defined in the synth definition for sensible defaults

## Out of Scope (v1)

- **Multi-synth layering** — single synth per session
- **Preset marketplace** — personal presets only, no sharing/selling
- **DAW automation recording** — no recording knob movements as automation lanes
- **Live performance mode** — this is a recording/sound design tool

## Resolved Questions

1. **MIDI file format for bounce:** Standard MIDI File (SMF Type 0). All CC automation is included natively — every DAW exports this. Notes, CC sweeps, program changes, mod wheel — all in the file.
2. **WAV delivery:** Direct transfer through the WireGuard tunnel. Cloud backup for later download is a future feature, not v1.
3. **Multi-take bounce:** One at a time for v1. User bounces, gets a WAV. Bounces again for a different take. No take management UI needed — they just end up with multiple audio files in the DAW.
4. **Preset format:** SysEx dump from the synth. Request the edit buffer, store the raw dump. 100% accurate hardware state. Format differs per synth but the synth definition's SysEx config handles parsing/sending.
