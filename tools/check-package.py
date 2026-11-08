#!/usr/bin/env python3
"""Checks a built np2kai-<id>.chimeraCore the way Chimera's contract tests do,
without building Chimera (the authority is Chimera's own contract test,
build/meson-linux/source/ui/test-packages <folder>, as AGENTS.md says).

What it holds the package to, each read from the Chimera checkout so a
change there is a change here:
  - every key a default binding names is one Chimera's window sends
    (source/ui/qt/key_codes.inc), every pad input one a game controller or a
    joystick sends, every axis bound to a mouse or stick axis, and a bound
    left or right modifier has its merged name beside it;
  - every binding names a control the controller has;
  - every tastudioColumns name is one of the input's buttons or axes;
  - mnemonics are one printable character (not . or |), headers one to
    eight with no space at either end and unique;
  - a language table for every language Chimera has, each with English's
    keys and placeholders, and every text field of the declarations a key;
  - runsOn lists systems of the six, one of them of core.wbx's CPU, and
    no core tool names a file "tool" or "settings";
  - every machine is a value of the machine setting and is left with one
    declaration of each setting;
  - every setting type is one Chimera knows, every enum default one of its
    options, every firmware requiredWhen names a setting and a value it has;
  - the licence list names every file it says it carries.

Usage: check-package.py <np2kai-<id>.chimeraCore> <chimera checkout>
"""
import json
import re
import sys
import zipfile

pkg, chimera = sys.argv[1], sys.argv[2]
z = zipfile.ZipFile(pkg)
files = set(z.namelist())
problems = []


def need(cond, what):
    if not cond:
        problems.append(what)


for f in ('core.wbx', 'waterbox.config', 'default_keybinds.json', 'file_slots.json', 'licenses/licenses.json'):
    need(f in files, 'the package has no ' + f)
cfg = json.loads(z.read('waterbox.config'))
binds = json.loads(z.read('default_keybinds.json'))
lic = json.loads(z.read('licenses/licenses.json'))
slot_ids = {sl['id'] for sl in json.loads(z.read('file_slots.json')).get('slots', [])}

# ---- the names Chimera sends, from its own source ----
# The window's key tables (source/ui/qt/key_codes.inc: every name a key is
# sent under, on Windows, its other layouts, Linux and macOS) and the sets
# its contract test holds bindings to (test_packages.cpp, "the input names
# the window sends").
codes = open(chimera + '/source/ui/qt/key_codes.inc', encoding='utf-8').read()
keys = set(re.findall(r'\{\s*0x[0-9A-Fa-f]+\s*,\s*"([^"]+)"\s*\}', codes))
keys |= {"Shift", "Ctrl", "Alt", "WMouse L", "WMouse M", "WMouse R", "WMouse 1", "WMouse 2"}
need(len(keys) > 100 and 'Number1' in keys and 'KeypadMultiply' in keys, 'could not read key_codes.inc (%d names)' % len(keys))
tests = open(chimera + '/source/ui/viewmodel/tests/test_packages.cpp', encoding='utf-8').read()


def string_set(name):
    m = re.search(r'std::set<std::string>\s+%s\s*=\s*\{(.*?)\};' % name, tests, re.S)
    return set(re.findall(r'"([^"]+)"', m.group(1))) if m else set()


pad_buttons = string_set('controllerButtons')
pad_axes = string_set('controllerAxes')
mouse_axes = string_set('mouseAxes')
need(pad_buttons and pad_axes and mouse_axes, 'could not read the test sets')
joy_button = re.compile(r'^([XYZWVSQPN][+-]|Axis\d+[+-]|B\d+|POV\d+[UDLR])$')
joy_axis = re.compile(r'^([XYZWVSQPN]|Axis\d+) Axis$')
merged = {"LeftShift": "Shift", "RightShift": "Shift", "LeftCtrl": "Ctrl", "RightCtrl": "Ctrl",
          "LeftAlt": "Alt", "RightAlt": "Alt"}

inp = cfg['input']
controls = set(inp['buttons']) | {a['name'] for a in inp['axes']}
for ctl, table in binds['AllTrollers'].items():
    need(ctl == inp['name'], 'bindings for a controller the package does not have: ' + ctl)
    bound = set()
    for button, value in table.items():
        need(button in controls, 'a binding for no control: ' + button)
        for part in [p.strip() for p in re.split(r'[,+]', value or '') if p.strip()]:
            bound.add(part)
            m = re.match(r'^([XJ])\d+ (.+)$', part)
            if m:
                ok = m.group(2) in pad_buttons if m.group(1) == 'X' else joy_button.match(m.group(2))
                need(ok, '%s: %s is no pad input Chimera sends' % (button, part))
            else:
                need(part in keys, '%s: %s is no key Chimera sends' % (button, part))
    for side, name in merged.items():
        need(side not in bound or name in bound, '%s is bound but not %s' % (side, name))
