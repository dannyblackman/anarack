"""
Anarack — Native Client with GUI

Low-latency remote synth control.
MIDI in via your controller, audio back via UDP, played through CoreAudio.

Usage:
    python anarack.py

Requirements:
    pip install mido python-rtmidi pyaudio
"""

import socket
import struct
import sys
import threading
import time
import tkinter as tk
from tkinter import ttk

import mido
import pyaudio

SAMPLE_RATE = 48000
CHANNELS = 1
FORMAT = pyaudio.paInt16
AUDIO_PORT = 9999
MIDI_PORT = 5555
BUFFER_FRAMES = 128


class AnarackApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Anarack")
        self.root.configure(bg="#0a0a0a")
        self.root.geometry("480x520")
        self.root.resizable(False, False)

        self.connected = False
        self.running = True
        self.midi_in = None
        self.midi_thread = None
        self.audio_thread = None
        self.audio_sock = None
        self.midi_sock = None
        self.pa = None
        self.stream = None

        self.midi_count = 0
        self.audio_count = 0
        self.latency_measurements = []
        self.note_send_times = {}

        self.build_ui()
        self.refresh_midi_ports()

        # Poll for UI updates
        self.update_stats()

        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def build_ui(self):
        style = {
            "bg": "#0a0a0a",
            "fg": "#e0e0e0",
            "font": ("Helvetica", 12),
            "font_small": ("Helvetica", 10),
            "font_mono": ("SF Mono", 11),
            "accent": "#6366f1",
            "green": "#4ade80",
            "red": "#f87171",
            "yellow": "#facc15",
            "dark": "#1a1a1a",
            "border": "#333333",
        }

        # Title
        tk.Label(
            self.root, text="Anarack", font=("Helvetica", 24, "bold"),
            bg=style["bg"], fg=style["fg"]
        ).pack(pady=(20, 2))

        tk.Label(
            self.root, text="Remote Synth Studio", font=style["font_small"],
            bg=style["bg"], fg="#666666"
        ).pack(pady=(0, 20))

        # Status indicator
        self.status_frame = tk.Frame(self.root, bg=style["bg"])
        self.status_frame.pack(pady=(0, 15))

        self.status_dot = tk.Canvas(
            self.status_frame, width=12, height=12,
            bg=style["bg"], highlightthickness=0
        )
        self.status_dot.pack(side=tk.LEFT, padx=(0, 8))
        self.status_dot_id = self.status_dot.create_oval(2, 2, 10, 10, fill=style["red"])

        self.status_label = tk.Label(
            self.status_frame, text="Disconnected", font=style["font_small"],
            bg=style["bg"], fg=style["red"]
        )
        self.status_label.pack(side=tk.LEFT)

        # Connection frame
        conn_frame = tk.Frame(self.root, bg=style["dark"], padx=20, pady=15)
        conn_frame.pack(fill=tk.X, padx=20, pady=(0, 10))

        # Server IP
        row1 = tk.Frame(conn_frame, bg=style["dark"])
        row1.pack(fill=tk.X, pady=(0, 8))

        tk.Label(
            row1, text="Server:", font=style["font_small"],
            bg=style["dark"], fg="#999999", width=8, anchor="e"
        ).pack(side=tk.LEFT)

        self.server_var = tk.StringVar(value="192.168.1.131")
        self.server_entry = tk.Entry(
            row1, textvariable=self.server_var, font=style["font_mono"],
            bg="#111111", fg=style["fg"], insertbackground=style["fg"],
            relief=tk.FLAT, width=18
        )
        self.server_entry.pack(side=tk.LEFT, padx=(8, 0))

        # MIDI Port
        row2 = tk.Frame(conn_frame, bg=style["dark"])
        row2.pack(fill=tk.X, pady=(0, 8))

        tk.Label(
            row2, text="MIDI In:", font=style["font_small"],
            bg=style["dark"], fg="#999999", width=8, anchor="e"
        ).pack(side=tk.LEFT)

        self.midi_port_var = tk.StringVar()
        self.midi_combo = ttk.Combobox(
            row2, textvariable=self.midi_port_var,
            font=style["font_small"], width=22, state="readonly"
        )
        self.midi_combo.pack(side=tk.LEFT, padx=(8, 0))

        refresh_btn = tk.Button(
            row2, text="↻", font=style["font_small"],
            bg=style["dark"], fg=style["fg"], relief=tk.FLAT,
            command=self.refresh_midi_ports
        )
        refresh_btn.pack(side=tk.LEFT, padx=(4, 0))

        # Connect button
        self.connect_btn = tk.Button(
            conn_frame, text="Connect", font=("Helvetica", 13, "bold"),
            bg=style["accent"], fg="white", relief=tk.FLAT,
            padx=30, pady=8, command=self.toggle_connection
        )
        self.connect_btn.pack(pady=(5, 0))

        # Latency display
        latency_frame = tk.Frame(self.root, bg=style["dark"], padx=20, pady=15)
        latency_frame.pack(fill=tk.X, padx=20, pady=(0, 10))

        tk.Label(
            latency_frame, text="LATENCY", font=("Helvetica", 9),
            bg=style["dark"], fg="#666666"
        ).pack()

        self.latency_label = tk.Label(
            latency_frame, text="--", font=("SF Mono", 36, "bold"),
            bg=style["dark"], fg=style["green"]
        )
        self.latency_label.pack()

        self.latency_unit = tk.Label(
            latency_frame, text="ms round-trip", font=style["font_small"],
            bg=style["dark"], fg="#666666"
        )
        self.latency_unit.pack()

        self.latency_range = tk.Label(
            latency_frame, text="", font=("SF Mono", 10),
            bg=style["dark"], fg="#666666"
        )
        self.latency_range.pack(pady=(4, 0))

        # Stats
        stats_frame = tk.Frame(self.root, bg=style["dark"], padx=20, pady=10)
        stats_frame.pack(fill=tk.X, padx=20, pady=(0, 10))

        self.stats_label = tk.Label(
            stats_frame, text="MIDI: 0 | Audio: 0", font=("SF Mono", 10),
            bg=style["dark"], fg="#666666"
        )
        self.stats_label.pack()

        # Footer
        tk.Label(
            self.root, text="Phase 0 Prototype — LAN / Tailscale",
            font=("Helvetica", 9), bg=style["bg"], fg="#444444"
        ).pack(side=tk.BOTTOM, pady=10)

    def refresh_midi_ports(self):
        ports = mido.get_input_names()
        self.midi_combo["values"] = ports
        if ports:
            # Try to auto-select a LaunchKey or similar
            for i, p in enumerate(ports):
                if "launch" in p.lower() or "key" in p.lower():
                    self.midi_combo.current(i)
                    return
            self.midi_combo.current(0)

    def toggle_connection(self):
        if self.connected:
            self.disconnect()
        else:
            self.connect()

    def connect(self):
        server = self.server_var.get().strip()
        midi_port = self.midi_port_var.get()

        if not server:
            self.set_status("No server IP", "#f87171")
            return
        if not midi_port:
            self.set_status("No MIDI port selected", "#f87171")
            return

        try:
            # Audio setup
            self.audio_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.audio_sock.bind(("0.0.0.0", AUDIO_PORT))
            self.audio_sock.settimeout(0.1)

            self.pa = pyaudio.PyAudio()
            self.stream = self.pa.open(
                format=FORMAT,
                channels=CHANNELS,
                rate=SAMPLE_RATE,
                output=True,
                frames_per_buffer=BUFFER_FRAMES,
            )

            # MIDI setup
            self.midi_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.server_addr = (server, MIDI_PORT)

            self.midi_in = mido.open_input(midi_port)

            # Reset stats
            self.midi_count = 0
            self.audio_count = 0
            self.latency_measurements = []
            self.note_send_times = {}

            self.connected = True

            # Send registration packet
            self.midi_sock.sendto(bytes([0xFE]), self.server_addr)

            # Start threads
            self.audio_thread = threading.Thread(target=self._audio_loop, daemon=True)
            self.audio_thread.start()

            self.midi_thread = threading.Thread(target=self._midi_loop, daemon=True)
            self.midi_thread.start()

            self.set_status("Connected", "#4ade80")
            self.connect_btn.config(text="Disconnect", bg="#ef4444")
            self.server_entry.config(state="disabled")

        except Exception as e:
            self.set_status(f"Error: {e}", "#f87171")
            self.disconnect()

    def disconnect(self):
        self.connected = False

        if self.midi_in:
            try:
                self.midi_in.close()
            except:
                pass
            self.midi_in = None

        if self.stream:
            try:
                self.stream.stop_stream()
                self.stream.close()
            except:
                pass
            self.stream = None

        if self.pa:
            try:
                self.pa.terminate()
            except:
                pass
            self.pa = None

        if self.audio_sock:
            try:
                self.audio_sock.close()
            except:
                pass
            self.audio_sock = None

        if self.midi_sock:
            try:
                self.midi_sock.close()
            except:
                pass
            self.midi_sock = None

        self.set_status("Disconnected", "#f87171")
        self.connect_btn.config(text="Connect", bg="#6366f1")
        self.server_entry.config(state="normal")

    def _midi_loop(self):
        while self.connected:
            try:
                for msg in self.midi_in.iter_pending():
                    raw = msg.bytes()
                    self.midi_sock.sendto(bytes(raw), self.server_addr)
                    self.midi_count += 1

                    if msg.type == "note_on" and msg.velocity > 0:
                        self.note_send_times[msg.note] = time.perf_counter()

                time.sleep(0.001)
            except:
                if self.connected:
                    continue
                break

    def _audio_loop(self):
        while self.connected:
            try:
                data, addr = self.audio_sock.recvfrom(65536)
                self.stream.write(data)
                self.audio_count += 1

                # Latency detection
                if self.note_send_times and len(data) >= 4:
                    samples = struct.unpack(f"<{len(data)//2}h", data)
                    peak = max(abs(s) for s in samples)
                    if peak > 300 and self.note_send_times:
                        earliest = min(self.note_send_times.values())
                        latency = (time.perf_counter() - earliest) * 1000
                        if latency < 200:
                            self.latency_measurements.append(latency)
                        self.note_send_times.clear()

            except socket.timeout:
                continue
            except:
                if self.connected:
                    continue
                break

    def set_status(self, text, color):
        self.status_label.config(text=text, fg=color)
        self.status_dot.itemconfig(self.status_dot_id, fill=color)

    def update_stats(self):
        if self.latency_measurements:
            recent = self.latency_measurements[-20:]
            avg = sum(recent) / len(recent)
            mn = min(recent)
            mx = max(recent)
            self.latency_label.config(text=f"{avg:.0f}")

            color = "#4ade80" if avg < 20 else "#facc15" if avg < 40 else "#f87171"
            self.latency_label.config(fg=color)

            self.latency_range.config(text=f"min {mn:.0f}ms / max {mx:.0f}ms")

        self.stats_label.config(
            text=f"MIDI: {self.midi_count} | Audio: {self.audio_count}"
        )

        self.root.after(200, self.update_stats)

    def on_close(self):
        self.disconnect()
        self.running = False
        self.root.destroy()


def main():
    root = tk.Tk()
    app = AnarackApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
