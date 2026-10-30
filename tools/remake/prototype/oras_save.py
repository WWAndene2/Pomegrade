#!/usr/bin/env python3
"""An ORAS save file ("main", 0x76000 bytes) read block by block (ORAS_ENGINE.md 0, "Run the game"): what the moved-save
tests of 8 October needed and did with throwaway scripts.

  oras_save.py blocks <save>                      the block table: index, id, offset, size, checksum ok, the game's class
  oras_save.py diff <save A> <save B>             the blocks that differ, how many bytes, and the first differing offsets
  oras_save.py characters <save>                  block 10, decoded: one line per saved character record
  oras_save.py take <save> <out> <template> <block>[,<block>...]
                                                  copies whole blocks from the template, then rewrites every checksum
  oras_save.py records <save> <out> <template> <record>[,<record>...]
                                                  copies block-10 records (0x108 bytes each) from the template
  oras_save.py move <save> <out> <slot> <index> <move>
                                                  a party Pokemon (slot 0-5) given a move at index 0-3 (its PP 15), its
                                                  data decrypted and encrypted again (28 October: Fly, 19, for a fly test)
  oras_save.py badges <save> <out> <mask>        the badges byte (Misc block 11 + 0xC, one bit a badge: 0xFF all eight)
  oras_save.py flag <save> <out> <flag>[,<flag>...]
                                                  sets event flags: 1-0xCFF in EventWork (block 19 from +0x2F0; 30 October: the
                                                  places' visited flags, for Fly), 0xD00 and up the moved ones (oras-engine --story
                                                  large: block 47's last 1000 bytes, OrasEngine.cpp), then rewrites every checksum
                                                  (26 October: a dust box spot's moved flag set in a save, run sc4)

What is read and what is only seen:
  - the party (block 18 PokePartySave, 6 slots of 0x104 bytes; checked 28 October on the owner's save: Torchic, its moves
    Scratch, Growl and Ember, its checksum matching): each slot the layout PKHeX names PK6, encrypted: its first 8 bytes
    plain (the encryption constant at +0, the checksum at +6, the sum of the decrypted words 8-0xE7), the rest XORed with
    the high halfword of the game's LCRNG (x * 0x41C64E6D + 0x6073, seeded with the constant), its four 56-byte blocks
    shuffled by ((constant >> 13) & 31) % 24; the moves at +0x5A of the decrypted data, their PP at +0x62.
  - the badges: Misc (block 11) + 0xC, a byte, next to the money at + 8 (3000 in the owner's save); with 0xFF Fly is
    accepted (run fly3).
  - the block table (checked on the owner's saves, OrasSave.cpp): at 0x75E14, one 8-byte entry per block, u32 size, u16 id,
    u16 CRC16-CCITT (initial 0xFFFF) of the block; blocks follow each other from offset 0, each on a 0x200 boundary.
  - block 10 (0x2000, 8,448 bytes; N + 1 records with oras-engine --characters N > 31, ORAS_ENGINE.md 6), not read in code: 32 records of 0x108 bytes, one per character of the zone the save was
    made in (0 bytes when unused); u16 +4 the character's number in its zone, u16 +6 its zone, u16 +8 its model, f32 +0x40
    and +0x44 its tile x and z (checked on the owner's two game saves against the zone files: zone 6's characters 0, 2, 7
    with models 289, 281, 221, character 0 at tile 104, 179; zone 24's character 0 with model 284); number 0xFF with model
    171 is the player's record (inferred). A save moved into another matrix loads only with block 10 from a save made there (runs matrix2-8); which
    of its words matter: record 1 of the Littleroot save (runs b10s1-b10s4), cause not read.
"""
import signal
import struct
import sys

signal.signal(signal.SIGPIPE, signal.SIG_DFL)  # quiet when piped into head

# the block table is at the footer's +0x14, the footer the file's last 0x200 bytes: 0x75E14 in the game's 0x76000-byte file; a
# save for oras-engine --characters N > 31 is longer (block 10 grown, OrasSave::ResizeMoveModels)
def table_at(data):
    return len(data) - 0x200 + 0x14


