"""
Anarack Native Client — Low-Latency Prototype

Bypasses the browser entirely for minimum latency:
- Reads MIDI from your controller via rtmidi (no Web MIDI API)
- Sends MIDI over UDP to the Pi (no WebSocket/TCP)
- Receives audio over UDP from the Pi (no WebSocket/TCP)
- Plays audio through PyAudio/CoreAudio with minimal buffering (no Web Audio API)

Usage:
    python native_client.py [--server 192.168.1.131] [--midi-port "LaunchKey"]

Requirements:
    pip install python-rtmidi pyaudio
"""

import argparse
import socket
import struct
import sys
import threading
import time

import rtmidi
import pyaudio

SAMPLE_RATE = 48000
CHANNELS = 1
FORMAT = pyaudio.paInt16
AUDIO_PORT = 9999
MIDI_PORT = 5555


def list_midi_ports():
    midi_in = rtmidi.MidiIn()
    ports = midi_in.get_ports()
    if not ports:
        print("No MIDI input ports found.")
    else:
        print("Available MIDI input ports:")
        for i, port in enumerate(ports):
            print(f"  [{i}] {port}")
    return ports


def open_midi_input(port_name=None):
    midi_in = rtmidi.MidiIn()
    ports = midi_in.get_ports()

    if not ports:
        print("No MIDI input ports available")
        return None

    if port_name is None:
        midi_in.open_port(0)
        print(f"Opened MIDI input: {ports[0]}")
        return midi_in

    # Try numeric index
    try:
        idx = int(port_name)
        if 0 <= idx < len(ports):
            midi_in.open_port(idx)
            print(f"Opened MIDI input: {ports[idx]}")
            return midi_in
    except ValueError:
        pass

    # Substring match
    for i, port in enumerate(ports):
        if port_name.lower() in port.lower():
            midi_in.open_port(i)
            print(f"Opened MIDI input: {port}")
            return midi_in

    print(f"MIDI port matching '{port_name}' not found. Available: {ports}")
    return None


def main():
    parser = argparse.ArgumentParser(description="Anarack Native Client")
    parser.add_argument("--server", default="192.168.1.131", help="Pi server IP")
    parser.add_argument("--midi-port", default=None, help="MIDI input port name or index")
    parser.add_argument("--list-ports", action="store_true", help="List MIDI ports and exit")
    parser.add_argument("--buffer-frames", type=int, default=256,
                        help="PyAudio buffer size in frames (lower = less latency, more CPU)")
    args = parser.parse_args()

    if args.list_ports:
        list_midi_ports()
        return

    # --- MIDI Setup ---
    midi_in = open_midi_input(args.midi_port)
    if midi_in is None:
        sys.exit(1)

    # UDP socket for sending MIDI to Pi
    midi_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server_addr = (args.server, MIDI_PORT)

    # --- Audio Setup ---
    audio_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    audio_sock.bind(("0.0.0.0", AUDIO_PORT))
    audio_sock.settimeout(0.1)

    pa = pyaudio.PyAudio()

    # Find the default output device and report its latency
    default_output = pa.get_default_output_device_info()
    print(f"Audio output: {default_output['name']}")
    print(f"  Default low latency: {default_output['defaultLowOutputLatency']*1000:.1f}ms")

    stream = pa.open(
        format=FORMAT,
        channels=CHANNELS,
        rate=SAMPLE_RATE,
        output=True,
        frames_per_buffer=args.buffer_frames,
    )

    midi_count = 0
    audio_count = 0
    latency_measurements = []

    # --- MIDI Callback ---
    # Track when we send notes for latency measurement
    note_send_times = {}

    def midi_callback(event, data=None):
        nonlocal midi_count
        message, delta = event
        # Send raw MIDI bytes over UDP
        midi_sock.sendto(bytes(message), server_addr)
        midi_count += 1

        # Track note-on times for latency measurement
        if len(message) >= 2:
            status = message[0] & 0xF0
            if status == 0x90 and message[2] > 0:
                note_send_times[message[1]] = time.perf_counter()

    midi_in.set_callback(midi_callback)

    # --- Audio Receive Thread ---
    running = True

    def audio_thread():
        nonlocal audio_count
        while running:
            try:
                data, addr = audio_sock.recvfrom(65536)
                stream.write(data)
                audio_count += 1

                # Simple latency detection: check for non-silence
                if note_send_times and len(data) >= 4:
                    samples = struct.unpack(f"<{len(data)//2}h", data)
                    peak = max(abs(s) for s in samples)
                    if peak > 300 and note_send_times:
                        # Audio detected — measure from earliest pending note
                        earliest_note = min(note_send_times.values())
                        latency = (time.perf_counter() - earliest_note) * 1000
                        if latency < 200:  # Sanity check
                            latency_measurements.append(latency)
                        note_send_times.clear()

            except socket.timeout:
                continue
            except OSError:
                if running:
                    continue
                break

    audio_recv = threading.Thread(target=audio_thread, daemon=True)
    audio_recv.start()

    # Send an initial MIDI packet to register with the server for audio
    midi_sock.sendto(bytes([0xFE]), server_addr)  # Active Sensing — harmless

    print(f"\n=== Anarack Native Client ===")
    print(f"  Server:     {args.server}")
    print(f"  MIDI:       UDP → {args.server}:{MIDI_PORT}")
    print(f"  Audio:      UDP ← port {AUDIO_PORT}")
    print(f"  Buffer:     {args.buffer_frames} frames ({args.buffer_frames/SAMPLE_RATE*1000:.1f}ms)")
    print(f"\n  Play your MIDI controller — audio comes through your Mac speakers.")
    print(f"  Press Ctrl+C to quit.\n")

    try:
        while True:
            time.sleep(2)
            # Print stats
            if latency_measurements:
                recent = latency_measurements[-10:]
                avg = sum(recent) / len(recent)
                mn = min(recent)
                mx = max(recent)
                print(f"  MIDI: {midi_count} msgs | Audio: {audio_count} packets | "
                      f"Latency: {avg:.0f}ms avg ({mn:.0f}-{mx:.0f}ms) [{len(latency_measurements)} samples]",
                      end="\r")
            else:
                print(f"  MIDI: {midi_count} msgs | Audio: {audio_count} packets | "
                      f"Latency: measuring...", end="\r")

    except KeyboardInterrupt:
        print("\n\nShutting down...")
        running = False
        midi_in.close_port()
        stream.stop_stream()
        stream.close()
        audio_sock.close()
        midi_sock.close()
        pa.terminate()

        if latency_measurements:
            avg = sum(latency_measurements) / len(latency_measurements)
            mn = min(latency_measurements)
            mx = max(latency_measurements)
            print(f"\nFinal latency: {avg:.0f}ms avg ({mn:.0f}-{mx:.0f}ms) over {len(latency_measurements)} samples")
        print("Done.")


if __name__ == "__main__":
    main()
