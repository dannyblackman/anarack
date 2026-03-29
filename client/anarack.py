"""
Anarack — Native Client with Rev2 Panel

Audio/MIDI engine in a separate process. GUI shows full Rev2 controls
matching the front panel layout with draggable knobs, keyboard, and MIDI learn.

Usage:
    python anarack.py

Requirements:
    pip install mido python-rtmidi pyaudio
"""

import math
import multiprocessing as mp
import ctypes
import socket
import struct
import sys
import time
import tkinter as tk
from tkinter import ttk


# ──────────────────────────────────────────────────────────────
# Prophet Rev2 MIDI CC Map (from official MIDI implementation)
# Laid out to match the Rev2 front panel left → right
# ──────────────────────────────────────────────────────────────

# Each group in front-panel order
PANEL_LAYOUT = [
    {
        "name": "OSC 1",
        "params": [
            {"id": "osc1_freq",   "cc": 20, "label": "Freq",   "default": 64, "type": "knob"},
            {"id": "osc1_fine",   "cc": 21, "label": "Fine",   "default": 64, "type": "knob"},
            {"id": "osc1_shape",  "cc": 22, "label": "Shape",  "default": 0,  "type": "knob"},
            {"id": "osc1_glide",  "cc": 23, "label": "Glide",  "default": 0,  "type": "knob"},
            {"id": "osc1_shmod",  "cc": 30, "label": "Sh Mod", "default": 0,  "type": "knob"},
        ]
    },
    {
        "name": "OSC 2",
        "params": [
            {"id": "osc2_freq",   "cc": 24, "label": "Freq",   "default": 64, "type": "knob"},
            {"id": "osc2_fine",   "cc": 25, "label": "Fine",   "default": 64, "type": "knob"},
            {"id": "osc2_shape",  "cc": 26, "label": "Shape",  "default": 0,  "type": "knob"},
            {"id": "osc2_glide",  "cc": 27, "label": "Glide",  "default": 0,  "type": "knob"},
            {"id": "osc2_shmod",  "cc": 31, "label": "Sh Mod", "default": 0,  "type": "knob"},
        ]
    },
    {
        "name": "MIXER",
        "params": [
            {"id": "osc_mix",    "cc": 28, "label": "Osc Mix", "default": 64, "type": "knob"},
            {"id": "noise",      "cc": 29, "label": "Noise",   "default": 0,  "type": "knob"},
            {"id": "sub_osc",    "cc": 8,  "label": "Sub Osc", "default": 0,  "type": "knob"},
            {"id": "slop",       "cc": 9,  "label": "Slop",    "default": 0,  "type": "knob"},
        ]
    },
    {
        "name": "LOW-PASS FILTER",
        "params": [
            {"id": "filt_cutoff", "cc": 102, "label": "Cutoff",   "default": 100, "type": "knob"},
            {"id": "filt_reso",   "cc": 103, "label": "Reso",     "default": 0,   "type": "knob"},
            {"id": "filt_key",    "cc": 104, "label": "Key Amt",  "default": 0,   "type": "knob"},
            {"id": "filt_audio",  "cc": 105, "label": "Aud Mod",  "default": 0,   "type": "knob"},
            {"id": "filt_envamt", "cc": 106, "label": "Env Amt",  "default": 64,  "type": "knob"},
            {"id": "filt_envvel", "cc": 107, "label": "Env Vel",  "default": 0,   "type": "knob"},
        ]
    },
    {
        "name": "FILT ENV",
        "params": [
            {"id": "fe_delay",   "cc": 108, "label": "Delay",   "default": 0,  "type": "knob"},
            {"id": "fe_attack",  "cc": 109, "label": "Attack",  "default": 0,  "type": "knob"},
            {"id": "fe_decay",   "cc": 110, "label": "Decay",   "default": 64, "type": "knob"},
            {"id": "fe_sustain", "cc": 111, "label": "Sustain", "default": 64, "type": "knob"},
            {"id": "fe_release", "cc": 112, "label": "Release", "default": 40, "type": "knob"},
        ]
    },
    {
        "name": "AMP",
        "params": [
            {"id": "amp_level",  "cc": 113, "label": "Level",   "default": 100, "type": "knob"},
            {"id": "amp_envamt", "cc": 115, "label": "Env Amt", "default": 127, "type": "knob"},
            {"id": "amp_envvel", "cc": 116, "label": "Env Vel", "default": 0,   "type": "knob"},
            {"id": "pan",        "cc": 114, "label": "Pan",     "default": 64,  "type": "knob"},
        ]
    },
    {
        "name": "AMP ENV",
        "params": [
            {"id": "ae_delay",   "cc": 117, "label": "Delay",   "default": 0,   "type": "knob"},
            {"id": "ae_attack",  "cc": 118, "label": "Attack",  "default": 0,   "type": "knob"},
            {"id": "ae_decay",   "cc": 119, "label": "Decay",   "default": 64,  "type": "knob"},
            {"id": "ae_sustain", "cc": 75,  "label": "Sustain", "default": 100, "type": "knob"},
            {"id": "ae_release", "cc": 76,  "label": "Release", "default": 40,  "type": "knob"},
        ]
    },
    {
        "name": "ENV 3",
        "params": [
            {"id": "e3_dest",    "cc": 85, "label": "Dest",    "default": 0,  "type": "knob"},
            {"id": "e3_amount",  "cc": 86, "label": "Amount",  "default": 0,  "type": "knob"},
            {"id": "e3_attack",  "cc": 89, "label": "Attack",  "default": 0,  "type": "knob"},
            {"id": "e3_decay",   "cc": 90, "label": "Decay",   "default": 64, "type": "knob"},
            {"id": "e3_sustain", "cc": 77, "label": "Sustain", "default": 64, "type": "knob"},
            {"id": "e3_release", "cc": 78, "label": "Release", "default": 40, "type": "knob"},
        ]
    },
    {
        "name": "EFFECTS",
        "params": [
            {"id": "fx_type",   "cc": 3,  "label": "Type",    "default": 0,  "type": "select",
             "options": ["Off", "Delay", "Chorus", "Flanger", "Phaser", "Rotary",
                         "Ring Mod", "Distort", "HP Filter"]},
            {"id": "fx_onoff",  "cc": 16, "label": "On/Off",  "default": 0,  "type": "toggle"},
            {"id": "fx_mix",    "cc": 17, "label": "Mix",     "default": 0,  "type": "knob"},
            {"id": "fx_param1", "cc": 12, "label": "Param 1", "default": 64, "type": "knob"},
            {"id": "fx_param2", "cc": 13, "label": "Param 2", "default": 64, "type": "knob"},
        ]
    },
    {
        "name": "GLOBAL",
        "params": [
            {"id": "volume",    "cc": 37, "label": "Volume",  "default": 100, "type": "knob"},
            {"id": "master",    "cc": 7,  "label": "Master",  "default": 100, "type": "knob"},
            {"id": "bpm",       "cc": 14, "label": "BPM",     "default": 64,  "type": "knob"},
            {"id": "glide_on",  "cc": 65, "label": "Glide",   "default": 0,   "type": "toggle"},
        ]
    },
]


