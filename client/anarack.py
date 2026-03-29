"""
Anarack — Native Client with Rev2 Panel

Audio/MIDI engine in a separate process. GUI shows full Rev2 controls
with draggable knobs, keyboard, and MIDI learn.

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
# Prophet Rev2 MIDI CC Map (complete, from official MIDI chart)
# ──────────────────────────────────────────────────────────────
REV2_PARAMS = {
    # OSC 1
    "osc1_freq":     {"cc": 20,  "label": "Freq",      "group": "OSC 1",     "default": 64},
    "osc1_fine":     {"cc": 21,  "label": "Fine",      "group": "OSC 1",     "default": 64},
    "osc1_shape":    {"cc": 22,  "label": "Shape",     "group": "OSC 1",     "default": 0},
    "osc1_glide":    {"cc": 23,  "label": "Glide",     "group": "OSC 1",     "default": 0},
    # OSC 2
    "osc2_freq":     {"cc": 24,  "label": "Freq",      "group": "OSC 2",     "default": 64},
    "osc2_fine":     {"cc": 25,  "label": "Fine",      "group": "OSC 2",     "default": 64},
    "osc2_shape":    {"cc": 26,  "label": "Shape",     "group": "OSC 2",     "default": 0},
    "osc2_glide":    {"cc": 27,  "label": "Glide",     "group": "OSC 2",     "default": 0},
    # MIXER
    "osc_mix":       {"cc": 28,  "label": "Mix",       "group": "MIXER",     "default": 64},
    "noise":         {"cc": 29,  "label": "Noise",     "group": "MIXER",     "default": 0},
    "sub_osc":       {"cc": 8,   "label": "Sub Osc",   "group": "MIXER",     "default": 0},
    "slop":          {"cc": 9,   "label": "Slop",      "group": "MIXER",     "default": 0},
    # OSC MOD
    "osc1_shmod":    {"cc": 30,  "label": "O1 ShMod",  "group": "OSC MOD",   "default": 0},
    "osc2_shmod":    {"cc": 31,  "label": "O2 ShMod",  "group": "OSC MOD",   "default": 0},
    # FILTER
    "filter_cutoff": {"cc": 102, "label": "Cutoff",    "group": "FILTER",    "default": 100},
    "filter_reso":   {"cc": 103, "label": "Reso",      "group": "FILTER",    "default": 0},
    "filter_key":    {"cc": 104, "label": "Key Amt",   "group": "FILTER",    "default": 0},
    "filter_audio":  {"cc": 105, "label": "Audio Mod", "group": "FILTER",    "default": 0},
    "filter_envamt": {"cc": 106, "label": "Env Amt",   "group": "FILTER",    "default": 64},
    "filter_envvel": {"cc": 107, "label": "Env Vel",   "group": "FILTER",    "default": 0},
    # FILTER ENV
    "filt_delay":    {"cc": 108, "label": "Delay",     "group": "FILT ENV",  "default": 0},
    "filt_attack":   {"cc": 109, "label": "Attack",    "group": "FILT ENV",  "default": 0},
    "filt_decay":    {"cc": 110, "label": "Decay",     "group": "FILT ENV",  "default": 64},
    "filt_sustain":  {"cc": 111, "label": "Sustain",   "group": "FILT ENV",  "default": 64},
    "filt_release":  {"cc": 112, "label": "Release",   "group": "FILT ENV",  "default": 40},
    # AMP
    "amp_level":     {"cc": 113, "label": "Level",     "group": "AMP",       "default": 100},
    "amp_envamt":    {"cc": 115, "label": "Env Amt",   "group": "AMP",       "default": 127},
    "amp_envvel":    {"cc": 116, "label": "Env Vel",   "group": "AMP",       "default": 0},
    "pan_spread":    {"cc": 114, "label": "Pan",       "group": "AMP",       "default": 64},
    # AMP ENV
    "amp_delay":     {"cc": 117, "label": "Delay",     "group": "AMP ENV",   "default": 0},
    "amp_attack":    {"cc": 118, "label": "Attack",    "group": "AMP ENV",   "default": 0},
    "amp_decay":     {"cc": 119, "label": "Decay",     "group": "AMP ENV",   "default": 64},
    "amp_sustain":   {"cc": 75,  "label": "Sustain",   "group": "AMP ENV",   "default": 100},
    "amp_release":   {"cc": 76,  "label": "Release",   "group": "AMP ENV",   "default": 40},
    # ENV 3
    "env3_dest":     {"cc": 85,  "label": "Dest",      "group": "ENV 3",     "default": 0},
    "env3_amount":   {"cc": 86,  "label": "Amount",    "group": "ENV 3",     "default": 0},
    "env3_attack":   {"cc": 89,  "label": "Attack",    "group": "ENV 3",     "default": 0},
    "env3_decay":    {"cc": 90,  "label": "Decay",     "group": "ENV 3",     "default": 64},
    "env3_sustain":  {"cc": 77,  "label": "Sustain",   "group": "ENV 3",     "default": 64},
    "env3_release":  {"cc": 78,  "label": "Release",   "group": "ENV 3",     "default": 40},
    # EFFECTS
    "fx_type":       {"cc": 3,   "label": "Type",      "group": "EFFECTS",   "default": 0},
    "fx_mix":        {"cc": 17,  "label": "Mix",       "group": "EFFECTS",   "default": 0},
    "fx_param1":     {"cc": 12,  "label": "Param 1",   "group": "EFFECTS",   "default": 64},
    "fx_param2":     {"cc": 13,  "label": "Param 2",   "group": "EFFECTS",   "default": 64},
    "fx_onoff":      {"cc": 16,  "label": "On/Off",    "group": "EFFECTS",   "default": 0},
}

# Layout: two rows of groups
ROW1_GROUPS = ["OSC 1", "OSC 2", "MIXER", "OSC MOD", "FILTER"]
ROW2_GROUPS = ["FILT ENV", "AMP", "AMP ENV", "ENV 3", "EFFECTS"]


# ──────────────────────────────────────────────────────────────
# Audio/MIDI Engine (separate process)
# ──────────────────────────────────────────────────────────────
def audio_midi_engine(server_ip, midi_port_name, audio_port, midi_udp_port,
                      buffer_frames, stats, control, cc_queue):
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
            # GUI CC/program messages
            while not cc_queue.empty():
                try:
                    cc_num, cc_val = cc_queue.get_nowait()
                    if cc_num == -1:
                        midi_sock.sendto(bytes([0xC0, cc_val]), server_addr)
                    else:
                        midi_sock.sendto(bytes([0xB0, cc_num, cc_val]), server_addr)
                    stats["midi_count"] += 1
                except:
                    break

            # MIDI input
            msg = midi_in.poll()
            if msg is not None:
                raw = msg.bytes()
                midi_sock.sendto(bytes(raw), server_addr)
                stats["midi_count"] += 1

                if msg.type == "note_on" and msg.velocity > 0:
                    note_send_times[msg.note] = time.perf_counter()
                    stats["last_note_on"] = msg.note
                elif msg.type == "note_off" or (msg.type == "note_on" and msg.velocity == 0):
                    stats["last_note_off"] = msg.note

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
    R = 18
    SWEEP = 270
    START = 225

    def __init__(self, parent, label="", cc=0, default=0, on_change=None,
                 on_learn=None, **kwargs):
        super().__init__(parent, width=48, height=64, bg="#1a1a1a",
                         highlightthickness=0, **kwargs)
        self.cc = cc
        self.value = default
        self.on_change = on_change
        self.on_learn = on_learn
        self.label_text = label
        self.learning = False
        self.mapped_cc = None
        self._drag_y = None
        self._drag_val = None

        self._draw()
        self.bind("<ButtonPress-1>", self._press)
        self.bind("<B1-Motion>", self._drag)
        self.bind("<ButtonRelease-1>", self._release)
        self.bind("<Double-Button-1>", self._dbl)
        self.bind("<MouseWheel>", self._scroll)

    def _draw(self):
        self.delete("all")
        cx, cy = 24, 26
        r = self.R

        # Track
        self.create_arc(cx-r, cy-r, cx+r, cy+r, start=-45, extent=270,
                        style=tk.ARC, outline="#333", width=2)
        # Value arc
        ext = (self.value / 127) * self.SWEEP
        col = "#facc15" if self.learning else "#6366f1"
        self.create_arc(cx-r, cy-r, cx+r, cy+r, start=self.START, extent=-ext,
                        style=tk.ARC, outline=col, width=2)
        # Body
        kr = r - 4
        self.create_oval(cx-kr, cy-kr, cx+kr, cy+kr,
                         fill="#333520" if self.learning else "#2a2a2a", outline="#444")
        # Indicator
        ang = math.radians(self.START - (self.value / 127) * self.SWEEP)
        self.create_line(cx, cy, cx + math.cos(ang)*12, cy - math.sin(ang)*12,
                         fill="white", width=2)
        # Value
        self.create_text(cx, cy, text=str(self.value), fill="#888", font=("SF Mono", 6))
        # Label
        self.create_text(cx, 55, text=self.label_text, fill="#999", font=("Helvetica", 7))
        # Mapped CC
        if self.mapped_cc is not None:
            self.create_text(cx, 4, text=f"CC{self.mapped_cc}", fill="#6366f1",
                             font=("SF Mono", 5))

    def set_value(self, val, send=True):
        self.value = max(0, min(127, int(val)))
        self._draw()
        if send and self.on_change:
            self.on_change(self.cc, self.value)

    def _press(self, e): self._drag_y, self._drag_val = e.y, self.value
    def _drag(self, e):
        if self._drag_y is None: return
        self.set_value(self._drag_val + (self._drag_y - e.y) * 0.8)
    def _release(self, e): self._drag_y = None
    def _dbl(self, e):
        if self.on_learn:
            self.learning = not self.learning
            self._draw()
            self.on_learn(self, self.learning)
    def _scroll(self, e): self.set_value(self.value + (1 if e.delta > 0 else -1))
    def set_learning(self, a): self.learning = a; self._draw()
    def set_mapped_cc(self, cc): self.mapped_cc = cc; self._draw()


# ──────────────────────────────────────────────────────────────
# Keyboard Widget
# ──────────────────────────────────────────────────────────────
class PianoKeyboard(tk.Canvas):
    """Visual keyboard showing active notes."""

    WHITE_W = 14
    WHITE_H = 50
    BLACK_W = 9
    BLACK_H = 30
    BLACK_OFFSETS = {1: -5, 3: -3, 6: -6, 8: -4, 10: -2}  # relative to white key

    def __init__(self, parent, start_note=36, num_octaves=4, **kwargs):
        self.start_note = start_note
        self.num_octaves = num_octaves
        self.active_notes = set()

        # Calculate width
        num_whites = num_octaves * 7 + 1  # +1 for final C
        w = num_whites * self.WHITE_W + 2
        super().__init__(parent, width=w, height=self.WHITE_H + 2, bg="#0a0a0a",
                         highlightthickness=0, **kwargs)
        self._draw()

    def _draw(self):
        self.delete("all")
        note = self.start_note
        x = 1
        white_positions = {}

        # Draw white keys first
        for octave in range(self.num_octaves):
            for i in range(7):  # 7 white keys per octave
                white_notes = [0, 2, 4, 5, 7, 9, 11]
                midi_note = self.start_note + octave * 12 + white_notes[i]
                active = midi_note in self.active_notes
                fill = "#6366f1" if active else "#d0d0d0"
                self.create_rectangle(x, 1, x + self.WHITE_W - 1, self.WHITE_H,
                                       fill=fill, outline="#555")
                white_positions[midi_note] = x
                x += self.WHITE_W

        # Final C
        final_c = self.start_note + self.num_octaves * 12
        active = final_c in self.active_notes
        fill = "#6366f1" if active else "#d0d0d0"
        self.create_rectangle(x, 1, x + self.WHITE_W - 1, self.WHITE_H,
                               fill=fill, outline="#555")

        # Draw black keys on top
        x = 1
        for octave in range(self.num_octaves):
            for i in range(12):
                if i in self.BLACK_OFFSETS:
                    midi_note = self.start_note + octave * 12 + i
                    # Find position relative to the white key
                    white_idx = [0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6][i]
                    bx = 1 + (octave * 7 + white_idx) * self.WHITE_W + self.WHITE_W + self.BLACK_OFFSETS[i]
                    active = midi_note in self.active_notes
                    fill = "#6366f1" if active else "#222222"
                    self.create_rectangle(bx, 1, bx + self.BLACK_W, self.BLACK_H,
                                           fill=fill, outline="#111")

    def note_on(self, note):
        self.active_notes.add(note)
        self._draw()

    def note_off(self, note):
        self.active_notes.discard(note)
        self._draw()

    def clear(self):
        self.active_notes.clear()
        self._draw()


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
            "last_note_on": -1, "last_note_off": -1,
        })
        self.control = self.manager.dict({"running": False, "learn": False})
        self.cc_queue = mp.Queue()

        self.knobs = {}
        self.learning_knob = None
        self.cc_to_knob = {}

        self.build_ui()
        self.refresh_midi_ports()
        self.update_ui()
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def build_ui(self):
        bg = "#0a0a0a"
        dark = "#1a1a1a"
        bar_bg = "#131313"

        # ── Header ──
        header = tk.Frame(self.root, bg="#111", padx=10, pady=6)
        header.pack(fill=tk.X)

        tk.Label(header, text="ANARACK", font=("Helvetica", 13, "bold"),
                 bg="#111", fg="#e0e0e0").pack(side=tk.LEFT)

        tk.Label(header, text="Prophet Rev2", font=("Helvetica", 10),
                 bg="#111", fg="#555").pack(side=tk.LEFT, padx=(8, 0))

        self.lat_label = tk.Label(header, text="", font=("SF Mono", 11, "bold"),
                                   bg="#111", fg="#4ade80")
        self.lat_label.pack(side=tk.RIGHT)

        self.status_label = tk.Label(header, text="Disconnected",
                                      font=("Helvetica", 9), bg="#111", fg="#f87171")
        self.status_label.pack(side=tk.RIGHT, padx=(0, 12))

        # ── Connection bar ──
        conn = tk.Frame(self.root, bg=bar_bg, padx=10, pady=5)
        conn.pack(fill=tk.X)

        tk.Label(conn, text="Server", font=("Helvetica", 9), bg=bar_bg, fg="#777").pack(side=tk.LEFT)
        self.server_var = tk.StringVar(value="192.168.1.131")
        tk.Entry(conn, textvariable=self.server_var, font=("SF Mono", 10),
                 bg="#222", fg="#eee", insertbackground="#eee", relief=tk.FLAT,
                 width=14).pack(side=tk.LEFT, padx=(6, 14))

        tk.Label(conn, text="MIDI", font=("Helvetica", 9), bg=bar_bg, fg="#777").pack(side=tk.LEFT)
        self.midi_var = tk.StringVar()
        self.midi_combo = ttk.Combobox(conn, textvariable=self.midi_var,
                                        font=("Helvetica", 9), width=24, state="readonly")
        self.midi_combo.pack(side=tk.LEFT, padx=(6, 14))

        self.connect_btn = tk.Button(
            conn, text="Connect", font=("Helvetica", 10, "bold"),
            bg="#6366f1", fg="white", activebackground="#4f46e5", activeforeground="white",
            relief=tk.FLAT, padx=16, pady=3, command=self.toggle_connection
        )
        self.connect_btn.pack(side=tk.RIGHT)

        # ── Synth Panel ──
        panel = tk.Frame(self.root, bg=bg, padx=4, pady=6)
        panel.pack(fill=tk.BOTH)

        # Row 1
        row1 = tk.Frame(panel, bg=bg)
        row1.pack(fill=tk.X, pady=(0, 2))
        for col, group_name in enumerate(ROW1_GROUPS):
            self._build_group(row1, group_name, col)

        # Row 2
        row2 = tk.Frame(panel, bg=bg)
        row2.pack(fill=tk.X, pady=(0, 2))
        for col, group_name in enumerate(ROW2_GROUPS):
            self._build_group(row2, group_name, col)

        # ── Program / Bank ──
        prog_frame = tk.Frame(self.root, bg=bar_bg, padx=10, pady=5)
        prog_frame.pack(fill=tk.X)

        tk.Label(prog_frame, text="PROGRAM", font=("Helvetica", 8, "bold"),
                 bg=bar_bg, fg="#777").pack(side=tk.LEFT)

        tk.Button(prog_frame, text=" < ", font=("Helvetica", 10),
                  bg="#333", fg="#eee", activebackground="#555", activeforeground="white",
                  relief=tk.FLAT, command=lambda: self.change_program(-1)
                  ).pack(side=tk.LEFT, padx=(8, 2))

        self.prog_var = tk.StringVar(value="0")
        tk.Entry(prog_frame, textvariable=self.prog_var, font=("SF Mono", 10),
                 bg="#222", fg="#eee", insertbackground="#eee", relief=tk.FLAT,
                 width=4, justify=tk.CENTER).pack(side=tk.LEFT)

        tk.Button(prog_frame, text=" > ", font=("Helvetica", 10),
                  bg="#333", fg="#eee", activebackground="#555", activeforeground="white",
                  relief=tk.FLAT, command=lambda: self.change_program(1)
                  ).pack(side=tk.LEFT, padx=(2, 16))

        tk.Label(prog_frame, text="BANK", font=("Helvetica", 8, "bold"),
                 bg=bar_bg, fg="#777").pack(side=tk.LEFT, padx=(0, 6))

        self.bank_var = tk.StringVar(value="A")
        for letter in "ABCDEFGH":
            tk.Radiobutton(prog_frame, text=letter, variable=self.bank_var,
                          value=letter, font=("SF Mono", 9), bg=bar_bg, fg="#ccc",
                          selectcolor="#6366f1", activebackground=bar_bg,
                          activeforeground="white",
                          indicatoron=0, padx=5, pady=1, relief=tk.FLAT,
                          command=self.send_bank_program).pack(side=tk.LEFT, padx=1)

        # ── Keyboard ──
        kb_frame = tk.Frame(self.root, bg=bg, pady=4)
        kb_frame.pack(fill=tk.X)

        self.keyboard = PianoKeyboard(kb_frame, start_note=36, num_octaves=4)
        self.keyboard.pack()

        # ── Status bar ──
        status_bar = tk.Frame(self.root, bg=bg, padx=10, pady=3)
        status_bar.pack(fill=tk.X)

        self.stats_label = tk.Label(status_bar, text="", font=("SF Mono", 8),
                                     bg=bg, fg="#444", anchor="w")
        self.stats_label.pack(side=tk.LEFT)

        self.learn_hint = tk.Label(status_bar, text="Double-click knob to MIDI learn",
                                    font=("Helvetica", 8), bg=bg, fg="#444")
        self.learn_hint.pack(side=tk.RIGHT)

    def _build_group(self, parent, group_name, col):
        """Build a group of knobs for a synth section."""
        dark = "#1a1a1a"
        params = [(k, v) for k, v in REV2_PARAMS.items() if v["group"] == group_name]
        if not params:
            return

        frame = tk.Frame(parent, bg=dark, padx=4, pady=4)
        frame.grid(row=0, column=col, padx=2, pady=1, sticky="n")

        tk.Label(frame, text=group_name, font=("Helvetica", 7, "bold"),
                 bg=dark, fg="#6366f1").pack(pady=(0, 2))

        knobs_frame = tk.Frame(frame, bg=dark)
        knobs_frame.pack()

        # Max 2 columns per group
        for i, (param_name, param) in enumerate(params):
            knob = Knob(
                knobs_frame, label=param["label"], cc=param["cc"],
                default=param["default"],
                on_change=self.on_knob_change, on_learn=self.on_midi_learn,
            )
            knob.grid(row=i // 2, column=i % 2, padx=1, pady=1)
            self.knobs[param_name] = knob

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
        if self.engine_process and self.engine_process.is_alive():
            self.cc_queue.put((cc, value))

    def on_midi_learn(self, knob, active):
        if active:
            if self.learning_knob and self.learning_knob != knob:
                self.learning_knob.set_learning(False)
            self.learning_knob = knob
            self.learn_hint.config(text="Twiddle a knob on your controller...", fg="#facc15")
            self.control["learn"] = True
            self.stats["learn_cc"] = -1
        else:
            self.learning_knob = None
            self.learn_hint.config(text="Double-click knob to MIDI learn", fg="#444")
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
                     "latency_min", "latency_max"]:
            self.stats[key] = 0
        self.stats["error"] = ""
        self.stats["learn_cc"] = -1
        self.stats["last_note_on"] = -1
        self.stats["last_note_off"] = -1
        self.control["running"] = True
        self.control["learn"] = False

        self.engine_process = mp.Process(
            target=audio_midi_engine,
            args=(server, midi_port, 9999, 5555, 128,
                  self.stats, self.control, self.cc_queue),
            daemon=True,
        )
        self.engine_process.start()
        self.connect_btn.config(text="Disconnect", bg="#ef4444",
                                 activebackground="#dc2626")

    def disconnect(self):
        self.control["running"] = False
        if self.engine_process:
            self.engine_process.join(timeout=2)
            if self.engine_process.is_alive():
                self.engine_process.terminate()
            self.engine_process = None
        self.status_label.config(text="Disconnected", fg="#f87171")
        self.connect_btn.config(text="Connect", bg="#6366f1",
                                 activebackground="#4f46e5")
        self.lat_label.config(text="")
        self.keyboard.clear()

    def change_program(self, delta):
        try:
            num = max(0, min(127, int(self.prog_var.get()) + delta))
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
        self.cc_queue.put((0, bank))
        self.cc_queue.put((-1, prog))

    def update_ui(self):
        status = self.stats.get("status", 0)
        if status == 1:
            self.status_label.config(text="Connected", fg="#4ade80")
        elif status == -1:
            self.status_label.config(text="Error", fg="#f87171")

        avg = self.stats.get("latency_avg", 0)
        if avg > 0:
            color = "#4ade80" if avg < 20 else "#facc15" if avg < 40 else "#f87171"
            self.lat_label.config(text=f"{avg:.0f}ms", fg=color)

        mn = self.stats.get("latency_min", 0)
        mx = self.stats.get("latency_max", 0)
        mc = self.stats.get("midi_count", 0)
        ac = self.stats.get("audio_count", 0)
        self.stats_label.config(text=f"MIDI: {mc}  |  Audio: {ac}  |  {mn:.0f}-{mx:.0f}ms")

        # Keyboard note display
        note_on = self.stats.get("last_note_on", -1)
        note_off = self.stats.get("last_note_off", -1)
        if note_on >= 0:
            self.keyboard.note_on(note_on)
            self.stats["last_note_on"] = -1
        if note_off >= 0:
            self.keyboard.note_off(note_off)
            self.stats["last_note_off"] = -1

        # MIDI learn
        learn_cc = self.stats.get("learn_cc", -1)
        if learn_cc >= 0 and self.learning_knob:
            knob = self.learning_knob
            old = knob.mapped_cc
            if old is not None and old in self.cc_to_knob:
                del self.cc_to_knob[old]
            knob.set_mapped_cc(learn_cc)
            knob.set_learning(False)
            self.cc_to_knob[learn_cc] = knob
            self.learning_knob = None
            self.learn_hint.config(text=f"Mapped CC{learn_cc} -> {knob.label_text}", fg="#4ade80")
            self.control["learn"] = False
            self.stats["learn_cc"] = -1

        self.root.after(80, self.update_ui)

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
