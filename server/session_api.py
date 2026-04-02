"""
Anarack Session API — coordinates P2P connections between plugins and Pis.

Handles:
  - Pi registration (WebSocket heartbeat with public endpoint)
  - Session creation (plugin requests a synth, gets Pi endpoint + pubkey)
  - STUN endpoint discovery (UDP echo of source IP:port)
  - Hole punch coordination (tells both sides to start probing)

For testing: runs on the Pi alongside midi_router.py.
For production: runs on a VPS/cloud with edge relays.

Usage:
    python session_api.py [--port 8800] [--stun-port 8801]
"""

import argparse
import asyncio
import json
import logging
import os
import secrets
import socket
import struct
import subprocess
import time
from dataclasses import dataclass, field, asdict
from typing import Optional

try:
    import websockets
except ImportError:
    websockets = None

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger("session-api")


# ─── Data Models ───────────────────────────────────────────────────────────────

@dataclass
class SynthInfo:
    """A synth available on this Pi."""
    id: str
    name: str
    midi_port: str  # rtmidi port name


@dataclass
class PiRegistration:
    """A Pi that has registered with the session API."""
    pi_id: str
    public_ip: str = ""
    public_port: int = 0
    local_ip: str = ""
    wg_pubkey: str = ""
    wg_port: int = 51820
    synths: list = field(default_factory=list)
    last_heartbeat: float = 0.0
    ws: object = None  # WebSocket connection


@dataclass
class Session:
    """An active session between a plugin and a Pi."""
    session_id: str
    pi_id: str
    plugin_pubkey: str
    plugin_endpoint: str = ""  # plugin's public IP:port (from STUN)
    pi_endpoint: str = ""      # Pi's public IP:port
    pi_pubkey: str = ""
    state: str = "pending"     # pending → punching → connected → ended
    created_at: float = 0.0
    relay_endpoint: str = ""   # fallback relay


# ─── Session Manager ───────────────────────────────────────────────────────────

class SessionManager:
    def __init__(self, relay_endpoint: str = ""):
        self.pis: dict[str, PiRegistration] = {}
        self.sessions: dict[str, Session] = {}
        self.relay_endpoint = relay_endpoint

    def register_pi(self, pi_id: str, wg_pubkey: str, synths: list,
                    local_ip: str = "", wg_port: int = 51820) -> PiRegistration:
        if pi_id not in self.pis:
            self.pis[pi_id] = PiRegistration(pi_id=pi_id)
        pi = self.pis[pi_id]
        pi.wg_pubkey = wg_pubkey
        pi.wg_port = wg_port
        pi.synths = synths
        pi.local_ip = local_ip
        pi.last_heartbeat = time.time()
        log.info(f"Pi registered: {pi_id} ({len(synths)} synths)")
        return pi

    def update_pi_endpoint(self, pi_id: str, public_ip: str, public_port: int):
        if pi_id in self.pis:
            pi = self.pis[pi_id]
            pi.public_ip = public_ip
            pi.public_port = public_port
            pi.last_heartbeat = time.time()

    def create_session(self, pi_id: str, plugin_pubkey: str,
                       plugin_endpoint: str = "") -> Optional[Session]:
        if pi_id not in self.pis:
            return None
        pi = self.pis[pi_id]

        session_id = secrets.token_hex(8)
        session = Session(
            session_id=session_id,
            pi_id=pi_id,
            plugin_pubkey=plugin_pubkey,
            plugin_endpoint=plugin_endpoint,
            pi_endpoint=f"{pi.public_ip}:{pi.public_port}" if pi.public_ip else "",
            pi_pubkey=pi.wg_pubkey,
            state="pending",
            created_at=time.time(),
            relay_endpoint=self.relay_endpoint,
        )
        self.sessions[session_id] = session
        log.info(f"Session created: {session_id} (pi={pi_id})")
        return session

    def end_session(self, session_id: str):
        if session_id in self.sessions:
            self.sessions[session_id].state = "ended"
            log.info(f"Session ended: {session_id}")

    def get_available_pis(self) -> list[dict]:
        now = time.time()
        return [
            {
                "pi_id": pi.pi_id,
                "synths": pi.synths,
                "online": (now - pi.last_heartbeat) < 60,
            }
            for pi in self.pis.values()
        ]