# ──────────────────────────────────────────────────────────────
# Audio/MIDI Engine (separate process)
# ──────────────────────────────────────────────────────────────
def audio_midi_engine(server_ip, midi_port_name, audio_port, midi_udp_port,
                      buffer_frames, stats, control, cc_queue,
                      learn_flag, learn_result):
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

        # Also open the DAW port if available (some controllers send knob CCs there)
        midi_in2 = None
        all_ports = mido.get_input_names()
        for p in all_ports:
            if p != midi_port_name and any(x in p.lower() for x in
                    [midi_port_name.split()[0].lower()]):
                try:
                    midi_in2 = mido.open_input(p)
                    break
                except:
                    pass

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
                                latency_min = min(latency_min, latency)
                                latency_max = max(latency_max, latency)
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

        threading.Thread(target=audio_loop, daemon=True).start()

        while control["running"]:
            # GUI messages
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

            # MIDI input — both ports
            got_msg = False
            for mi in ([midi_in, midi_in2] if midi_in2 else [midi_in]):
                msg = mi.poll()
                if msg is not None:
                    got_msg = True

                    if msg.type == "control_change":
                        # CCs never forwarded raw — GUI handles all CC routing
                        # via knob drag or MIDI learn mapping
                        stats["incoming_cc"] = msg.control
                        stats["incoming_val"] = msg.value

                        if learn_flag.value:
                            learn_result.value = msg.control
                            learn_flag.value = 0
                    else:
                        # Notes, pitch bend, etc. — forward directly
                        midi_sock.sendto(bytes(msg.bytes()), server_addr)
                        stats["midi_count"] += 1

                        if msg.type == "note_on" and msg.velocity > 0:
                            note_send_times[msg.note] = time.perf_counter()
                            stats["last_note_on"] = msg.note
                        elif msg.type == "note_off" or (msg.type == "note_on" and msg.velocity == 0):
                            stats["last_note_off"] = msg.note

            if not got_msg:
                time.sleep(0.0001)

    except Exception as e:
        stats["error"] = str(e)[:100]
        stats["status"] = -1
    finally:
        stats["status"] = 0
        try:
            midi_in.close()
            if midi_in2:
                midi_in2.close()
            stream.stop_stream()
            stream.close()
            audio_sock.close()
            midi_sock.close()
            pa.terminate()
        except:
            pass