RECORD = 0x108
# each block's class in the game (RTTI names savedata::<class>; Save_SaveData_Construct 0x115DE0 builds them in block order,
# sizes checked against the vtables' size getters, 8 October, but for 35, placed by the order alone); 19 has no named class
CLASSES = ("Kawaigari MYITEM BAG GameTime Situation RandomGroup PlayTime Fashion MiniGame GimmickWork MoveModelSave Misc BOX "
           "BattleBox PssPersonalSaveData PssPersonalSaveData PssPersonalSaveData MyStatus PokePartySave (unnamed) ZukanData "
           "HologramMailData UnionPokemon Config KawaigariGoods AssistPowerData FieldRockData Promotion GtsData FieldMenu "
           "ProfileEnqueteData Encount BossHistory CecHistory BattleMatch AccessPointSaveData Dendou BattleHouseData Sodateya "
           "TrialHouse KinomiData WonderGiftSaveData SubEventSaveData PokeDiarySaveData Record FriendSafariSaveData "
           "TrainingSaveData ReservedSaveData EShopSaveData ProfileHistory GameSync MyPhotoIconSaveData ValidationSaveData "
           "Contest SecretBase SangoNetworkSavedata BoxPokemon PhotoSaveData").split()


def crc16(data):
    c = 0xFFFF
    for x in data:
        c ^= x << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c


def blocks(data):
    """[(index, id, offset, size, stored crc)] from the table"""
    out, offset = [], 0
    for i, at in enumerate(range(table_at(data), len(data) - 7, 8)):
        size, bid, crc = struct.unpack_from("<IHH", data, at)
        if size == 0 or offset + size > table_at(data):
            break
        out.append((i, bid, offset, size, crc))
        offset = (offset + size + 0x1FF) & ~0x1FF
    return out


def read(path):
    data = bytearray(open(path, "rb").read())
    if len(data) < 0x76000 or len(data) % 0x200 or data[table_at(data) - 4:table_at(data)] != b"FEEB":
        sys.exit(f"{path}: not an ORAS save (0x76000 bytes or a grown one, FEEB before the table)")
    return data


def write_checksums(data):
    for i, _, offset, size, _ in blocks(data):
        struct.pack_into("<H", data, table_at(data) + i * 8 + 6, crc16(data[offset:offset + size]))


def block_at(data, index):
    for b in blocks(data):
        if b[0] == index:
            return b
    sys.exit(f"no block {index}")


def cmd_blocks(path):
    data = read(path)
    for i, bid, offset, size, crc in blocks(data):
        ok = crc16(data[offset:offset + size]) == crc
        name = CLASSES[i] if i < len(CLASSES) else "?"
        print(f"block {i:2} id {bid:2} at 0x{offset:05X}, {size:6} bytes, checksum {'ok' if ok else 'WRONG'}, {name}")


def cmd_diff(pa, pb):
    a, b = read(pa), read(pb)
    for i, _, offset, size, _ in blocks(a):
        d = [k for k in range(size) if a[offset + k] != b[offset + k]]
        if d:
            print(f"block {i:2} at 0x{offset:05X} ({size} bytes): {len(d)} bytes differ, first at "
                  + " ".join(f"+0x{k:X}" for k in d[:8]))


