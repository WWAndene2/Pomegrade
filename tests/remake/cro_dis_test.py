#!/usr/bin/env python3
"""cro_dis.py (tools/remake/prototype) on a CRO written here to the loader's layout (Azahar's cro_helper.h), no game data:
a code segment calling an exported function and an imported one, a data word relocated to a rodata string.
  python3 tests/remake/cro_dis_test.py
"""
import os
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, "..", "..", "tools", "remake", "prototype", "cro_dis.py")


def build():
    fields = {}
    data = bytearray(0x138)
    code_off = 0x200
    # code: 0x200 push {lr}; 0x204 bl 0x210 (local export); 0x208 bl 0x208 (patched by the import); 0x20C pop {pc}
    #       0x210 bx lr
    code = struct.pack("<5I", 0xE92D4000, 0xEB000001, 0xEBFFFFFE, 0xE8BD8000, 0xE12FFF1E)
    data += bytes(code_off - len(data)) + code
    rodata_off = len(data)
    data += b"title\0\0\0"
    data_off = len(data)
    data += struct.pack("<I", 0)  # relocated to rodata + 0
    strings = len(data)
    data += b"Dll\0Entry\0Other\0play_motion\0"
    seg = len(data)
    data += struct.pack("<9I", code_off, len(code), 0, rodata_off, 8, 1, data_off, 4, 2)
    exp = len(data)
    data += struct.pack("<II", strings + 4, (0x210 - code_off) << 4 | 0)  # "Entry" at code + 0x10
    ext = len(data)
    data += struct.pack("<IBBBBI", (0x208 - code_off) << 4 | 0, 28, 1, 0, 0, 0)  # the bl at 0x208, batch end
    imp = len(data)
    data += struct.pack("<II", strings + 16, ext)  # named import "play_motion"
    mod = len(data)
    data += struct.pack("<5I", strings + 10, 0, 0, 0, 0)  # module "Other", nothing indexed
    rel = len(data)
    data += struct.pack("<IBBBBI", 0 << 4 | 2, 2, 1, 0, 0, 0)  # data segment word 0 -> rodata + 0
    fields.update(Magic=0x304F5243, FileSize=len(data), ModuleNameOffset=strings, ModuleNameSize=4,
                  SegmentTableOffset=seg, SegmentNum=3, ExportNamedSymbolTableOffset=exp, ExportNamedSymbolNum=1,
                  ImportModuleTableOffset=mod, ImportModuleNum=1, ExternalRelocationTableOffset=ext,
                  ExternalRelocationNum=1, ImportNamedSymbolTableOffset=imp, ImportNamedSymbolNum=1,
                  InternalRelocationTableOffset=rel, InternalRelocationNum=1,
                  UnkSegmentTag=0xFFFFFFFF, OnLoadSegmentTag=0xFFFFFFFF, OnExitSegmentTag=0xFFFFFFFF,
                  OnUnresolvedSegmentTag=0xFFFFFFFF)
    sys.path.insert(0, os.path.dirname(TOOL))
    import cro_dis
    for name, value in fields.items():
        struct.pack_into("<I", data, 0x80 + 4 * cro_dis.HEADER_FIELDS.index(name), value)
    return data


def run(path, *args):
    return subprocess.run([sys.executable, TOOL, path, *args], capture_output=True, text=True, check=True).stdout


def main():
    ok = True

    def check(cond, what):
        nonlocal ok
        print(f"{what}: {'yes' if cond else 'NO'}")
        ok = ok and cond

    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "Dll.cro")
        open(path, "wb").write(build())
        info = run(path, "info")
        check("module Dll" in info and "segment 0 code: 0x200, 20 bytes" in info, "info: module name and segments")
        symbols = run(path, "symbols")
        check("0x210 code export Entry" in symbols, "the named export at its segment tag")
        check("import play_motion: 1 site(s) 0x208" in symbols, "the named import and its relocation site")
        try:
            import capstone  # noqa: F401
        except ImportError:
            print("capstone missing: dis/xref not checked")
            return 0 if ok else 1
        dis = run(path, "dis", "0x200", "5")
        check("Entry:" in dis and "import play_motion" in dis, "disassembly names the export and the imported call")
        xref = run(path, "xref", "Entry")
        check("call at 0x204" in xref, "xref finds the call to the export")
        check("import play_motion used at 0x208" in run(path, "xref", "play_motion"), "xref finds the import's site")
        word = run(path, "dis", "0x21C", "1")  # the data segment's word: rodata (0x214) + 0
        check("-> 0x214" in word and "00000214" in word, "the internal relocation written as the loader would")
    print("all passed" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
