#!/usr/bin/env python3
"""An Omega Ruby function and its X/Y twin compared with what they call, down a few levels (26 October, step 2's
inventory: the Sky and Inverse Battle natives, their setups; ORAS_ENGINE.md 0.0). Unlike xy_code_compare.py it needs no
Ghidra export: the linked modules and both .code files only.

  call_tree_compare.py <work dir> ORAS:X [ORAS:X ...] [--depth N] [--quiet]

<work dir> as headless/session_setup.sh lays it out: dumps/code.bin and dumps/xcode.bin (.code from 0x100000),
linked/modules.bin and xlinked/modules.bin (prototype/cro_link.py, from 0x10000000). Each pair: two addresses, in a
module or in .code. Instructions are compared with what moves between the games set aside: a call's or branch's target
(the calls are followed instead, in order), a literal pool word that is an address (its value kept when it is below
0x100000: a constant). A module's import stub (`ldr pc, [pc, #-4]` and its word) is followed to its target. A function
ends at its `pop {..., pc}` or `bx lr`. Each line: SAME or DIFF, the two addresses, how many instructions agree; with a
DIFF, the differing instructions (--quiet: the counts only). Calls of a DIFF function are still followed: the
differences show where they are.
"""
import re
import signal
import struct
import sys

from capstone import CS_ARCH_ARM, CS_MODE_ARM, Cs

signal.signal(signal.SIGPIPE, signal.SIG_DFL)  # quiet when piped into head
MODULES, CODE = 0x10000000, 0x100000
md = Cs(CS_ARCH_ARM, CS_MODE_ARM)


class Game:
    def __init__(self, modules, code):
        self.m, self.c = open(modules, 'rb').read(), open(code, 'rb').read()

    def image(self, a):
        return (self.m, MODULES) if a >= MODULES else (self.c, CODE)

    def word(self, a):
        img, base = self.image(a)
        return struct.unpack_from('<I', img, a - base)[0] if 0 <= a - base <= len(img) - 4 else None

    def follow(self, a):
        for _ in range(3):
            if self.word(a) == 0xE51FF004:  # ldr pc, [pc, #-4]
                a = self.word(a + 4)
            else:
                break
        return a

    def body(self, a):
        img, base = self.image(a)
        out, calls = [], []
        for ins in md.disasm(img[a - base:a - base + 0x1000], a):
            text = ins.mnemonic + ' ' + ins.op_str
            if ins.mnemonic in ('bl', 'blx') and ins.op_str.startswith('#'):
                calls.append(self.follow(int(ins.op_str[1:], 16)))
                text = ins.mnemonic + ' <call>'
            elif ins.mnemonic[:1] == 'b' and ins.mnemonic not in ('bic', 'bics', 'bfi', 'bfc') and ins.op_str.startswith('#'):
                text = ins.mnemonic + ' <target>'
            m = re.search(r'\[pc, #(0x[0-9a-f]+)\]', ins.op_str)
            if m:
                v = self.word(ins.address + 8 + int(m.group(1), 16))
                text = text.replace(m.group(0), '=' + (hex(v) if v is not None and v < CODE else '<address>'))
            out.append(text)
            if (ins.mnemonic.startswith('pop') and 'pc' in ins.op_str) or (ins.mnemonic == 'bx' and ins.op_str == 'lr'):
                break
        return out, calls


def compare(oras, xy, o, x, depth, most, quiet, seen, label='function'):
    if (o, x) in seen or depth > most:
        return
    seen.add((o, x))
    a, ac = oras.body(o)
    b, bc = xy.body(x)
    alike = sum(1 for p, q in zip(a, b) if p == q)
    pad = '  ' * depth
    print(f'{pad}{"SAME" if a == b else "DIFF"} {label} oras {o:#x} / x {x:#x}: {alike} of {max(len(a), len(b))} alike')
    if a != b and not quiet:
        for k, (p, q) in enumerate(zip(a, b)):
            if p != q:
                print(f'{pad}    {k:3} oras {p:<40} x {q}')
        if len(a) != len(b):
            print(f'{pad}    lengths {len(a)} and {len(b)}')
    for k, (p, q) in enumerate(zip(ac, bc)):
        compare(oras, xy, p, q, depth + 1, most, quiet, seen, f'call {k}')


def main():
    a = sys.argv[1:]
    if len(a) < 2:
        sys.exit(__doc__)
    most, quiet = 3, '--quiet' in a
    if '--depth' in a:
        most = int(a[a.index('--depth') + 1])
    work = a[0]
    oras = Game(f'{work}/linked/modules.bin', f'{work}/dumps/code.bin')
    xy = Game(f'{work}/xlinked/modules.bin', f'{work}/dumps/xcode.bin')
    seen = set()
    for pair in a[1:]:
        if ':' in pair:
            o, x = (int(v, 16) for v in pair.split(':'))
            compare(oras, xy, o, x, 0, most, quiet, seen)


if __name__ == '__main__':
    main()