for ctl, table in binds.get('AllTrollersAnalog', {}).items():
    for axis, b in table.items():
        need(axis in controls, 'an analog binding for no control: ' + axis)
        for part in [p.strip() for p in (b.get('Value') or '').split(',') if p.strip()]:
            m = re.match(r'^([XJ])\d+ (.+)$', part)
            if m:
                ok = (m.group(2).endswith(' Axis') and m.group(2)[:-5] in pad_axes) if m.group(1) == 'X' else joy_axis.match(m.group(2))
            else:
                ok = part in mouse_axes
            need(ok, '%s: %s is no axis' % (axis, part))

# ---- where it runs, and its tools' files (Chimera 2026-11-05, 2026-11-02) ----
SYSTEMS = {'windows-x86_64', 'windows-aarch64', 'linux-x86_64', 'linux-aarch64', 'macos-x86_64', 'macos-aarch64'}
runs_on = cfg.get('runsOn')
need(isinstance(runs_on, list) and runs_on, "it does not say where it runs ('runsOn')")
for r in runs_on or []:
    need(r in SYSTEMS, "'runsOn' lists %r, which is not one of the six" % r)
wbx = z.read('core.wbx')[:20] if 'core.wbx' in files else b''
cpu = {62: 'x86_64', 183: 'aarch64'}.get(int.from_bytes(wbx[18:20], 'little')) if len(wbx) == 20 else None
need(cpu is None or any(r.endswith('-' + cpu) for r in runs_on or []),
     "core.wbx is built for %s, and 'runsOn' lists no system of it" % cpu)
for t in cfg.get('tools', []):
    for lst in ('inputs', 'outputs'):
        for f in t.get(lst, []):
            need(f.get('name') not in ('tool', 'settings'),
                 "tool %r names a file %r, the name of a file Chimera gives every tool" % (t.get('id'), f.get('name')))

# ---- the texts: a table for every language Chimera has, each with
# English's keys, and every text field a key (Chimera docs/translations.md) ----
import glob, os
codes = sorted(os.path.basename(f)[:-5] for f in glob.glob(chimera + '/languages/*.json'))
need(len(codes) >= 26, 'could not read Chimera\'s languages (%d)' % len(codes))
tables = {}
for code in codes:
    name = 'languages/%s.json' % code
    if name not in files:
        problems.append(name + ' is not in the package')
        continue
    tables[code] = json.loads(z.read(name)).get('texts', {})
en = tables.get('en', {})
PH = re.compile(r'%[1-9n]')
for code, t in tables.items():
    if code == 'en':
        continue
    for k in sorted(set(en) - set(t)):
        problems.append('%s: %s: missing' % (code, k))
    for k in sorted(set(t) - set(en)):
        problems.append('%s: %s: not an English key' % (code, k))
    for k in set(en) & set(t):
        if sorted(PH.findall(en[k])) != sorted(PH.findall(t[k])):
            problems.append('%s: %s: placeholders differ from English' % (code, k))
        for noun in ('Chimera', 'Windows', 'Linux', cfg.get('coreName', 'NP2kai')):
            if noun in en[k] and noun not in t[k]:
                problems.append('%s: %s: %s is not there as itself' % (code, k, noun))
fields = []
for s_ in cfg['settings']:
    fields += [s_.get('display'), s_.get('description')] + list((s_.get('optionLabels') or {}).values())
for f_ in cfg['firmware']:
    fields += [f_.get('display'), f_.get('description'), f_.get('label')]
for sl in json.loads(z.read('file_slots.json')).get('slots', []):
    fields += [sl.get('title'), sl.get('help'), sl.get('nameHelp')]
for v in fields:
    if v:
        need(v.split('\x1f')[0] in en, 'a text field is not a key of languages/en.json: %r' % v[:60])

# ---- mnemonics and headers ----
def bare(n):
    head, _, rest = n.partition(' ')
    return rest if rest and head[:1] == 'P' and head[1:].isdigit() else n


for b in inp['buttons']:
    mn = inp['mnemonics'].get(b, inp['mnemonics'].get(bare(b)))
    need(mn is not None and len(mn) == 1 and 32 < ord(mn) < 127 and mn not in '.|', 'bad or no mnemonic for ' + b)
