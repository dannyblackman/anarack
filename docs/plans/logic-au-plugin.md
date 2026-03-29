# Logic Pro AU Plugin — Anarack Rev2

**Status:** In Progress

## Overview

JUCE-based Audio Unit instrument plugin for Logic Pro. The plugin sends MIDI from Logic to the Anarack server over UDP and receives live audio back, allowing producers to play the Prophet Rev2 as if it were a local software instrument.

## Architecture

### Plugin Identity

| Field             | Value                  |
|-------------------|------------------------|
| Format            | AU instrument (`aumu`) |
| Plugin name       | Anarack Rev2           |
| Manufacturer code | `Anak`                 |
| Plugin code       | `Arv2`                 |

### Network Protocol

**MIDI out (plugin to server):**
- UDP port **5555**
- Raw 3-byte MIDI messages (status, data1, data2)
- Lowest possible latency — no framing, no headers

**Audio in (server to plugin):**
- UDP port **9999**
- int16 PCM, mono, 48 kHz
- 128-sample blocks per packet

**Connection handshake:**
- Plugin sends a single `0xFE` (Active Sensing) byte to the server on connect
- Server treats this as a registration and begins streaming audio back to the sender's address

### Threading Model

```
Logic audio thread
  |
  |-- reads audio from ring buffer --> processBlock output
  |-- writes MIDI to MIDI FIFO -----> network send thread
  |
Network receive thread
  |-- writes PCM into ring buffer (lock-free)
  |
Network send thread
  |-- reads MIDI from FIFO, sends UDP (lock-free)
```

**Ring buffer (audio):** `juce::AbstractFifo` wrapping a flat int16 array. The network receive thread is the sole writer; the audio `processBlock` is the sole reader. No locks on the audio path.

**MIDI FIFO:** Lock-free single-producer single-consumer FIFO. The audio thread enqueues MIDI events during `processBlock`; a dedicated network send thread dequeues and transmits via UDP.

### UI

Minimal editor panel:

- **Server IP** text field (e.g. `100.64.0.1`)
- **Connect / Disconnect** button
- **Status** label (disconnected / connecting / streaming)
- **Buffer level** indicator (ring buffer fill %)

## File Layout

```
plugin/
  CMakeLists.txt                  # JUCE project config, AU target
  JUCE/                           # JUCE framework (git submodule)
  src/
    PluginProcessor.h             # AudioProcessor subclass
    PluginProcessor.cpp
    PluginEditor.h                # Editor (UI) subclass
    PluginEditor.cpp
    NetworkTransport.h            # UDP send/receive, connection logic
    NetworkTransport.cpp
    AudioRingBuffer.h             # Lock-free ring buffer (AbstractFifo wrapper)
```

JUCE is added as a git submodule at `plugin/JUCE`.

## Build

```bash
# From the plugin/ directory
cmake -B build -G Xcode
cmake --build build --config Release
```

The built `.component` bundle is copied to `~/Library/Audio/Plug-Ins/Components/` automatically by JUCE's CMake integration. Rescan in Logic or run `auval -a` to verify.

## Requirements

- Logic Pro session **must be set to 48 kHz** sample rate to match the server's JACK configuration
- macOS with Xcode and CMake installed
- Network access to the Anarack server (Tailscale for prototype)
