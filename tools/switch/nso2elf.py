# Converts a Switch NSO module (exefs main, sdk, subsdk*) to an AArch64 ELF that Ghidra's own ELF
# loader reads: one PT_LOAD per segment (text, rodata, data+bss) and PT_DYNAMIC found through MOD0,
# so the dynamic symbols and relocations are applied. Usage: python3 nso2elf.py <nso> <out.elf>
import struct,sys,lz4.block
d=open(sys.argv[1],'rb').read(); assert d[:4]==b'NSO0'
flags=struct.unpack_from('<I',d,0xC)[0]
segs=[struct.unpack_from('<III',d,0x10+i*0x10) for i in range(3)]
bss=struct.unpack_from('<I',d,0x3C)[0]; csz=struct.unpack_from('<III',d,0x60)
img=bytearray(segs[2][1]+segs[2][2])
for i,(fo,mo,sz) in enumerate(segs):
    raw=d[fo:fo+csz[i]]
    if flags>>i&1: raw=lz4.block.decompress(raw,uncompressed_size=sz)
    assert len(raw)==sz; img[mo:mo+sz]=raw
mod0=struct.unpack_from('<I',img,4)[0]; assert img[mod0:mod0+4]==b'MOD0'
dyn=mod0+struct.unpack_from('<i',img,mod0+4)[0]
dynsz=0
while True:
    tag=struct.unpack_from('<q',img,dyn+dynsz)[0]; dynsz+=16
    if tag==0: break
phs=[(1,5,segs[0][1],segs[0][2],segs[0][2]),(1,4,segs[1][1],segs[1][2],segs[1][2]),
     (1,6,segs[2][1],segs[2][2],segs[2][2]+bss),(2,6,dyn,dynsz,dynsz)]
hoff=0x40+56*len(phs); hoff=(hoff+0xFFF)&~0xFFF
out=bytearray(hoff)+img
struct.pack_into('<4sBBBB8xHHIQQQIHHHHHH',out,0,b'\x7fELF',2,1,1,0,3,183,1,0,0x40,0,0,64,56,len(phs),64,0,0)
for i,(t,f,va,fs,ms) in enumerate(phs):
    struct.pack_into('<IIQQQQQQ',out,0x40+56*i,t,f,hoff+va,va,va,fs,ms,0x1000 if t==1 else 8)
open(sys.argv[2],'wb').write(out); print('ok',hex(len(img)),'bss',hex(bss),'dyn',hex(dyn))
