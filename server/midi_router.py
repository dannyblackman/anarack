"""
Anarack Server — Phase 0 Prototype

MIDI routing + audio streaming in one process.
- Receives MIDI over WebSocket from browser, forwards to hardware synth via rtmidi
- Captures audio from JACK (Scarlett input) and streams to browser via WebSocket
- Also accepts MIDI over UDP for future plugin use

Usage:
    python midi_router.py [--ws-port 8765] [--midi-port "Prophet Rev2"]
"""

import argparse
import asyncio
import json
import signal
import struct
import threading
import time
import queue

import rtmidi
import websockets

from synth_manager import SynthManager, PresetManager

# Try to import JACK — audio streaming is optional
try:
    import jack
    HAS_JACK = True
except ImportError:
    HAS_JACK = False
    print("WARNING: python-jack-client not installed. Audio streaming disabled.")
    print("  Install with: pip install JACK-Client")


def list_midi_ports():
    """List available MIDI output ports."""
    midi_out = rtmidi.MidiOut()
    ports = midi_out.get_ports()
    if not ports:
        print("No MIDI output ports found.")
    else:
        print("Available MIDI output ports:")
        for i, port in enumerate(ports):
            print(f"  [{i}] {port}")
    midi_out.delete()
    return ports


def open_midi_input(port_name: str | None) -> rtmidi.MidiIn:
    """Open a MIDI input port by name (substring match) or index."""
    midi_in = rtmidi.MidiIn()
    ports = midi_in.get_ports()

    if not ports:
        raise RuntimeError("No MIDI input ports available")

    if port_name is None:
        midi_in.open_port(0)
        print(f"Opened MIDI input port: {ports[0]}")
        return midi_in

    # Try numeric index first
    try:
        idx = int(port_name)
        if 0 <= idx < len(ports):
            midi_in.open_port(idx)
            print(f"Opened MIDI input port: {ports[idx]}")
            return midi_in
    except ValueError:
        pass

    # Substring match
    for i, port in enumerate(ports):
        if port_name.lower() in port.lower():
            midi_in.open_port(i)
            print(f"Opened MIDI input port: {port}")
            return midi_in

    raise RuntimeError(f"MIDI input port matching '{port_name}' not found. Available: {ports}")


def open_midi_port(port_name: str | None) -> rtmidi.MidiOut:
    """Open a MIDI output port by name (substring match) or index."""
    midi_out = rtmidi.MidiOut()
    ports = midi_out.get_ports()

    if not ports:
        raise RuntimeError("No MIDI output ports available")

    if port_name is None:
        midi_out.open_port(0)
        print(f"Opened MIDI port: {ports[0]}")
        return midi_out

    # Try numeric index first
    try:
        idx = int(port_name)
        if 0 <= idx < len(ports):
            midi_out.open_port(idx)
            print(f"Opened MIDI port: {ports[idx]}")
            return midi_out
    except ValueError:
        pass

    # Substring match
    for i, port in enumerate(ports):
        if port_name.lower() in port.lower():
            midi_out.open_port(i)
            print(f"Opened MIDI port: {port}")
            return midi_out

    raise RuntimeError(f"MIDI port matching '{port_name}' not found. Available: {ports}")


