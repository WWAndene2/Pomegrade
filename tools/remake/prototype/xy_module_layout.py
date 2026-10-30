#!/usr/bin/env python3
"""Where an X/Y code module reaches into the game's structures, and which of those places are checked in Omega Ruby
(30 October, the gyms' restart, DEVELOPMENT_NOTE.md 3e step 5): a ported module runs X's code against Omega Ruby's
objects, so each offset it reads or writes in them must be the same in both games or be patched (OrasXyGymModules.h's
Words). Grant's module wrote X's field +0xFC +0xC4 on the way back from a won battle, a null pointer in Omega Ruby
(B1): this lists every such access, so none is found by a run again.

  xy_module_layout.py <checked.tsv> <module.cro>...

How (static, an over-approximation): the area module's constructor is called with (object, field) and keeps the field
pointer in the object; the constructors are found as the functions storing a pointer to the module's own data at
[r0] (the vtable) and r1 at [r0, #k] (the field slots k). Then each function is read straight through (no control
flow): a register loaded from [this, #k], k a field slot and this the method's r0 (followed through movs), holds the
field (in a constructor, r1 too); one loaded from [field, #a] holds the field's
member a; an access [field, #a] is reported as "field +a", [member a, #b] as "field +a +b". Calls clobber r0-r3 and r12.
Not seen: the field reached another way (a getter of the static module, a script native's argument), registers kept
across branches, and other objects (the player's: OrasXyGymModules.cpp patches +0xC64 by hand).

checked.tsv: one line per place checked, `<path>\t<Omega Ruby path or same>\t<how it was checked>`, a path written
as printed here ("field +0xFC +0xC4"); prototype/xy_module_layout.tsv holds those read so far. Prints, per module,
each place with its accesses (function, instruction, r/w) and its state: checked (Omega Ruby's path), or UNCHECKED;
then the count. Exit status 1 when any is unchecked.

  xy_module_layout.py <checked.tsv> --words <module.cro>

prints, for each instruction reaching a field member that lies elsewhere in Omega Ruby (field +a checked as field +b),
its patch as OrasXyGymModules.cpp's XyModulePort::Words lines: the file offset, X's word, the word with the offset b
(ldr / str / ldrb / strb with a 12-bit immediate, ldrh / strh / ldrsb / ldrsh / ldrd with an 8-bit one; another
form is refused).
"""
import collections
import os
import re
import struct
import sys

from capstone import CS_ARCH_ARM, CS_MODE_ARM, Cs
from capstone.arm import ARM_OP_MEM, ARM_OP_REG

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cro_dis import Cro  # noqa: E402

CLOBBERED = {'r0', 'r1', 'r2', 'r3', 'r12', 'ip'}


def checked_places(path):
    places = {}
    for line in open(path):
        if line.strip() and not line.startswith('#'):
            parts = line.rstrip('\n').split('\t')
            places[parts[0]] = parts[1] if len(parts) > 1 else '?'
    return places


def code_range(cro):
    off, size, _ = next(s for s in cro.segments if s[2] == 0)
    return off, off + size


def instructions(cro, md, start=None, end=None):
    """the code's instructions word by word (capstone's run stops at the first literal pool word it cannot decode)"""
    first, last = code_range(cro)
    start, end = start or first, min(end or last, last)
    for at in range(start, end, 4):
        decoded = list(md.disasm(bytes(cro.data[at:at + 4]), at))
        if decoded:
            yield decoded[0]


def functions(cro, md):
    """function starts: `push {..., lr}`, bl targets (as cro_dis.py's `functions`) and the code a data word points to
    (the vtables, in the data segments)"""
    start, end = code_range(cro)
    starts = set()
    starts |= {a for a in cro.labels if start <= a < end}  # its exports (X's DllField imports the constructors)
    for at, what in cro.patches.items():  # relocated words of the data segments: the vtables (a switch's jump table,
        word = struct.unpack_from('<I', cro.data, at)[0]  # in the code segment, names cases inside a function: not starts)
        if what.startswith('->') and start <= word < end and word % 4 == 0 and not start <= at < end:
            starts.add(word)
    listing = list(instructions(cro, md))
    literals = set()  # the words pc-relative loads read: data, not code
    for ins in listing:
        m = re.match(r'[rsd]\d+, \[pc, #(0x[0-9a-f]+|\d+)\]$', ins.op_str)
        if ins.mnemonic in ('ldr', 'vldr') and m:
            literals.add(ins.address + 8 + int(m.group(1), 0))
    after_return = False  # the first code word after a return and its literals starts a function (a leaf has no push)
    for ins in listing:
        if after_return and ins.address not in literals:
            starts.add(ins.address)
            after_return = False
        if ins.mnemonic == 'bx' and ins.op_str == 'lr' or ins.mnemonic == 'pop' and 'pc' in ins.op_str:
            after_return = True
        if ins.mnemonic in ('push', 'stmdb') and 'lr' in ins.op_str:
            starts.add(ins.address)
        if ins.mnemonic == 'bl' and ins.op_str.startswith('#'):
            target = int(ins.op_str[1:], 0)
            if start <= target < end:
                starts.add(target)
    return sorted(starts)


