# Lists the files of an XCI's secure partition (the NCAs) with their absolute offsets and sizes,
# so one NCA can be cut out without extracting the whole cartridge. Usage: python3 xci_contents.py <xci>
import struct, sys

def hfs0(f, base):
    f.seek(base)
    magic, count, strings_size, _ = struct.unpack('<4sIII', f.read(16))
    assert magic == b'HFS0', hex(base)
    entries = [struct.unpack('<QQIIQ32s', f.read(64)) for _ in range(count)]
    strings = f.read(strings_size)
    data = base + 16 + 64 * count + strings_size
    for offset, size, name_offset, *_ in entries:
        name = strings[name_offset:strings.index(b'\0', name_offset)].decode()
        yield name, data + offset, size

with open(sys.argv[1], 'rb') as f:
    f.seek(0x100)
    assert f.read(4) == b'HEAD'
    f.seek(0x130)
    root = struct.unpack('<Q', f.read(8))[0]
    secure = dict((name, offset) for name, offset, _ in hfs0(f, root))['secure']
    for name, offset, size in hfs0(f, secure):
        print(hex(offset), size, name)
