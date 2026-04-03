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

# Start Anarack server
export PYTHONUNBUFFERED=1
cd /home/pi/anarack
exec venv/bin/python server/midi_router.py --midi-port 'Prophet Rev2'
