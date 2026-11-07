#!/usr/bin/env python3
"""Checks that the bindings the package ships are the machine's in Chimera's
window when nobody changed them.

Usage: check-keybinds.py <the machine's bindings (frontend-keys.lua)> <default_keybinds.json> <controller>
"""
import json
import sys

cfg = json.load(open(sys.argv[1], encoding="utf-8-sig"))
shipped = json.load(open(sys.argv[2], encoding="utf-8"))
name = sys.argv[3]

want = shipped["AllTrollers"][name]
if not want:
    sys.exit("the package ships no bindings for '%s' - there is nothing to check" % name)
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
            g = (got_analog.get(axis) or {}).get(field)
            # no key is "" in a package and null in the window: the same
            if g != v and not (g in (None, "") and v in (None, "")):
                sys.exit("analog binding %s.%s differs: shipped %r, config %r"
                         % (axis, field, v, (got_analog.get(axis) or {}).get(field)))
    n_analog = len(want_analog)
print("%s: %d bindings and %d analog bindings adopted from the package" % (name, len(want), n_analog))
