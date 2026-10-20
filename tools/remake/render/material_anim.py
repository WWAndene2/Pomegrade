# Reads a BCH's material animations (what moves each texture: element, target, keyframes), as SPICA's
# H3DAnimation layout (relocation as tools/remake/src/Bch.cpp). Usage: python3 material_anim.py <file.bch>
import struct,sys
TT="Bone MatConst0 MatConst1 MatConst2 MatConst3 MatConst4 MatConst5 Emission Ambient Diffuse Specular0 Specular1 Map0Border Map0Texture Map1Border Map1Texture Map2Border Map2Texture BlendColor TexCoord0Scale TexCoord0Rot TexCoord0Trans TexCoord1Scale TexCoord1Rot TexCoord1Trans TexCoord2Scale TexCoord2Rot TexCoord2Trans".split()
PT="Float Integer Vector2D Vector3D Transform RGBA Texture QuatTransform Boolean MtxTransform".split()
D=bytearray(open(sys.argv[1],'rb').read())
u32=lambda o:struct.unpack_from('<I',D,o)[0]; u16=lambda o:struct.unpack_from('<H',D,o)[0]; f32=lambda o:struct.unpack_from('<f',D,o)[0]
ver=D[4]; ext=ver>=0x21; n=6 if ext else 5
C,Sg,Cm,Rw=u32(8),u32(12),u32(16),u32(20); RE=u32(24) if ext else 0
Rel=u32(8+4*(n-1)); RelL=u32(8+4*n+4*(n-1))
def base(s): return {0:C,1:Sg,2:Cm,3:Cm,4:Rw,5:Rw,6:Rw,8:Rw,7:Rw,9:RE,10:RE,11:RE,13:RE,12:RE,14:0}[s]
for off in range(0,RelL,4):
    v=u32(Rel+off); ptr=v&0x1FFFFFF; tg=(v>>25)&0xF; src=v>>29
    if ver>7 and ver<0x21 and tg>=6: tg-=1
    if tg!=1: ptr<<=2
    at=base(src)+ptr; struct.pack_into('<I',D,at,(u32(at)+base(tg))&0xffffffff)
def cstr(o):
    e=D.index(0,o); return D[o:e].decode()
def group(o,const):
    if const: return ('const',f32(o))
    a=u32(o); sf,ef=f32(a),f32(a+4); pre,post=D[a+8],D[a+9]
    q=D[a+13]; cnt=u16(a+14); vs,vo,fs=f32(a+16),f32(a+20),f32(a+24); kp=u32(a+32)
    kf=[]
    for i in range(cnt):
        if q==0: fr,va=f32(kp+16*i),f32(kp+16*i+4)
        elif q==1: w=u32(kp+8*i); fr,va=w&0xfff,(w>>12)&0xfffff
        elif q==3: fr,va=f32(kp+12*i),f32(kp+12*i+4)
        elif q==4: fr,va=u16(kp+6*i),u16(kp+6*i+2)
        elif q==6: fr,va=f32(kp+8*i),f32(kp+8*i+4)
        elif q==7: w=u32(kp+4*i); fr,va=w&0xfff,(w>>12)&0xfffff
        else: return ('quant%d'%q,cnt)
        kf.append((round(fr*fs,2),round(va*vs+vo,4)))
    return ('curve',sf,ef,'loop' if post else 'once',kf)
for kind,idx in [('material',9),('skeletal',8),('visibility',10)]:
    lst,cnt=u32(C+idx*12),u32(C+idx*12+4)
    for i in range(cnt):
        a=u32(lst+4*i); name=cstr(u32(a)); frames=f32(a+8); el,ec=u32(a+12),u32(a+16)
        print(f'[{kind}] {name} frames={frames:.0f} elements={ec}')
        for j in range(ec):
            e=u32(el+4*j); en=cstr(u32(e)); t=u16(e+4); p=u16(e+6)
            tn=TT[t] if t<len(TT) else t; pn=PT[p] if p<len(PT) else p
            out=''
            if pn in('Vector2D','Float','Texture'):
                fl=u32(e+8); axes=2 if pn=='Vector2D' else 1
                for ax in range(axes):
                    if fl>>(ax+(axes if axes>1 else 1))&1 and False: pass
                    const=(fl>>ax)&1; ex=not((fl>>(ax+ (2 if axes==2 else 1)))&1)
                    if ex: out+=f' {"xy"[ax] if axes==2 else ""}={group(e+12+4*ax,const)}'
            print(f'    {en}: {tn} ({pn}){out[:400]}')
