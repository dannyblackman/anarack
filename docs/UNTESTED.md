# Untested features — verify when hardware is back online

This is a log of plugin/server features that have been built but not yet tested
against the actual Pi/Rev2/Scarlett rig. Work through this list once everything
is powered back on. Tick items off as they're verified.

## v0.4.1 (build 67) — End-to-end latency test
- [ ] Click "Test Latency" button in the panel header runs a 5x test
- [ ] Latency value updates with the average from 5 tests
- [ ] Hover the latency pill shows min/max in tooltip
- [ ] Log shows "Latency: avg X ms | min Y ms | max Z ms"
- [ ] Threshold (`LATENCY_PEAK_THRESHOLD = 800`) actually catches a C4 vel 127 attack
  — if not, lower the threshold
- [ ] Test from LAN → expect ~80-100ms
- [ ] Test from WG → expect ~200-250ms

## v0.4.7 (build 73) — Fix false "connected" state
- [ ] When Pi is off, plugin shows "Connecting" forever (not "Live")
- [ ] When Pi turns on mid-connect, status flips to "Live" once audio arrives

## v0.4.8 (build 74) — Offline state
- [ ] After 15s with no audio, status becomes "Offline" and Connect button reappears
- [ ] Click Connect from Offline state successfully retries

## v0.4.9 (build 75) — Auto-fit screen
- [ ] On a 16" MacBook (1728px usable), editor opens at ~1693px wide
- [ ] On a larger external display, editor opens at ~98% of that screen

## v0.4.11 (build 77) — Latch automation
- [ ] In Logic with Latch mode active, moving a knob records automation
- [ ] Each CC change creates a discrete keyframe (begin/end gesture per change)

## v0.5.x — Dropdowns
- [ ] FX Type dropdown sends correct CC 3 values (Off, Delay Mono, ... HP Filter)
- [ ] Arp Mode dropdown sends correct CC 34 values
- [ ] Aux Env Destination dropdown sends correct CC 85 values
- [ ] LFO Destination dropdown sends correct NRPN per active LFO (40/45/50/55)
- [ ] Sequencer Type dropdown sends CC 19 (Gated/Poly)
- [ ] Modulation Source/Destination dropdowns send the right NRPN per slot

## v0.6.0 (build 82) — Mod matrix + Unison + Layer + FX Clk Sync
- [ ] **Mod Matrix slot 1**: change Source → hear effect on synth
- [ ] **Mod Matrix slot 1**: change Destination → hear effect on synth
- [ ] **Mod Matrix slot 1**: change Amount (bipolar around 127) → hear effect
- [ ] **Mod Matrix slot 2-8**: switch slot via Select dropdown, verify each saves/loads independently
- [ ] **Mod Matrix**: source/dest/amount on slot 1 don't change when switching to slot 2 and back
- [ ] **Mod Matrix**: NRPN values from sysex dump correctly populate all 8 slots (after v0.6.1 deployed)
- [ ] **Unison On/Off** (NRPN 168): toggle plays unison
- [ ] **Unison Mode** (NRPN 169): 0=chord, 1-16=voice count
- [ ] **Unison Detune** (NRPN 167): adjusts detune amount
- [ ] **Layer Mode**: Split A|B button toggles split mode (NRPN 163 = 2)
- [ ] **Layer Mode**: Stack A+B button toggles stacked (NRPN 163 = 1)
- [ ] **FX Clk Sync** (NRPN 158): toggles correctly

## v0.6.1 (build 83) — Boot state + NRPN sync from synth
- [ ] Bipolar knobs (Fine Tune, Filter Env Amt etc.) start at 12 o'clock on plugin load
- [ ] All buttons start in OFF state on plugin load
- [ ] **Pi deploy required**: server/midi_router.py + server/synth_manager.py
- [ ] After Pi deploy: send a patch with non-default mod matrix → mod matrix dropdowns
  should populate when you switch to that patch
- [ ] After Pi deploy: Unison on/off and detune values from a patch should reflect in UI
- [ ] After Pi deploy: Sync button reflects sync state from patch
- [ ] After Pi deploy: FX Clk Sync reflects from patch
- [ ] Newly hooked NRPN controls animate (not just snap) when patch loads — boot animation

## Things known NOT to work (don't bother testing)
- Global / Write / Compare buttons (SysEx-only, no implementation yet)
- Edit Layer B (needs layerBOffset support in midi_router)
- Tap Tempo (no MIDI equivalent on Rev2)
- Programme Parameter / Value knobs (hardware nav only, no MIDI)
- Sequencer Mode / Track / Play / Destination / Record (sequencer programming
  isn't practical via MIDI; entire section is greyed out)
- Hold button (no NRPN exposed by Rev2)
- Transpose buttons (hardware-only)

## Pi deployment commands

After verifying changes locally, deploy to the Pi:

```bash
scp server/midi_router.py pi@anarack.local:~/anarack/server/
scp server/synth_manager.py pi@anarack.local:~/anarack/server/
ssh pi@anarack.local "pkill -f midi_router.py; cd /home/pi/anarack && PYTHONUNBUFFERED=1 nohup venv/bin/python server/midi_router.py --midi-port 'Prophet Rev2' > /tmp/anarack.log 2>&1 &"
ssh pi@anarack.local "tail -20 /tmp/anarack.log"
```
