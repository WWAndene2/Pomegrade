#!/usr/bin/env python3
"""A 3DS code module (CRO, or the static module static.crs) read as the loader reads it: its segments, the symbols it
exports and imports, its relocations applied, and its code disassembled with those names, so that a function and its
calls into other modules can be followed (ORAS_TITLE.md: which code animates the title's Pokemon).

  cro_dis.py <module.cro> [other modules...] info
  cro_dis.py <module.cro> [others...] symbols
  cro_dis.py <module.cro> [others...] dis <address|symbol> [count]
  cro_dis.py <module.cro> [others...] xref <address|symbol|imported name>
  cro_dis.py <module.cro> [others...] functions

The layout is Azahar's loader (azahar/src/core/hle/service/ldr_ro/cro_helper.h): a 0x80-byte hash, then the header's
u32 fields; a segment tag is a segment index (low 4 bits) and an offset into it (the rest). In the file as stored
(not rebased), table and segment offsets are file offsets, so an address here is a file offset (the .bss segment, absent
from the file, is placed after it). Other modules given after the first name the symbols it imports by index or anonymous
position (named imports carry their name). Relocations: types 2 and 38 write symbol + addend, 3 writes it relative to the
patched word, 10, 28 and 29 are branches (the Azahar loader leaves them unimplemented; they are shown, not applied).
Needs capstone (pip install capstone).
"""
import argparse
import struct
import sys

HEADER_FIELDS = [
    "Magic", "NameOffset", "NextCRO", "PreviousCRO", "FileSize", "BssSize", "FixedSize", "UnknownZero",
    "UnkSegmentTag", "OnLoadSegmentTag", "OnExitSegmentTag", "OnUnresolvedSegmentTag",
    "CodeOffset", "CodeSize", "DataOffset", "DataSize", "ModuleNameOffset", "ModuleNameSize",
    "SegmentTableOffset", "SegmentNum",
    "ExportNamedSymbolTableOffset", "ExportNamedSymbolNum", "ExportIndexedSymbolTableOffset", "ExportIndexedSymbolNum",
    "ExportStringsOffset", "ExportStringsSize", "ExportTreeTableOffset", "ExportTreeNum",
    "ImportModuleTableOffset", "ImportModuleNum", "ExternalRelocationTableOffset", "ExternalRelocationNum",
    "ImportNamedSymbolTableOffset", "ImportNamedSymbolNum", "ImportIndexedSymbolTableOffset", "ImportIndexedSymbolNum",
    "ImportAnonymousSymbolTableOffset", "ImportAnonymousSymbolNum", "ImportStringsOffset", "ImportStringsSize",
    "StaticAnonymousSymbolTableOffset", "StaticAnonymousSymbolNum", "InternalRelocationTableOffset",
    "InternalRelocationNum", "StaticRelocationTableOffset", "StaticRelocationNum",
]
HASH_SIZE = 0x80
SEGMENT_TYPES = {0: "code", 1: "rodata", 2: "data", 3: "bss"}
RELOCATION_TYPES = {0: "none", 2: "abs", 3: "rel", 10: "thumb-branch", 28: "arm-branch", 29: "arm-branch-mod",
                    38: "abs2", 42: "aligned-rel"}