def cmd_characters(path):
    data = read(path)
    _, _, offset, size, _ = block_at(data, 10)
    for r in range(size // RECORD):
        rec = data[offset + r * RECORD:offset + (r + 1) * RECORD]
        if not any(rec):
            continue
        number, zone, model = struct.unpack_from("<HHH", rec, 4)
        who = "player (0xFF)" if number == 0xFF else f"character {number}"
        print(f"record {r:2}: {who}, zone {zone}, model {model}; head {rec[:16].hex(' ', 2)}")


def cmd_take(path, out, template, which):
    data, t = read(path), read(template)
    for index in map(int, which.split(",")):
        _, _, offset, size, _ = block_at(data, index)
        data[offset:offset + size] = t[offset:offset + size]
    write_checksums(data)
    open(out, "wb").write(data)
    print(f"{out}: blocks {which} from {template}, checksums rewritten")


def cmd_records(path, out, template, which):
    data, t = read(path), read(template)
    _, _, offset, size, _ = block_at(data, 10)
    for r in map(int, which.split(",")):
        at = offset + r * RECORD
        data[at:at + RECORD] = t[at:at + RECORD]
    write_checksums(data)
    open(out, "wb").write(data)
    print(f"{out}: block-10 records {which} from {template}, checksums rewritten")


def cmd_flag(path, out, which):
    # flags 1-0xCFF are EventWork's (block 19); 0xD00-0x2C3F the moved ones, bit (f - 0xD00) of block 47 from its +1000
    # (block 19's flags from its +0x2F0: ORAS_ENGINE.md, "More room", checked across a save and a reload, runs svC-svE)
    data = read(path)
    _, _, offset, size, _ = block_at(data, 47)
    _, _, work, _, _ = block_at(data, 19)
    for f in (int(v, 0) for v in which.split(",")):
        if 1 <= f < 0xD00:
            data[work + 0x2F0 + f // 8] |= 1 << (f % 8)
            continue
        if not 0xD00 <= f < 0xD00 + 8 * (size - 1000):
            sys.exit(f"flag 0x{f:X}: not one of the flags 1-0x{0xD00 + 8 * (size - 1000) - 1:X}")
        data[offset + 1000 + (f - 0xD00) // 8] |= 1 << ((f - 0xD00) % 8)
    write_checksums(data)
    open(out, "wb").write(data)
    print(f"{out}: flags {which} set, checksums rewritten")


# PK6 (the party's Pokemon, above): the order of the four blocks for each shuffle value
SHUFFLE = [[0, 1, 2, 3], [0, 1, 3, 2], [0, 2, 1, 3], [0, 3, 1, 2], [0, 2, 3, 1], [0, 3, 2, 1], [1, 0, 2, 3], [1, 0, 3, 2],
           [2, 0, 1, 3], [3, 0, 1, 2], [2, 0, 3, 1], [3, 0, 2, 1], [1, 2, 0, 3], [1, 3, 0, 2], [2, 1, 0, 3], [3, 1, 0, 2],
           [2, 3, 0, 1], [3, 2, 0, 1], [1, 2, 3, 0], [1, 3, 2, 0], [2, 1, 3, 0], [3, 1, 2, 0], [2, 3, 1, 0], [3, 2, 1, 0]]
SLOT = 0x104


def pk6_xor(data, seed):
    out = bytearray(data)
    for i in range(8, len(out) - 1, 2):
        seed = (seed * 0x41C64E6D + 0x6073) & 0xFFFFFFFF
        struct.pack_into("<H", out, i, struct.unpack_from("<H", out, i)[0] ^ (seed >> 16))
    return out


def pk6_decrypt(slot):
    ec = struct.unpack_from("<I", slot, 0)[0]
    d, order = pk6_xor(slot, ec), SHUFFLE[((ec >> 13) & 31) % 24]
    out = bytearray(d)
    for i in range(4):
        k = order.index(i)
        out[8 + 56 * i:64 + 56 * i] = d[8 + 56 * k:64 + 56 * k]
    return out


def pk6_encrypt(plain):
    ec = struct.unpack_from("<I", plain, 0)[0]
    order, d = SHUFFLE[((ec >> 13) & 31) % 24], bytearray(plain)
    for i in range(4):
        d[8 + 56 * i:64 + 56 * i] = plain[8 + 56 * order[i]:64 + 56 * order[i]]
    return pk6_xor(d, ec)


def cmd_move(path, out, slot, index, move):
    slot, index, move = int(slot), int(index), int(move, 0)
    if not 0 <= slot < 6 or not 0 <= index < 4:
        sys.exit("slot 0-5, move index 0-3")
    data = read(path)
    _, _, offset, _, _ = block_at(data, 18)
    at = offset + slot * SLOT
    p = pk6_decrypt(data[at:at + SLOT])
    if struct.unpack_from("<H", p, 6)[0] != sum(struct.unpack_from("<112H", p, 8)) & 0xFFFF:
        sys.exit(f"slot {slot}: its checksum does not match once decrypted (an empty slot, or not this layout)")
    struct.pack_into("<H", p, 0x5A + 2 * index, move)
    p[0x62 + index] = 15
    struct.pack_into("<H", p, 6, sum(struct.unpack_from("<112H", p, 8)) & 0xFFFF)
    data[at:at + SLOT] = pk6_encrypt(p)
    write_checksums(data)
    open(out, "wb").write(data)
    print(f"{out}: party slot {slot} (species {struct.unpack_from('<H', p, 8)[0]}) given move {move} at {index}, checksums rewritten")


def cmd_badges(path, out, mask):
    data = read(path)
    _, _, offset, _, _ = block_at(data, 11)
    data[offset + 0xC] = int(mask, 0) & 0xFF
    write_checksums(data)
    open(out, "wb").write(data)
    print(f"{out}: badges 0x{int(mask, 0) & 0xFF:02X}, checksums rewritten")


def main():
    a = sys.argv[1:]
    commands = {("blocks", 1): cmd_blocks, ("diff", 2): cmd_diff, ("characters", 1): cmd_characters,
                ("take", 4): cmd_take, ("records", 4): cmd_records, ("flag", 3): cmd_flag,
                ("move", 5): cmd_move, ("badges", 3): cmd_badges}
    if not a or (a[0], len(a) - 1) not in commands:
        sys.exit(__doc__)
    commands[(a[0], len(a) - 1)](*a[1:])


if __name__ == "__main__":
    main()
