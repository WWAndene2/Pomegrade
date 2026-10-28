#!/usr/bin/env python3
"""What X/Y's zone scripts can still run in a build without the story (oras-xy-world --story off), by a static reading:
each character it keeps (no vanish flag, or an item or a trainer) and each step event of the fifth kind names a case of
its zone script; the case's code is followed through the script's own functions and the natives it calls are named
(ghidra/function_names.tsv, hash 0x...). Each is then sorted: scene (event camera, fades, map changes, battles, characters
added or moved, a badge), gift or service (items, Pokemon, trades, healing, money), state (flags or works only),
dialogue, or unknown (the case not in the script's main table). 28 October, the list in XY_NEUTRAL_SCRIPTS.md.

  XyZoneDump <x.3ds> <dir>                    (prototype/XyZoneDump.cpp: X's zones as text)
  xy_zone_cases.py <dir> <function_names.tsv> > XY_NEUTRAL_SCRIPTS.md

The main table is the casetbl of the script's main (its `load.p.pri` of g_mode, then `switch`); calls into the shared
scripts (GlobalCall) are not followed, and the shared scripts' characters (ids 2000 and up) are not read.
"""
import collections
import os
import re
import sys

SCENE = re.compile(r'^(EVCamera|FadeRequest|LayoutFade|_MapChange|_CallTrainerBattleCore|CallTrainerLose|CallFieldCaptureDemo|'
                   r'MdlAcmdMakeMoveEx|MdlSetGPos|MdlSetPos|MdlAdd|MdlDel|MdlSetVisible|BuildMdl|MdlChangeMoveCode|ReserveScript|'
                   r'IEReqChangeScript|BadgeSetFlag|GymSetVictoryInfo|GlobalCall|CallPokeSelect)')
GIFT = re.compile(r'^(ItemAdd$|PokePartyAddParam|CallFieldTrade|PokePartyRecoverAll|PlayerSubMoney|ItemSub)')
STATE = re.compile(r'^(FlagSet|FlagReset|WorkSet)$')


def native_names(tsv):
    names = {}
    for line in open(tsv, encoding='utf-8'):
        m = re.search(r'Script_Native_(\S+)\t.*hash 0x([0-9A-F]{8})', line)
        if m:
            names[m.group(2)] = m.group(1)
    return names


def cases_of(path, names):
    """{case: the natives its code reaches}"""
    lines = open(path, encoding='utf-8').read().split('\n')
    natives = [l.split('#')[1].strip() for l in lines if l.startswith('native #')]
    code = lines[lines.index('=== SCRIPT'):] if '=== SCRIPT' in lines else []
    blocks, order, current = {}, [], None
    for l in code:
        m = re.match(r'^(L[0-9A-F]+):', l)
        if m:
            current = m.group(1)
            order.append(current)
            blocks[current] = []
        elif current:
            blocks[current].append(l.strip())
    functions, function_of, f = collections.OrderedDict(), {}, None
    for label in order:
        if blocks[label] and blocks[label][0] == 'proc':
            f = label
            functions[f] = []
        if f:
            functions[f] += blocks[label]
            function_of[label] = f
    name = lambda i: names.get(natives[i], '#' + natives[i]) if i < len(natives) else '?'
    memo = {}

    def reach(fn, seen):
        if fn in memo:
            return memo[fn]
        if fn in seen:
            return set()
        seen.add(fn)
        out = set()
        for l in functions.get(fn, []):
            m = re.match(r'sysreq\.n (\d+)', l)
            if m:
                out.add(name(int(m.group(1))))
            m = re.match(r'call (L[0-9A-F]+)', l)
            if m:
                out |= reach(function_of.get(m.group(1), m.group(1)), seen)
        memo[fn] = out
        return out

    # main: the header's entry, a load of g_mode then a switch to its case table
    header = next((l for l in code if l.startswith('header ')), '')
    main = header.split()[4] if len(header.split()) > 4 else ''
    switch = next((l.split()[1] for l in blocks.get(main, [])[:4] if l.startswith('switch')), None)
    table = next((l for l in blocks.get(switch, []) if l.startswith('casetbl')), '') if switch else ''
    words = table.split()[1:]
    cases = {}
    for k in range(int(words[0]) if words else 0):
        value, label = int(words[2 + 2 * k]), words[3 + 2 * k]
        reached = set()
        for l in blocks.get(label, []):
            m = re.match(r'call (L[0-9A-F]+)', l)
            if m:
                reached |= reach(function_of.get(m.group(1), m.group(1)), set())
            m = re.match(r'sysreq\.n (\d+)', l)
            if m:
                reached.add(name(int(m.group(1))))
            if l.startswith('jump'):
                break
        cases[value] = reached
    return cases


def main():
    folder, tsv = sys.argv[1], sys.argv[2]
    names = native_names(tsv)
    counts, rows = collections.Counter(), []
    for f in sorted(os.listdir(folder), key=lambda n: int(n[1:-4])):
        zone = int(f[1:-4])
        path = os.path.join(folder, f)
        lines = open(path, encoding='utf-8').read().split('\n')
        place = lines[0][5:]
        kept = []
        for l in lines:
            m = re.match(r'CHAR id (\d+) model \d+ kind (\d+) flag (\d+) script (\d+)', l)
            if m:
                i, kind, flag, script = map(int, m.groups())
                item = 7000 <= script < 8000
                trainer = kind in (1, 3, 9) and 3000 < script < 7000
                if script < 1000 and (flag == 0 or item or trainer):
                    kept.append(('character', i, script))
            m = re.match(r'STEP script (\d+)', l)
            if m:
                kept.append(('step', None, int(m.group(1))))
        if not kept:
            continue
        try:
            cases = cases_of(path, names)
        except (ValueError, IndexError):
            cases = {}
        for what, i, script in kept:
            reached = cases.get(script)
            if reached is None:
                category, why = 'unknown', []
            else:
                scene = sorted(n for n in reached if SCENE.match(n))
                gift = sorted(n for n in reached if GIFT.match(n))
                state = sorted(n for n in reached if STATE.match(n))
                category = 'scene' if scene else 'gift/service' if gift else 'state' if state else 'dialogue'
                why = scene or gift or state
            counts[(what, category)] += 1
            if category != 'dialogue':
                rows.append((zone, place, what, '' if i is None else i, script, category, ', '.join(why)))
    print('# What X/Y\'s zone scripts can still run without the story\n')
    print('Made by `prototype/xy_zone_cases.py` (its header says how). X/Y zone z is the whole-Kalos build\'s zone 538 + z.\n')
    print('Counts: ' + ', '.join(f'{w} {c}: {n}' for (w, c), n in sorted(counts.items())) + '\n')
    print('| X/Y zone | Mod zone | Place | What | Character | Case | Category | Natives that sort it |')
    print('|---|---|---|---|---|---|---|---|')
    for zone, place, what, i, script, category, why in rows:
        print(f'| {zone} | {538 + zone} | {place} | {what} | {i} | {script} | {category} | {why} |')


if __name__ == '__main__':
    main()
