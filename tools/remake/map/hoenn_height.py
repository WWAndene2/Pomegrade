# map/height.npy: the assembled world's terrain height, 4 samples a tile (each piece's highest ground mesh, piece_height),
# from hoenn_assemble.py's placement; REMAKE_PIECE_HEIGHT names the piece_height program
import os,subprocess,json
HERE=os.path.dirname(os.path.abspath(__file__))
exec(open(os.path.join(HERE,'hoenn_assemble.py')).read().split("im=Image.new")[0])
import numpy as np
ph=os.environ.get('REMAKE_PIECE_HEIGHT','piece_height');os.makedirs('map/hgt',exist_ok=True)
args=[]
for p in sorted(set(p for m,p,x,z in boxes)):
  src=[f for f in os.listdir('map/a_0_3_9') if f.split('.')[0]==str(p)]
  if src and not os.path.exists('map/hgt/%d.f32'%p): args+=['map/a_0_3_9/'+src[0],'map/hgt/%d.f32'%p]
for i in range(0,len(args),200): subprocess.run([ph]+args[i:i+200])
W,H=(maxx-minx)*4,(maxz-minz)*4;Hm=np.full((H,W),np.nan,np.float32)
for m,p,x,z in boxes:
  f='map/hgt/%d.f32'%p
  if not os.path.exists(f): continue
  a=np.fromfile(f,np.float32).reshape(160,160);sub=Hm[(z-minz)*4:(z-minz)*4+160,(x-minx)*4:(x-minx)*4+160];ok=~np.isnan(a)
  sub[ok]=np.where(np.isnan(sub[ok]),a[ok],np.fmax(sub[ok],a[ok]))
np.save('map/height.npy',Hm);json.dump({'minx':minx,'minz':minz,'px':4},open('map/height.json','w'))
