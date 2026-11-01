# Extracts files from Scarlet/Violet's asset archive (romfs arc/data.trpfs, indexed by arc/data.trpfd).
# Packs are named in the trpfd; a pack's offset in the trpfs is found by the FNV-1a 64 hash of its name.
# Each pack (.trpak) holds files named by hash; compression type 3 is Oodle, decompressed with oozdec
# (ooz_linux/), 255 is stored as is.
# Usage: python3 trpfs_extract.py <arc dir> <text in pack name> <out dir> <oozdec path>
import os, struct, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import flatbuffer_reader as fb

def fnv1a64(text):
    h = 0xcbf29ce484222645
    for c in text.encode():
        h = ((h ^ c) * 0x100000001b3) & 0xffffffffffffffff
    return h

arc, pattern, out_dir, oozdec = sys.argv[1:5]
names = fb.root(open(os.path.join(arc, 'data.trpfd'), 'rb').read()).strings(1)
with open(os.path.join(arc, 'data.trpfs'), 'rb') as f:
    assert f.read(8) == b'ONEPACK\0'
    index_offset = struct.unpack('<Q', f.read(8))[0]
    f.seek(index_offset)
    index = fb.root(f.read())
    hashes, offsets = index.scalars(0), index.scalars(1)
    by_hash = dict(zip(hashes, offsets))
    ends = sorted(offsets) + [index_offset]
    for name in (n for n in names if pattern in n):
        start = by_hash[fnv1a64(name)]
        end = next(o for o in ends if o > start)
        f.seek(start)
        pack = f.read(end - start)
        table = fb.root(pack)
        pack_dir = os.path.join(out_dir, name.replace('/', '_'))
        os.makedirs(pack_dir, exist_ok=True)
        for file_hash, entry in zip(table.scalars(0), table.tables(1)):
            size, data = entry.vec(4)
            compression, out_size = entry.scalar(1, '<B'), entry.scalar(3)
            out = os.path.join(pack_dir, '%016x' % file_hash)
            if compression == 255:
                open(out, 'wb').write(pack[data:data + size])
            elif compression == 3:
                with tempfile.NamedTemporaryFile() as block:
                    block.write(pack[data:data + size]); block.flush()
                    subprocess.run([oozdec, block.name, out, str(out_size)], check=True)
            else:
                sys.exit('unknown compression %d in %s' % (compression, name))
        print(name, len(table.scalars(0)), 'files')