def field_slots(cro, md):
    """the object offsets where a constructor keeps its second argument (the field), and the constructors: a function
    that, before its first
    call, stores a pointer to the module's own data at [this] (the vtable) and r1 at [this, #k] (str, or stm's k-th
    word), r0 and r1 followed through movs"""
    data_segments = [(o, o + s) for o, s, k in cro.segments if k in (1, 2)]
    slots, constructors = set(), set()
    for function in functions(cro, md):
        this, field, literal = {'r0'}, {'r1'}, set()
        stored, vtable = set(), False
        for ins in instructions(cro, md, function, function + 0xA0):
            op = ins.op_str
            if ins.mnemonic.startswith('b'):
                break
            m = re.match(r'(r\d+), \[pc, #(0x[0-9a-f]+|\d+)\]$', op)
            if ins.mnemonic == 'ldr' and m:
                value = struct.unpack_from('<I', cro.data, ins.address + 8 + int(m.group(2), 0))[0]
                if any(a <= value < b for a, b in data_segments):
                    literal.add(m.group(1))
                continue
            m = re.match(r'(r\d+), (r\d+)$', op)
            if ins.mnemonic == 'mov' and m:
                for group in (this, field, literal):
                    group.discard(m.group(1))
                    if m.group(2) in group:
                        group.add(m.group(1))
                continue
            m = re.match(r'(r\d+), \[(r\d+)(?:, #(0x[0-9a-f]+|\d+))?\]$', op)
            if ins.mnemonic == 'str' and m and m.group(2) in this:
                k = int(m.group(3), 0) if m.group(3) else 0
                if m.group(1) in field:
                    stored.add(k)
                if k == 0 and m.group(1) in literal:
                    vtable = True
                continue
            m = re.match(r'(r\d+), (r\d+), \[(r\d+)(?:, #(0x[0-9a-f]+|\d+))?\]$', op)
            if ins.mnemonic == 'strd' and m and m.group(3) in this:
                k = int(m.group(4), 0) if m.group(4) else 0
                for j, reg in enumerate((m.group(1), m.group(2))):
                    if reg in field:
                        stored.add(k + 4 * j)
                    if k + 4 * j == 0 and reg in literal:
                        vtable = True
                continue
            m = re.match(r'(r\d+), \{([^}]*)\}$', op)
            if ins.mnemonic in ('stm', 'stmia') and m and m.group(1) in this:
                for k, reg in enumerate(r.strip() for r in m.group(2).split(',')):
                    if reg in field:
                        stored.add(4 * k)
                    if k == 0 and reg in literal:
                        vtable = True
                continue
            dest = op.split(',')[0].strip()
            for group in (this, field, literal):
                if ins.mnemonic not in ('str', 'strd', 'vstr', 'cmp', 'tst', 'push', 'stm', 'stmia'):
                    group.discard(dest)
        if vtable and stored - {0}:
            slots |= stored - {0}  # [this] is the vtable
            constructors.add(function)
    return slots, constructors


