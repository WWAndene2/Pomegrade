#!/usr/bin/env python3
# A Pokemon of Omega Ruby as the player's field character (owner's request, 2 November: Torchic walking in place of the
# player). Writes an a/0/2/1 member ("MM" container: model, motion pack, material animation) to put over the player's
# characters 171 (May) and 172 (Brendan) with `remake_tool oras-copy ... a/0/2/1 171=file:<out> 172=file:<out>`.
#
#   pokemon_player.py <remake_tool> <spica> <oras.3ds> <work dir> <model member> <out member>
#
# <model member>: the Pokemon form's model in a/0/0/8 (its PC member holding the BCH model; Torchic 2827). A form's
# block is 8 members; from the model, +2 is its textures (PT) and +6 its Pokemon-Amie motions (PK) (seen on Torchic,
# 2826-2833, and Giratina, ORAS_TITLE.md 1).
# - model: the form's BCH model and its textures merged by `spica bch`, renamed after Brendan's model and given his
#   field material (shader FieldChar instead of the battle's PokePack, which the field does not load) and his LUT table
#   (`--material-from`); without both the game stops binding the model (Gfx_ModelBinding_Update, DllField 0x102C27FC).
# - motions: Brendan's pack of 239 slots, slot 0 the Pokemon's skeleton; each slot Brendan fills takes the Pokemon's
#   Amie move (kw33_moveA01) when it lasts 14 to 16 frames (his walks and runs, by length; GfMotionPack.h), else its
#   Amie wait (kw01_wait01). ORAS has no walk or run for a Pokemon: the move is its hop in Pokemon-Amie.
# - material animation: Brendan's (blinking), kept for the container's third file; it names materials the Pokemon
#   lacks (inferred harmless: the game ran with it).
# Checked headless (2 November, Torchic): loads, turns and walks in Littleroot. Not checked: the motions while moving,
# the bike, surf and the other player motions (all the wait), the phone.
import os
import re
import struct
import subprocess
import sys

BRENDAN = 172


def run(*args):
    subprocess.run([str(a) for a in args], check=True, stdout=subprocess.DEVNULL)


def container(data):
    count = struct.unpack_from('<H', data, 2)[0]
    offs = struct.unpack_from('<%dI' % (count + 1), data, 4)
    return [data[offs[k]:offs[k + 1]] for k in range(count)]


def build_container(magic, files, align=0x80):
    out = bytearray((4 + 4 * (len(files) + 1) + align - 1) // align * align)
    offs = []
    for f in files:
        while len(out) % align:
            out.append(0)
        offs.append(len(out))
        out += f
    offs.append(len(out))
    out[0:2] = magic
    struct.pack_into('<H', out, 2, len(files))
    struct.pack_into('<%dI' % len(offs), out, 4, *offs)
    return bytes(out)


def pack_slots(pack):
    count = struct.unpack_from('<I', pack, 0)[0]
    offs = struct.unpack_from('<%dI' % (count + 1), pack, 4)

    def slot(k):
        if not offs[k]:
            return None
        end = next((offs[j] for j in range(k + 1, count) if offs[j]), offs[count])
        return pack[offs[k]:end]
    return count, slot


def main():
    if len(sys.argv) != 7:
        sys.exit(__doc__ or 'usage: pokemon_player.py <remake_tool> <spica> <oras.3ds> <work dir> <model member> <out member>')
    tool, spica, rom, work, model, out = sys.argv[1:]
    model = int(model)
    os.makedirs(work, exist_ok=True)
    w = lambda name: os.path.join(work, name)
    run(tool, 'oras-member', rom, 'a/0/0/8', model, 0, w('model.bch'))
    run(tool, 'oras-member', rom, 'a/0/0/8', model + 2, 0, w('textures.bch'))
    run(tool, 'oras-member', rom, 'a/0/0/8', model + 6, -1, w('amie.bin'))
    run(tool, 'oras-member', rom, 'a/0/2/1', BRENDAN, -1, w('brendan.bin'))
    run(spica, 'bch', w('player.bch'), w('model.bch'), w('textures.bch'), '--rename', 'rstr0002_00_fi',
        '--material-from', w('brendan.bin'))

    amie = container(open(w('amie.bin'), 'rb').read())
    brendan = container(open(w('brendan.bin'), 'rb').read())
    # an Amie pack: file 0 the motions, file k + 1 the BCH animation of slot k, named <form>_kwNN_<name>
    names = {}
    for k in range(1, len(amie) - 1):
        m = re.search(rb'_(kw\d\d_[A-Za-z0-9]+)', amie[k + 1])
        if m:
            names.setdefault(m.group(1).decode(), k)
    _, pslot = pack_slots(amie[0])
    for need in ('kw01_wait01', 'kw33_moveA01'):
        if need not in names or pslot(names[need]) is None:
            sys.exit(f'model {model}: no {need} in its Amie motions ({sorted(names)})')
    wait, move = pslot(names['kw01_wait01']), pslot(names['kw33_moveA01'])

    count, bslot = pack_slots(brendan[1])
    slots = []
    for k in range(count):
        b = bslot(k)
        if k == 0:
            slots.append(pslot(0))
        elif b is None:
            slots.append(None)
        else:
            slots.append(move if struct.unpack_from('<H', b, 2)[0] in (14, 15, 16) else wait)
    head = 4 + 4 * (count + 1)
    data, offs = bytearray(), []
    for s in slots:
        if s is None:
            offs.append(0)
            continue
        while (head + len(data)) % 4:
            data.append(0)
        offs.append(head + len(data))
        data += s
    pack = struct.pack('<I', count) + struct.pack('<%dI' % (count + 1), *offs, head + len(data)) + data

    member = build_container(b'MM', [open(w('player.bch'), 'rb').read(), pack, brendan[2]])
    open(out, 'wb').write(member)
    print(f'{out}: model {model} as the player, {len(member)} bytes')


if __name__ == '__main__':
    main()
