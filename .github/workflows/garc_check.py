# TEMPORARY diagnostic (to be deleted once read): how the game's a/0/3/9 is laid out, and whether a rewrite with
# Garc::Write's rules gives back the same bytes
import struct, sys
d = open(sys.argv[1], 'rb').read()
u32 = lambda o: struct.unpack_from('<I', d, o)[0]
hs, ds, fs, largest = u32(4), u32(0x10), u32(0x14), u32(0x18)
print('header', hs, 'data start', ds, hex(ds), 'size', fs, len(d), 'largest', largest)
fato = hs; count = struct.unpack_from('<H', d, fato + 8)[0]; fatb = fato + u32(fato + 4)
print('fato pad', hex(struct.unpack_from('<H', d, fato + 10)[0]), 'count', count, 'fatb count', u32(fatb + 8))
entries = []; at = fatb + 12
for i in range(count):
    mask = u32(at); at += 4; subs = []
    for b in range(32):
        if mask >> b & 1: subs.append(struct.unpack_from('<3I', d, at)); at += 12
    entries.append((mask, subs))
fimb = at; print('fimb', d[fimb:fimb+4], 'fimb hdr', u32(fimb+4), u32(fimb+8))
mods = {}; pads = {}; lz = 0
for i, (m, subs) in enumerate(entries):
    for s, e, l in subs:
        mods[(ds + s) % 128] = mods.get((ds + s) % 128, 0) + 1
        pads[e - s - l] = pads.get(e - s - l, 0) + 1
        if d[ds + s] == 0x11: lz += 1
print('start % 128:', sorted(mods.items())[:12], 'padding sizes:', sorted(pads.items())[:12], 'LZ11-looking members', lz)
print('padding bytes of member 0:', d[ds + entries[0][1][0][0] + entries[0][1][0][2]: ds + entries[0][1][0][1]].hex())
for i in (5, 6, 7, 8):
    s, e, l = entries[i][1][0]; print('member', i, 'start', s, 'end', e, 'len', l, 'first bytes', d[ds+s:ds+s+8].hex())