class MidiRouter:
    ECHO_SUPPRESS_MS = 50  # Suppress echoed CCs within this window

    def __init__(self, midi_out: rtmidi.MidiOut, synth_manager: SynthManager = None):
        self.midi_out = midi_out
        self.synth_manager = synth_manager
        self.message_count = 0
        self.midi_ws_clients: set = set()  # MIDI WebSocket clients for sending CC updates
        self._last_sent: dict[int, float] = {}  # {cc: timestamp} for echo suppression
        self._loop: asyncio.AbstractEventLoop | None = None
        self._udp_transport = None  # set by udp_server after startup
        self._udp_clients: set = set()  # {(ip, port)} for CC broadcast
        import socket as _socket
        self._cc_sock = _socket.socket(_socket.AF_INET, _socket.SOCK_DGRAM)

        # Build SysEx mapping from active synth definition
        self._sysex_offset_to_cc = {}
        self._sysex_config = {}
        self._load_synth_config()

    def _load_synth_config(self):
        """Load SysEx config from the active synth definition."""
        if not self.synth_manager or not self.synth_manager.active_synth:
            return
        defn = self.synth_manager.active_synth
        self._sysex_offset_to_cc = defn.get_sysex_offset_to_cc_map()
        self._sysex_config = defn.sysex
        if self._sysex_offset_to_cc:
            print(f"Loaded {len(self._sysex_offset_to_cc)} SysEx→CC mappings from {defn.name}")

    def send(self, message: list[int]):
        """Send a MIDI message to the hardware synth."""
        self.midi_out.send_message(message)
        self.message_count += 1
        # Track sent CCs for echo suppression
        if len(message) >= 3 and (message[0] & 0xF0) == 0xB0:
            self._last_sent[message[1]] = time.monotonic()

    def send_from_udp(self, data: bytes):
        """Parse and forward a UDP MIDI packet."""
        if len(data) < 2:
            return
        if data[0] == 0xFE:
            return  # registration packet, not MIDI
        msg = list(data[:3] if len(data) >= 3 else data)
        status = msg[0] & 0xF0
        if status in (0xC0, 0xD0):
            self.send(msg[:2])
            # On program change, request edit buffer so UI knobs update
            if status == 0xC0 and self._loop:
                self._loop.call_soon_threadsafe(
                    self._loop.call_later, 0.1, self.request_edit_buffer
                )
        else:
            self.send(msg)

    def send_from_websocket(self, payload: str):
        """Parse and forward a WebSocket MIDI message."""
        msg = json.loads(payload)
        status = msg["status"]
        data1 = msg.get("data1", 0)
        data2 = msg.get("data2", 0)
        # Program Change (0xC0) and Channel Pressure (0xD0) are 2-byte messages
        if (status & 0xF0) in (0xC0, 0xD0):
            self.send([status, data1])
            # On program change, request edit buffer so UI knobs update
            if (status & 0xF0) == 0xC0 and self._loop:
                self._loop.call_soon_threadsafe(
                    self._loop.call_later, 0.1, self.request_edit_buffer
                )
        else:
            self.send([status, data1, data2])

    def request_edit_buffer(self):
        """Send SysEx request for the synth's current edit buffer."""
        req = self._sysex_config.get("edit_buffer_request",
                                     [0xF0, 0x01, 0x2F, 0x06, 0xF7])
        self.midi_out.send_message(req)

    def _unpack_sysex(self, packed: list[int]) -> list[int]:
        """Unpack DSI 7-bit encoded SysEx data into raw bytes."""
        raw = []
        i = 0
        while i + 7 < len(packed):
            ms_bits = packed[i]
            for x in range(7):
                if i + 1 + x < len(packed):
                    raw.append(packed[i + 1 + x] | ((ms_bits >> x) & 1) << 7)
            i += 8
        return raw

    def _handle_sysex(self, message: list[int]):
        """Parse a SysEx edit buffer dump and broadcast CC values."""
        if len(message) < 6:
            return

        mfr_id = self._sysex_config.get("manufacturer_id", [0x01])
        dev_id = self._sysex_config.get("device_id", 0x2F)
        resp_cmd = self._sysex_config.get("edit_buffer_response_cmd", 0x03)

        # Validate header
        print(f"SysEx: len={len(message)} hdr=[{message[1]:02X},{message[2]:02X},{message[3]:02X}] expect=[{mfr_id[0]:02X},{dev_id:02X},{resp_cmd:02X}]")
        if message[1] != mfr_id[0] or message[2] != dev_id:
            print(f"SysEx: manufacturer/device mismatch, skipping")
            return
        if message[3] != resp_cmd:
            print(f"SysEx: command mismatch ({message[3]:02X} != {resp_cmd:02X}), skipping")
            return

        # Unpack the data (skip F0, header bytes, and trailing F7)
        packed = message[4:-1]
        raw = self._unpack_sysex(packed)
        print(f"SysEx unpacked: {len(raw)} bytes, {len(self._sysex_offset_to_cc)} mappings")

        # Extract patch name (Rev2: 20 ASCII chars at offset 235-254)
        if len(raw) >= 255:
            name_bytes = raw[235:255]
            patch_name = ''.join(chr(b) for b in name_bytes if 32 <= b < 127).strip()
            self._broadcast_patch_name(patch_name)

        # Broadcast each mapped parameter as a CC update (scaled to 0-127)
        for offset, (cc, native_max) in self._sysex_offset_to_cc.items():
            if offset < len(raw):
                if native_max <= 127:
                    value = min(127, raw[offset])
                else:
                    value = round(raw[offset] * 127 / native_max)
                self._broadcast_cc(cc, value)

    def _is_echo(self, cc: int) -> bool:
        """Check if a received CC is likely an echo of one we just sent."""
        sent_time = self._last_sent.get(cc)
        if sent_time is None:
            return False
        elapsed_ms = (time.monotonic() - sent_time) * 1000
        return elapsed_ms < self.ECHO_SUPPRESS_MS

    def on_synth_message(self, event, data=None):
        """Callback invoked by rtmidi when the Rev2 sends a MIDI message."""
        message, _ = event
        if not message:
            return
        status = message[0] & 0xF0

        # SysEx message
        if message[0] == 0xF0:
            self._handle_sysex(message)
            return

        # Program Change — request edit buffer to get all parameter values
        if (message[0] & 0xF0) == 0xC0:
            if self._loop:
                self._loop.call_soon_threadsafe(
                    self._loop.call_later, 0.05, self.request_edit_buffer
                )
            return

        if len(message) < 3:
            print(f"Short message: {message}")
            return
        status = message[0] & 0xF0
        if status == 0xB0:
            cc, value = message[1], message[2]
            if not self._is_echo(cc):
                self._broadcast_cc(cc, value)

    def _broadcast_patch_name(self, name: str):
        """Send a patch name update to all connected clients."""
        msg = json.dumps({"type": "patchName", "name": name})
        # WebSocket clients (browser)
        if self.midi_ws_clients and self._loop:
            for ws in self.midi_ws_clients:
                try:
                    asyncio.run_coroutine_threadsafe(ws.send(msg), self._loop)
                except Exception:
                    pass
        # UDP plugin clients
        if self._udp_clients and self._cc_sock:
            encoded = msg.encode()
            for addr in list(self._udp_clients):
                try:
                    self._cc_sock.sendto(encoded, addr)
                except Exception:
                    pass

    def _broadcast_cc(self, cc: int, value: int):
        """Send a CC update to all connected clients (WebSocket + UDP plugin)."""
        msg = json.dumps({"type": "cc", "cc": cc, "value": value})
        # WebSocket clients (browser)
        if self.midi_ws_clients and self._loop:
            dead = set()
            for ws in self.midi_ws_clients:
                try:
                    asyncio.run_coroutine_threadsafe(ws.send(msg), self._loop)
                except Exception:
                    dead.add(ws)
            self.midi_ws_clients -= dead
        # UDP plugin clients
        if self._udp_clients and self._cc_sock:
            encoded = msg.encode()
            for addr in list(self._udp_clients):
                try:
                    self._cc_sock.sendto(encoded, addr)
                except Exception:
                    pass


