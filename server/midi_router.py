"""
Airsynth MIDI Router — Phase 0 Prototype

Receives MIDI messages over UDP and forwards them to a hardware synth via rtmidi.
Also runs a WebSocket server so the browser demo can send MIDI.

Usage:
    python midi_router.py [--udp-port 5555] [--ws-port 8765] [--midi-port "Prophet Rev2"]
"""

import argparse
import asyncio
import json
import struct
import signal

import rtmidi
import websockets


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
        # Use the first available port
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
        """Parse and forward a UDP MIDI packet.

        Format: 3 bytes — [status, data1, data2]
        SysEx: variable length, starts with 0xF0, ends with 0xF7
        """
        if len(data) >= 2:
            self.send(list(data[:3] if len(data) >= 3 else data))

    def send_from_websocket(self, payload: str):
        """Parse and forward a WebSocket MIDI message.

        Format: JSON {"status": int, "data1": int, "data2": int}
        """
        msg = json.loads(payload)
        status = msg["status"]
        data1 = msg.get("data1", 0)
        data2 = msg.get("data2", 0)
        self.send([status, data1, data2])


async def udp_server(router: MidiRouter, host: str, port: int):
    """UDP server for receiving MIDI from the plugin / netjack2 client."""

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


async def websocket_handler(router: MidiRouter, ws):
    """Handle a single WebSocket connection from the browser demo."""
    remote = ws.remote_address
    print(f"WebSocket client connected: {remote}")
    try:
        async for message in ws:
            try:
                router.send_from_websocket(message)
            except (json.JSONDecodeError, KeyError) as e:
                await ws.send(json.dumps({"error": str(e)}))
    except websockets.ConnectionClosed:
        pass
    finally:
        print(f"WebSocket client disconnected: {remote}")


async def main():
    parser = argparse.ArgumentParser(description="Airsynth MIDI Router")
    parser.add_argument("--udp-port", type=int, default=5555, help="UDP port for MIDI input")
    parser.add_argument("--ws-port", type=int, default=8765, help="WebSocket port for browser MIDI")
    parser.add_argument("--host", default="0.0.0.0", help="Listen address")
    parser.add_argument("--midi-port", default=None, help="MIDI output port name or index")
    parser.add_argument("--list-ports", action="store_true", help="List MIDI ports and exit")
    args = parser.parse_args()

    if args.list_ports:
        list_midi_ports()
        return

    midi_out = open_midi_port(args.midi_port)
    router = MidiRouter(midi_out)

    # Start UDP server
    udp_transport = await udp_server(router, args.host, args.udp_port)

    # Start WebSocket server
    ws_server = await websockets.serve(
        lambda ws: websocket_handler(router, ws),
        args.host,
        args.ws_port,
    )
    print(f"WebSocket MIDI server listening on {args.host}:{args.ws_port}")

    print(f"\nAirsynth MIDI Router running. Ctrl+C to stop.")
    print(f"  MIDI messages sent: check with --list-ports to verify connection\n")

    # Run until interrupted
    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, stop.set)

    await stop.wait()

    # Cleanup
    udp_transport.close()
    ws_server.close()
    await ws_server.wait_closed()
    midi_out.close_port()
    print(f"\nShutdown complete. Sent {router.message_count} MIDI messages.")


if __name__ == "__main__":
    asyncio.run(main())
