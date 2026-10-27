import os
HERE=os.path.dirname(os.path.abspath(__file__))
exec(open(os.path.join(HERE,'hoenn_assemble.py')).read().split("im=Image.new")[0])
import numpy as np
COL={(110,190,230):'effect',(110,100,90):'cliff',(70,100,55):'forest',(30,120,60):'ground',(120,220,120):'light',(200,170,100):'path',(20,60,40):'shadow',(70,130,220):'water',(150,150,150):'structure',(255,255,0):'blades',(255,0,0):'rim',(190,90,190):'other',(24,24,24):'none'}
KI={k:i for i,k in enumerate(COL.values())}
lut={c:KI[n] for c,n in COL.items()}
W,H=maxx-minx,maxz-minz
G=np.full((H,W),KI['none'],np.uint8)
for m,p,x,z in boxes:
  out='map/topk/%d.png'%p
  if not os.path.exists(out):
    src=[f for f in os.listdir('map/a_0_3_9') if f.split('.')[0]==str(p)]
    if not src: continue
    subprocess.run([R,'topview','map/a_0_3_9/'+src[0],out,'--px','4'],capture_output=True)
  if not os.path.exists(out): continue
  a=np.array(Image.open(out).convert('RGB'))[2::4,2::4]
  k=np.vectorize(lambda r,g,b:lut.get((r,g,b),KI['other']))(a[...,0],a[...,1],a[...,2]).astype(np.uint8)
  G[z-minz:z-minz+40,x-minx:x-minx+40]=k
np.save('map/kinds.npy',G);json.dump({'minx':minx,'minz':minz,'kinds':list(COL.values())},open('map/kinds.json','w'))
print(G.shape,np.bincount(G.ravel()))