def accesses(cro, md, slots, constructors=()):
    """(place, function, address, 'r' or 'w') for each access through the field (in a constructor, through r1 too)"""
    starts = set(functions(cro, md))
    found = []
    held = {}  # register -> path ('field', 'field +0xA')
    function = None
    for ins in instructions(cro, md):  # 'this': the method's r0, followed through movs
        if ins.address in starts:
            function, held = ins.address, {'r0': 'this'}
            if ins.address in constructors:
                held['r1'] = 'field'
        regs = [ins.reg_name(o.reg) for o in ins.operands if o.type == ARM_OP_REG]
        mems = [o for o in ins.operands if o.type == ARM_OP_MEM]
        if ins.mnemonic in ('bl', 'blx') or ins.mnemonic.startswith('bl'):
            for r in CLOBBERED:
                held.pop(r, None)
            continue
        if ins.mnemonic.startswith(('ldr', 'str', 'vldr', 'vstr')) and mems and function is not None:
            mem = mems[0].mem
            base = ins.reg_name(mem.base)
            disp = mem.disp if mem.index == 0 else None
            dest = regs[0] if regs else None
            load = ins.mnemonic.startswith(('ldr', 'vldr'))
            if held.get(base) == 'this':
                if load and ins.mnemonic == 'ldr' and disp in slots and dest:
                    held[dest] = 'field'
                    continue
            elif base in held and disp is not None:
                place = '%s +0x%X' % (held[base], disp)
                if place.count('+') <= 2:
                    found.append((place, function, ins.address, 'r' if load else 'w'))
                if load and dest and ins.mnemonic == 'ldr' and place.count('+') < 2:
                    held[dest] = place
                    continue
            if load and dest:
                held.pop(dest, None)
            continue
        if ins.mnemonic == 'mov' and len(regs) == 2 and regs[1] in held:
            held[regs[0]] = held[regs[1]]
        elif regs:
            held.pop(regs[0], None)
    return found


def member(path):
    """the field member a path starts with (field +0x50 +0x1C -> 0x50), None if not the field's"""
    m = re.match(r'field \+0x([0-9A-F]+)', path)
    return int(m.group(1), 16) if m else None


def words(checked, path, md):
    """the patches moving a module's accesses to the field's members Omega Ruby keeps elsewhere"""
    cro = Cro(path)
    cro.read_imports([])
    raw = Cro(path).data  # the words as stored (the patch's Expected)
    slots, constructors = field_slots(cro, md)
    lines = []
    for place, _function, address, _kind in accesses(cro, md, slots, constructors):
        if place.count('+') != 1 or member(checked.get(place, '')) in (None, member(place)):
            continue
        x, to = member(place), member(checked[place])
        word = struct.unpack_from('<I', raw, address)[0]
        if (word >> 25) & 7 == 2 and word & 0xFFF == x and (word >> 23) & 1 and to < 0x1000:
            patched = word & ~0xFFF | to  # ldr / str / ldrb / strb, a 12-bit immediate
        elif (word & 0x0E4000F0) in (0x004000B0, 0x004000D0, 0x004000F0) and (word >> 23) & 1 \
                and ((word >> 4) & 0xF0 | word & 0xF) == x and to < 0x100:
            patched = word & ~0xF0F | (to & 0xF0) << 4 | to & 0xF  # ldrh / strh / ldrsb / ldrsh / ldrd: 8 bits, split
        else:
            sys.exit('%s 0x%X: %08X is no load or store [rn, #+0x%X] (or +0x%X does not fit)' % (cro.name, address, word, x, to))
        lines.append('{0x%X, 0x%08X, 0x%08X, "the field\'s +0x%X (X\'s +0x%X)"},' % (address, word, patched, to, x))
    return cro.name, lines


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    checked = checked_places(sys.argv[1])
    md = Cs(CS_ARCH_ARM, CS_MODE_ARM)
    md.detail = True
    if sys.argv[2] == '--words':
        for path in sys.argv[3:]:
            name, lines = words(checked, path, md)
            print('// %s: %d word(s)' % (name, len(lines)))
            print('\n'.join(lines))
        return
    unchecked = 0
    for path in sys.argv[2:]:
        cro = Cro(path)
        cro.read_imports([])  # applies the internal relocations: the vtable's literal becomes its address
        slots, constructors = field_slots(cro, md)
        places = collections.defaultdict(list)
        for place, function, address, kind in accesses(cro, md, slots, constructors):
            places[place].append('%s 0x%X@0x%X' % (kind, address, function))
        print('== %s (field kept at %s)' % (cro.name, ', '.join('+0x%X' % s for s in sorted(slots)) or 'none found'))
        for place in sorted(places, key=lambda p: [int(x, 16) for x in re.findall(r'0x([0-9A-F]+)', p)]):
            state = checked.get(place)
            if state is None:
                unchecked += 1
            print('  %-22s %-28s %s' % (place, 'UNCHECKED' if state is None else 'checked: ' + state,
                                       ' '.join(places[place][:6]) + (' ...' if len(places[place]) > 6 else '')))
    print('%d place(s) unchecked' % unchecked)
    sys.exit(1 if unchecked else 0)


if __name__ == '__main__':
    main()
