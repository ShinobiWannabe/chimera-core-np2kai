#!/usr/bin/env python3
"""Checks a built np2kai-<id>.chimeraCore the way Chimera's contract tests do,
for a machine with no .NET (the tests themselves are the authority:
InstalledCorePackagesTests and MnemonicUniquenessTests, run as
docs/new-core.md says).

What it holds the package to, each read from the Chimera checkout so a
change there is a change here:
  - every key a default binding names is one Chimera sends (DistinctKey and
    DistinctKeyNameOverrides), every pad input one a game controller or a
    joystick sends, every axis bound to a mouse or stick axis, and a bound
    left or right modifier has its merged name beside it;
  - every binding names a control the controller has;
  - mnemonics are one printable character (not . or |), headers one to
    eight with no space at either end and unique;
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
src = open(chimera + '/source/gui/Chimera.Input/KeyMouseInput/DistinctKey.cs', encoding='utf-8').read()
body = src.split('enum DistinctKey', 1)[1].split('{', 1)[1].split('}', 1)[0]
body = re.sub(r'///.*', '', body)
over = dict(re.findall(r'DistinctKey\.(\w+)\s*=>\s*"([^"]+)"',
                       open(chimera + '/source/gui/Chimera.Client.Common/input/DistinctKeyNameOverrides.cs', encoding='utf-8').read()))
# A key is sent under DistinctKeyNameOverrides' name, else the enum's
# ToString. Two names for one value ("Enter = Return") are one key: it is
# sent under the override when either has one, else under the later name,
# which is what the frontend's own tests find ("PageUp = Prior" is sent as
# PageUp), so the earlier name is never sent.
enum, alias = [], {}
for m in re.finditer(r'^\s*([A-Za-z_][A-Za-z0-9_]*)\s*(?:=\s*([^,]+))?,', body, re.M):
    name, value = m.group(1), (m.group(2) or '').strip()
    if re.fullmatch(r'[A-Za-z_]\w*', value):
        alias[value] = name
    enum.append(name)
keys = set()
for k in enum:
    if k in alias.values():
        target = next(t for t, a in alias.items() if a == k)
        keys.add(over.get(k) or over.get(target) or k)
    elif k in alias:
        continue
    else:
        keys.add(over.get(k, k))
keys |= {"Shift", "Ctrl", "Alt", "WMouse L", "WMouse M", "WMouse R", "WMouse 1", "WMouse 2"}
need(len(enum) > 100 and 'Number1' in keys and 'KeypadMultiply' in keys, 'could not read DistinctKey (%d names)' % len(enum))
tests = open(chimera + '/source/gui/Chimera.Tests.Client.Common/CorePackages/InstalledCorePackagesTests.cs', encoding='utf-8').read()


def string_set(name):
    m = re.search(r'%s\s*=\s*new\(StringComparer\.Ordinal\)\s*\{(.*?)\};' % name, tests, re.S)
    return set(re.findall(r'"([^"]+)"', m.group(1))) if m else set()


pad_buttons = string_set('ControllerButtons')
pad_axes = string_set('ControllerAxes')
mouse_axes = string_set('MouseAxes')
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

# ---- settings and firmware ----
types = {'bool', 'string', 'float', 'enum', 'int', 'uint', 'long', 'ulong', 'datetime'}
settings = {s['name']: s for s in cfg['settings']}
# the machines: the setting that picks them, and one declaration of each
# setting a machine is left with ("when" lists, InstalledCorePackagesTests)
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
