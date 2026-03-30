"""
Synth Definition Manager — loads synth definitions from JSON,
manages parameter state, handles SysEx parsing/sending.
"""

import json
import os
import time
from pathlib import Path


class SynthDefinition:
    """Represents a loaded synth definition with all its parameters."""

    def __init__(self, data: dict):
        self.id = data["id"]
        self.name = data["name"]
        self.manufacturer = data.get("manufacturer", "")
        self.midi_channel = data.get("midiChannel", 1)
        self.sysex = data.get("sysex", {})
        self.groups = data.get("groups", [])
        self.enums = data.get("enums", {})
        self.presets_config = data.get("presets", {})
        self.controller_profiles = data.get("controller_profiles", {})
        self._raw = data

        # Build lookup tables
        self._params_by_id = {}
        self._params_by_cc = {}
        self._params_by_sysex_offset = {}
        for group in self.groups:
            for p in group.get("parameters", []):
                self._params_by_id[p["id"]] = p
                if p.get("cc") is not None:
                    self._params_by_cc[p["cc"]] = p
                if p.get("sysex") is not None:
                    self._params_by_sysex_offset[p["sysex"]] = p

    def get_param_by_cc(self, cc: int) -> dict | None:
        return self._params_by_cc.get(cc)

    def get_param_by_id(self, param_id: str) -> dict | None:
        return self._params_by_id.get(param_id)

    def get_sysex_offset_to_cc_map(self) -> dict[int, tuple[int, int]]:
        """Returns {sysex_offset: (cc, native_max)} for all parameters with both."""
        result = {}
        for offset, param in self._params_by_sysex_offset.items():
            if param.get("cc") is not None:
                result[offset] = (param["cc"], param.get("max", 127))
        return result

    def scale_to_cc(self, param_id: str, native_value: int) -> int:
        """Scale a native parameter value to 0-127 CC range."""
        param = self._params_by_id.get(param_id)
        if not param:
            return min(127, native_value)
        native_max = param.get("max", 127)
        if native_max <= 127:
            return min(127, native_value)
        return round(native_value * 127 / native_max)

    def to_json(self) -> str:
        """Return the raw definition as JSON for sending to clients."""
        return json.dumps(self._raw)

    def to_dict(self) -> dict:
        return self._raw


class SynthManager:
    """Loads and manages synth definitions from disk."""

    def __init__(self, synths_dir: str = None):
        if synths_dir is None:
            # Default: synths/ relative to project root
            synths_dir = str(Path(__file__).parent.parent / "synths")
        self.synths_dir = synths_dir
        self.definitions: dict[str, SynthDefinition] = {}
        self.active_synth: SynthDefinition | None = None
        self._load_all()

    def _load_all(self):
        """Load all synth definitions from the synths directory."""
        if not os.path.isdir(self.synths_dir):
            print(f"WARNING: Synths directory not found: {self.synths_dir}")
            return

        for filename in os.listdir(self.synths_dir):
            if filename.endswith(".json") and not filename.startswith("schema"):
                filepath = os.path.join(self.synths_dir, filename)
                try:
                    with open(filepath) as f:
                        data = json.load(f)
                    defn = SynthDefinition(data)
                    self.definitions[defn.id] = defn
                    print(f"Loaded synth definition: {defn.name} ({defn.id}) — "
                          f"{sum(len(g.get('parameters', [])) for g in defn.groups)} parameters")
                except Exception as e:
                    print(f"WARNING: Failed to load {filename}: {e}")

    def get_definition(self, synth_id: str) -> SynthDefinition | None:
        return self.definitions.get(synth_id)

    def set_active(self, synth_id: str) -> SynthDefinition | None:
        defn = self.definitions.get(synth_id)
        if defn:
            self.active_synth = defn
            print(f"Active synth: {defn.name}")
        return defn

    def list_synths(self) -> list[dict]:
        return [{"id": d.id, "name": d.name, "manufacturer": d.manufacturer}
                for d in self.definitions.values()]


class PresetManager:
    """Manages user presets (SysEx dumps) per synth."""

    def __init__(self, presets_dir: str = None):
        if presets_dir is None:
            presets_dir = str(Path(__file__).parent.parent / "presets")
        self.presets_dir = presets_dir
        os.makedirs(presets_dir, exist_ok=True)

    def _synth_dir(self, synth_id: str) -> str:
        d = os.path.join(self.presets_dir, synth_id)
        os.makedirs(d, exist_ok=True)
        return d

    def save(self, synth_id: str, name: str, sysex_data: list[int]):
        """Save a preset as a SysEx dump."""
        import base64
        filepath = os.path.join(self._synth_dir(synth_id), f"{name}.json")
        preset = {
            "name": name,
            "synth_id": synth_id,
            "created": time.strftime("%Y-%m-%d %H:%M:%S"),
            "sysex": base64.b64encode(bytes(sysex_data)).decode("ascii"),
        }
        with open(filepath, "w") as f:
            json.dump(preset, f, indent=2)
        print(f"Preset saved: {name} for {synth_id}")

    def load(self, synth_id: str, name: str) -> list[int] | None:
        """Load a preset's SysEx data."""
        import base64
        filepath = os.path.join(self._synth_dir(synth_id), f"{name}.json")
        if not os.path.exists(filepath):
            return None
        with open(filepath) as f:
            preset = json.load(f)
        return list(base64.b64decode(preset["sysex"]))

    def list_presets(self, synth_id: str) -> list[dict]:
        """List all user presets for a synth."""
        d = self._synth_dir(synth_id)
        presets = []
        for filename in sorted(os.listdir(d)):
            if filename.endswith(".json"):
                with open(os.path.join(d, filename)) as f:
                    preset = json.load(f)
                presets.append({"name": preset["name"], "created": preset.get("created", "")})
        return presets

    def delete(self, synth_id: str, name: str) -> bool:
        filepath = os.path.join(self._synth_dir(synth_id), name + ".json")
        if os.path.exists(filepath):
            os.remove(filepath)
            return True
        return False