class AudioStreamer:
    """Captures audio from JACK and makes it available to WebSocket and UDP clients."""

    # Xrun watchdog: restart JACK if xruns accumulate
    XRUN_THRESHOLD = 5       # xruns in the window = restart
    XRUN_WINDOW_SECS = 60    # rolling window

    def __init__(self, capture_port: str = "system:capture_1"):
        self.capture_port = capture_port
        self.clients: set = set()  # WebSocket clients
        self.udp_clients: dict = {}  # {addr: socket} for UDP audio clients
        self.udp_socket = None  # Shared UDP socket for sending audio
        self.jack_client = None
        self.audio_queue: queue.Queue = queue.Queue(maxsize=200)
        self._running = False
        self._seq = 0
        self._ts = 0
        self._blocksize = 128
        # Packet duplication: ring of recent packets, resent after ~10ms delay
        self._dup_delay = 4  # resend packet from N packets ago (~10ms at 2.67ms/pkt)
        self._dup_ring = [None] * (self._dup_delay + 2)
        self._dup_idx = 0
        # Xrun tracking
        self._xrun_times: list[float] = []
        self._restart_requested = False

    def start(self):
        """Start the JACK audio capture client."""
        if not HAS_JACK:
            print("Audio streaming unavailable (no JACK client library)")
            return

        try:
            import numpy as np

            self.jack_client = jack.Client("anarack_audio")
            self.jack_client.inports.register("input_1")

            # Pre-allocate conversion buffer to avoid allocation in RT callback
            blocksize = self.jack_client.blocksize
            self._int16_buf = np.zeros(blocksize, dtype=np.int16)
            self._scale = np.float32(32767.0)

            @self.jack_client.set_process_callback
            def process(frames):
                # Get audio data from JACK port (float32) — minimal work in RT callback
                audio_data = self.jack_client.inports[0].get_array()
                # Fast in-place conversion: float32 → int16
                np.multiply(audio_data, self._scale, out=self._int16_buf, casting='unsafe')
                try:
                    self.audio_queue.put_nowait(self._int16_buf.tobytes())
                except queue.Full:
                    pass  # Drop if consumers are too slow

            @self.jack_client.set_xrun_callback
            def xrun_callback(delay):
                now = time.monotonic()
                self._xrun_times.append(now)
                # Prune old xruns outside the window
                cutoff = now - self.XRUN_WINDOW_SECS
                self._xrun_times = [t for t in self._xrun_times if t > cutoff]
                count = len(self._xrun_times)
                print(f"JACK xrun (delay={delay:.1f}ms) — {count} in last {self.XRUN_WINDOW_SECS}s")
                if count >= self.XRUN_THRESHOLD:
                    print(f"⚠ Xrun threshold reached ({count} >= {self.XRUN_THRESHOLD}) — requesting JACK restart")
                    self._restart_requested = True

            self.jack_client.activate()

            # Connect to the Scarlett capture port
            try:
                self.jack_client.connect(self.capture_port, "anarack_audio:input_1")
                print(f"Audio capture connected: {self.capture_port} → anarack_audio:input_1")
            except jack.JackError as e:
                print(f"WARNING: Could not auto-connect audio: {e}")
                print(f"  Manually connect with: jack_connect {self.capture_port} anarack_audio:input_1")

            self._running = True
            print(f"Audio streaming active (JACK @ {self.jack_client.samplerate}Hz, "
                  f"buffer {self.jack_client.blocksize} frames)")

        except Exception as e:
            print(f"WARNING: Could not start audio capture: {e}")
            self.jack_client = None

    def stop(self):
        """Stop the JACK client."""
        self._running = False
        if self.jack_client:
            try:
                self.jack_client.deactivate()
                self.jack_client.close()
            except Exception:
                pass
            self.jack_client = None

    def restart(self):
        """Restart JACK client to clear accumulated xruns."""
        import subprocess
        print("🔄 Restarting JACK...")
        self.stop()
        # Kill JACK server and restart fresh
        subprocess.run(["pkill", "-9", "jackd"], capture_output=True)
        time.sleep(3)
        subprocess.Popen(
            ["jackd", "-R", "-d", "alsa", "-d", "hw:0", "-r", "48000", "-p", "128", "-n", "3"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
        )
        # Wait for JACK to accept connections
        for i in range(20):
            time.sleep(1)
            result = subprocess.run(["jack_lsp"], capture_output=True)
            if result.returncode == 0:
                print(f"  JACK ready after {i+1}s")
                break
        # Restart our JACK client
        self._xrun_times.clear()
        self._restart_requested = False
        self.start()
        print("✅ JACK restarted successfully")

    def add_client(self, ws):
        self.clients.add(ws)

    def remove_client(self, ws):
        self.clients.discard(ws)

    def add_udp_client(self, addr, sock):
        """Register a UDP client to receive audio."""
        self.udp_clients[addr] = sock
        print(f"UDP audio client added: {addr}")

    def remove_udp_client(self, addr):
        self.udp_clients.pop(addr, None)
        print(f"UDP audio client removed: {addr}")

    async def stream_to_clients(self):
        """Continuously send audio data to all connected WebSocket clients."""
        while self._running:
            try:
                # Check if JACK restart was requested (xrun watchdog)
                if self._restart_requested:
                    # Notify UDP clients that a restart is happening
                    restart_msg = json.dumps({"type": "jackRestart"}).encode()
                    for addr, sock in self.udp_clients.items():
                        try:
                            sock.sendto(restart_msg, addr)
                        except OSError:
                            pass
                    # Run restart in executor to avoid blocking event loop
                    loop = asyncio.get_event_loop()
                    await loop.run_in_executor(None, self.restart)
                    continue

                # Poll the queue rapidly instead of blocking in executor
                try:
                    chunk = self.audio_queue.get_nowait()
                except queue.Empty:
                    await asyncio.sleep(0.001)  # 1ms poll — fast enough for audio
                    continue

                if not self.clients and not self.udp_clients:
                    continue

                # Send to WebSocket clients
                if self.clients:
                    dead_clients = set()
                    for ws in self.clients:
                        try:
                            await ws.send(chunk)
                        except websockets.ConnectionClosed:
                            dead_clients.add(ws)
                    self.clients -= dead_clients

                # Send to UDP clients with 12-byte header + packet duplication
                if self.udp_clients:
                    hdr = struct.pack("<IIHh", self._seq, self._ts, 0, 0)
                    pkt = hdr + chunk
                    self._seq += 1
                    self._ts += self._blocksize

                    dead_udp = []
                    for addr, sock in self.udp_clients.items():
                        try:
                            sock.sendto(pkt, addr)
                        except OSError:
                            dead_udp.append(addr)

                    # Store for delayed duplication
                    self._dup_ring[self._dup_idx % len(self._dup_ring)] = pkt
                    self._dup_idx += 1

                    # Fire-and-forget: send duplicate from N packets ago
                    old_idx = (self._dup_idx - self._dup_delay - 1) % len(self._dup_ring)
                    old_pkt = self._dup_ring[old_idx]
                    if old_pkt is not None:
                        loop = asyncio.get_event_loop()
                        for addr, sock in list(self.udp_clients.items()):
                            loop.call_later(0.005, sock.sendto, old_pkt, addr)

                    for addr in dead_udp:
                        self.udp_clients.pop(addr, None)

            except Exception as e:
                print(f"Audio stream error: {e}")
                await asyncio.sleep(0.1)


async def udp_server(router: MidiRouter, host: str, port: int, audio_port: int = 9999):
    """UDP server for receiving MIDI from native clients.
    Also registers them for UDP audio return on audio_port."""

    import socket as _socket
    # Create a raw UDP socket for sending audio back
    audio_sock = _socket.socket(_socket.AF_INET, _socket.SOCK_DGRAM)

    class MidiUDPProtocol(asyncio.DatagramProtocol):
        def connection_made(self, transport):
            router._udp_transport = transport

        def datagram_received(self, data, addr):
            router.send_from_udp(data)
            # Register client for CC broadcast on the AUDIO port
            client_addr = (addr[0], audio_port)
            is_new = client_addr not in router._udp_clients
            router._udp_clients.add(client_addr)
            if is_new:
                print(f"Registered UDP client for CC broadcast: {client_addr}")

            # Request edit buffer on EVERY registration packet (not just new clients)
            # This handles reconnects where the client IP:port hasn't changed
            if not hasattr(router, '_last_edit_request') or \
               (time.monotonic() - router._last_edit_request) > 3.0:
                router._last_edit_request = time.monotonic()
                if router._loop:
                    router._loop.call_soon_threadsafe(
                        router._loop.call_later, 3.0, router.request_edit_buffer
                    )

            # Auto-register this client for audio return
            if audio_streamer and (addr[0], audio_port) not in audio_streamer.udp_clients:
                audio_streamer.add_udp_client((addr[0], audio_port), audio_sock)

    loop = asyncio.get_running_loop()
    transport, _ = await loop.create_datagram_endpoint(
        MidiUDPProtocol,
        local_addr=(host, port),
    )
    print(f"UDP MIDI server listening on {host}:{port}")
    print(f"UDP audio will stream back to clients on port {audio_port}")
    return transport


# Global state — shared between handlers
audio_streamer: AudioStreamer | None = None
synth_mgr: SynthManager | None = None
preset_mgr: PresetManager | None = None


async def ws_handler(router: MidiRouter, ws, path=None):
    """Handle all WebSocket connections. Route by path: /audio or /midi (default)."""
    remote = ws.remote_address

    # Try multiple ways to get the path (varies by websockets library version)
    ws_path = path or getattr(ws, 'path', None) or getattr(ws, 'request', None) and getattr(ws.request, 'path', None) or '/'
    # Also check the request headers for the path
    try:
        if hasattr(ws, 'request') and hasattr(ws.request, 'path'):
            ws_path = ws.request.path
    except:
        pass

    print(f"WebSocket connection: {remote} path='{ws_path}'")

    if '/api' in str(ws_path):
        # API requests over WebSocket (definition, presets)
        try:
            async for message in ws:
                try:
                    req = json.loads(message)
                    action = req.get("action", "")
                    resp = {"error": "unknown action"}

                    if action == "get_definition":
                        if synth_mgr and synth_mgr.active_synth:
                            resp = {"action": "definition", "data": synth_mgr.active_synth.to_dict()}
                        else:
                            resp = {"error": "no active synth"}

                    elif action == "list_synths":
                        if synth_mgr:
                            resp = {"action": "synths", "data": synth_mgr.list_synths()}

                    elif action == "save_preset":
                        name = req.get("name", "")
                        if name and preset_mgr and router:
                            # Request edit buffer from synth
                            router.request_edit_buffer()
                            await asyncio.sleep(0.3)
                            # For now, save a placeholder — full SysEx capture needs callback
                            preset_mgr.save(synth_mgr.active_synth.id if synth_mgr and synth_mgr.active_synth else "unknown", name, [])
                            resp = {"action": "preset_saved", "name": name}

                    elif action == "list_presets":
                        synth_id = synth_mgr.active_synth.id if synth_mgr and synth_mgr.active_synth else ""
                        if preset_mgr and synth_id:
                            resp = {"action": "presets", "data": preset_mgr.list_presets(synth_id)}

                    elif action == "load_preset":
                        name = req.get("name", "")
                        synth_id = synth_mgr.active_synth.id if synth_mgr and synth_mgr.active_synth else ""
                        if preset_mgr and synth_id and name:
                            sysex = preset_mgr.load(synth_id, name)
                            if sysex and router:
                                router.midi_out.send_message(sysex)
                                resp = {"action": "preset_loaded", "name": name}
                            else:
                                resp = {"error": "preset not found"}

                    await ws.send(json.dumps(resp))
                except Exception as e:
                    await ws.send(json.dumps({"error": str(e)}))
        except websockets.ConnectionClosed:
            pass
        return

    if '/audio' in str(ws_path):
        # Audio stream client
        print(f"Audio client connected: {remote}")
        if audio_streamer:
            audio_streamer.add_client(ws)
        try:
            async for _ in ws:
                pass
        except websockets.ConnectionClosed:
            pass
        finally:
            if audio_streamer:
                audio_streamer.remove_client(ws)
            print(f"Audio client disconnected: {remote}")
    else:
        # MIDI client (default)
        print(f"MIDI client connected: {remote}")
        router.midi_ws_clients.add(ws)
        try:
            async for message in ws:
                try:
                    router.send_from_websocket(message)
                except (json.JSONDecodeError, KeyError) as e:
                    await ws.send(json.dumps({"error": str(e)}))
        except websockets.ConnectionClosed:
            pass
        finally:
            router.midi_ws_clients.discard(ws)
            print(f"MIDI client disconnected: {remote}")


def start_http_server(host: str, port: int):
    """Simple HTTP server for serving the plugin UI and API."""
    from http.server import HTTPServer, SimpleHTTPRequestHandler
    import os

    ui_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "plugin", "ui")

    class Handler(SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, directory=ui_dir, **kwargs)

        def do_GET(self):
            if self.path == '/api/synth/definition' and synth_mgr and synth_mgr.active_synth:
                body = synth_mgr.active_synth.to_json().encode()
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(body)
            elif self.path == '/api/synths' and synth_mgr:
                body = json.dumps(synth_mgr.list_synths()).encode()
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(body)
            else:
                super().do_GET()

        def log_message(self, format, *args):
            pass  # Suppress HTTP logs

    server = HTTPServer((host, port), Handler)
    print(f"HTTP UI server: http://{host}:{port}")
    # Run in a thread
    import threading
    t = threading.Thread(target=server.serve_forever, daemon=True)
    t.start()


