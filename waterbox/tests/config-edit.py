#!/usr/bin/env python3
"""Writes a copy of a Chimera config for one run of the NP2kai package.

Usage: config-edit.py <source config> <output config> [--forget <controller>]

The copy picks NP2kai for the PC9821 system (an install can hold another
package claiming it; --core only loads a package, it does not choose it).
With --forget, every binding for that controller is removed: the state of a
fresh install, which is when the package's own bindings must fill in.
"""
import json
import sys

src, out = sys.argv[1], sys.argv[2]
cfg = json.load(open(src, encoding="utf-8-sig"))
cfg.setdefault("DefaultCores", {})["PC9821"] = "NP2kai"
if len(sys.argv) > 4 and sys.argv[3] == "--forget":
    for section in ("AllTrollers", "AllTrollersAutoFire", "AllTrollersAnalog", "AllTrollersFeedbacks"):
        cfg.get(section, {}).pop(sys.argv[4], None)
json.dump(cfg, open(out, "w", encoding="utf-8"), indent=2)
