#!/usr/bin/env python3
"""X/Y's action commands (the numbers zone scripts give MdlAcmdSet) as Omega Ruby's (2 November, Clemont's quiz rooms;
ORAS_ENGINE.md 0.00, "Action commands"): prints the C++ table OrasXyActionCommands.cpp holds.

  xy_action_commands.py <work>        (session_setup.sh's linked/ and xlinked/ programs)

Each game's DllField builds its command table in one function (X 0x102446F0, Omega Ruby 0x1027E180): per command a
0x48-byte object whose word 0 is its class's vtable (then its steps), stored at the table's slot 4 x n. The builders are
read straight through, registers followed (literals, immediates, the allocations' objects, `str` / `stm` into them, the
object stored into the table), so each command number gets its class's RTTI name. X has 212 commands, Omega Ruby 260.

X's command n becomes:
- the Omega Ruby command of the same class name (108: 9 at the same number, 99 at another);
- else, for an animation command ActionCmdAnimation<Acmd, DrawStatus, mode>, the Omega Ruby animation command whose
  DrawStatus is X's moved by its block's offset: the two lists of animations line up in blocks (inferred from the
  sequences, not read in the code): X's DrawStatus 84-100 +32, 136-161 +25, 164-191 +20, 35-41 +1 (the irregular run
  143 144 145 141 140 146 is 168 169 170 166 165 171; every third command of 164-191 has mode 0 in both);
- else itself (X's own commands: bicycle, climbing, analog jumps, Rhyhorn's turns; the other animations): as before
  the translation, another Omega Ruby command.
"""
import collections
import re
import struct
import sys

from capstone import CS_ARCH_ARM, CS_MODE_ARM, Cs
from capstone.arm import ARM_OP_IMM, ARM_OP_MEM, ARM_OP_REG

BASE = 0x10000000
BLOCKS = [(84, 100, 32), (136, 161, 25), (164, 191, 20), (35, 41, 1)]


def commands(path, start, size=0x6000):
    m = open(path, 'rb').read()
    u = lambda a: struct.unpack_from('<I', m, a - BASE)[0]

    def rtti(v):
        try:
            name = m[u(u(v - 4) + 4) - BASE:][:160].split(b'\0')[0]
            return re.sub(r'^N5field6mmodel\d+', '', name.decode('latin1'))
        except (struct.error, IndexError):
            return None
    md = Cs(CS_ARCH_ARM, CS_MODE_ARM)
    md.detail = True
    regs, objs, table, base, count = {}, {}, {}, None, 0
    for a in range(start, start + size, 4):
        decoded = list(md.disasm(m[a - BASE:a - BASE + 4], a, 1))
        if not decoded:
            continue
        i = decoded[0]
        ops = i.operands
        name = lambda o: i.reg_name(o.reg)
        if i.mnemonic == 'ldr' and len(ops) == 2 and ops[1].type == ARM_OP_MEM:
            regs[name(ops[0])] = ('lit', u(a + 8 + ops[1].mem.disp)) if i.reg_name(ops[1].mem.base) == 'pc' else None
        elif i.mnemonic in ('mov', 'mvn') and len(ops) == 2:
            regs[name(ops[0])] = ('imm', ops[1].imm) if ops[1].type == ARM_OP_IMM else regs.get(name(ops[1]))
        elif i.mnemonic == 'add' and len(ops) == 3 and ops[2].type == ARM_OP_IMM:
            s = regs.get(name(ops[1]))
            regs[name(ops[0])] = (s[0], s[1], s[2] + ops[2].imm) if s and s[0] == 'obj' else \
                ('lit', s[1] + ops[2].imm) if s and s[0] == 'lit' else None
        elif i.mnemonic == 'bl':
            count += 1
            objs[count] = {}
            regs.update({'r0': ('obj', count, 0), 'r1': None, 'r2': None, 'r3': None, 'r12': None, 'lr': None})
        elif i.mnemonic == 'str' and len(ops) == 2 and ops[1].type == ARM_OP_MEM:
            b, v = regs.get(i.reg_name(ops[1].mem.base)), regs.get(name(ops[0]))
            if b and b[0] == 'obj':
                objs[b[1]][b[2] + ops[1].mem.disp] = v
            elif b and b[0] == 'lit' and v and v[0] == 'obj':
                base = b[1] if base is None else base
                table[(b[1] + ops[1].mem.disp - base) // 4] = v[1]
        elif i.mnemonic == 'stm' and ops[0].type == ARM_OP_REG:
            b = regs.get(name(ops[0]))
            if b and b[0] == 'obj':
                for k, o in enumerate(ops[1:]):
                    objs[b[1]][b[2] + 4 * k] = regs.get(name(o))
        elif i.mnemonic == 'pop' and 'pc' in i.op_str and a > start + 0x3300:
            break
    out = {}
    for n, obj in table.items():
        v = objs[obj].get(0)
        out[n] = rtti(v[1]) if v and v[0] == 'lit' else None
    return out


def animation(name):
    m = re.search(r'AcmdE\d+ELNS0_10DrawStatusE(\d+)ELNS0_22ActionCmdAnimationModeE(\d+)', name or '')
    return (int(m.group(1)), int(m.group(2))) if m else None


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    work = sys.argv[1]
    x = commands(work + '/xlinked/modules.bin', 0x102446F0)
    o = commands(work + '/linked/modules.bin', 0x1027E180)
    if sorted(x) != list(range(212)) or sorted(o) != list(range(260)):
        sys.exit(f'the builders read {len(x)} and {len(o)} commands, not 212 and 260')
    by_name = collections.defaultdict(list)
    by_status = collections.defaultdict(list)  # DrawStatus -> [(command, mode)]: a few hold two commands of two modes
    for n, c in sorted(o.items()):
        by_name[c].append(n)
        if animation(c) is not None:
            by_status[animation(c)[0]].append((n, animation(c)[1]))
    table, kinds = [], collections.Counter()
    for n in range(212):
        same = by_name.get(x[n], [])
        if len(same) == 1:
            table.append(same[0])
            kinds['name'] += 1
            continue
        status, mode = animation(x[n]) or (None, None)
        moved = next((status + off for lo, hi, off in BLOCKS if status is not None and lo <= status <= hi), None)
        if moved in by_status:
            # the command of X's mode, else the first (two of X's modes differ from their block's)
            table.append(next((c for c, md in by_status[moved] if md == mode), by_status[moved][0][0]))
            kinds['animation'] += 1
        else:
            table.append(n)
            kinds['kept'] += 1
    print(f'// {kinds["name"]} by name, {kinds["animation"]} animations by DrawStatus block, {kinds["kept"]} kept')
    for row in range(0, 212, 16):
        print('    ' + ', '.join(str(v) for v in table[row:row + 16]) + ',')


if __name__ == '__main__':
    main()
