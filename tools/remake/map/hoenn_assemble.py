import os
HERE=os.path.dirname(os.path.abspath(__file__))
import struct,os,subprocess,sys,json
sys.path.insert(0,'/home/user/Pomegrade/tools/remake/prototype');from bch_sections import lz11
from PIL import Image,ImageDraw
exec(open(os.path.join(HERE,'hoenn_place.py')).read().split("print('--- residuals")[0].replace("for c in conf: print","for c in []: print").replace("for c in comps: print","for c in []: print"))
R=os.environ.get('REMAKE_TOOL','build-remake/remake_tool');PX=2
def mat(m):
  d=open('map/a_0_4_0/%d.unknown'%m,'rb').read();d=lz11(d) if d[:1]==b'\x11' else d
  n=struct.unpack_from('<H',d,2)[0];o=struct.unpack_from('<%dI'%(n+1),d,4);f=d[o[0]:o[1]];w,h=struct.unpack_from('<2H',f,4)
  return w,h,struct.unpack_from('<%dH'%(w*h),f,8)
def piece(p):
  out='map/top/%d.png'%p
  if not os.path.exists(out):
    src=[f for f in os.listdir('map/a_0_3_9') if f.split('.')[0]==str(p)][0]
    subprocess.run([R,'topview','map/a_0_3_9/'+src,out,'--tiles','--px','4'],capture_output=True)
  return Image.open(out).convert('RGB') if os.path.exists(out) else None
ms=[m for m in max(comps,key=len) if any(out(z) for z in Z if M(z)==m)]
AP={17:(783,-77,'Sootopolis City (approx: crater centre)'),19:(973,-72,'Pokemon League (approx: north of Ever Grande)'),158:(346,244,'Southern Island (approx: DexNav map)'),155:(747,169,'Battle Resort (approx: DexNav map)')}
ms=[m for m in ms if mat(m)[:2]!=(1,1)]
for m,(x,z,_) in AP.items(): pos[m]=(x,z)
ms+=list(AP)
boxes=[]
for m in ms:
  w,h,ps=mat(m);x,z=pos[m]
  for i,p in enumerate(ps):
    if p!=0xffff: boxes.append((m,p,x+(i%w)*40,z+(i//w)*40))
minx=min(b[2] for b in boxes);minz=min(b[3] for b in boxes);maxx=max(b[2] for b in boxes)+40;maxz=max(b[3] for b in boxes)+40
im=Image.new('RGB',((maxx-minx)*PX,(maxz-minz)*PX),(16,32,70))
for m,p,x,z in boxes:
  t=piece(p)
  if t: im.paste(t.resize((40*PX,40*PX)),((x-minx)*PX,(z-minz)*PX))
d=ImageDraw.Draw(im)
# broken seams: the crossings whose link disagrees, marked where they are in their own matrix
for zz in Z:
  if not out(zz) or M(zz) not in pos: continue
  h,ws=Z[zz]
  for w in ws:
    k=w[2]&0xffff;dz=w[0]&0xffff
    if k in step and dz in Z and out(dz) and M(dz)!=M(zz) and M(dz) in pos:
      dd=Z[dz][1][w[1]];sx,sz=step[k]
      rx=pos[M(dz)][0]-pos[M(zz)][0]-round(w[4]/18+sx-dd[4]/18);rz=pos[M(dz)][1]-pos[M(zz)][1]-round(w[6]/18+sz-dd[6]/18)
      X=(pos[M(zz)][0]+w[4]/18-minx)*PX;Y=(pos[M(zz)][1]+w[6]/18-minz)*PX
      col=(255,40,40) if abs(rx)>2 or abs(rz)>2 else (255,255,255)
      d.rectangle([X-3,Y-3,X+max(3,w[7]*PX),Y+max(3,w[8]*PX)],outline=col,width=2)
for m in ms:
  xs=[b[2] for b in boxes if b[0]==m];zs=[b[3] for b in boxes if b[0]==m]
  label=sorted(set(nm(z) for z in Z if M(z)==m and out(z)))
  d.text(((min(xs)-minx)*PX+4,(min(zs)-minz)*PX+4),'m%d %s'%(m,', '.join(label)[:40]),fill=(255,255,255))
for m,(x,z,t) in AP.items():
  w,h,_=mat(m);X=(x-minx)*PX;Y=(z-minz)*PX
  d.rectangle([X,Y,X+w*40*PX,Y+h*40*PX],outline=(255,220,0),width=3);d.text((X+4,Y+16),t,fill=(255,220,0))
im.save('hoenn_assembled.png');print(im.size,len(ms),'matrices')