# ──────────────────────────────────────────────────────────────
# Knob Widget
# ──────────────────────────────────────────────────────────────
class Knob(tk.Canvas):
    def __init__(self, parent, label="", cc=0, default=0, on_change=None,
                 on_learn=None, size=44, **kwargs):
        self.sz = size
        lh = 12
        super().__init__(parent, width=size, height=size + lh, bg="#1a1a1a",
                         highlightthickness=0, **kwargs)
        self.cc = cc
        self.value = default
        self.on_change = on_change
        self.on_learn = on_learn
        self.label_text = label
        self.learning = False
        self.mapped_cc = None
        self._dy = None
        self._dv = None
        self._draw()
        self.bind("<ButtonPress-1>", lambda e: self._set_drag(e))
        self.bind("<B1-Motion>", lambda e: self._do_drag(e))
        self.bind("<ButtonRelease-1>", lambda e: self._end_drag())
        self.bind("<Double-Button-1>", lambda e: self._toggle_learn())
        self.bind("<MouseWheel>", lambda e: self.set_value(self.value + (1 if e.delta > 0 else -1)))

    def _draw(self):
        self.delete("all")
        s = self.sz
        cx, cy = s // 2, s // 2
        r = s // 2 - 4

        self.create_arc(cx-r, cy-r, cx+r, cy+r, start=-45, extent=270,
                        style=tk.ARC, outline="#333", width=2)
        ext = (self.value / 127) * 270
        col = "#facc15" if self.learning else "#6366f1"
        self.create_arc(cx-r, cy-r, cx+r, cy+r, start=225, extent=-ext,
                        style=tk.ARC, outline=col, width=2)
        kr = r - 4
        self.create_oval(cx-kr, cy-kr, cx+kr, cy+kr,
                         fill="#333520" if self.learning else "#2a2a2a", outline="#444")
        ang = math.radians(225 - (self.value / 127) * 270)
        il = r - 6
        self.create_line(cx, cy, cx + math.cos(ang)*il, cy - math.sin(ang)*il,
                         fill="white", width=2)
        self.create_text(cx, cy, text=str(self.value), fill="#777", font=("SF Mono", 6))
        self.create_text(cx, s + 5, text=self.label_text, fill="#999", font=("Helvetica", 7))
        if self.mapped_cc is not None:
            self.create_text(cx, 3, text=f"CC{self.mapped_cc}", fill="#6366f1", font=("SF Mono", 5))

    def set_value(self, val, send=True):
        self.value = max(0, min(127, int(val)))
        self._draw()
        if send and self.on_change:
            self.on_change(self.cc, self.value)

    def _set_drag(self, e): self._dy, self._dv = e.y, self.value
    def _do_drag(self, e):
        if self._dy is not None:
            self.set_value(self._dv + (self._dy - e.y) * 0.8)
    def _end_drag(self): self._dy = None
    def _toggle_learn(self):
        if self.on_learn:
            self.learning = not self.learning
            self._draw()
            self.on_learn(self, self.learning)
    def set_learning(self, a): self.learning = a; self._draw()
    def set_mapped_cc(self, cc): self.mapped_cc = cc; self._draw()


