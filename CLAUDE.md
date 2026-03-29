# Anarack

Remote hardware synth studio — producers control real synths over the internet, hear near-real-time audio, download lossless recordings.

## Project Phase

Currently in **Phase 0 — Prototype**. Proving that remote synth control with <50ms latency is viable using a Raspberry Pi 5 + Scarlett 18i20 + Prophet Rev2/Sub37.

## Tech Stack (Prototype)

- **Server (Pi):** Python 3.11+, JACK2 + netjack2, rtmidi, asyncio
- **Client (Web):** Vanilla HTML/JS (no framework for prototype), WebSocket for MIDI
- **Networking:** Tailscale (prototype only — production will use embedded WireGuard)
- **Audio:** JACK at 64 samples / 48kHz, raw PCM over netjack2

## Project Structure

```
docs/strategy/  — Business plan & strategy docs (overview, financials, timelines, etc.)
docs/plans/     — Implementation plans and allium specs (from /plan and /interview)
server/         — Runs on Raspberry Pi 5 (MIDI routing, audio engine, session management)
client/         — Browser-based demo UI (virtual keyboard + CC sliders)
scripts/        — Setup and utility scripts (JACK config, Pi setup, dev server)
worktrees/      — Git worktrees for feature branches (gitignored)
```

## Development Workflow

### Branches

- `main` is the default branch — always deployable
- Feature branches: `feat/description`
- Bug fixes: `fix/description`
- Chores/maintenance: `chore/description`

### Worktrees

Use git worktrees for parallel work on feature branches:

```bash
# Create a worktree for a new feature branch
git worktree add worktrees/feat-my-feature -b feat/my-feature

# Clean up when done (after merging)
git worktree remove worktrees/feat-my-feature
```

### Commands

```bash
# Local dev (serves client UI)
./scripts/dev.sh                   # http://localhost:8080

# Server (on Pi)
cd server && pip install -r requirements.txt
python midi_router.py              # Start MIDI routing server
```

## Raspberry Pi Deployment

The server runs on a Raspberry Pi 5. Deploy directly via SSH — never ask the user to SSH manually.

```bash
# Connection
ssh pi@anarack.local              # also: 100.94.117.51 via Tailscale

# Deploy server
scp server/midi_router.py pi@anarack.local:~/anarack/server/midi_router.py

# Restart server (use venv Python, PYTHONUNBUFFERED for log visibility)
ssh pi@anarack.local "cd /home/pi/anarack && PYTHONUNBUFFERED=1 nohup venv/bin/python server/midi_router.py --midi-port 'Prophet Rev2' > /tmp/anarack.log 2>&1 &"

# Check logs
ssh pi@anarack.local "cat /tmp/anarack.log"
```

**Important:**
- Python venv is at `~/anarack/venv/` — always use `venv/bin/python`
- JACK runs as a separate process (`jackd -R -d alsa -d hw:0 -r 48000 -p 128 -n 3`)
- Scarlett 18i16 is `hw:0`, Rev2 MIDI is on USB

## Key Decisions

- Prototype uses Tailscale for networking simplicity; production uses embedded boringtun (WireGuard)
- JACK2 with netjack2 for audio transport during prototype
- Python for prototype speed; production server may stay Python or move to Rust for audio path
- No framework for prototype web UI — keep it minimal