async def main():
    global audio_streamer, synth_mgr, preset_mgr

    parser = argparse.ArgumentParser(description="Anarack Server")
    parser.add_argument("--udp-port", type=int, default=5555, help="UDP port for MIDI input")
    parser.add_argument("--ws-port", type=int, default=8765, help="WebSocket port (MIDI + audio)")
    parser.add_argument("--host", default="0.0.0.0", help="Listen address")
    parser.add_argument("--midi-port", default=None, help="MIDI output port name or index")
    parser.add_argument("--capture-port", default="system:capture_1", help="JACK capture port for audio")
    parser.add_argument("--list-ports", action="store_true", help="List MIDI ports and exit")
    args = parser.parse_args()

    if args.list_ports:
        list_midi_ports()
        return

    # Load synth definitions
    synth_mgr = SynthManager()
    preset_mgr = PresetManager()

    # Auto-select the first available synth (or Prophet Rev2 if available)
    if "sequential-prophet-rev2" in synth_mgr.definitions:
        synth_mgr.set_active("sequential-prophet-rev2")
    elif synth_mgr.definitions:
        synth_mgr.set_active(next(iter(synth_mgr.definitions)))

    midi_out = open_midi_port(args.midi_port)
    router = MidiRouter(midi_out, synth_mgr)
    router._loop = asyncio.get_running_loop()

    # Open MIDI input from synth via raw ALSA device (not rtmidi sequencer).
    # rtmidi can't receive on the same ALSA port that's open for output — the
    # sequencer doesn't deliver data. Raw device access bypasses this limitation.
    midi_in_fd = None
    midi_in_dev = None
    try:
        import subprocess as _sp
        # Find the raw MIDI device for the synth
        amidi_out = _sp.run(["amidi", "-l"], capture_output=True, text=True).stdout
        for line in amidi_out.strip().split("\n"):
            if args.midi_port and args.midi_port.lower() in line.lower():
                midi_in_dev = line.split()[1]  # e.g. "hw:3,0,0"
                break
        if midi_in_dev:
            import os as _os
            # Map hw:X,Y,Z to /dev/snd/midiCXDY
            parts = midi_in_dev.replace("hw:", "").split(",")
            raw_path = f"/dev/snd/midiC{parts[0]}D{parts[1] if len(parts) > 1 else '0'}"
            midi_in_fd = _os.open(raw_path, _os.O_RDONLY | _os.O_NONBLOCK)
            print(f"Bidirectional MIDI active (raw device: {raw_path})")
        else:
            print("WARNING: Could not find raw MIDI device for synth input")
    except Exception as e:
        print(f"WARNING: Raw MIDI input failed: {e}")

    async def poll_raw_midi():
        """Read raw MIDI bytes from /dev/snd/midiCxDy and dispatch to router."""
        import os as _os, select as _sel
        buf = bytearray()
        print("Raw MIDI polling started")
        while True:
            try:
                if midi_in_fd is not None:
                    r, _, _ = _sel.select([midi_in_fd], [], [], 0.002)
                    if r:
                        data = _os.read(midi_in_fd, 1024)
                        if data:
                            buf.extend(data)
                            # Parse complete MIDI messages from buffer
                            while buf:
                                status = buf[0]
                                if status == 0xF0:
                                    # SysEx: find F7 terminator
                                    end = buf.find(0xF7)
                                    if end < 0:
                                        break  # incomplete
                                    msg = list(buf[:end+1])
                                    del buf[:end+1]
                                    router.on_synth_message((msg, 0))
                                elif status & 0x80:
                                    # Channel message
                                    if (status & 0xF0) in (0xC0, 0xD0):
                                        needed = 2
                                    else:
                                        needed = 3
                                    if len(buf) < needed:
                                        break  # incomplete
                                    msg = list(buf[:needed])
                                    del buf[:needed]
                                    router.on_synth_message((msg, 0))
                                else:
                                    del buf[0]  # skip stray data byte
                else:
                    await asyncio.sleep(0.1)
            except Exception as e:
                print(f"Raw MIDI poll error: {e}")
                await asyncio.sleep(0.1)
            await asyncio.sleep(0)

    # Start UDP server
    udp_transport = await udp_server(router, args.host, args.udp_port)

    # Start WebSocket server (MIDI on /midi or /, audio on /audio)
    ws_server = await websockets.serve(
        lambda ws, path=None: ws_handler(router, ws, path),
        args.host,
        args.ws_port,
    )
    print(f"WebSocket server listening on {args.host}:{args.ws_port}")
    print(f"  MIDI: ws://host:{args.ws_port}/      Audio: ws://host:{args.ws_port}/audio")

    # Start audio capture and streaming
    audio_task = None
    if HAS_JACK:
        audio_streamer = AudioStreamer(capture_port=args.capture_port)
        audio_streamer.start()
        audio_task = asyncio.create_task(audio_streamer.stream_to_clients())

    # Start HTTP server for plugin UI
    start_http_server(args.host, 8080)

    print(f"\nAnarack Server running. Ctrl+C to stop.")
    print(f"  WebSocket: ws://{args.host}:{args.ws_port} (MIDI: / , Audio: /audio)")
    print(f"  Plugin UI: http://{args.host}:8080")
    print()

    # Start raw MIDI polling
    midi_poll_task = asyncio.create_task(poll_raw_midi()) if midi_in_fd is not None else None

    # Run until interrupted
    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, stop.set)

    await stop.wait()

    # Cleanup
    if HAS_JACK and audio_streamer:
        audio_streamer.stop()
        if audio_task:
            audio_task.cancel()

    udp_transport.close()
    ws_server.close()
    await ws_server.wait_closed()
    if midi_in:
        midi_in.close_port()
    midi_out.close_port()
    print(f"\nShutdown complete. Sent {router.message_count} MIDI messages.")


if __name__ == "__main__":
    asyncio.run(main())
