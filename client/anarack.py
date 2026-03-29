"""
Anarack — Native Client with Rev2 Panel

Audio/MIDI engine in a separate process. GUI shows Rev2 controls with
draggable knobs and MIDI learn support.

Usage:
    python anarack.py

Requirements:
    pip install mido python-rtmidi pyaudio
"""

import math
import multiprocessing as mp
import socket
import struct
import sys
import time
import tkinter as tk
from tkinter import ttk


# ──────────────────────────────────────────────────────────────
# Rev2 MIDI CC Map (from official MIDI implementation chart)
# ──────────────────────────────────────────────────────────────
REV2_PARAMS = {
    "osc1_shape":    {"cc": 22,  "label": "Shape",    "group": "OSC 1",     "default": 0},
    "osc1_level":    {"cc": 26,  "label": "Level",    "group": "OSC 1",     "default": 100},
    "osc2_shape":    {"cc": 23,  "label": "Shape",    "group": "OSC 2",     "default": 0},
    "osc2_level":    {"cc": 27,  "label": "Level",    "group": "OSC 2",     "default": 100},
    "osc_mix":       {"cc": 28,  "label": "Mix",      "group": "MIXER",     "default": 64},
    "noise":         {"cc": 29,  "label": "Noise",    "group": "MIXER",     "default": 0},
    "filter_cutoff": {"cc": 102, "label": "Cutoff",   "group": "FILTER",    "default": 100},
    "filter_reso":   {"cc": 103, "label": "Reso",     "group": "FILTER",    "default": 0},
    "filter_env":    {"cc": 104, "label": "Env Amt",  "group": "FILTER",    "default": 64},
    "filt_attack":   {"cc": 105, "label": "Attack",   "group": "FILT ENV",  "default": 0},
    "filt_decay":    {"cc": 106, "label": "Decay",    "group": "FILT ENV",  "default": 64},
    "filt_sustain":  {"cc": 107, "label": "Sustain",  "group": "FILT ENV",  "default": 64},
    "filt_release":  {"cc": 108, "label": "Release",  "group": "FILT ENV",  "default": 40},
    "amp_attack":    {"cc": 118, "label": "Attack",   "group": "AMP ENV",   "default": 0},
    "amp_decay":     {"cc": 119, "label": "Decay",    "group": "AMP ENV",   "default": 64},
    "amp_sustain":   {"cc": 75,  "label": "Sustain",  "group": "AMP ENV",   "default": 100},
    "amp_release":   {"cc": 76,  "label": "Release",  "group": "AMP ENV",   "default": 40},
}

# Group ordering for layout
GROUP_ORDER = ["OSC 1", "OSC 2", "MIXER", "FILTER", "FILT ENV", "AMP ENV"]


