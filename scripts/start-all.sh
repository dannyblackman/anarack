#!/bin/bash
set -e

# Wait for Scarlett USB audio
echo 'Waiting for Scarlett...'
for i in $(seq 1 30); do
    aplay -l 2>/dev/null | grep -q Scarlett && break
    sleep 1
done

# Kill any stale JACK
pkill -9 -f jackd 2>/dev/null || true
sleep 2

# Find Scarlett card number (it may not be hw:0)
CARD=$(aplay -l 2>/dev/null | grep Scarlett | head -1 | sed 's/card \([0-9]*\).*/\1/')
if [ -z "$CARD" ]; then
    echo 'Scarlett not found!'
    exit 1
fi
echo "Scarlett is hw:$CARD"

# Start JACK in background
echo 'Starting JACK...'
JACK_NO_AUDIO_RESERVATION=1 /usr/bin/jackd -R -d alsa -d hw:$CARD -r 48000 -p 128 -n 3 &
JACK_PID=$!

# Wait for JACK to actually accept connections (not just process alive)
echo 'Waiting for JACK to accept connections...'
for i in $(seq 1 20); do
    if jack_lsp >/dev/null 2>&1; then
        echo "JACK ready after ${i}s"
        break
    fi
    kill -0 $JACK_PID 2>/dev/null || { echo 'JACK died'; exit 1; }
    sleep 1
done

# Verify JACK is really working
if ! jack_lsp >/dev/null 2>&1; then
    echo 'JACK failed to start'
    exit 1
fi

# Start Anarack services
export PYTHONUNBUFFERED=1
cd /home/pi/anarack

# Pi Agent (power control + session management) in background
venv/bin/python server/pi_agent.py \
    --synth-config synths/sequential-prophet-rev2.json \
    > /tmp/pi_agent.log 2>&1 &
echo "Pi Agent started (PID $!)"

# Wait for Pi Agent HTTP API to be ready
echo 'Waiting for Pi Agent...'
for i in $(seq 1 10); do
    if curl -s http://localhost:8803/synths/prophet-rev2/power >/dev/null 2>&1; then
        echo "Pi Agent ready after ${i}s"
        break
    fi
    sleep 1
done

# Power on the Rev2 via Shelly and wait for boot
echo 'Powering on Rev2...'
curl -s -X POST http://localhost:8803/synths/prophet-rev2/power \
    -H 'Content-Type: application/json' -d '{"state":"on"}'
echo ''

# Wait for Rev2 USB MIDI port to appear (up to 20s)
echo 'Waiting for Rev2 MIDI port...'
for i in $(seq 1 20); do
    if arecordmidi -l 2>/dev/null | grep -q 'Prophet Rev2'; then
        echo "Rev2 MIDI port found after ${i}s"
        break
    fi
    sleep 1
done

if ! arecordmidi -l 2>/dev/null | grep -q 'Prophet Rev2'; then
    echo 'WARNING: Rev2 MIDI port not found, starting midi_router anyway'
fi

# MIDI/Audio server (foreground — systemd monitors this process)
exec venv/bin/python server/midi_router.py --midi-port 'Prophet Rev2'
