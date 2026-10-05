# scratch: which Platinum textures cover each tile (from the reference glTF, ORAS units: tile = 18)
import os
import json,base64,re,sys
import numpy as np
from collections import Counter, defaultdict
S = os.environ.get('REMAKE_WORK', '.')
GLTF=sys.argv[2] if len(sys.argv)>2 else S+'/pref/ref.gltf'
OUT=sys.argv[3] if len(sys.argv)>3 else None   # output name; offsets: the scene's x z added (to share Platinum's grid)
OX=float(sys.argv[4]) if len(sys.argv)>4 else 0.0; OZ=float(sys.argv[5]) if len(sys.argv)>5 else 0.0
j=json.loads(open(GLTF).read())
buf=base64.b64decode(j['buffers'][0]['uri'].split(',',1)[1])
def acc(i):
    a=j['accessors'][i]; v=j['bufferViews'][a['bufferView']]; off=v.get('byteOffset',0)+a.get('byteOffset',0)
    n={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]; dt={5126:np.float32,5125:np.uint32,5123:np.uint16,5121:np.uint8}[a['componentType']]
    stride=v.get('byteStride',0); size=np.dtype(dt).itemsize*n
    if stride and stride!=size:
        raw=np.frombuffer(buf,dtype=np.uint8,count=stride*a['count'],offset=off).reshape(a['count'],stride)[:,:size]
        return np.frombuffer(raw.tobytes(),dtype=dt).reshape(a['count'],n)
    return np.frombuffer(buf,dtype=dt,count=a['count']*n,offset=off).reshape(a['count'],n)
R=int(sys.argv[1]) if len(sys.argv)>1 else 1   # samples a tile edge: the grid is in 1/R tiles
U=18.0/R
cover=defaultdict(set)   # (gc,gr) in 1/R tiles -> material base names
top=defaultdict(lambda:(-1e9,''))
for m in j['meshes']:
    for p in m['primitives']:
        name=re.sub(r'_lm\d+$','',j['materials'][p['material']].get('name','?')) if 'material' in p else '?'
        P=acc(p['attributes']['POSITION']).copy(); P[:,0]+=OX; P[:,2]+=OZ; I=acc(p['indices']).reshape(-1).astype(np.int64).reshape(-1,3)
        for t in I:
            a,b,c=P[t[0]],P[t[1]],P[t[2]]
            xs=[a[0],b[0],c[0]]; zs=[a[2],b[2],c[2]]
            c0=int(np.floor(min(xs)/U-0.5))+1; c1=int(np.floor(max(xs)/U-0.5)); r0=int(np.floor(min(zs)/U-0.5))+1; r1=int(np.floor(max(zs)/U-0.5))
            for gr in range(r0,r1+1):
                for gc in range(c0,c1+1):
                    x=(gc+0.5)*U; z=(gr+0.5)*U
                    d=(b[2]-c[2])*(a[0]-c[0])+(c[0]-b[0])*(a[2]-c[2])
                    if abs(d)<1e-9: continue
                    l1=((b[2]-c[2])*(x-c[0])+(c[0]-b[0])*(z-c[2]))/d; l2=((c[2]-a[2])*(x-c[0])+(a[0]-c[0])*(z-c[2]))/d; l3=1-l1-l2
                    if min(l1,l2,l3)<-1e-6: continue
                    y=l1*a[1]+l2*b[1]+l3*c[1]
                    cover[(gc,gr)].add(name)
                    if y>top[(gc,gr)][0]: top[(gc,gr)]=(y,name)
json.dump({f'{k[0]},{k[1]}':sorted(v) for k,v in cover.items()},open(S+'/slice/'+(OUT+'_cover.json' if OUT else (f'cover{R}.json' if R>1 else 'cover.json')),'w'))
json.dump({f'{k[0]},{k[1]}':[round(float(v[0]),1),v[1]] for k,v in top.items()},open(S+'/slice/'+(OUT+'_top.json' if OUT else (f'top{R}.json' if R>1 else 'top.json')),'w'))
print(len(cover),'tiles covered')
