#!/usr/bin/env python3
"""The classes of a running game's objects, from a heap dump, by their RTTI names (25 October: how X's and Omega Ruby's
action command tables, effect kinds, move codes and models were matched, ORAS_ENGINE.md 0.0).

  heap_classes.py table <heap.bin> <modules.bin> <field base> <address> <count>   each entry's object -> its class
  heap_classes.py find  <heap.bin> <modules.bin> <field base> <class name>        the objects of a class, who points at them
  heap_classes.py refs  <heap.bin> <address>                                      the words holding an address

heap.bin: a retro_host `dump 0x08000000 0x4000000 heap.bin` (the heap at 0x08000000). modules.bin: the game's CROs linked
at 0x10000000 (cro_link.py), DllField's linked base 0x10242000 (Omega Ruby) or 0x101F9000 (X). field base: where
DllField runs in that dump (Omega Ruby 0x6F3000, X 0x6A5000, a mod's moves: its MoveCodeNone object's vtable minus the
linked one). A class of the static code (.code) is named too when --code <code.bin> is given (its base 0x100000).
"""
import argparse
import struct

HEAP_BASE, LINKED_BASE, CODE_BASE = 0x08000000, 0x10000000, 0x100000


class Image:
    def __init__(self, data, base):
        self.data, self.base = data, base

    def word(self, address):
        at = address - self.base
        return struct.unpack_from('<I', self.data, at)[0] if 0 <= at <= len(self.data) - 4 else None

    def text(self, address):
        at = address - self.base
        return self.data[at:at + 96].split(b'\0')[0].decode(errors='replace') if 0 <= at < len(self.data) else None


class Namer:
    def __init__(self, heap, modules, field_run, field_linked, code=None):
        self.heap, self.modules, self.code = heap, modules, code
        self.field_run, self.field_linked = field_run, field_linked

    def vtable_image(self, vtable):
        # a run-time vtable -> (image, the address in it)
        if self.field_run <= vtable < self.field_run + 0x200000:
            return self.modules, vtable - self.field_run + self.field_linked
        if self.code and CODE_BASE <= vtable < CODE_BASE + len(self.code.data):
            return self.code, vtable
        return None, None

    def name(self, obj):
        vtable = self.heap.word(obj)
        if vtable is None:
            return None
        image, at = self.vtable_image(vtable)
        if not image:
            return None
        type_info = image.word(at - 4)
        name = image.word(type_info + 4) if type_info else None
        text = image.text(name) if name else None
        return text if text and text[:1].isalnum() else None


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('mode', choices=['table', 'find', 'refs'])
    p.add_argument('args', nargs='+')
    p.add_argument('--code', help="the game's .code (code.bin), to name its classes too")
    p.add_argument('--field-linked', type=lambda v: int(v, 0), default=0x10242000,
                   help="DllField's linked base (Omega Ruby 0x10242000, the default; X 0x101F9000)")
    a = p.parse_args()

    heap = Image(open(a.args[0], 'rb').read(), HEAP_BASE)
    if a.mode == 'refs':
        target = int(a.args[1], 0)
        words = struct.unpack_from('<%dI' % (len(heap.data) // 4), heap.data)
        for i, w in enumerate(words):
            if w == target:
                print(hex(HEAP_BASE + 4 * i))
        return
    modules = Image(open(a.args[1], 'rb').read(), LINKED_BASE)
    code = Image(open(a.code, 'rb').read(), CODE_BASE) if a.code else None
    namer = Namer(heap, modules, int(a.args[2], 0), a.field_linked, code)

    if a.mode == 'table':
        table, count = int(a.args[3], 0), int(a.args[4], 0)
        for k in range(count):
            obj = heap.word(table + 4 * k)
            print('%s %s %s' % (hex(k), hex(obj) if obj else None, namer.name(obj) if obj else None))
    else:
        wanted = a.args[3]
        words = struct.unpack_from('<%dI' % (len(heap.data) // 4), heap.data)
        for i in range(len(words)):
            obj = HEAP_BASE + 4 * i
            n = namer.name(obj)
            if n and wanted in n:
                refs = [hex(HEAP_BASE + 4 * j) for j, w in enumerate(words) if w == obj][:8]
                print(hex(obj), n, 'pointed at from', ' '.join(refs) or 'nowhere in the heap')


if __name__ == '__main__':
    main()