# ──────────────────────────────────────────────────────────────
# Audio/MIDI Engine (separate process)
# ──────────────────────────────────────────────────────────────
def audio_midi_engine(server_ip, midi_port_name, audio_port, midi_udp_port,
                      buffer_frames, stats, control, cc_queue):
    """Runs in a separate process — zero GUI interference."""
    import mido
    import pyaudio
    import threading

    SAMPLE_RATE = 48000

    try:
        midi_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        server_addr = (server_ip, midi_udp_port)

        audio_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        audio_sock.bind(("0.0.0.0", audio_port))
        audio_sock.settimeout(0.1)

        pa = pyaudio.PyAudio()
        stream = pa.open(
            format=pyaudio.paInt16, channels=1, rate=SAMPLE_RATE,
            output=True, frames_per_buffer=buffer_frames,
        )

        midi_in = mido.open_input(midi_port_name)
        midi_sock.sendto(bytes([0xFE]), server_addr)
        stats["status"] = 1

        note_send_times = {}
        latency_sum = 0.0
        latency_count = 0
        latency_min = 999.0
        latency_max = 0.0

        def audio_loop():
            nonlocal latency_sum, latency_count, latency_min, latency_max
            while control["running"]:
                try:
                    data, addr = audio_sock.recvfrom(65536)
                    stream.write(data)
                    stats["audio_count"] += 1

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
                            note_send_times.clear()
                except socket.timeout:
                    continue
                except:
                    if control["running"]:
                        continue
                    break

        audio_thread = threading.Thread(target=audio_loop, daemon=True)
        audio_thread.start()

        while control["running"]:
            # Check for CC/program messages from the GUI
            while not cc_queue.empty():
                try:
                    cc_num, cc_val = cc_queue.get_nowait()
                    if cc_num == -1:
                        # Program change
                        midi_sock.sendto(bytes([0xC0, cc_val]), server_addr)
                    else:
                        midi_sock.sendto(bytes([0xB0, cc_num, cc_val]), server_addr)
                    stats["midi_count"] += 1
                except:
                    break

            # Read MIDI input
            msg = midi_in.poll()
            if msg is not None:
                raw = msg.bytes()
                midi_sock.sendto(bytes(raw), server_addr)
                stats["midi_count"] += 1

                if msg.type == "note_on" and msg.velocity > 0:
                    note_send_times[msg.note] = time.perf_counter()

                # If MIDI learn is active, report CC messages back to GUI
                if control.get("learn", False) and msg.type == "control_change":
                    stats["learn_cc"] = msg.control
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


# ──────────────────────────────────────────────────────────────
# Custom Knob Widget
# ──────────────────────────────────────────────────────────────
class Knob(tk.Canvas):
    """A rotary knob control drawn on a Canvas."""

    KNOB_RADIUS = 20
    INDICATOR_LEN = 14
    START_ANGLE = 225  # degrees, 0=right, counter-clockwise
    END_ANGLE = -45
    RANGE = 270  # total sweep in degrees

    def __init__(self, parent, label="", cc=0, default=0, on_change=None,
                 on_learn=None, **kwargs):
        super().__init__(parent, width=56, height=72, bg="#1a1a1a",
                         highlightthickness=0, **kwargs)
        self.cc = cc
        self.value = default
        self.on_change = on_change
        self.on_learn = on_learn
        self.label_text = label
        self.learning = False
        self.mapped_cc = None  # CC from physical controller mapped to this knob

        self._drag_start_y = None
        self._drag_start_val = None

        # Draw
        self._draw_knob()

        # Events
        self.bind("<ButtonPress-1>", self._on_press)
        self.bind("<B1-Motion>", self._on_drag)
        self.bind("<ButtonRelease-1>", self._on_release)
        self.bind("<Double-Button-1>", self._on_double_click)
        self.bind("<MouseWheel>", self._on_scroll)

    def _draw_knob(self):
        self.delete("all")
        cx, cy = 28, 30
        r = self.KNOB_RADIUS

        # Track arc (background)
        self.create_arc(
            cx - r, cy - r, cx + r, cy + r,
            start=self.END_ANGLE, extent=self.RANGE,
            style=tk.ARC, outline="#333333", width=3
        )

        # Value arc
        value_extent = (self.value / 127) * self.RANGE
        self.create_arc(
            cx - r, cy - r, cx + r, cy + r,
            start=self.START_ANGLE, extent=-value_extent,
            style=tk.ARC, outline="#6366f1" if not self.learning else "#facc15", width=3
        )

        # Knob body
        kr = r - 5
        color = "#2a2a2a" if not self.learning else "#3a3520"
        self.create_oval(cx - kr, cy - kr, cx + kr, cy + kr, fill=color, outline="#444")

        # Indicator line
        angle_deg = self.START_ANGLE - (self.value / 127) * self.RANGE
        angle_rad = math.radians(angle_deg)
        ix = cx + math.cos(angle_rad) * self.INDICATOR_LEN
        iy = cy - math.sin(angle_rad) * self.INDICATOR_LEN
        self.create_line(cx, cy, ix, iy, fill="white", width=2)

        # Label
        self.create_text(cx, 62, text=self.label_text, fill="#888",
                         font=("Helvetica", 8))

        # Value text (shown on hover/drag)
        self.create_text(cx, cy, text=str(self.value), fill="#aaa",
                         font=("SF Mono", 7), tags="value_text")

        # MIDI learn indicator
        if self.mapped_cc is not None:
            self.create_text(cx, 5, text=f"CC{self.mapped_cc}", fill="#6366f1",
                             font=("SF Mono", 6))

    def set_value(self, val, send=True):
        self.value = max(0, min(127, int(val)))
        self._draw_knob()
        if send and self.on_change:
            self.on_change(self.cc, self.value)

    def _on_press(self, event):
        self._drag_start_y = event.y
        self._drag_start_val = self.value

    def _on_drag(self, event):
        if self._drag_start_y is None:
            return
        dy = self._drag_start_y - event.y
        new_val = self._drag_start_val + dy * 0.8
        self.set_value(new_val)

    def _on_release(self, event):
        self._drag_start_y = None

    def _on_double_click(self, event):
        if self.on_learn:
            self.learning = not self.learning
            self._draw_knob()
            self.on_learn(self, self.learning)

    def _on_scroll(self, event):
        delta = 1 if event.delta > 0 else -1
        self.set_value(self.value + delta)

    def set_learning(self, active):
        self.learning = active
        self._draw_knob()

    def set_mapped_cc(self, cc):
        self.mapped_cc = cc
        self._draw_knob()


