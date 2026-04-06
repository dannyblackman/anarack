#!/usr/bin/env python3
"""
Generate the REV2_PARAMS JS table from synths/sequential-prophet-rev2.json
and splice it into plugin/ui/rev2-panel.html between the marker comments:

    // ==== REV2_PARAMS_BEGIN ====
    // ==== REV2_PARAMS_END ====

This keeps the panel UI's NRPN/CC mappings in sync with the canonical synth
definition. Run whenever the JSON changes:

    python3 scripts/gen-rev2-params.py
"""

import json
import re
from pathlib import Path

ROOT = Path(__file__).parent.parent
SYNTH_JSON = ROOT / "synths" / "sequential-prophet-rev2.json"
PANEL_HTML = ROOT / "plugin" / "ui" / "rev2-panel.html"

BEGIN_MARKER = "// ==== REV2_PARAMS_BEGIN ===="
END_MARKER = "// ==== REV2_PARAMS_END ===="


def main():
    with open(SYNTH_JSON) as f:
        d = json.load(f)

    params = {}
    for group in d.get("groups", []):
        for p in group.get("parameters", []):
            pid = p.get("id")
            if not pid:
                continue
            params[pid] = {
                "cc": p.get("cc"),
                "nrpn": p.get("nrpn"),
                "min": p.get("min", 0),
                "max": p.get("max", 127),
                "default": p.get("default", 0),
            }

    enums = d.get("enums", {})

    # Build the replacement block (keep everything between the markers)
    block = BEGIN_MARKER + "\n"
    block += "// AUTO-GENERATED — do not edit by hand.\n"
    block += "// Run `python3 scripts/gen-rev2-params.py` to regenerate from\n"
    block += "// synths/sequential-prophet-rev2.json.\n"
    block += "const REV2_PARAMS = " + json.dumps(params, separators=(",", ":")) + ";\n"
    block += "const REV2_ENUMS = " + json.dumps(enums, separators=(",", ":")) + ";\n"
    block += """function param(id) {
  const p = REV2_PARAMS[id];
  if (!p) throw new Error('Unknown param id: ' + id);
  return p;
}
function paramOpts(id) {
  const p = param(id);
  const opts = {};
  if (p.cc !== null && p.cc !== undefined) opts.cc = p.cc;
  else if (p.nrpn !== null && p.nrpn !== undefined) opts.nrpn = p.nrpn;
  if (p.max !== undefined) opts.max = p.max;
  return opts;
}
function paramEnum(name) {
  const e = REV2_ENUMS[name];
  if (!e) throw new Error('Unknown enum: ' + name);
  return e;
}
"""
    block += END_MARKER

    html = PANEL_HTML.read_text()
    pattern = re.compile(
        re.escape(BEGIN_MARKER) + r".*?" + re.escape(END_MARKER),
        re.DOTALL,
    )
    if pattern.search(html):
        new_html = pattern.sub(block.replace("\\", "\\\\"), html)
    else:
        raise SystemExit(
            f"Couldn't find {BEGIN_MARKER} markers in {PANEL_HTML}.\n"
            f"Add them to the <script> section before running this."
        )

    PANEL_HTML.write_text(new_html)
    print(f"Spliced {len(params)} params + {len(enums)} enums into {PANEL_HTML}")


if __name__ == "__main__":
    main()
