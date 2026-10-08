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

What is read and what is only seen:
  - the block table (checked on the owner's saves, OrasSave.cpp): at 0x75E14, one 8-byte entry per block, u32 size, u16 id,
    u16 CRC16-CCITT (initial 0xFFFF) of the block; blocks follow each other from offset 0, each on a 0x200 boundary.
  - block 10 (0x2000, 8,448 bytes), not read in code: 32 records of 0x108 bytes, one per character of the zone the save was
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

TABLE = 0x75E14
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
    for i, at in enumerate(range(TABLE, len(data) - 7, 8)):
        size, bid, crc = struct.unpack_from("<IHH", data, at)
        if size == 0 or offset + size > TABLE:
            break
        out.append((i, bid, offset, size, crc))
        offset = (offset + size + 0x1FF) & ~0x1FF
    return out


def read(path):
    data = bytearray(open(path, "rb").read())
    if len(data) != 0x76000 or data[TABLE - 4:TABLE] != b"FEEB":
        sys.exit(f"{path}: not an ORAS save (0x76000 bytes, FEEB before the table)")
    return data


def write_checksums(data):
    for i, _, offset, size, _ in blocks(data):
        struct.pack_into("<H", data, TABLE + i * 8 + 6, crc16(data[offset:offset + size]))


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


def main():
    a = sys.argv[1:]
    commands = {("blocks", 1): cmd_blocks, ("diff", 2): cmd_diff, ("characters", 1): cmd_characters,
                ("take", 4): cmd_take, ("records", 4): cmd_records}
    if not a or (a[0], len(a) - 1) not in commands:
        sys.exit(__doc__)
    commands[(a[0], len(a) - 1)](*a[1:])


if __name__ == "__main__":
    main()