# ──────────────────────────────────────────────────────────────
# Piano Keyboard
# ──────────────────────────────────────────────────────────────
class PianoKeyboard(tk.Canvas):
    WW = 18  # white key width
    WH = 60  # white key height
    BW = 11  # black key width
    BH = 36  # black key height
    BLACK = {1: -6, 3: -4, 6: -7, 8: -5, 10: -3}

    def __init__(self, parent, start=36, octaves=5, **kwargs):
        self.start = start
        self.octaves = octaves
        self.active = set()
        nw = octaves * 7 + 1
        super().__init__(parent, width=nw * self.WW + 2, height=self.WH + 2,
                         bg="#0a0a0a", highlightthickness=0, **kwargs)
        self._draw()

    def _draw(self):
        self.delete("all")
        whites = [0, 2, 4, 5, 7, 9, 11]
        x = 1
        # White keys
        for o in range(self.octaves):
            for w in whites:
                n = self.start + o * 12 + w
                f = "#6366f1" if n in self.active else "#d4d4d4"
                self.create_rectangle(x, 1, x + self.WW - 1, self.WH, fill=f, outline="#888")
                x += self.WW
        # Final C
        n = self.start + self.octaves * 12
        f = "#6366f1" if n in self.active else "#d4d4d4"
        self.create_rectangle(x, 1, x + self.WW - 1, self.WH, fill=f, outline="#888")

        # Black keys
        for o in range(self.octaves):
            for i in range(12):
                if i in self.BLACK:
                    n = self.start + o * 12 + i
                    wi = [0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6][i]
                    bx = 1 + (o * 7 + wi) * self.WW + self.WW + self.BLACK[i]
                    f = "#6366f1" if n in self.active else "#1a1a1a"
                    self.create_rectangle(bx, 1, bx + self.BW, self.BH, fill=f, outline="#000")

    def note_on(self, n):
        self.active.add(n)
        self._draw()

    def note_off(self, n):
        self.active.discard(n)
        self._draw()

    def clear(self):
        self.active.clear()
        self._draw()


