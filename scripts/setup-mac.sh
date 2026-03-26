#!/usr/bin/env bash
# Airsynth — Mac client setup
# Installs what you need on your Mac to receive audio from the Pi
#
# Prerequisites: Homebrew installed

set -euo pipefail

echo "=== Airsynth Mac Setup ==="

# Check for Homebrew
if ! command -v brew &>/dev/null; then
  echo "ERROR: Homebrew not found. Install it first: https://brew.sh"
  exit 1
fi

echo "[1/3] Installing JACK and Tailscale..."
brew install jack tailscale

echo "[2/3] Installing Python dependencies..."
python3 -m venv venv
source venv/bin/activate
pip install -q python-rtmidi websockets

echo "[3/3] Done."
echo ""
echo "Next steps:"
echo "  1. Start Tailscale: open the Tailscale app or run 'tailscale up'"
echo "  2. Start JACK: jackd -d coreaudio -r 48000 -p 128"
echo "  3. Open client/index.html in Chrome"
echo "  4. Set the server address to your Pi's Tailscale IP:8765"