class Cro:
    def __init__(self, path):
        self.path = path
        self.data = bytearray(open(path, "rb").read())
        if self.data[HASH_SIZE:HASH_SIZE + 4] not in (b"CRO0", b"FIXD"):
            raise ValueError(f"{path}: not a CRO (no CRO0 at 0x80)")
        self.h = {name: self.u32(HASH_SIZE + 4 * i) for i, name in enumerate(HEADER_FIELDS)}
        self.name = self.cstr(self.h["ModuleNameOffset"])
        self.segments = []  # (address, size, type)
        for i in range(self.h["SegmentNum"]):
            off, size, kind = struct.unpack_from("<III", self.data, self.h["SegmentTableOffset"] + 12 * i)
            if kind == 3:  # .bss: not in the file, placed after it
                off = (len(self.data) + 0xFFF) & ~0xFFF
            self.segments.append((off, size, kind))
        self.labels = {}       # address -> name
        self.patches = {}      # patched address -> description
        self.imports = []      # (name, module, [(target address, type, addend)])
        self.exports_indexed = []
        for i in range(self.h["ExportNamedSymbolNum"]):
            name_off, tag = struct.unpack_from("<II", self.data, self.h["ExportNamedSymbolTableOffset"] + 8 * i)
            self.labels[self.tag(tag)] = self.cstr(name_off)
        for i in range(self.h["ExportIndexedSymbolNum"]):
            tag = self.u32(self.h["ExportIndexedSymbolTableOffset"] + 4 * i)
            self.exports_indexed.append(self.tag(tag))
            self.labels.setdefault(self.tag(tag), f"{self.name}#{i}")
        for k, field in (("OnLoad", "OnLoadSegmentTag"), ("OnExit", "OnExitSegmentTag"),
                         ("OnUnresolved", "OnUnresolvedSegmentTag"), ("Unk", "UnkSegmentTag")):
            if self.h[field] != 0xFFFFFFFF and self.tag(self.h[field]) is not None:
                self.labels.setdefault(self.tag(self.h[field]), f"{self.name}::{k}")

    def u32(self, at):
        return struct.unpack_from("<I", self.data, at)[0]

    def cstr(self, at):
        end = self.data.index(0, at)
        return self.data[at:end].decode("ascii", "replace")

    def tag(self, raw):
        index, offset = raw & 0xF, raw >> 4
        if index >= len(self.segments) or offset > self.segments[index][1]:
            return None
        return self.segments[index][0] + offset

    def segment_of(self, address):
        for off, size, kind in self.segments:
            if off <= address < off + size:
                return SEGMENT_TYPES.get(kind, str(kind))
        return None

    def batch(self, at):
        """the relocations of a batch (ExternalRelocationTable entries until is_batch_end)"""
        out = []
        while True:
            tag, kind, end, _resolved, _pad, addend = struct.unpack_from("<IBBBBI", self.data, at)
            out.append((self.tag(tag), kind, addend))
            if end:
                return out
            at += 12

    def read_imports(self, others):
        by_name = {o.name: o for o in others}
        h = self.h
        for i in range(h["ImportNamedSymbolNum"]):
            name_off, batch = struct.unpack_from("<II", self.data, h["ImportNamedSymbolTableOffset"] + 8 * i)
            self.imports.append((self.cstr(name_off), None, self.batch(batch)))
        for m in range(h["ImportModuleNum"]):
            name_off, idx_off, idx_num, anon_off, anon_num = struct.unpack_from("<5I", self.data, h["ImportModuleTableOffset"] + 20 * m)
            module = self.cstr(name_off)
            other = by_name.get(module)
            for i in range(idx_num):
                index, batch = struct.unpack_from("<II", self.data, idx_off + 8 * i)
                name = f"{module}#{index}"
                if other and index < len(other.exports_indexed):
                    name = other.labels.get(other.exports_indexed[index], name)
                self.imports.append((name, module, self.batch(batch)))
            for i in range(anon_num):
                tag, batch = struct.unpack_from("<II", self.data, anon_off + 8 * i)
                name = f"{module}@seg{tag & 0xF}+0x{tag >> 4:X}"
                if module == "|static|" and tag & 0xF == 0:
                    # the static module is the program itself: its segment 0 is the .code loaded at 0x100000, so the
                    # name matches code_find.py's addresses in `remake_tool oras-code`'s code.bin
                    name = f"code:0x{0x100000 + (tag >> 4):X}"
                if other:
                    address = other.tag(tag)
                    if address is not None and address in other.labels:
                        name = f"{module}::{other.labels[address]}"
                    elif address is not None:
                        name = f"{module}:0x{address:X}"
                self.imports.append((name, module, self.batch(batch)))
        for name, _module, relocations in self.imports:
            for target, kind, addend in relocations:
                if target is not None:
                    self.patches[target] = f"import {name}" + (f"+0x{addend:X}" if addend else "") + f" ({RELOCATION_TYPES.get(kind, kind)})"
        # internal relocations: written as the loader would, with this file's addresses
        for i in range(h["InternalRelocationNum"]):
            tag, kind, seg, _p1, _p2, addend = struct.unpack_from("<IBBBBI", self.data, h["InternalRelocationTableOffset"] + 12 * i)
            target = self.tag(tag)
            if target is None or seg >= len(self.segments):
                continue
            symbol = self.segments[seg][0]
            if kind in (2, 38):
                struct.pack_into("<I", self.data, target, (symbol + addend) & 0xFFFFFFFF)
                self.patches.setdefault(target, f"-> 0x{symbol + addend:X}")
            elif kind == 3:
                struct.pack_into("<I", self.data, target, (symbol + addend - target) & 0xFFFFFFFF)
                self.patches.setdefault(target, f"-> 0x{symbol + addend:X} (relative)")
            else:
                self.patches.setdefault(target, f"-> 0x{symbol + addend:X} ({RELOCATION_TYPES.get(kind, kind)})")

    def resolve(self, text):
        for address, name in self.labels.items():
            if name == text:
                return address
        return int(text, 0)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="+", help="the module, other modules, then the command and its arguments")
    a = ap.parse_args()
    files = [f for f in a.files if f.lower().endswith((".cro", ".crs"))]
    rest = a.files[len(files):]
    if not files or not rest:
        ap.error("expected <module.cro> [others] <command>")
    cro = Cro(files[0])
    others = [Cro(f) for f in files[1:]]
    cro.read_imports(others)
    command, args = rest[0], rest[1:]
    if command == "info":
        print(f"module {cro.name}, {len(cro.data)} bytes, magic {cro.data[0x80:0x84].decode()}")
        for i, (off, size, kind) in enumerate(cro.segments):
            print(f"  segment {i} {SEGMENT_TYPES.get(kind, kind)}: 0x{off:X}, {size} bytes")
        for k in HEADER_FIELDS:
            if k.endswith("Num"):
                print(f"  {k[:-3]}: {cro.h[k]}")
        modules = sorted({m for _n, m, _r in cro.imports if m})
        print(f"imports from: {', '.join(modules) or '(named only)'}")
    elif command == "symbols":
        for address in sorted(cro.labels):
            print(f"0x{address:X} {cro.segment_of(address)} export {cro.labels[address]}")
        for name, module, relocations in cro.imports:
            sites = ", ".join(f"0x{t:X}" for t, _k, _a in relocations if t is not None)
            print(f"import {name}: {len(relocations)} site(s) {sites}")
    elif command in ("dis", "xref", "functions"):
        from capstone import CS_ARCH_ARM, CS_MODE_ARM, Cs
        from capstone.arm import ARM_OP_IMM
        md = Cs(CS_ARCH_ARM, CS_MODE_ARM)
        md.detail = True
        code = [(off, size) for off, size, kind in cro.segments if kind == 0]

        def name_of(address):
            if address in cro.labels:
                return cro.labels[address]
            if address in cro.patches:
                return cro.patches[address]
            return None

        def branch_target(ins):
            if ins.mnemonic.startswith(("bl", "b")) and ins.operands and ins.operands[0].type == ARM_OP_IMM:
                return ins.operands[0].imm
            return None

        def line(off):
            # words outside the code segments are data, shown as such
            ins = next(md.disasm(bytes(cro.data[off:off + 4]), off), None) if cro.segment_of(off) == "code" else None
            text = f"{ins.mnemonic} {ins.op_str}" if ins else f"(data) {cro.u32(off):08X}"
            notes = []
            if off in cro.patches:
                notes.append(cro.patches[off])
            if ins:
                target = branch_target(ins)
                if target is not None:
                    # a branch patched by an import is the import's call
                    notes.append(cro.patches.get(off) or name_of(target) or "")
                if "[pc, #" in ins.op_str:
                    lit = off + 8 + int(ins.op_str.split("[pc, #")[1].split("]")[0], 0)
                    if 0 <= lit < len(cro.data) - 3:
                        notes.append(f"=0x{cro.u32(lit):08X}" + (f" {cro.patches[lit]}" if lit in cro.patches else ""))
            label = f"{cro.labels[off]}:\n" if off in cro.labels else ""
            return label + f"  0x{off:X}: {text}" + ("  ; " + "; ".join(n for n in notes if n) if any(notes) else "")

        if command == "dis":
            start = cro.resolve(args[0])
            for off in range(start, start + 4 * (int(args[1], 0) if len(args) > 1 else 64), 4):
                print(line(off))
        elif command == "xref":
            target_text = args[0]
            target = None
            try:
                target = cro.resolve(target_text)
            except ValueError:
                pass
            for name, _module, relocations in cro.imports:
                if name == target_text or name.endswith("::" + target_text):
                    for t, _k, _a in relocations:
                        print(f"import {name} used at 0x{t:X}")
            if target is not None:
                for off, size in code:
                    for ins in md.disasm(bytes(cro.data[off:off + size]), off):
                        if branch_target(ins) == target:
                            print(f"call at 0x{ins.address:X}: {ins.mnemonic} {ins.op_str}")
                for at in range(0, len(cro.data) - 3, 4):
                    if cro.u32(at) == target and at in cro.patches:
                        print(f"pointer at 0x{at:X} ({cro.segment_of(at)})")
        else:  # functions: starts of "push {..., lr}" and branch-and-link targets
            starts = {}
            for off, size in code:
                for ins in md.disasm(bytes(cro.data[off:off + size]), off):
                    if ins.mnemonic in ("push", "stmdb") and "lr" in ins.op_str:
                        starts.setdefault(ins.address, 0)
                    t = branch_target(ins)
                    if t is not None and ins.mnemonic.startswith("bl"):
                        starts[t] = starts.get(t, 0) + 1
            for address in sorted(starts):
                print(f"0x{address:X} called {starts[address]}x {name_of(address) or ''}")
    else:
        ap.error(f"unknown command {command}")


if __name__ == "__main__":
    sys.exit(main())
