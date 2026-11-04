# Reads Scarlet's romfs files straight from the owner's XCI, decrypting only the ranges asked for: the game's 11 GB
# of data is never extracted to disk. SCARLET_DUMP is the folder holding scarlet.xci and prod.keys (the owner's own
# dump and keys; never committed, uploaded or put in an artifact, CLAUDE.md section 7).
# Formats: XCI (HFS0 partitions), NCA3 (AES-XTS header, key area, AES-CTR sections), RomFS (IVFC level 6).
import struct, re, os
from Crypto.Cipher import AES
DUMP = os.environ.get('SCARLET_DUMP', '.')

def keys():
    k = {}
    for line in open(os.path.join(DUMP, 'prod.keys')):
        m = re.match(r'\s*(\w+)\s*=\s*([0-9a-fA-F]+)', line)
        if m: k[m.group(1)] = bytes.fromhex(m.group(2))
    return k
K = keys()

def xts_decrypt(data, key, sector0, sector_size=0x200):
    k1, k2 = AES.new(key[:16], AES.MODE_ECB), AES.new(key[16:], AES.MODE_ECB)
    out = bytearray()
    for s in range(len(data) // sector_size):
        tweak = bytearray(k2.encrypt((sector0 + s).to_bytes(16, 'big')))
        blk = data[s * sector_size:(s + 1) * sector_size]
        for i in range(0, sector_size, 16):
            x = bytes(a ^ b for a, b in zip(blk[i:i + 16], tweak))
            out += bytes(a ^ b for a, b in zip(k1.decrypt(x), tweak))
            carry = 0
            for j in range(16):  # multiply tweak by x in GF(2^128), little-endian
                nc = tweak[j] >> 7; tweak[j] = ((tweak[j] << 1) | carry) & 0xff; carry = nc
            if carry: tweak[0] ^= 0x87
    return bytes(out)

class Xci:
    def __init__(self, path):
        self.f = open(path, 'rb')
    def read(self, off, size):
        self.f.seek(off); return self.f.read(size)
    def hfs0(self, off):
        magic, n, strsize = struct.unpack('<4sII', self.read(off, 12))
        assert magic == b'HFS0'
        ents = [struct.unpack('<QQIIII32s', self.read(off + 16 + i * 0x40, 0x40)) for i in range(n)]
        strs = self.read(off + 16 + n * 0x40, strsize)
        data = off + 16 + n * 0x40 + strsize
        return {strs[e[2]:strs.index(b'\0', e[2])].decode(): (data + e[0], e[1]) for e in ents}
    def secure(self):
        root = self.hfs0(0xf000)
        return self.hfs0(root['secure'][0])

class Nca:
    def __init__(self, xci, off, size):
        self.x, self.off, self.size = xci, off, size
        h = xts_decrypt(xci.read(off, 0xc00), K['header_key'], 0)
        assert h[0x200:0x204] == b'NCA3', h[0x200:0x204]
        self.h = h
        kg = max(h[0x206], h[0x220]); kg = kg - 1 if kg else 0
        kaek = K['key_area_key_application_%02x' % kg]
        self.key = AES.new(kaek, AES.MODE_ECB).decrypt(h[0x300:0x340])[0x20:0x30]
        self.content_type = h[0x205]
        self.sections = []
        for i in range(4):
            start, end = struct.unpack('<II', h[0x240 + i * 16:0x248 + i * 16])
            if end: self.sections.append((i, start * 0x200, end * 0x200, h[0x400 + i * 0x200:0x600 + i * 0x200]))
    def section_reader(self, idx):
        i, start, end, fs = [s for s in self.sections if s[0] == idx][0]
        crypt, hashtype = fs[4], fs[3]
        ctr_hi = fs[0x140:0x148][::-1]
        def read(off, size):  # off relative to the section
            a = off & ~0xf; b = (off + size + 15) & ~0xf
            raw = self.x.read(self.off + start + a, b - a)
            ctr = ctr_hi + ((start + a) >> 4).to_bytes(8, 'big')
            dec = AES.new(self.key, AES.MODE_CTR, initial_value=ctr, nonce=b'').decrypt(raw)
            return dec[off - a:off - a + size]
        return read, fs

class RomFs:
    def __init__(self, read, base):
        self.r, self.base = read, base
        hdr = struct.unpack('<10Q', read(base, 80))
        _, dh_off, dh_sz, dm_off, dm_sz, fh_off, fh_sz, fm_off, fm_sz, self.data = hdr
        dm = read(base + dm_off, dm_sz); fm = read(base + fm_off, fm_sz)
        dirs = {}
        o = 0
        while o < len(dm):
            parent, sib, child, cfile, nxt, nlen = struct.unpack('<6I', dm[o:o + 24])
            dirs[o] = (parent, dm[o + 24:o + 24 + nlen].decode())
            o += 24 + ((nlen + 3) & ~3)
        def path(d):
            p = []
            while d in dirs and d != 0:
                p.append(dirs[d][1]); d = dirs[d][0]
            return '/'.join(reversed(p))
        self.files = {}
        o = 0
        while o < len(fm):
            parent, sib, off, size, nxt, nlen = struct.unpack('<IIQQII', fm[o:o + 32])
            name = fm[o + 32:o + 32 + nlen].decode()
            self.files[(path(parent) + '/' + name).lstrip('/')] = (off, size)
            o += 32 + ((nlen + 3) & ~3)
    def read(self, name, off=0, size=None):
        fo, fs = self.files[name]
        if size is None: size = fs - off
        return self.r(self.base + self.data + fo + off, size)

def open_romfs():
    x = Xci(os.path.join(DUMP, 'scarlet.xci'))
    for name, (off, size) in sorted(x.secure().items(), key=lambda kv: -kv[1][1]):
        if not name.endswith('.nca') or name.endswith('.cnmt.nca'): continue
        nca = Nca(x, off, size)
        if nca.content_type != 0: continue  # program
        for idx, s, e, fs in nca.sections:
            if fs[3] == 3:  # IVFC hash = romfs
                read, fs = nca.section_reader(idx)
                level5 = struct.unpack('<Q', fs[0x8 + 0x10 + 5 * 0x18:0x8 + 0x10 + 5 * 0x18 + 8])[0]
                return RomFs(read, level5)
    raise RuntimeError('no romfs')

if __name__ == '__main__':
    # lists the romfs's largest files: a check that the dump and keys are read
    r = open_romfs()
    for n, (o, s) in sorted(r.files.items(), key=lambda kv: -kv[1][1])[:15]:
        print(f'{s:>14,} {n}')
    print(len(r.files), 'files')
