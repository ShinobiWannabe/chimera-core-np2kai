#!/usr/bin/env python3
"""Checks that the bindings the package ships became the frontend's defaults
for its controller.

Usage: check-keybinds.py <config Chimera wrote> <default_keybinds.json> <controller>
"""
import json
import sys

cfg = json.load(open(sys.argv[1], encoding="utf-8-sig"))
shipped = json.load(open(sys.argv[2], encoding="utf-8"))
name = sys.argv[3]

want = shipped["AllTrollers"][name]
got = cfg.get("AllTrollers", {}).get(name)
if got is None:
    sys.exit("the frontend has no bindings at all for '%s'" % name)
differing = {k: (v, got.get(k)) for k, v in want.items() if got.get(k) != v}
if differing:
    sys.exit("bindings differ from the package's: %s" % differing)
n_analog = 0
want_analog = shipped.get("AllTrollersAnalog", {}).get(name)
if want_analog:
    got_analog = cfg.get("AllTrollersAnalog", {}).get(name) or {}
    for axis, bind in want_analog.items():
        for field, v in bind.items():
            if (got_analog.get(axis) or {}).get(field) != v:
                sys.exit("analog binding %s.%s differs: shipped %r, config %r"
                         % (axis, field, v, (got_analog.get(axis) or {}).get(field)))
    n_analog = len(want_analog)
print("%s: %d bindings and %d analog bindings adopted from the package" % (name, len(want), n_analog))
