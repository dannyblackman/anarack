"""
Anarack Native Client — Low-Latency Prototype

Bypasses the browser entirely for minimum latency:
- Reads MIDI from your controller via mido (no browser/Web MIDI API)
- Sends MIDI over UDP to the Pi (no WebSocket/TCP)
- Receives audio over UDP from the Pi (no WebSocket/TCP)
- Plays audio through PyAudio/CoreAudio with minimal buffering (no Web Audio API)

Usage:
    python native_client.py [--server 192.168.1.131] [--midi-port "LaunchKey"]

Requirements:
    pip install mido pyaudio
"""

import argparse
import socket
import struct
import sys
import threading
import time

import mido
import pyaudio

SAMPLE_RATE = 48000
CHANNELS = 1
FORMAT = pyaudio.paInt16
AUDIO_PORT = 9999
MIDI_PORT = 5555


def list_midi_ports():
    ports = mido.get_input_names()
    if not ports:
        print("No MIDI input ports found.")
    else:
        print("Available MIDI input ports:")
        for i, port in enumerate(ports):
            print(f"  [{i}] {port}")
    return ports


def find_midi_port(port_name=None):
    ports = mido.get_input_names()

    if not ports:
        print("No MIDI input ports available")
        return None

    if port_name is None:
        print(f"Using first MIDI input: {ports[0]}")
        return ports[0]

    # Try numeric index
    try:
        idx = int(port_name)
        if 0 <= idx < len(ports):
            print(f"Using MIDI input: {ports[idx]}")
            return ports[idx]
    except ValueError:
        pass

    # Substring match
    for port in ports:
        if port_name.lower() in port.lower():
            print(f"Using MIDI input: {port}")
            return port

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
    port_name = find_midi_port(args.midi_port)
    if port_name is None:
        sys.exit(1)

    # UDP socket for sending MIDI to Pi
    midi_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server_addr = (args.server, MIDI_PORT)

    # --- Audio Setup ---
    audio_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    audio_sock.bind(("0.0.0.0", AUDIO_PORT))
    audio_sock.settimeout(0.1)

    pa = pyaudio.PyAudio()

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
    note_send_times = {}

    # --- Audio Receive Thread ---
    running = True

    def audio_thread():
        nonlocal audio_count
        while running:
            try:
                data, addr = audio_sock.recvfrom(65536)
                stream.write(data)
                audio_count += 1

                # Latency detection: check for non-silence
                if note_send_times and len(data) >= 4:
                    samples = struct.unpack(f"<{len(data)//2}h", data)
                    peak = max(abs(s) for s in samples)
                    if peak > 300 and note_send_times:
                        earliest_note = min(note_send_times.values())
                        latency = (time.perf_counter() - earliest_note) * 1000
                        if latency < 200:
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

    # Send an initial packet to register with the server for audio
    midi_sock.sendto(bytes([0xFE]), server_addr)

    print(f"\n=== Anarack Native Client ===")
    print(f"  Server:     {args.server}")
    print(f"  MIDI:       UDP -> {args.server}:{MIDI_PORT}")
    print(f"  Audio:      UDP <- port {AUDIO_PORT}")
    print(f"  Buffer:     {args.buffer_frames} frames ({args.buffer_frames/SAMPLE_RATE*1000:.1f}ms)")
    print(f"\n  Play your MIDI controller. Press Ctrl+C to quit.\n")

    # Computer keyboard → MIDI note mapping (same as browser client)
    import sys, tty, termios, select
    KEY_MAP = {
        'a': 0, 'w': 1, 's': 2, 'e': 3, 'd': 4, 'f': 5,
        't': 6, 'g': 7, 'y': 8, 'h': 9, 'u': 10, 'j': 11,
        'k': 12, 'o': 13, 'l': 14, 'p': 15,
    }
    BASE_NOTE = 60  # C4
    held_keys = set()
    old_term = termios.tcgetattr(sys.stdin)
    tty.setcbreak(sys.stdin.fileno())

    print("  Computer keyboard: A-L row plays notes (C4+). Press Q to quit.\n")

    # --- MIDI Input Loop ---
    try:
        with mido.open_input(port_name) as midi_in:
            while True:
                # Check computer keyboard
                if select.select([sys.stdin], [], [], 0)[0]:
                    key = sys.stdin.read(1).lower()
                    if key == 'q':
                        break
                    if key in KEY_MAP and key not in held_keys:
                        note = BASE_NOTE + KEY_MAP[key]
                        midi_sock.sendto(bytes([0x90, note, 100]), server_addr)
                        midi_count += 1
                        note_send_times[note] = time.perf_counter()
                        held_keys.add(key)
                    # Note: can't detect key-up in cbreak mode, so notes sustain.
                    # Press same key again to send note-off
                    elif key in KEY_MAP and key in held_keys:
                        note = BASE_NOTE + KEY_MAP[key]
                        midi_sock.sendto(bytes([0x80, note, 0]), server_addr)
                        midi_count += 1
                        held_keys.discard(key)

                # Check MIDI controller
                for msg in midi_in.iter_pending():
                    raw = msg.bytes()
                    midi_sock.sendto(bytes(raw), server_addr)
                    midi_count += 1

                    if msg.type == 'note_on' and msg.velocity > 0:
                        note_send_times[msg.note] = time.perf_counter()

                time.sleep(0.001)

                if midi_count % 100 == 0 or audio_count % 500 == 0:
                    if latency_measurements:
                        recent = latency_measurements[-10:]
                        avg = sum(recent) / len(recent)
                        mn = min(recent)
                        mx = max(recent)
                        print(f"  MIDI: {midi_count} | Audio: {audio_count} | "
                              f"Latency: {avg:.0f}ms avg ({mn:.0f}-{mx:.0f}ms)   ",
                              end="\r")

    except KeyboardInterrupt:
        pass
    finally:
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_term)
        print("\n\nShutting down...")
        running = False
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
