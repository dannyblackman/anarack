# Hardware

## Architecture

Single rackmount setup in a home studio. All synths permanently wired to a central computer via multi-channel audio interface and MIDI interfaces. No per-synth computers.

```
┌─────────────────────────────────────────────────┐
│              Mac Mini (M4/M5)                     │
│                                                   │
│  CoreAudio + JACK (64 samples, 48kHz)            │
│  ├─ netjack2 session per user (UDP audio)        │
│  ├─ UDP MIDI routing per user                    │
│  ├─ Per-session WAV recording to NVMe            │
│  └─ Booking/session management server            │
│                                                   │
└──────┬──────────────────┬────────────────────────┘
       │                  │
  Multi-channel       MIDI interfaces
  audio interface     (iConnectivity mioXL
  (MOTU / RME)        or similar)
       │                  │
  ┌────┴────┐        ┌────┴────┐
  │Patchbay │        │Patchbay │
  └────┬────┘        └────┬────┘
       │                  │
  Synth rack (10-20 units, each with fixed
  audio channel pair + MIDI port assignment)
```

## Central Computer

**Mac Mini (M4 Pro or later)**

- Handles 20+ simultaneous audio streams easily
- CoreAudio is more reliable than Linux ALSA for multi-channel audio
- Quiet, small, low power (~30W)
- Needs to run headless — disable sleep, auto-updates, enable remote management
- NVMe SSD for recording multiple WAV streams (20 stereo streams @ 48/24 = ~23 MB/s, trivial)

**Estimated cost: £1,400-2,000**

## Audio Interface

Need 20+ inputs and outputs. Options:

| Interface | Channels | Linux/Mac | Price | Notes |
|---|---|---|---|---|
| MOTU 24Ai + 24Ao | 24 in / 24 out | Both | ~£1,500 each | Rock solid, class-compliant |
| RME MADIface | 64+ channels | Both | ~£3,000+ | Professional standard, best drivers |
| Behringer ADA8200 x3 | 24 channels via ADAT | Both | ~£600 total | Budget option, needs ADAT host |
| Focusrite Scarlett 18i20 x2 | 18 in each | Both | ~£800 total | Aggregate via CoreAudio |

**Recommended: MOTU 24Ai + 24Ao pair** — good balance of quality, reliability, and price. Expandable with ADAT if needed later.

**Estimated cost: £2,000-3,000**

## MIDI Interfaces

| Interface | Ports | Price | Notes |
|---|---|---|---|
| iConnectivity mioXL | 10 DIN + USB host | ~£300 | Also routes USB MIDI |
| MOTU MIDI Express XT | 8 DIN | ~£250 | Proven, rack-mountable |
| Multiple USB MIDI cables | Per-synth | ~£10 each | For USB MIDI synths only |

Many modern synths (Rev2, Sub37) support USB MIDI directly. Use that where possible, DIN MIDI interfaces for older/analog-only gear.

**Estimated cost: £300-500**

## Synth Rack — Starter Collection

Priority: iconic sounds that producers actually want and can't easily replicate in software.

### Phase 1 — Launch (5 synths)

| Synth | Type | Why | Price (used) |
|---|---|---|---|
| Sequential Prophet Rev2 | Poly analog | Already owned. Lush pads, versatile | — |
| Moog Sub37 | Mono analog | Already owned. The Moog bass sound | — |
| Roland Juno-106 / JU-06A | Poly analog/digital | The most requested vintage synth sound | £200-1,500 |
| Korg Minilogue XD | Poly analog + digital | Modern classic, great for leads | £350 |
| Arturia MiniBrute 2 | Mono analog | Raw, aggressive, semi-modular | £300 |

### Phase 2 — Growth (add 5-10 more)

| Synth | Type | Why | Price (used) |
|---|---|---|---|
| Dave Smith OB-6 | Poly analog | Oberheim sound, hugely desirable | £1,800 |
| Moog Grandmother | Semi-modular | Hands-on Moog, great for learning | £500 |
| Sequential Prophet 6 | Poly analog | Classic Prophet sound | £1,500 |
| Roland Jupiter-X/Xm | Digital/VA | Jupiter, Juno, SH emulations in one box | £600 |
| Novation Peak/Summit | Poly hybrid | Modern, versatile, great effects | £800-1,200 |

### Phase 3 — Premium

Rare/vintage gear at higher session rates: Yamaha CS-80 (if you can find one), ARP 2600, Roland Jupiter-8, Minimoog Model D.

## Rack & Infrastructure

- 19" rack enclosure (12-20U): £100-300
- Audio patchbay (2x 48-point): £100-200
- MIDI patchbay: £50-100
- Quality instrument cables (40+): £200-400
- Power conditioning (Furman or similar): £100-200
- UPS for the Mac Mini: £100-200
- Ethernet switch + cabling: £50

**Estimated cost: £700-1,400**

## Network

- **Wired ethernet** from Mac Mini to router — non-negotiable
- Stable broadband with low jitter — fibre preferred, 100Mbps+ upload
- Static IP or dynamic DNS for Tailscale/WireGuard endpoint
- Consider a dedicated connection if sharing with household use

## Prototype Hardware (Phase 0)

For proving the concept before investing:

| Item | Cost |
|---|---|
| Raspberry Pi 5 (8GB) | £75 |
| Focusrite Scarlett (already owned) | £0 |
| USB cables | £10 |
| **Total** | **~£85** |

Use with the Prophet Rev2 or Sub37 you already own. Prove latency is acceptable, then invest in the full rack.
