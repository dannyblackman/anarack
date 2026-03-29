"""
Anarack — Native Client with GUI

Audio/MIDI engine runs in a separate process so the GUI can't affect latency.

Usage:
    python anarack.py

Requirements:
    pip install mido python-rtmidi pyaudio
"""

import multiprocessing as mp
import socket
import struct
import sys
import time
import tkinter as tk
from tkinter import ttk


def audio_midi_engine(server_ip, midi_port_name, audio_port, midi_udp_port,
                      buffer_frames, stats, control):
    """Runs in a separate process — zero GUI interference."""
    import mido
    import pyaudio
    import threading

    SAMPLE_RATE = 48000

    try:
        # MIDI setup
        midi_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        server_addr = (server_ip, midi_udp_port)

        # Audio setup
        audio_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        audio_sock.bind(("0.0.0.0", audio_port))
        audio_sock.settimeout(0.1)

        pa = pyaudio.PyAudio()
        stream = pa.open(
            format=pyaudio.paInt16,
            channels=1,
            rate=SAMPLE_RATE,
            output=True,
            frames_per_buffer=buffer_frames,
        )

        midi_in = mido.open_input(midi_port_name)

        # Send registration packet
        midi_sock.sendto(bytes([0xFE]), server_addr)

        stats["status"] = 1  # Connected

        note_send_times = {}
        latency_sum = 0.0
        latency_count = 0
        latency_min = 999.0
        latency_max = 0.0

        # Audio thread — highest priority
        def audio_loop():
            nonlocal latency_sum, latency_count, latency_min, latency_max
            while control["running"]:
                try:
                    data, addr = audio_sock.recvfrom(65536)
                    stream.write(data)
                    stats["audio_count"] += 1

                    # Latency detection
                    if note_send_times and len(data) >= 4:
                        samples = struct.unpack(f"<{len(data)//2}h", data)
                        peak = max(abs(s) for s in samples)
                        if peak > 300 and note_send_times:
                            earliest = min(note_send_times.values())
                            latency = (time.perf_counter() - earliest) * 1000
                            if latency < 200:
                                latency_sum += latency
                                latency_count += 1
                                if latency < latency_min:
                                    latency_min = latency
                                if latency > latency_max:
                                    latency_max = latency
                                stats["latency_avg"] = latency_sum / latency_count
                                stats["latency_min"] = latency_min
                                stats["latency_max"] = latency_max
                                stats["latency_last"] = latency
                            note_send_times.clear()

                except socket.timeout:
                    continue
                except:
                    if control["running"]:
                        continue
                    break

        audio_thread = threading.Thread(target=audio_loop, daemon=True)
        audio_thread.start()

        # MIDI loop — tight polling in main thread of this process
        while control["running"]:
            msg = midi_in.poll()
            if msg is not None:
                raw = msg.bytes()
                midi_sock.sendto(bytes(raw), server_addr)
                stats["midi_count"] += 1

                if msg.type == "note_on" and msg.velocity > 0:
                    note_send_times[msg.note] = time.perf_counter()
            else:
                time.sleep(0.0001)

    except Exception as e:
        stats["error"] = str(e)[:100]
        stats["status"] = -1
    finally:
        stats["status"] = 0
        try:
            midi_in.close()
            stream.stop_stream()
            stream.close()
            audio_sock.close()
            midi_sock.close()
            pa.terminate()
        except:
            pass


class AnarackApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Anarack")
        self.root.configure(bg="#0a0a0a")
        self.root.geometry("420x500")
        self.root.resizable(False, False)

        self.engine_process = None
        self.manager = mp.Manager()
        self.stats = self.manager.dict({
            "status": 0,
            "midi_count": 0,
            "audio_count": 0,
            "latency_avg": 0.0,
            "latency_min": 0.0,
            "latency_max": 0.0,
            "latency_last": 0.0,
            "error": "",
        })
        self.control = self.manager.dict({"running": False})

        self.build_ui()
        self.refresh_midi_ports()
        self.update_ui()

        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def build_ui(self):
        bg = "#0a0a0a"
        fg = "#e0e0e0"
        dark = "#1a1a1a"
        accent = "#6366f1"

        # Title
        tk.Label(
            self.root, text="Anarack", font=("Helvetica", 22, "bold"),
            bg=bg, fg=fg
        ).pack(pady=(20, 0))

        tk.Label(
            self.root, text="Remote Synth Studio", font=("Helvetica", 10),
            bg=bg, fg="#555555"
        ).pack(pady=(0, 16))

        # Status
        self.status_label = tk.Label(
            self.root, text="● Disconnected", font=("Helvetica", 11),
            bg=bg, fg="#f87171"
        )
        self.status_label.pack(pady=(0, 12))

        # Connection panel
        panel = tk.Frame(self.root, bg=dark, padx=16, pady=12)
        panel.pack(fill=tk.X, padx=20, pady=(0, 8))

        # Server
        r1 = tk.Frame(panel, bg=dark)
        r1.pack(fill=tk.X, pady=3)
        tk.Label(r1, text="Server", font=("Helvetica", 10), bg=dark, fg="#888", width=7, anchor="e").pack(side=tk.LEFT)
        self.server_var = tk.StringVar(value="192.168.1.131")
        tk.Entry(r1, textvariable=self.server_var, font=("SF Mono", 11), bg="#111", fg=fg,
                 insertbackground=fg, relief=tk.FLAT, width=16).pack(side=tk.LEFT, padx=(8, 0))

        # MIDI
        r2 = tk.Frame(panel, bg=dark)
        r2.pack(fill=tk.X, pady=3)
        tk.Label(r2, text="MIDI In", font=("Helvetica", 10), bg=dark, fg="#888", width=7, anchor="e").pack(side=tk.LEFT)
        self.midi_var = tk.StringVar()
        self.midi_combo = ttk.Combobox(r2, textvariable=self.midi_var, font=("Helvetica", 10),
                                        width=20, state="readonly")
        self.midi_combo.pack(side=tk.LEFT, padx=(8, 0))

        # Connect button
        self.connect_btn = tk.Button(
            panel, text="Connect", font=("Helvetica", 12, "bold"),
            bg=accent, fg="white", relief=tk.FLAT, padx=24, pady=6,
            command=self.toggle_connection
        )
        self.connect_btn.pack(pady=(8, 2))

        # Latency display
        lat_panel = tk.Frame(self.root, bg=dark, padx=16, pady=16)
        lat_panel.pack(fill=tk.X, padx=20, pady=(0, 8))

        tk.Label(lat_panel, text="ROUND-TRIP LATENCY", font=("Helvetica", 8),
                 bg=dark, fg="#555").pack()

        self.lat_num = tk.Label(lat_panel, text="--", font=("SF Mono", 44, "bold"),
                                bg=dark, fg="#4ade80")
        self.lat_num.pack()

        tk.Label(lat_panel, text="milliseconds", font=("Helvetica", 10),
                 bg=dark, fg="#555").pack()

        self.lat_range = tk.Label(lat_panel, text="", font=("SF Mono", 9),
                                  bg=dark, fg="#555")
        self.lat_range.pack(pady=(4, 0))

        # Stats
        self.stats_label = tk.Label(self.root, text="", font=("SF Mono", 9),
                                     bg=bg, fg="#444")
        self.stats_label.pack(pady=4)

        # Error
        self.error_label = tk.Label(self.root, text="", font=("Helvetica", 9),
                                     bg=bg, fg="#f87171", wraplength=380)
        self.error_label.pack()

        # Footer
        tk.Label(self.root, text="Phase 0 Prototype", font=("Helvetica", 8),
                 bg=bg, fg="#333").pack(side=tk.BOTTOM, pady=8)

    def refresh_midi_ports(self):
        import mido
        ports = mido.get_input_names()
        self.midi_combo["values"] = ports
        if ports:
            for i, p in enumerate(ports):
                if "launch" in p.lower() or "key" in p.lower():
                    self.midi_combo.current(i)
                    return
            self.midi_combo.current(0)

    def toggle_connection(self):
        if self.engine_process and self.engine_process.is_alive():
            self.disconnect()
        else:
            self.connect()

    def connect(self):
        server = self.server_var.get().strip()
        midi_port = self.midi_var.get()

        if not server or not midi_port:
            self.error_label.config(text="Enter server IP and select MIDI port")
            return

        self.error_label.config(text="")

        # Reset stats
        self.stats["status"] = 0
        self.stats["midi_count"] = 0
        self.stats["audio_count"] = 0
        self.stats["latency_avg"] = 0.0
        self.stats["latency_min"] = 0.0
        self.stats["latency_max"] = 0.0
        self.stats["latency_last"] = 0.0
        self.stats["error"] = ""
        self.control["running"] = True

        self.engine_process = mp.Process(
            target=audio_midi_engine,
            args=(server, midi_port, 9999, 5555, 128, self.stats, self.control),
            daemon=True,
        )
        self.engine_process.start()

        self.connect_btn.config(text="Disconnect", bg="#ef4444")

    def disconnect(self):
        self.control["running"] = False
        if self.engine_process:
            self.engine_process.join(timeout=2)
            if self.engine_process.is_alive():
                self.engine_process.terminate()
            self.engine_process = None

        self.status_label.config(text="● Disconnected", fg="#f87171")
        self.connect_btn.config(text="Connect", bg="#6366f1")

    def update_ui(self):
        status = self.stats.get("status", 0)

        if status == 1:
            self.status_label.config(text="● Connected", fg="#4ade80")
        elif status == -1:
            self.status_label.config(text="● Error", fg="#f87171")
            self.error_label.config(text=self.stats.get("error", ""))
        elif self.engine_process and self.engine_process.is_alive():
            self.status_label.config(text="● Connecting...", fg="#facc15")

        avg = self.stats.get("latency_avg", 0)
        mn = self.stats.get("latency_min", 0)
        mx = self.stats.get("latency_max", 0)

        if avg > 0:
            self.lat_num.config(text=f"{avg:.0f}")
            color = "#4ade80" if avg < 20 else "#facc15" if avg < 40 else "#f87171"
            self.lat_num.config(fg=color)
            self.lat_range.config(text=f"min {mn:.0f}ms  /  max {mx:.0f}ms")

        midi_c = self.stats.get("midi_count", 0)
        audio_c = self.stats.get("audio_count", 0)
        self.stats_label.config(text=f"MIDI: {midi_c}  |  Audio: {audio_c}")

        self.root.after(150, self.update_ui)

    def on_close(self):
        self.disconnect()
        self.root.destroy()


def main():
    mp.set_start_method("spawn", force=True)
    root = tk.Tk()
    app = AnarackApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
