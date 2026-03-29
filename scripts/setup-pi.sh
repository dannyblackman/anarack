#!/usr/bin/env bash
# Anarack — Raspberry Pi 5 setup script
# Run on a fresh Raspberry Pi OS Lite (64-bit, Bookworm)
#
# Prerequisites:
#   - Pi connected to internet via ethernet
#   - SSH enabled (done via Raspberry Pi Imager)
#   - Scarlett 2i2 and Rev2 plugged in via USB
#
# Usage:
#   ssh pi@<pi-ip>
#   git clone <repo> ~/anarack
#   cd ~/anarack && bash scripts/setup-pi.sh

set -euo pipefail

echo "=== Anarack Pi Setup ==="
echo ""

# --- System packages ---
echo "[1/6] Installing system packages..."
sudo apt-get update -qq
sudo apt-get install -y -qq \
  jackd2 \
  a2jmidid \
  python3-pip \
  python3-venv \
  python3-dev \
  libasound2-dev \
  libjack-jackd2-dev \
  alsa-utils \
  ffmpeg \
  git

# --- Tailscale ---
echo "[2/6] Installing Tailscale..."
if ! command -v tailscale &>/dev/null; then
  curl -fsSL https://tailscale.com/install.sh | sh
else
  echo "  Tailscale already installed, skipping"
fi

# --- Python environment ---
echo "[3/6] Setting up Python environment..."
VENV_DIR="$HOME/anarack/venv"
python3 -m venv "$VENV_DIR"
source "$VENV_DIR/bin/activate"
pip install -q python-rtmidi websockets JACK-Client numpy

# --- Realtime audio permissions ---
echo "[4/6] Configuring realtime audio..."

# Add user to audio group
sudo usermod -aG audio "$USER"

# Set realtime limits for the audio group
sudo tee /etc/security/limits.d/audio.conf > /dev/null << 'EOF'
@audio   -  rtprio     95
@audio   -  memlock    unlimited
EOF

# --- Detect audio device ---
echo "[5/6] Detecting USB audio device..."
echo ""

# List ALSA cards to help find the Scarlett
echo "  ALSA devices found:"
aplay -l 2>/dev/null | grep "^card" | while read -r line; do
  echo "    $line"
done

# Try to find the Scarlett specifically
CARD_NUM=$(aplay -l 2>/dev/null | grep -i "scarlett\|focusrite\|USB Audio" | head -1 | grep -oP 'card \K[0-9]+' || echo "")

if [ -n "$CARD_NUM" ]; then
  echo ""
  echo "  Found USB audio on card $CARD_NUM"
  ALSA_DEVICE="hw:$CARD_NUM"
else
  echo ""
  echo "  WARNING: Could not auto-detect Scarlett."
  echo "  Make sure it's plugged in, then check 'aplay -l' for the card number."
  echo "  You'll need to set ALSA_DEVICE manually when starting JACK."
  ALSA_DEVICE="hw:USB"
fi

# --- JACK config ---
echo "[6/6] Writing JACK configuration..."

# jackdrc — JACK auto-start config
cat > "$HOME/.jackdrc" << EOF
/usr/bin/jackd -R -d alsa -d $ALSA_DEVICE -r 48000 -p 256 -n 3
EOF

# Also write a startup script
cat > "$HOME/anarack/start.sh" << 'SCRIPT'
#!/usr/bin/env bash
# Start the Anarack server stack
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"

# Kill any stale processes from a previous run
echo "Cleaning up stale processes..."
sudo fuser -k 5555/udp 8765/tcp 8766/tcp 2>/dev/null || true
killall jackd a2jmidid 2>/dev/null || true
sleep 1

# Auto-detect USB audio device (Scarlett)
ALSA_DEVICE=$(aplay -l 2>/dev/null | grep -i "scarlett\|focusrite" | head -1 | sed 's/card \([0-9]*\).*/hw:\1/' || echo "")
if [ -z "$ALSA_DEVICE" ]; then
  echo "WARNING: Could not detect Scarlett. Falling back to hw:0"
  ALSA_DEVICE="hw:0"
fi
echo "Using audio device: $ALSA_DEVICE"

echo "Starting JACK..."
jackd -R -d alsa -d "$ALSA_DEVICE" -r 48000 -p 256 -n 3 &
JACK_PID=$!
sleep 2

# Bridge ALSA MIDI to JACK (makes USB MIDI devices visible to JACK)
echo "Starting ALSA-JACK MIDI bridge..."
a2jmidid -e &
A2J_PID=$!
sleep 1

# Start the Anarack server (MIDI routing + audio streaming)
echo "Starting Anarack server..."
source "$DIR/venv/bin/activate"
python "$DIR/server/midi_router.py" "$@" &
SERVER_PID=$!

PI_IP=$(hostname -I | awk '{print $1}')
TS_IP=$(tailscale ip -4 2>/dev/null || echo "not configured")

echo ""
echo "=== Anarack running ==="
echo "  JACK PID:     $JACK_PID"
echo "  a2jmidid PID: $A2J_PID"
echo "  Server PID:   $SERVER_PID"
echo ""
echo "  LAN:       http://${PI_IP}:8765"
echo "  Tailscale: http://${TS_IP}:8765"
echo ""
echo "  Open the browser client and enter the server address to connect."
echo "  Audio streams automatically — no ffplay needed."
echo ""
echo "Press Ctrl+C to stop all services."

cleanup() {
  echo "Shutting down..."
  kill $SERVER_PID $A2J_PID $JACK_PID 2>/dev/null
  wait 2>/dev/null
  echo "Done."
}
trap cleanup INT TERM

wait
SCRIPT
chmod +x "$HOME/anarack/start.sh"

# --- Systemd service (auto-start on boot) ---
echo "[7/7] Creating systemd service for auto-start..."

sudo tee /etc/systemd/system/anarack.service > /dev/null << SVCEOF
[Unit]
Description=Anarack Remote Synth Server
After=network-online.target sound.target
Wants=network-online.target

[Service]
Type=simple
User=$USER
WorkingDirectory=$HOME/anarack
ExecStart=$HOME/anarack/start.sh --midi-port "Prophet Rev2"
Restart=on-failure
RestartSec=5

# Audio needs these
Environment=JACK_NO_AUDIO_RESERVATION=1

[Install]
WantedBy=multi-user.target
SVCEOF

sudo systemctl daemon-reload
sudo systemctl enable anarack.service

echo ""
echo "=== Setup complete ==="
echo ""
echo "Anarack will auto-start on boot."
echo ""
echo "Next steps:"
echo "  1. Reboot (required for audio group + realtime limits)"
echo "     sudo reboot"
echo ""
echo "  2. After reboot, authenticate Tailscale (one-time only):"
echo "     sudo tailscale up"
echo ""
echo "  3. That's it! Anarack starts automatically."
echo "     Open the browser client and connect to this Pi's IP."
echo ""
echo "Manual commands (if needed):"
echo "  sudo systemctl status anarack    # Check if running"
echo "  sudo systemctl restart anarack   # Restart"
echo "  sudo journalctl -u anarack -f    # View logs"
echo "  ~/anarack/start.sh --list-ports  # List MIDI ports"
