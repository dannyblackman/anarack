#!/usr/bin/env bash
# Airsynth — Raspberry Pi 5 setup script
# Run on a fresh Raspberry Pi OS Lite (64-bit, Bookworm)
#
# Prerequisites:
#   - Pi connected to internet via ethernet
#   - SSH enabled (done via Raspberry Pi Imager)
#   - Scarlett 2i2 and Rev2 plugged in via USB
#
# Usage:
#   ssh pi@<pi-ip>
#   git clone <repo> ~/airsynth
#   cd ~/airsynth && bash scripts/setup-pi.sh

set -euo pipefail

echo "=== Airsynth Pi Setup ==="
echo ""

# --- System packages ---
echo "[1/6] Installing system packages..."
sudo apt-get update -qq
sudo apt-get install -y -qq \
  jackd2 \
  jack-tools \
  a2jmidid \
  python3-pip \
  python3-venv \
  python3-dev \
  libasound2-dev \
  libjack-jackd2-dev \
  alsa-utils \
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
VENV_DIR="$HOME/airsynth/venv"
python3 -m venv "$VENV_DIR"
source "$VENV_DIR/bin/activate"
pip install -q python-rtmidi websockets

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
/usr/bin/jackd -R -d alsa -d $ALSA_DEVICE -r 48000 -p 64 -n 3
EOF

# Also write a startup script for clarity
cat > "$HOME/airsynth/start.sh" << 'SCRIPT'
#!/usr/bin/env bash
# Start the Airsynth server stack
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"

echo "Starting JACK..."
jackd -R -d alsa -d hw:USB -r 48000 -p 64 -n 3 &
JACK_PID=$!
sleep 2

# Bridge ALSA MIDI to JACK (makes USB MIDI devices visible to JACK)
echo "Starting ALSA-JACK MIDI bridge..."
a2jmidid -e &
A2J_PID=$!
sleep 1

echo "Starting MIDI router..."
source "$DIR/venv/bin/activate"
python "$DIR/server/midi_router.py" "$@" &
ROUTER_PID=$!

echo ""
echo "=== Airsynth running ==="
echo "  JACK PID:     $JACK_PID"
echo "  a2jmidid PID: $A2J_PID"
echo "  Router PID:   $ROUTER_PID"
echo ""
echo "  WebSocket MIDI: ws://$(hostname -I | awk '{print $1}'):8765"
echo "  UDP MIDI:       $(hostname -I | awk '{print $1}'):5555"
echo ""
echo "Press Ctrl+C to stop all services."

cleanup() {
  echo "Shutting down..."
  kill $ROUTER_PID $A2J_PID $JACK_PID 2>/dev/null
  wait 2>/dev/null
  echo "Done."
}
trap cleanup INT TERM

wait
SCRIPT
chmod +x "$HOME/airsynth/start.sh"

echo ""
echo "=== Setup complete ==="
echo ""
echo "Next steps:"
echo "  1. Reboot (required for audio group + realtime limits)"
echo "     sudo reboot"
echo ""
echo "  2. After reboot, authenticate Tailscale:"
echo "     sudo tailscale up"
echo ""
echo "  3. Plug in Scarlett + Rev2, then start everything:"
echo "     cd ~/airsynth && ./start.sh"
echo ""
echo "  4. Check MIDI ports:"
echo "     source ~/airsynth/venv/bin/activate"
echo "     python ~/airsynth/server/midi_router.py --list-ports"
