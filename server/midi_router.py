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
import queue

import rtmidi
import websockets

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
    def __init__(self, midi_out: rtmidi.MidiOut):
        self.midi_out = midi_out
        self.message_count = 0

    def send(self, message: list[int]):
        """Send a MIDI message to the hardware synth."""
        self.midi_out.send_message(message)
        self.message_count += 1

    def send_from_udp(self, data: bytes):
        """Parse and forward a UDP MIDI packet."""
        if len(data) >= 2:
            self.send(list(data[:3] if len(data) >= 3 else data))

    def send_from_websocket(self, payload: str):
        """Parse and forward a WebSocket MIDI message."""
        msg = json.loads(payload)
        status = msg["status"]
        data1 = msg.get("data1", 0)
        data2 = msg.get("data2", 0)
        # Program Change (0xC0) and Channel Pressure (0xD0) are 2-byte messages
        if (status & 0xF0) in (0xC0, 0xD0):
            self.send([status, data1])
        else:
            self.send([status, data1, data2])


class AudioStreamer:
    """Captures audio from JACK and makes it available to WebSocket clients."""

    def __init__(self, capture_port: str = "system:capture_1"):
        self.capture_port = capture_port
        self.clients: set = set()
        self.jack_client = None
        self.audio_queue: queue.Queue = queue.Queue(maxsize=200)
        self._running = False

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
            self.jack_client.deactivate()
            self.jack_client.close()

    def add_client(self, ws):
        self.clients.add(ws)

    def remove_client(self, ws):
        self.clients.discard(ws)

    async def stream_to_clients(self):
        """Continuously send audio data to all connected WebSocket clients."""
        while self._running:
            try:
                # Wait for audio data from the JACK callback
                try:
                    chunk = await asyncio.get_event_loop().run_in_executor(
                        None, lambda: self.audio_queue.get(timeout=0.1)
                    )
                except queue.Empty:
                    continue

                if not self.clients:
                    continue

                # Send immediately — don't batch. Each chunk is one JACK buffer
                # (256 samples = 5.3ms @ 48kHz). Low latency > fewer packets.
                dead_clients = set()
                for ws in self.clients:
                    try:
                        await ws.send(chunk)
                    except websockets.ConnectionClosed:
                        dead_clients.add(ws)
                self.clients -= dead_clients

            except Exception as e:
                print(f"Audio stream error: {e}")
                await asyncio.sleep(0.1)


async def udp_server(router: MidiRouter, host: str, port: int):
    """UDP server for receiving MIDI from the plugin."""

    class MidiUDPProtocol(asyncio.DatagramProtocol):
        def datagram_received(self, data, addr):
            router.send_from_udp(data)

    loop = asyncio.get_running_loop()
    transport, _ = await loop.create_datagram_endpoint(
        MidiUDPProtocol,
        local_addr=(host, port),
    )
    print(f"UDP MIDI server listening on {host}:{port}")
    return transport


# Global audio streamer — shared between MIDI and audio WebSocket handlers
audio_streamer: AudioStreamer | None = None


async def midi_ws_handler(router: MidiRouter, ws):
    """Handle MIDI WebSocket connections from the browser."""
    remote = ws.remote_address
    print(f"MIDI client connected: {remote}")
    try:
        async for message in ws:
            try:
                router.send_from_websocket(message)
            except (json.JSONDecodeError, KeyError) as e:
                await ws.send(json.dumps({"error": str(e)}))
    except websockets.ConnectionClosed:
        pass
    finally:
        print(f"MIDI client disconnected: {remote}")


async def audio_ws_handler(ws):
    """Handle audio WebSocket connections from the browser."""
    remote = ws.remote_address
    print(f"Audio client connected: {remote}")

    if audio_streamer:
        audio_streamer.add_client(ws)

    try:
        # Keep connection alive — client doesn't send data, just receives
        async for _ in ws:
            pass
    except websockets.ConnectionClosed:
        pass
    finally:
        if audio_streamer:
            audio_streamer.remove_client(ws)
        print(f"Audio client disconnected: {remote}")


async def main():
    global audio_streamer

    parser = argparse.ArgumentParser(description="Anarack Server")
    parser.add_argument("--udp-port", type=int, default=5555, help="UDP port for MIDI input")
    parser.add_argument("--ws-port", type=int, default=8765, help="WebSocket port for browser MIDI")
    parser.add_argument("--audio-ws-port", type=int, default=8766, help="WebSocket port for audio stream")
    parser.add_argument("--host", default="0.0.0.0", help="Listen address")
    parser.add_argument("--midi-port", default=None, help="MIDI output port name or index")
    parser.add_argument("--capture-port", default="system:capture_1", help="JACK capture port for audio")
    parser.add_argument("--list-ports", action="store_true", help="List MIDI ports and exit")
    args = parser.parse_args()

    if args.list_ports:
        list_midi_ports()
        return

    midi_out = open_midi_port(args.midi_port)
    router = MidiRouter(midi_out)

    # Start UDP server
    udp_transport = await udp_server(router, args.host, args.udp_port)

    # Start MIDI WebSocket server
    midi_ws = await websockets.serve(
        lambda ws: midi_ws_handler(router, ws),
        args.host,
        args.ws_port,
    )
    print(f"MIDI WebSocket listening on {args.host}:{args.ws_port}")

    # Start audio capture and streaming
    if HAS_JACK:
        audio_streamer = AudioStreamer(capture_port=args.capture_port)
        audio_streamer.start()

        # Start audio WebSocket server
        audio_ws = await websockets.serve(
            audio_ws_handler,
            args.host,
            args.audio_ws_port,
        )
        print(f"Audio WebSocket listening on {args.host}:{args.audio_ws_port}")

        # Start the audio streaming loop
        audio_task = asyncio.create_task(audio_streamer.stream_to_clients())

    print(f"\nAnarack Server running. Ctrl+C to stop.")
    print(f"  MIDI WebSocket: ws://{args.host}:{args.ws_port}")
    if HAS_JACK:
        print(f"  Audio WebSocket: ws://{args.host}:{args.audio_ws_port}")
    print()

    # Run until interrupted
    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, stop.set)

    await stop.wait()

    # Cleanup
    if HAS_JACK and audio_streamer:
        audio_streamer.stop()
        audio_task.cancel()
        audio_ws.close()
        await audio_ws.wait_closed()

    udp_transport.close()
    midi_ws.close()
    await midi_ws.wait_closed()
    midi_out.close_port()
    print(f"\nShutdown complete. Sent {router.message_count} MIDI messages.")


if __name__ == "__main__":
    asyncio.run(main())
