#!/bin/bash
# Anarack Pi setup — run this on a fresh Pi to configure everything
# Usage: scp -r scripts/pi-setup pi@anarack.local:~ && ssh pi@anarack.local "bash ~/pi-setup/setup.sh"

set -e
echo "=== Anarack Pi Setup ==="

# Install system deps
sudo apt-get update
sudo apt-get install -y jackd2 python3-venv python3-pip watchdog wireguard

# WireGuard
sudo cp ~/pi-setup/wg0.conf /etc/wireguard/wg0.conf
sudo chmod 600 /etc/wireguard/wg0.conf
sudo systemctl enable wg-quick@wg0
sudo systemctl start wg-quick@wg0

# Watchdog
echo 'dtparam=watchdog=on' | sudo tee -a /boot/firmware/config.txt > /dev/null
sudo cp ~/pi-setup/watchdog.conf /etc/watchdog.conf
sudo systemctl enable watchdog

# Journal limit
sudo sed -i 's/#SystemMaxUse=/SystemMaxUse=50M/' /etc/systemd/journald.conf
sudo systemctl restart systemd-journald

# Anarack service
sudo cp ~/pi-setup/anarack.service /etc/systemd/system/anarack.service
sudo systemctl daemon-reload
sudo systemctl enable anarack

# Python venv
cd /home/pi/anarack
python3 -m venv venv
venv/bin/pip install -r server/requirements.txt

echo "=== Setup complete. Reboot to start everything. ==="
echo "sudo reboot"
