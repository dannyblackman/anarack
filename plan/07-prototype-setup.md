# Prototype Setup Guide

Step-by-step instructions for getting the Phase 0 prototype running.

## What You Need

- Raspberry Pi 5 (4GB) + official USB-C PSU + microSD card
- Focusrite Scarlett 2i2
- Sequential Prophet Rev2
- USB cables for both (USB-A to USB-B)
- Ethernet cable (Pi to router)
- Your Mac (with Tailscale already installed)
- A MIDI keyboard connected to your Mac (USB or Bluetooth)

## Step 1: Flash the microSD Card

1. Download and install [Raspberry Pi Imager](https://www.raspberrypi.com/software/) on your Mac
2. Insert the microSD card into your Mac (you may need a USB adapter)
3. Open Raspberry Pi Imager:
   - **Device:** Raspberry Pi 5
   - **OS:** Raspberry Pi OS Lite (64-bit) — under "Raspberry Pi OS (other)"
   - **Storage:** your microSD card
4. Click the **gear icon** (or "Edit Settings") before writing:
   - **Set hostname:** `anarack`
   - **Enable SSH:** yes, use password authentication
   - **Set username:** `pi`
   - **Set password:** something you'll remember
   - **Configure WiFi:** skip this (you're using ethernet)
5. Write the image. Takes a few minutes.

## Step 2: Boot the Pi

1. Insert the microSD card into the Pi
2. Plug in the ethernet cable (Pi to your router)
3. Plug in the USB-C power — the Pi boots automatically
4. Wait ~60 seconds for it to finish its first boot

## Step 3: Find the Pi on Your Network

From your Mac terminal:

```bash
ping anarack.local
```

If that doesn't resolve, check your router's admin page for the Pi's IP address, or:

```bash
dns-sd -B _ssh._tcp
```

## Step 4: SSH In and Run Setup

```bash
ssh pi@anarack.local
```

Then clone the repo and run the setup script:

```bash
git clone https://github.com/dannyblackman/anarack.git ~/anarack
cd ~/anarack
bash scripts/setup-pi.sh
```

This installs everything: JACK2, Tailscale, Python, rtmidi, realtime audio config.

When it finishes, reboot:

```bash
sudo reboot
```

## Step 5: Set Up Tailscale on the Pi

After reboot, SSH back in:

```bash
ssh pi@anarack.local
sudo tailscale up
```

This prints a URL — open it in your browser to authenticate the Pi to your Tailscale network.

Verify both machines can see each other:

```bash
# On the Pi
tailscale status
```

You should see both the Pi and your Mac listed. Note the Pi's Tailscale IP (starts with 100.x.x.x).

## Step 6: Plug In the Hardware

Connect to the Pi via USB:
- Scarlett 2i2 (audio interface)
- Prophet Rev2 (MIDI)

Verify the Pi sees them:

```bash
# Check audio devices
aplay -l

# Check MIDI devices (activate the Python env first)
source ~/anarack/venv/bin/activate
python ~/anarack/server/midi_router.py --list-ports
```

You should see the Scarlett in the audio list and the Rev2 in the MIDI list.

## Step 7: Start the Server on the Pi

```bash
cd ~/anarack
./start.sh
```

This starts JACK, the ALSA-MIDI bridge, and the MIDI router. It prints the WebSocket address — you'll need this for the next step.

## Step 8: Set Up Your Mac

If you haven't already:

```bash
cd ~/path/to/anarack
bash scripts/setup-mac.sh
```

## Step 9: Test It

### Test 1: LAN (same network)

1. Open `client/index.html` in Chrome on your Mac
2. Set the server address to the Pi's **local** IP (e.g. `192.168.1.x:8765`)
3. Click Connect
4. Play your MIDI keyboard — the Rev2 should respond
5. You'll hear audio directly from the Rev2 in the room (no network audio yet)

This confirms: MIDI keyboard → browser → WebSocket → Pi → Rev2 works.

### Test 2: Over Tailscale (simulated remote)

1. Change the server address to the Pi's **Tailscale** IP (e.g. `100.x.x.x:8765`)
2. Tether your Mac to your phone's mobile hotspot (so traffic actually goes over the internet)
3. Play your MIDI keyboard
4. The Rev2 should still respond — the latency you feel now is real network latency

This is the go/no-go test. If the MIDI response feels playable (<50ms), the concept works.

### Test 3: Network Audio (full round trip)

This is the final piece — getting the Rev2's audio back to your Mac over the network via netjack2. We'll set this up after Tests 1 and 2 pass, since it needs additional JACK configuration on both ends.

## Troubleshooting

### Pi doesn't appear on the network
- Check the ethernet cable is plugged in at both ends
- Try `ping anarack.local` — if it doesn't resolve, find the IP from your router's admin page

### JACK won't start
- Check the Scarlett is plugged in: `aplay -l`
- The device name might differ — check `aplay -l` output and update `~/.jackdrc` if needed
- Try a larger buffer if you get xruns: change `-p 64` to `-p 128` in `~/.jackdrc`

### No MIDI ports found
- Check the Rev2 is plugged in via USB
- Run `amidi -l` to see raw MIDI devices
- Try unplugging and replugging the Rev2

### WebSocket won't connect
- Check the MIDI router is running: `ps aux | grep midi_router`
- Check the port isn't blocked: `curl http://<pi-ip>:8765` (should get a WebSocket error, not a timeout)
- If using Tailscale IP, make sure both machines are authed and connected: `tailscale status`

### Latency feels too high
- Make sure the Pi is on ethernet, not WiFi
- For the mobile hotspot test, try a different location (4G signal strength matters)
- Check JACK isn't reporting xruns: `jack_lsp` and watch the start.sh output