# ─── STUN Service ──────────────────────────────────────────────────────────────

class StunService:
    """Minimal STUN-like UDP echo — tells callers their public IP:port."""

    def __init__(self, port: int = 8801):
        self.port = port
        self.sock: Optional[socket.socket] = None

    async def run(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(("0.0.0.0", self.port))
        self.sock.setblocking(False)
        log.info(f"STUN service listening on UDP :{self.port}")

        loop = asyncio.get_event_loop()
        while True:
            try:
                data, addr = await loop.run_in_executor(None, self._recv)
                if data:
                    response = json.dumps({"ip": addr[0], "port": addr[1]}).encode()
                    self.sock.sendto(response, addr)
            except Exception as e:
                log.error(f"STUN error: {e}")
                await asyncio.sleep(0.1)

    def _recv(self):
        """Blocking recv — runs in executor."""
        import select
        ready, _, _ = select.select([self.sock], [], [], 1.0)
        if ready:
            return self.sock.recvfrom(64)
        return None, None


# ─── HTTP API ──────────────────────────────────────────────────────────────────

class HttpApi:
    """Simple HTTP API for session management."""

    def __init__(self, manager: SessionManager, port: int = 8800):
        self.manager = manager
        self.port = port

    async def run(self):
        server = await asyncio.start_server(self._handle, "0.0.0.0", self.port)
        log.info(f"HTTP API listening on :{self.port}")
        async with server:
            await server.serve_forever()

    async def _handle(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        try:
            request_line = await asyncio.wait_for(reader.readline(), timeout=5)
            if not request_line:
                writer.close()
                return

            line = request_line.decode().strip()
            parts = line.split()
            if len(parts) < 2:
                writer.close()
                return

            method, path = parts[0], parts[1]

            # Read headers
            content_length = 0
            while True:
                header = await asyncio.wait_for(reader.readline(), timeout=5)
                if header in (b"\r\n", b"\n", b""):
                    break
                if header.lower().startswith(b"content-length:"):
                    content_length = int(header.split(b":")[1].strip())

            # Read body
            body = b""
            if content_length > 0:
                body = await asyncio.wait_for(reader.readexactly(content_length), timeout=5)

            # Route
            status, response = self._route(method, path, body)
            resp_body = json.dumps(response).encode()

            writer.write(f"HTTP/1.1 {status}\r\n".encode())
            writer.write(b"Content-Type: application/json\r\n")
            writer.write(b"Access-Control-Allow-Origin: *\r\n")
            writer.write(f"Content-Length: {len(resp_body)}\r\n".encode())
            writer.write(b"\r\n")
            writer.write(resp_body)
            await writer.drain()
        except Exception as e:
            log.error(f"HTTP error: {e}")
        finally:
            writer.close()

    def _route(self, method: str, path: str, body: bytes) -> tuple:
        """Route HTTP requests. Returns (status_line, response_dict)."""

        # GET /pis — list available Pis
        if method == "GET" and path == "/pis":
            return "200 OK", {"pis": self.manager.get_available_pis()}

        # POST /sessions — create a new session
        if method == "POST" and path == "/sessions":
            try:
                data = json.loads(body) if body else {}
            except json.JSONDecodeError:
                return "400 Bad Request", {"error": "invalid JSON"}

            pi_id = data.get("pi_id", "")
            plugin_pubkey = data.get("plugin_pubkey", "")
            plugin_endpoint = data.get("plugin_endpoint", "")

            if not pi_id or not plugin_pubkey:
                return "400 Bad Request", {"error": "pi_id and plugin_pubkey required"}

            session = self.manager.create_session(pi_id, plugin_pubkey, plugin_endpoint)
            if not session:
                return "404 Not Found", {"error": f"Pi '{pi_id}' not registered"}

            # Notify Pi to add this peer and start hole punching
            pi = self.manager.pis.get(pi_id)
            if pi and pi.ws:
                asyncio.ensure_future(self._notify_pi(pi, session))

            return "200 OK", {
                "session_id": session.session_id,
                "pi_endpoint": session.pi_endpoint,
                "pi_pubkey": session.pi_pubkey,
                "pi_local_ip": pi.local_ip if pi else "",
                "pi_wg_port": pi.wg_port if pi else 51820,
                "relay_endpoint": session.relay_endpoint,
                "state": session.state,
            }

        # DELETE /sessions/{id} — end a session
        if method == "DELETE" and path.startswith("/sessions/"):
            session_id = path.split("/")[-1]
            self.manager.end_session(session_id)
            return "200 OK", {"status": "ended"}

        # GET /sessions/{id} — get session info
        if method == "GET" and path.startswith("/sessions/"):
            session_id = path.split("/")[-1]
            session = self.manager.sessions.get(session_id)
            if not session:
                return "404 Not Found", {"error": "session not found"}
            return "200 OK", asdict(session)

        return "404 Not Found", {"error": "not found"}

    async def _notify_pi(self, pi: PiRegistration, session: Session):
        """Tell the Pi about a new session so it can add the WG peer."""
        if pi.ws:
            try:
                await pi.ws.send(json.dumps({
                    "type": "new_session",
                    "session_id": session.session_id,
                    "plugin_pubkey": session.plugin_pubkey,
                    "plugin_endpoint": session.plugin_endpoint,
                }))
            except Exception as e:
                log.error(f"Failed to notify Pi: {e}")


# ─── Pi WebSocket Handler ─────────────────────────────────────────────────────

class PiWebSocketHandler:
    """Handles WebSocket connections from Pis."""

    def __init__(self, manager: SessionManager, port: int = 8802):
        self.manager = manager
        self.port = port

    async def run(self):
        if websockets is None:
            log.warning("websockets not installed — Pi registration disabled")
            return
        async with websockets.serve(self._handle, "0.0.0.0", self.port):
            log.info(f"Pi WebSocket listening on :{self.port}")
            await asyncio.Future()  # run forever

    async def _handle(self, ws):
        pi_id = None
        try:
            async for msg in ws:
                data = json.loads(msg)
                msg_type = data.get("type", "")

                if msg_type == "register":
                    pi_id = data.get("pi_id", "")
                    pi = self.manager.register_pi(
                        pi_id=pi_id,
                        wg_pubkey=data.get("wg_pubkey", ""),
                        synths=data.get("synths", []),
                        local_ip=data.get("local_ip", ""),
                        wg_port=data.get("wg_port", 51820),
                    )
                    pi.ws = ws
                    await ws.send(json.dumps({"type": "registered", "pi_id": pi_id}))

                elif msg_type == "heartbeat":
                    if pi_id and pi_id in self.manager.pis:
                        pi = self.manager.pis[pi_id]
                        pi.last_heartbeat = time.time()
                        # Update public endpoint if provided
                        if "public_ip" in data:
                            pi.public_ip = data["public_ip"]
                        if "public_port" in data:
                            pi.public_port = data["public_port"]

                elif msg_type == "punch_ready":
                    # Pi reports it's ready to receive hole punch probes
                    session_id = data.get("session_id", "")
                    if session_id in self.manager.sessions:
                        self.manager.sessions[session_id].state = "punching"

        except websockets.ConnectionClosed:
            pass
        finally:
            if pi_id and pi_id in self.manager.pis:
                self.manager.pis[pi_id].ws = None
                log.info(f"Pi disconnected: {pi_id}")


# ─── Main ──────────────────────────────────────────────────────────────────────

async def main(args):
    manager = SessionManager(relay_endpoint=args.relay)

    tasks = [
        asyncio.create_task(HttpApi(manager, args.port).run()),
        asyncio.create_task(StunService(args.stun_port).run()),
    ]

    if websockets is not None:
        tasks.append(asyncio.create_task(PiWebSocketHandler(manager, args.ws_port).run()))

    log.info(f"Session API starting (http=:{args.port}, stun=:{args.stun_port}, ws=:{args.ws_port})")
    await asyncio.gather(*tasks)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Anarack Session API")
    parser.add_argument("--port", type=int, default=8800, help="HTTP API port")
    parser.add_argument("--stun-port", type=int, default=8801, help="STUN UDP port")
    parser.add_argument("--ws-port", type=int, default=8802, help="Pi WebSocket port")
    parser.add_argument("--relay", default="66.245.195.65:51820",
                        help="Fallback relay endpoint")
    args = parser.parse_args()

    try:
        asyncio.run(main(args))
    except KeyboardInterrupt:
        log.info("Shutting down")