# ──────────────────────────────────────────────────────────────
# Main App
# ──────────────────────────────────────────────────────────────
class AnarackApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Anarack — Prophet Rev2")
        self.root.configure(bg="#0a0a0a")
        self.root.resizable(False, False)

        self.engine = None
        self.mgr = mp.Manager()
        self.stats = self.mgr.dict({
            "status": 0, "midi_count": 0, "audio_count": 0,
            "latency_avg": 0.0, "latency_min": 0.0, "latency_max": 0.0,
            "error": "", "last_note_on": -1, "last_note_off": -1,
            "incoming_cc": -1, "incoming_val": 0,
        })
        self.control = self.mgr.dict({"running": False})
        self.cc_queue = mp.Queue()

        # Reliable IPC for MIDI learn
        self.learn_flag = mp.Value(ctypes.c_int, 0)
        self.learn_result = mp.Value(ctypes.c_int, -1)

        self.knobs = {}
        self.learning_knob = None
        self.cc_to_knob = {}

        self.build_ui()
        self.refresh_midi()
        self.tick()
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def build_ui(self):
        bg = "#0a0a0a"
        bar = "#131313"

        # ── Header ──
        h = tk.Frame(self.root, bg="#111", padx=10, pady=5)
        h.pack(fill=tk.X)
        tk.Label(h, text="ANARACK", font=("Helvetica", 13, "bold"), bg="#111", fg="#e0e0e0").pack(side=tk.LEFT)
        tk.Label(h, text="Prophet Rev2", font=("Helvetica", 10), bg="#111", fg="#555").pack(side=tk.LEFT, padx=8)
        self.lat_lbl = tk.Label(h, text="", font=("SF Mono", 11, "bold"), bg="#111", fg="#4ade80")
        self.lat_lbl.pack(side=tk.RIGHT)
        self.status_lbl = tk.Label(h, text="Disconnected", font=("Helvetica", 9), bg="#111", fg="#f87171")
        self.status_lbl.pack(side=tk.RIGHT, padx=(0, 12))

        # ── Connection ──
        c = tk.Frame(self.root, bg=bar, padx=10, pady=4)
        c.pack(fill=tk.X)
        tk.Label(c, text="Server", font=("Helvetica", 9), bg=bar, fg="#777").pack(side=tk.LEFT)
        self.sv = tk.StringVar(value="192.168.1.131")
        tk.Entry(c, textvariable=self.sv, font=("SF Mono", 10), bg="#222", fg="#eee",
                 insertbackground="#eee", relief=tk.FLAT, width=14).pack(side=tk.LEFT, padx=(6, 12))
        tk.Label(c, text="MIDI", font=("Helvetica", 9), bg=bar, fg="#777").pack(side=tk.LEFT)
        self.mv = tk.StringVar()
        self.mc = ttk.Combobox(c, textvariable=self.mv, font=("Helvetica", 9), width=24, state="readonly")
        self.mc.pack(side=tk.LEFT, padx=(6, 12))
        self.cbtn = tk.Button(c, text="Connect", font=("Helvetica", 10, "bold"),
                               bg="#6366f1", fg="white", activebackground="#4f46e5",
                               activeforeground="white", relief=tk.FLAT, padx=14, pady=2,
                               command=self.toggle)
        self.cbtn.pack(side=tk.RIGHT)

        # ── Synth Panel — single row matching Rev2 left→right ──
        panel = tk.Frame(self.root, bg=bg, padx=2, pady=4)
        panel.pack(fill=tk.X)

        for col, group in enumerate(PANEL_LAYOUT):
            self._build_group(panel, group, col)

        # ── Program / Bank ──
        pf = tk.Frame(self.root, bg=bar, padx=10, pady=4)
        pf.pack(fill=tk.X)

        tk.Label(pf, text="PROGRAM", font=("Helvetica", 8, "bold"), bg=bar, fg="#777").pack(side=tk.LEFT)
        tk.Button(pf, text=" < ", font=("SF Mono", 12, "bold"), bg="#444", fg="white",
                  activebackground="#666", activeforeground="white", relief=tk.FLAT,
                  padx=8, pady=3, command=lambda: self.prog(-1)).pack(side=tk.LEFT, padx=(8, 2))
        self.pv = tk.StringVar(value="0")
        tk.Entry(pf, textvariable=self.pv, font=("SF Mono", 10), bg="#222", fg="#eee",
                 insertbackground="#eee", relief=tk.FLAT, width=4, justify=tk.CENTER).pack(side=tk.LEFT)
        tk.Button(pf, text=" > ", font=("SF Mono", 12, "bold"), bg="#444", fg="white",
                  activebackground="#666", activeforeground="white", relief=tk.FLAT,
                  padx=8, pady=3, command=lambda: self.prog(1)).pack(side=tk.LEFT, padx=(2, 16))

        tk.Label(pf, text="BANK", font=("Helvetica", 8, "bold"), bg=bar, fg="#777").pack(side=tk.LEFT, padx=(0, 6))
        self.bv = tk.StringVar(value="A")
        for l in "ABCDEFGH":
            tk.Radiobutton(pf, text=l, variable=self.bv, value=l, font=("SF Mono", 10, "bold"),
                          bg="#444", fg="white", selectcolor="#6366f1", activebackground="#555",
                          activeforeground="white", indicatoron=0, padx=6, pady=2,
                          relief=tk.FLAT, command=self.send_prog).pack(side=tk.LEFT, padx=1)

        # ── Keyboard ──
        kf = tk.Frame(self.root, bg=bg, pady=4)
        kf.pack()
        self.kbd = PianoKeyboard(kf, start=36, octaves=5)
        self.kbd.pack()

        # ── Footer ──
        ff = tk.Frame(self.root, bg=bg, padx=10, pady=3)
        ff.pack(fill=tk.X)
        self.stat_lbl = tk.Label(ff, text="", font=("SF Mono", 8), bg=bg, fg="#444")
        self.stat_lbl.pack(side=tk.LEFT)
        self.learn_lbl = tk.Label(ff, text="Double-click knob to MIDI learn", font=("Helvetica", 8), bg=bg, fg="#444")
        self.learn_lbl.pack(side=tk.RIGHT)

    def _build_group(self, parent, group, col):
        dark = "#1a1a1a"
        f = tk.Frame(parent, bg=dark, padx=3, pady=3)
        f.grid(row=0, column=col, padx=1, pady=1, sticky="n")

        tk.Label(f, text=group["name"], font=("Helvetica", 6, "bold"),
                 bg=dark, fg="#6366f1").pack(pady=(0, 2))

        gf = tk.Frame(f, bg=dark)
        gf.pack()

        cols = 2 if len(group["params"]) > 3 else min(len(group["params"]), 3)

        for i, p in enumerate(group["params"]):
            if p["type"] == "knob":
                w = Knob(gf, label=p["label"], cc=p["cc"], default=p["default"],
                         on_change=self.on_cc, on_learn=self.on_learn, size=40)
                w.grid(row=i // cols, column=i % cols, padx=1, pady=1)
                self.knobs[p["id"]] = w

            elif p["type"] == "select":
                sf = tk.Frame(gf, bg=dark)
                sf.grid(row=i // cols, column=i % cols, padx=1, pady=1)
                tk.Label(sf, text=p["label"], font=("Helvetica", 6), bg=dark, fg="#999").pack()
                var = tk.StringVar(value=p["options"][0])
                om = ttk.Combobox(sf, textvariable=var, values=p["options"],
                                   font=("Helvetica", 7), width=8, state="readonly")
                om.pack()
                cc = p["cc"]
                opts = p["options"]
                def on_select(event, cc=cc, var=var, opts=opts):
                    idx = opts.index(var.get()) if var.get() in opts else 0
                    val = int(idx * 127 / max(len(opts) - 1, 1))
                    self.on_cc(cc, val)
                om.bind("<<ComboboxSelected>>", on_select)

            elif p["type"] == "toggle":
                tf = tk.Frame(gf, bg=dark)
                tf.grid(row=i // cols, column=i % cols, padx=1, pady=1)
                var = tk.IntVar(value=p["default"])
                cc = p["cc"]
                def on_toggle(cc=cc, var=var):
                    self.on_cc(cc, 127 if var.get() else 0)
                cb = tk.Checkbutton(tf, text=p["label"], variable=var,
                                    font=("Helvetica", 7), bg=dark, fg="#999",
                                    selectcolor="#333", activebackground=dark,
                                    activeforeground="#eee", command=lambda: on_toggle())
                cb.pack()

    def refresh_midi(self):
        import mido
        ports = mido.get_input_names()
        self.mc["values"] = ports
        if ports:
            for i, p in enumerate(ports):
                if "launch" in p.lower() or "key" in p.lower():
                    self.mc.current(i)
                    return
            self.mc.current(0)

    def on_cc(self, cc, val):
        if self.engine and self.engine.is_alive():
            self.cc_queue.put((cc, val))
            self.learn_lbl.config(text=f"Sent CC {cc} = {val}", fg="#666")

    def on_learn(self, knob, active):
        if active:
            if self.learning_knob and self.learning_knob != knob:
                self.learning_knob.set_learning(False)
            self.learning_knob = knob
            self.learn_lbl.config(text="Twiddle a knob on your controller...", fg="#facc15")
            self.learn_result.value = -1
            self.learn_flag.value = 1
        else:
            self.learning_knob = None
            self.learn_lbl.config(text="Double-click knob to MIDI learn", fg="#444")
            self.learn_flag.value = 0

    def toggle(self):
        if self.engine and self.engine.is_alive():
            self.disconnect()
        else:
            self.connect()

    def connect(self):
        server = self.sv.get().strip()
        midi = self.mv.get()
        if not server or not midi:
            return

        for k in ["status", "midi_count", "audio_count", "latency_avg", "latency_min", "latency_max"]:
            self.stats[k] = 0
        self.stats["error"] = ""
        self.stats["last_note_on"] = -1
        self.stats["last_note_off"] = -1
        self.control["running"] = True
        self.learn_flag.value = 0
        self.learn_result.value = -1

        self.engine = mp.Process(
            target=audio_midi_engine,
            args=(server, midi, 9999, 5555, 128,
                  self.stats, self.control, self.cc_queue,
                  self.learn_flag, self.learn_result),
            daemon=True,
        )
        self.engine.start()
        self.cbtn.config(text="Disconnect", bg="#dc2626", fg="white", activebackground="#b91c1c")

    def disconnect(self):
        self.control["running"] = False
        if self.engine:
            self.engine.join(timeout=2)
            if self.engine.is_alive():
                self.engine.terminate()
            self.engine = None
        self.status_lbl.config(text="Disconnected", fg="#f87171")
        self.cbtn.config(text="Connect", bg="#6366f1", fg="white", activebackground="#4f46e5")
        self.lat_lbl.config(text="")
        self.kbd.clear()

    def prog(self, d):
        try:
            n = max(0, min(127, int(self.pv.get()) + d))
            self.pv.set(str(n))
            self.send_prog()
        except ValueError:
            pass

    def send_prog(self):
        if not (self.engine and self.engine.is_alive()):
            return
        b = ord(self.bv.get()) - 65
        try:
            p = int(self.pv.get())
        except ValueError:
            p = 0
        self.cc_queue.put((0, b))
        self.cc_queue.put((-1, p))

    def tick(self):
        st = self.stats.get("status", 0)
        if st == 1:
            self.status_lbl.config(text="Connected", fg="#4ade80")
        elif st == -1:
            self.status_lbl.config(text="Error", fg="#f87171")

        avg = self.stats.get("latency_avg", 0)
        if avg > 0:
            c = "#4ade80" if avg < 20 else "#facc15" if avg < 40 else "#f87171"
            self.lat_lbl.config(text=f"{avg:.0f}ms", fg=c)

        mn = self.stats.get("latency_min", 0)
        mx = self.stats.get("latency_max", 0)
        self.stat_lbl.config(
            text=f"MIDI: {self.stats.get('midi_count',0)}  |  Audio: {self.stats.get('audio_count',0)}  |  {mn:.0f}-{mx:.0f}ms"
        )

        # Keyboard
        non = self.stats.get("last_note_on", -1)
        noff = self.stats.get("last_note_off", -1)
        if non >= 0:
            self.kbd.note_on(non)
            self.stats["last_note_on"] = -1
        if noff >= 0:
            self.kbd.note_off(noff)
            self.stats["last_note_off"] = -1

        # Incoming CC → update mapped knobs
        inc_cc = self.stats.get("incoming_cc", -1)
        if inc_cc >= 0 and inc_cc in self.cc_to_knob:
            val = self.stats.get("incoming_val", 0)
            knob = self.cc_to_knob[inc_cc]
            knob.set_value(val, send=True)  # Updates knob AND sends the synth's CC
            self.stats["incoming_cc"] = -1

        # MIDI learn — check Value (reliable)
        if self.learning_knob and self.learn_result.value >= 0:
            cc = self.learn_result.value
            knob = self.learning_knob
            old = knob.mapped_cc
            if old is not None and old in self.cc_to_knob:
                del self.cc_to_knob[old]
            knob.set_mapped_cc(cc)
            knob.set_learning(False)
            self.cc_to_knob[cc] = knob
            self.learning_knob = None
            self.learn_lbl.config(text=f"Mapped CC{cc} → {knob.label_text}", fg="#4ade80")
            self.learn_result.value = -1

        self.root.after(80, self.tick)

    def on_close(self):
        self.disconnect()
        self.root.destroy()


def main():
    mp.set_start_method("spawn", force=True)
    root = tk.Tk()
    AnarackApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
