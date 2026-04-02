"""
Anarack Pi Agent — registers this Pi with the Session API and manages
WireGuard peers for incoming plugin sessions.

Runs alongside midi_router.py on the Pi. Handles:
  - STUN endpoint discovery (learns our public IP:port)
  - Registration with Session API via WebSocket
  - Dynamic WireGuard peer management (add/remove plugin peers)
  - Heartbeat with current endpoint

Usage:
    python pi_agent.py [--api-host localhost] [--api-ws-port 8802]
                       [--stun-host localhost] [--stun-port 8801]
                       [--pi-id anarack-pi-01]
"""

import argparse
import asyncio
import json
import logging
import os
import socket
import subprocess

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger("pi-agent")

try:
    import websockets
except ImportError:
    log.error("websockets required: pip install websockets")
    raise


def get_wg_pubkey(interface: str = "wg0") -> str:
    """Read this Pi's WireGuard public key."""
    try:
        result = subprocess.run(
            ["sudo", "wg", "show", interface, "public-key"],
            capture_output=True, text=True, timeout=5,
        )
        return result.stdout.strip()
    except Exception as e:
        log.error(f"Failed to get WG pubkey: {e}")
        return ""


def get_wg_listen_port(interface: str = "wg0") -> int:
    """Read this Pi's WireGuard listen port."""
    try:
        result = subprocess.run(
            ["sudo", "wg", "show", interface, "listen-port"],
            capture_output=True, text=True, timeout=5,
        )
        return int(result.stdout.strip())
    except Exception as e:
        log.error(f"Failed to get WG listen port: {e}")
        return 51820


def get_local_ip() -> str:
    """Get the Pi's LAN IP."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except:
        return "192.168.1.131"


def discover_stun_endpoint(stun_host: str, stun_port: int) -> tuple[str, int]:
    """Send a STUN probe and get our public IP:port back."""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(3)
        sock.sendto(b"STUN", (stun_host, stun_port))
        data, _ = sock.recvfrom(256)
        result = json.loads(data.decode())
        sock.close()
        return result["ip"], result["port"]
    except Exception as e:
        log.warning(f"STUN discovery failed: {e}")
        return "", 0


def add_wg_peer(pubkey: str, tunnel_ip: str, interface: str = "wg0"):
    """Dynamically add a WireGuard peer for a plugin session."""
    try:
        subprocess.run(
            ["sudo", "wg", "set", interface, "peer", pubkey,
             "allowed-ips", f"{tunnel_ip}/32"],
            check=True, timeout=5,
        )
        log.info(f"Added WG peer: {pubkey[:16]}... → {tunnel_ip}")
    except Exception as e:
        log.error(f"Failed to add WG peer: {e}")


def remove_wg_peer(pubkey: str, interface: str = "wg0"):
    """Remove a WireGuard peer when session ends."""
    try:
        subprocess.run(
            ["sudo", "wg", "set", interface, "peer", pubkey, "remove"],
            check=True, timeout=5,
        )
        log.info(f"Removed WG peer: {pubkey[:16]}...")
    except Exception as e:
        log.error(f"Failed to remove WG peer: {e}")


async def run_agent(args):
    pi_id = args.pi_id
    wg_pubkey = get_wg_pubkey()
    wg_port = get_wg_listen_port()
    local_ip = get_local_ip()

    if not wg_pubkey:
        log.error("Could not read WireGuard public key. Is wg0 up?")
        return

    log.info(f"Pi agent starting: id={pi_id}, pubkey={wg_pubkey[:16]}..., local_ip={local_ip}, wg_port={wg_port}")

    # Track active sessions for peer cleanup
    active_sessions: dict[str, str] = {}  # session_id → plugin_pubkey

    while True:
        try:
            uri = f"ws://{args.api_host}:{args.api_ws_port}"
            log.info(f"Connecting to Session API: {uri}")

            async with websockets.connect(uri) as ws:
                # Discover public endpoint
                public_ip, public_port = discover_stun_endpoint(
                    args.stun_host, args.stun_port
                )
                if public_ip:
                    log.info(f"Public endpoint: {public_ip}:{public_port}")

                # Register
                await ws.send(json.dumps({
                    "type": "register",
                    "pi_id": pi_id,
                    "wg_pubkey": wg_pubkey,
                    "wg_port": wg_port,
                    "local_ip": local_ip,
                    "synths": [{"id": "rev2", "name": "Prophet Rev2", "midi_port": "Prophet Rev2"}],
                }))

                # Start heartbeat + message loop
                async def heartbeat():
                    while True:
                        await asyncio.sleep(15)
                        # Re-discover endpoint (in case public IP changes)
                        ip, port = discover_stun_endpoint(args.stun_host, args.stun_port)
                        await ws.send(json.dumps({
                            "type": "heartbeat",
                            "public_ip": ip,
                            "public_port": port,
                        }))

                async def handle_messages():
                    async for msg in ws:
                        data = json.loads(msg)
                        msg_type = data.get("type", "")

                        if msg_type == "registered":
                            log.info(f"Registered with API as: {data.get('pi_id')}")

                        elif msg_type == "new_session":
                            session_id = data["session_id"]
                            plugin_pubkey = data["plugin_pubkey"]
                            plugin_endpoint = data.get("plugin_endpoint", "")

                            log.info(f"New session {session_id}: plugin={plugin_pubkey[:16]}...")

                            # Clean up any previous sessions first
                            for old_id, old_key in list(active_sessions.items()):
                                remove_wg_peer(old_key)
                            active_sessions.clear()

                            # Add WireGuard peer for this plugin
                            # Use 10.0.0.10+ for direct sessions (10.0.0.3 is reserved for VPS relay)
                            add_wg_peer(plugin_pubkey, "10.0.0.10")
                            active_sessions[session_id] = plugin_pubkey

                            # Tell API we're ready for hole punching
                            await ws.send(json.dumps({
                                "type": "punch_ready",
                                "session_id": session_id,
                            }))

                        elif msg_type == "end_session":
                            session_id = data["session_id"]
                            pubkey = active_sessions.pop(session_id, None)
                            if pubkey:
                                remove_wg_peer(pubkey)

                await asyncio.gather(heartbeat(), handle_messages())

        except websockets.ConnectionClosed:
            log.warning("Lost connection to Session API, reconnecting in 5s...")
        except ConnectionRefusedError:
            log.warning("Session API not available, retrying in 5s...")
        except Exception as e:
            log.error(f"Agent error: {e}, reconnecting in 5s...")

        # Clean up peers on disconnect
        for session_id, pubkey in active_sessions.items():
            remove_wg_peer(pubkey)
        active_sessions.clear()

        await asyncio.sleep(5)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Anarack Pi Agent")
    parser.add_argument("--pi-id", default="anarack-pi-01", help="Unique Pi identifier")
    parser.add_argument("--api-host", default="localhost", help="Session API host")
    parser.add_argument("--api-ws-port", type=int, default=8802, help="Session API WebSocket port")
    parser.add_argument("--stun-host", default="localhost", help="STUN service host")
    parser.add_argument("--stun-port", type=int, default=8801, help="STUN service port")
    args = parser.parse_args()

    try:
        asyncio.run(run_agent(args))
    except KeyboardInterrupt:
        log.info("Pi agent shutting down")