hdrs = [inp.get('headers', {}).get(b, inp['mnemonics'].get(b, '')) for b in inp['buttons']]
for b, h in inp.get('headers', {}).items():
    need(b in inp['buttons'], 'a header for no button: ' + b)
    need(1 <= len(h) <= 8 and h == h.strip() and all(32 <= ord(c) < 127 for c in h), 'bad header %r' % h)
long_headers = list(inp.get('headers', {}).values())
need(len(long_headers) == len(set(long_headers)), 'two columns share a header')
for a in inp['axes']:
    need(1 <= len(a.get('header', '')) <= 5, 'bad axis header on ' + a['name'])

# ---- TAStudio's first columns (Chimera issue 544) ----
# every name one of the input's buttons or axes, as test_packages refuses
controls = set(inp['buttons']) | {a['name'] for a in inp['axes']}
cols = inp.get('tastudioColumns', [])
need(isinstance(cols, list) and all(isinstance(c, str) for c in cols), 'tastudioColumns is not a list of names')
for c in cols:
    need(c in controls, 'tastudioColumns names no control: %s' % c)

# ---- settings and firmware ----
types = {'bool', 'string', 'float', 'enum', 'int', 'uint', 'long', 'ulong', 'datetime'}
settings = {s['name']: s for s in cfg['settings']}
# the machines: the setting that picks them, and one declaration of each
# setting a machine is left with ("when" lists, Chimera's contract test)
machines = cfg.get('machines') or []
if machines:
    ms = settings.get(cfg.get('machineSetting'))
    need(ms is not None, 'machines but no machineSetting setting')
    for m in machines:
        need(m.get('id') and m.get('when'), 'a machine with no id or no when')
        for w in m.get('when', []):
            need(ms and w in ms.get('options', []), '%s: %r is no value of the machine setting' % (m.get('id'), w))
        mine = [s['name'] for s in cfg['settings'] if not s.get('when') or set(s['when']) & set(m.get('when', []))]
        need(len(mine) == len(set(mine)), '%s is left with two declarations of one setting' % m.get('id'))
for s in cfg['settings']:
    need(s.get('type') in types, '%s: type %r' % (s['name'], s.get('type')))
    need(s.get('description'), s['name'] + ' has no description')
    if s.get('type') == 'enum':
        need(s['default'] in s['options'], '%s: default not an option' % s['name'])
    if 'min' in s:
        need(s['min'] <= s['default'] <= s['max'], '%s: default outside min..max' % s['name'])
def leaves(cond):
    for key in ('all', 'any'):
        if key in cond:
            return [l for c in cond[key] for l in leaves(c)]
    return leaves(cond['not']) if 'not' in cond else [cond]


for f in cfg['firmware']:
    rw = f.get('requiredWhen')
    need(rw is not None, f['id'] + ' is optional firmware, which Chimera does not have')
    for leaf in leaves(rw) if rw else []:
        if 'slot' in leaf:
            need(leaf['slot'] in slot_ids, '%s names no slot %r' % (f['id'], leaf['slot']))
            continue
        s = settings.get(leaf.get('setting'))
        need(s is not None, f['id'] + ' names no setting')
        if s:
            values = s.get('options', [True, False] if s['type'] == 'bool' else [])
            for v in leaf.get('in', [leaf.get('is')]):
                need(v in values, '%s: %r is no value of %s' % (f['id'], v, s['name']))

# ---- the frame rate and sound ----
v = cfg['video']
need(v['vsyncNumerator'] > 0 and v['vsyncDenominator'] > 0, 'no frame rate')
need(cfg['audio'].get('rate') == 44100, 'the sound rate is not 44100')

# ---- the controller's picture ----
pic = inp.get('picture')
if pic:
    need(pic in files and z.read(pic)[:8] == b'\x89PNG\r\n\x1a\n', pic + ' is not a PNG in the package')
    need(pic.rsplit('/', 1)[-1] in z.read('licenses/licenses.json').decode('utf-8'), 'licenses.json does not name ' + pic)

# ---- licences ----
for c in lic.get('components', []):
    need(('licenses/' + c.get('licenseFile', '')) in files, 'licence file missing for ' + c.get('name', '?'))

if problems:
    print('%d problems:' % len(problems))
    for p in problems:
        print('  ' + p)
    sys.exit(1)
print('package checks passed: %d machines, %d buttons, %d bindings, %d settings, %d firmware, %d licences, %d key names known'
      % (len(machines) or 1, len(inp['buttons']), sum(len(t) for t in binds['AllTrollers'].values()), len(settings),
         len(cfg['firmware']), len(lic.get('components', [])), len(keys)))