# ──────────────────────────────────────────────────────────────
# Main Application
# ──────────────────────────────────────────────────────────────
class AnarackApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Anarack — Prophet Rev2")
        self.root.configure(bg="#0a0a0a")
        self.root.resizable(False, False)

        self.engine_process = None
        self.manager = mp.Manager()
        self.stats = self.manager.dict({
            "status": 0, "midi_count": 0, "audio_count": 0,
            "latency_avg": 0.0, "latency_min": 0.0, "latency_max": 0.0,
            "error": "", "learn_cc": -1,
        })
        self.control = self.manager.dict({"running": False, "learn": False})
        self.cc_queue = mp.Queue()

        self.knobs = {}  # param_name -> Knob widget
        self.learning_knob = None  # Currently learning knob
        self.cc_to_knob = {}  # CC number -> Knob (for incoming MIDI mapping)

        self.build_ui()
        self.refresh_midi_ports()
        self.update_ui()

        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def build_ui(self):
        bg = "#0a0a0a"
        dark = "#1a1a1a"

        # Header
        header = tk.Frame(self.root, bg="#111111", padx=12, pady=8)
        header.pack(fill=tk.X)

        tk.Label(header, text="ANARACK", font=("Helvetica", 14, "bold"),
                 bg="#111111", fg="#e0e0e0").pack(side=tk.LEFT)

        self.status_label = tk.Label(header, text="● Disconnected",
                                      font=("Helvetica", 10), bg="#111111", fg="#f87171")
        self.status_label.pack(side=tk.LEFT, padx=(12, 0))

        # Latency in header
        self.lat_label = tk.Label(header, text="", font=("SF Mono", 10, "bold"),
                                   bg="#111111", fg="#4ade80")
        self.lat_label.pack(side=tk.RIGHT)

        # Connection bar
        conn = tk.Frame(self.root, bg="#151515", padx=12, pady=6)
        conn.pack(fill=tk.X)

        tk.Label(conn, text="Server", font=("Helvetica", 9), bg="#151515",
                 fg="#666").pack(side=tk.LEFT)
        self.server_var = tk.StringVar(value="192.168.1.131")
        tk.Entry(conn, textvariable=self.server_var, font=("SF Mono", 10),
                 bg="#111", fg="#ddd", insertbackground="#ddd", relief=tk.FLAT,
                 width=14).pack(side=tk.LEFT, padx=(6, 12))

        tk.Label(conn, text="MIDI", font=("Helvetica", 9), bg="#151515",
                 fg="#666").pack(side=tk.LEFT)
        self.midi_var = tk.StringVar()
        self.midi_combo = ttk.Combobox(conn, textvariable=self.midi_var,
                                        font=("Helvetica", 9), width=22, state="readonly")
        self.midi_combo.pack(side=tk.LEFT, padx=(6, 12))

        self.connect_btn = tk.Button(conn, text="Connect", font=("Helvetica", 10, "bold"),
                                      bg="#6366f1", fg="white", relief=tk.FLAT,
                                      padx=16, pady=2, command=self.toggle_connection)
        self.connect_btn.pack(side=tk.RIGHT)

        # Synth panel
        panel = tk.Frame(self.root, bg=bg, padx=8, pady=8)
        panel.pack(fill=tk.BOTH, expand=True)

        # Build knob groups
        for col, group_name in enumerate(GROUP_ORDER):
            group_params = [(k, v) for k, v in REV2_PARAMS.items()
                           if v["group"] == group_name]

            frame = tk.Frame(panel, bg=dark, padx=6, pady=6)
            frame.grid(row=0, column=col, padx=3, pady=3, sticky="n")

            tk.Label(frame, text=group_name, font=("Helvetica", 8, "bold"),
                     bg=dark, fg="#6366f1").pack(pady=(0, 4))

            knobs_frame = tk.Frame(frame, bg=dark)
            knobs_frame.pack()

            for i, (param_name, param) in enumerate(group_params):
                knob = Knob(
                    knobs_frame,
                    label=param["label"],
                    cc=param["cc"],
                    default=param["default"],
                    on_change=self.on_knob_change,
                    on_learn=self.on_midi_learn,
                )
                row = i // 2
                kcol = i % 2
                knob.grid(row=row, column=kcol, padx=2, pady=2)
                self.knobs[param_name] = knob

        # Program change row
        prog_frame = tk.Frame(self.root, bg="#151515", padx=12, pady=6)
        prog_frame.pack(fill=tk.X)

        tk.Label(prog_frame, text="PROGRAM", font=("Helvetica", 8),
                 bg="#151515", fg="#666").pack(side=tk.LEFT)

        tk.Button(prog_frame, text="<", font=("SF Mono", 10), bg="#222", fg="#ddd",
                  relief=tk.FLAT, padx=6, command=lambda: self.change_program(-1)
                  ).pack(side=tk.LEFT, padx=(8, 2))

        self.prog_var = tk.StringVar(value="0")
        tk.Entry(prog_frame, textvariable=self.prog_var, font=("SF Mono", 10),
                 bg="#111", fg="#ddd", insertbackground="#ddd", relief=tk.FLAT,
                 width=4, justify=tk.CENTER).pack(side=tk.LEFT)

        tk.Button(prog_frame, text=">", font=("SF Mono", 10), bg="#222", fg="#ddd",
                  relief=tk.FLAT, padx=6, command=lambda: self.change_program(1)
                  ).pack(side=tk.LEFT, padx=(2, 12))

        # Bank
        tk.Label(prog_frame, text="BANK", font=("Helvetica", 8),
                 bg="#151515", fg="#666").pack(side=tk.LEFT, padx=(8, 0))
        self.bank_var = tk.StringVar(value="A")
        for i, letter in enumerate("ABCDEFGH"):
            tk.Radiobutton(prog_frame, text=letter, variable=self.bank_var,
                          value=letter, font=("SF Mono", 8), bg="#151515", fg="#999",
                          selectcolor="#333", indicatoron=0, padx=4, pady=1,
                          command=self.send_bank_program).pack(side=tk.LEFT)

        # Stats bar
        self.stats_bar = tk.Label(self.root, text="", font=("SF Mono", 8),
                                   bg="#0a0a0a", fg="#444", anchor="w", padx=12)
        self.stats_bar.pack(fill=tk.X, pady=(2, 4))

        # MIDI learn hint
        self.learn_hint = tk.Label(self.root, text="Double-click a knob to MIDI learn",
                                    font=("Helvetica", 8), bg="#0a0a0a", fg="#333")
        self.learn_hint.pack(pady=(0, 4))

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

    def on_knob_change(self, cc, value):
        """Called when a knob is dragged in the GUI."""
        if self.engine_process and self.engine_process.is_alive():
            self.cc_queue.put((cc, value))

    def on_midi_learn(self, knob, active):
        """Called when a knob is double-clicked for MIDI learn."""
        if active:
            # Cancel previous learn
            if self.learning_knob and self.learning_knob != knob:
                self.learning_knob.set_learning(False)
            self.learning_knob = knob
            self.learn_hint.config(text="Move a knob on your controller...", fg="#facc15")
            self.control["learn"] = True
            self.stats["learn_cc"] = -1
        else:
            self.learning_knob = None
            self.learn_hint.config(text="Double-click a knob to MIDI learn", fg="#333")
            self.control["learn"] = False

    def toggle_connection(self):
        if self.engine_process and self.engine_process.is_alive():
            self.disconnect()
        else:
            self.connect()

    def connect(self):
        server = self.server_var.get().strip()
        midi_port = self.midi_var.get()
        if not server or not midi_port:
            return

        for key in ["status", "midi_count", "audio_count", "latency_avg",
                     "latency_min", "latency_max", "error"]:
            self.stats[key] = 0 if key != "error" else ""
        self.stats["learn_cc"] = -1
        self.control["running"] = True

        self.engine_process = mp.Process(
            target=audio_midi_engine,
            args=(server, midi_port, 9999, 5555, 128,
                  self.stats, self.control, self.cc_queue),
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
        self.lat_label.config(text="")

    def change_program(self, delta):
        try:
            num = int(self.prog_var.get()) + delta
            num = max(0, min(127, num))
            self.prog_var.set(str(num))
            self.send_bank_program()
        except ValueError:
            pass

    def send_bank_program(self):
        if not (self.engine_process and self.engine_process.is_alive()):
            return
        bank = ord(self.bank_var.get()) - ord("A")
        try:
            prog = int(self.prog_var.get())
        except ValueError:
            prog = 0
        # Bank select MSB then program change — send as raw bytes via CC queue
        # CC 0 = bank select, then we need program change
        self.cc_queue.put((0, bank))  # Bank Select MSB
        # For program change we need to send it differently — add special handling
        self.cc_queue.put((-1, prog))  # -1 signals program change

    def update_ui(self):
        status = self.stats.get("status", 0)
        if status == 1:
            self.status_label.config(text="● Connected", fg="#4ade80")
        elif status == -1:
            self.status_label.config(text="● Error", fg="#f87171")

        avg = self.stats.get("latency_avg", 0)
        if avg > 0:
            color = "#4ade80" if avg < 20 else "#facc15" if avg < 40 else "#f87171"
            self.lat_label.config(text=f"{avg:.0f}ms", fg=color)

        mc = self.stats.get("midi_count", 0)
        ac = self.stats.get("audio_count", 0)
        mn = self.stats.get("latency_min", 0)
        mx = self.stats.get("latency_max", 0)
        self.stats_bar.config(
            text=f"MIDI: {mc}  |  Audio: {ac}  |  Latency: {mn:.0f}-{mx:.0f}ms"
        )

        # Check for MIDI learn
        learn_cc = self.stats.get("learn_cc", -1)
        if learn_cc >= 0 and self.learning_knob:
            knob = self.learning_knob
            # Remove old mapping if exists
            old_cc = knob.mapped_cc
            if old_cc is not None and old_cc in self.cc_to_knob:
                del self.cc_to_knob[old_cc]
            # Set new mapping
            knob.set_mapped_cc(learn_cc)
            knob.set_learning(False)
            self.cc_to_knob[learn_cc] = knob
            self.learning_knob = None
            self.learn_hint.config(
                text=f"Mapped CC {learn_cc} -> {knob.label_text}", fg="#4ade80"
            )
            self.control["learn"] = False
            self.stats["learn_cc"] = -1

        self.root.after(100, self.update_ui)

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
