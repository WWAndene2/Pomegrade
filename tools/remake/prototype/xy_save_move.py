#!/usr/bin/env python3
"""An X/Y save moved to a zone and tile (24 October: X run headless on Route 13 to see how it draws a dust cloud).

  xy_save_move.py <X/Y main> <out> ZONE TILE_X TILE_Z HEIGHT_PX

X/Y's save has Omega Ruby's layout (checked on the owner's X save: "FEEB" 0x1F0 bytes before the end, 8-byte entries u32
length, u16 id, u16 CRC16-CCITT; 55 blocks, all matching): block 4 at 0x1400 holds the zone at +2 and +0xF4, the position
(pixels, 18 a tile) at +0x10/+0x18 and +0x104/+0x10C, the height at +0x14/+0x108; on a zone change block 10's records are
emptied but the player's (OrasSave::MoveTo, done the same). Checked: the owner's X save moved to zone 272 (126.5, 81.5,
height 2) loads there (run xr2). Run X with POMEGRADE_GAME=dumps/x.3ds POMEGRADE_SAVE=<out> (headless/run_local.sh).
"""
src,out,zone,tx,tz,h=sys.argv[1],sys.argv[2],int(sys.argv[3]),float(sys.argv[4]),float(sys.argv[5]),float(sys.argv[6])
d=bytearray(open(src,'rb').read())
foot=len(d)-0x200; assert d[foot+0x10:foot+0x14]==b'FEEB'
def crc(b):
    c=0xFFFF
    for x in b:
        c^=x<<8
        for _ in range(8): c=((c<<1)^0x1021)&0xFFFF if c&0x8000 else (c<<1)&0xFFFF
    return c
blocks=[];off=0;e=foot+0x14
while e+8<=len(d):
    ln,bid,ck=struct.unpack_from('<IHH',d,e)
    if ln==0: break
    blocks.append([off,ln,bid,ck,e]); off=(off+ln+0x1FF)&~0x1FF; e+=8
bad=[b[2] for b in blocks if crc(d[b[0]:b[0]+b[1]])!=b[3]]; print(len(blocks),'blocks, bad',bad)
old=struct.unpack_from('<H',d,0x1402)[0]
if zone!=old:
    b=[x for x in blocks if x[2]==10][0]
    for at in range(b[0],b[0]+b[1]-0x107,0x108):
        if struct.unpack_from('<H',d,at+4)[0]==0xFF and struct.unpack_from('<H',d,at+6)[0]==old: struct.pack_into('<H',d,at+6,zone)
        else: d[at:at+0x108]=bytes(0x108)
for at in (0x1402,0x14F4): struct.pack_into('<H',d,at,zone)
for at in (0x1410,0x1504): struct.pack_into('<f',d,at,tx*18); struct.pack_into('<f',d,at+8,tz*18); struct.pack_into('<f',d,at+4,h)
for b in blocks: struct.pack_into('<H',d,b[4]+6,crc(d[b[0]:b[0]+b[1]]))
open(out,'wb').write(d); print('moved',old,'->',zone)
