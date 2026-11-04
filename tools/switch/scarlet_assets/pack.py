# Scarlet's file system: arc/data.trpfd (the index: file hashes, pack names, file -> pack) and arc/data.trpfs (the
# packs, each a FlatBuffer of files, mostly Oodle Kraken compressed). Files are found by the FNV-1a 64 hash of their
# path, e.g. field_graphic/tr_share_tex/<name>/<name>.bntx (format checked on the fir textures, 4 November).
# Oodle is decompressed in-process by ooz (github.com/powzix/ooz, GPL-3.0) as a shared library, built by
# ../ooz_linux/build_libooz.sh; FlatBuffers are read by ../flatbuffer_reader.py.
import sys, struct, ctypes, os
sys.path.insert(0, os.path.dirname(__file__)); sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import xci
import flatbuffer_reader as fb
OOZ = ctypes.CDLL(os.environ.get('OOZ_LIB', os.path.join(os.path.dirname(__file__), '..', 'ooz_linux', 'build', 'libooz.so')))
CACHE = os.environ.get('SCARLET_CACHE', os.path.join(xci.DUMP, 'scarlet_cache'))

def fnv(s):
    h = 0xcbf29ce484222645
    for c in s.encode(): h = ((h ^ c) * 0x100000001b3) & 0xffffffffffffffff
    return h

class Packs:
    def __init__(self):
        self.r = xci.open_romfs()
        os.makedirs(CACHE, exist_ok=True)
        fd_path, tab_path = os.path.join(CACHE, 'data.trpfd'), os.path.join(CACHE, 'trpfs_table.bin')
        if not os.path.exists(fd_path):
            open(fd_path, 'wb').write(self.r.read('arc/data.trpfd'))
        if not os.path.exists(tab_path):
            magic, table_off = struct.unpack('<8sQ', self.r.read('arc/data.trpfs', 0, 16))
            assert magic == b'ONEPACK\0', magic
            size = self.r.files['arc/data.trpfs'][1]
            open(tab_path, 'wb').write(self.r.read('arc/data.trpfs', table_off, size - table_off))
        fd = fb.root(open(fd_path, 'rb').read())
        self.names = fd.strings(1); self.pinfo = fd.tables(3)
        self.file_pack = {h: fi.scalar(0, '<Q') for h, fi in zip(fd.scalars(0, 'Q'), fd.tables(2))}
        t = fb.root(open(tab_path, 'rb').read())
        self.off = dict(zip(t.scalars(0, 'Q'), t.scalars(1, 'Q')))
        self._cache = {}

    def files(self, name):
        """The files of one pack (by its name in the index): [(path hash, decompressed bytes)]."""
        i = self.names.index(name); size = self.pinfo[i].scalar(0, '<Q')
        p = fb.root(self.r.read('arc/data.trpfs', self.off[fnv(name)], size)); out = []
        for h, f in zip(p.scalars(0, 'Q'), p.tables(1)):
            buf = f.bytes_(4); dsize = f.scalar(3, '<Q')
            if f.scalar(1, '<b') == 3:  # Oodle
                dst = ctypes.create_string_buffer(dsize + 64)
                n = OOZ.ooz_decompress(buf, len(buf), dst, dsize)
                if n != dsize: raise ValueError(f'ooz: {n} of {dsize} bytes in {name}')
                buf = dst.raw[:dsize]
            out.append((h, buf))
        return out

    def file(self, path):
        """One file by its path, through the index's file -> pack table (None if the index has no such path)."""
        h = fnv(path)
        if h not in self.file_pack: return None
        name = self.names[self.file_pack[h]]
        if name not in self._cache: self._cache = {name: dict(self.files(name))}
        return self._cache[name].get(h)
