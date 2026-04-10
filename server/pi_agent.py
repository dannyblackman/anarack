"""
Anarack Pi Agent — registers this Pi with the Session API and manages
WireGuard peers for incoming plugin sessions.

Runs alongside midi_router.py on the Pi. Handles:
  - STUN endpoint discovery (learns our public IP:port)
  - Registration with Session API via WebSocket
  - Dynamic WireGuard peer management (add/remove plugin peers)
  - Heartbeat with current endpoint
  - Power management for synths via Shelly smart plugs (HTTP on :8803)

Usage:
    python pi_agent.py [--api-host localhost] [--api-ws-port 8802]
                       [--stun-host localhost] [--stun-port 8801]
                       [--pi-id anarack-pi-01] [--synth-config ../synths/sequential-prophet-rev2.json]
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

try:
    from aiohttp import web
except ImportError:
    log.error("aiohttp required: pip install aiohttp")
    raise

from power_manager import PowerManager


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


def load_synth_power_config(config_path: str) -> tuple[str, dict] | None:
    """Load synth config and extract power block."""
    try:
        with open(config_path) as f:
            synth = json.load(f)
        if "power" in synth:
            return synth["id"], synth["power"]
        else:
            log.info(f"No power config in {config_path}")
            return None
    except Exception as e:
        log.error(f"Failed to load synth config {config_path}: {e}")
        return None


async def run_agent(args):
    pi_id = args.pi_id
    wg_pubkey = get_wg_pubkey()
    wg_port = get_wg_listen_port()
    local_ip = get_local_ip()

    if not wg_pubkey:
        log.error("Could not read WireGuard public key. Is wg0 up?")
        return

    log.info(f"Pi agent starting: id={pi_id}, pubkey={wg_pubkey[:16]}..., local_ip={local_ip}, wg_port={wg_port}")

    # --- Power Manager ---
    power_mgr = PowerManager()
    if args.synth_config:
        result = load_synth_power_config(args.synth_config)
        if result:
            synth_id, power_config = result
            power_mgr.register_synth(synth_id, power_config)

    # --- HTTP API for power control (port 8803) ---
    async def handle_power_post(request):
        synth_id = request.match_info["synth_id"]
        try:
            body = await request.json()
        except Exception:
            body = {}
        state = body.get("state", "on")
        if state == "on":
            result = await power_mgr.power_on(synth_id)
        elif state == "off":
            result = await power_mgr.power_off(synth_id)
        else:
            result = {"ok": False, "error": f"Invalid state: {state}"}
        status_code = 200 if result.get("ok") else 400
        return web.json_response(result, status=status_code)

    async def handle_power_get(request):
        synth_id = request.match_info["synth_id"]
        result = await power_mgr.status(synth_id)
        status_code = 200 if result.get("ok") else 404
        return web.json_response(result, status=status_code)

    app = web.Application()
    app.router.add_post("/synths/{synth_id}/power", handle_power_post)
    app.router.add_get("/synths/{synth_id}/power", handle_power_get)

    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, "0.0.0.0", args.http_port)
    await site.start()
    log.info(f"Power control HTTP API listening on :{args.http_port}")

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
                            synth_id = data.get("synth_id", "prophet-rev2")

                            log.info(f"New session {session_id}: plugin={plugin_pubkey[:16]}...")

                            # Power on synth if needed (waits for boot)
                            if synth_id in power_mgr.synths:
                                power_result = await power_mgr.session_started(synth_id)
                                log.info(f"Power state for {synth_id}: {power_result}")

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
                            synth_id = data.get("synth_id", "prophet-rev2")
                            pubkey = active_sessions.pop(session_id, None)
                            if pubkey:
                                remove_wg_peer(pubkey)
                            # Start idle timer — will power off if no new session
                            if synth_id in power_mgr.synths:
                                await power_mgr.session_ended(synth_id)

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
    parser.add_argument("--http-port", type=int, default=8803, help="Power control HTTP port")
    parser.add_argument("--synth-config", default=None, help="Path to synth JSON config with power block")
    args = parser.parse_args()

    try:
        asyncio.run(run_agent(args))
    except KeyboardInterrupt:
        log.info("Pi agent shutting down")
