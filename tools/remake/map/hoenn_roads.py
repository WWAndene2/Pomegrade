import os
HERE=os.path.dirname(os.path.abspath(__file__))
exec(open(os.path.join(HERE,'hoenn_lines.py')).read().split("raw=Image.open")[0])
import numpy as np,heapq
from PIL import ImageFilter,ImageFont
G=np.load('map/kinds.npy');KN=json.load(open('map/kinds.json'))['kinds'];ki=KN.index
Hh,Ww=G.shape
# hubs (same as style2)
SEAR={105,106,107,108,109,122,124,125,126,127,128,129,130,131,132,133,134}
drawn=set(ms);hub={}
for z in Z:
  m=M(z);n=nm(z)
  if m not in drawn or not out(z) or not Z[z][1]: continue
  hub.setdefault(n,[]).extend([(pos[m][0]+w[4]/18,pos[m][1]+w[6]/18) for w in Z[z][1]])
H_={n:(sum(p[0] for p in v)/len(v),sum(p[1] for p in v)/len(v)) for n,v in hub.items()}
for m,(x,z,t) in AP.items():
  w,h,_=mat(m);H_[t.split(' (')[0].replace('Pokemon','Pokémon')]=(x+w*20,z+h*20)
# land: the tiles' own terrain, gaps between nearby strips closed, enclosed voids filled
from PIL import ImageFilter as IF
from collections import deque
TL=~np.isin(G,[ki('none'),ki('water'),ki('other'),ki('cliff')])
img=lambda a:Image.fromarray((a*255).astype(np.uint8))
A=np.array(img(TL).filter(IF.MaxFilter(21)).filter(IF.MinFilter(21)))>0
A|=(np.array(img(A).filter(IF.MaxFilter(9)))>0)&(G==ki('cliff'))
A=np.array(img(A).filter(IF.MaxFilter(7)).filter(IF.MinFilter(7)))>0
# drop specks: components under 600 tiles
lab=np.zeros(A.shape,np.int32);nl=0;sizes={}
for y0,x0 in zip(*np.nonzero(A)):
  if lab[y0,x0]: continue
  nl+=1;lab[y0,x0]=nl;q=deque([(y0,x0)]);c=0
  while q:
    y,x=q.popleft();c+=1
    for dy,dx in ((1,0),(-1,0),(0,1),(0,-1)):
      n=(y+dy,x+dx)
      if 0<=n[0]<Hh and 0<=n[1]<Ww and A[n] and not lab[n]: lab[n]=nl;q.append(n)
  sizes[nl]=c
keep={l for l,c in sizes.items() if c>=600}
A=np.isin(lab,list(keep))
# fill voids not reachable from the border
def flood(A):
 sea=np.zeros_like(A);q=deque()
 for y in range(Hh):
  for x in (0,Ww-1):
    if not A[y,x] and not sea[y,x]: sea[y,x]=1;q.append((y,x))
 for x in range(Ww):
  for y in (0,Hh-1):
    if not A[y,x] and not sea[y,x]: sea[y,x]=1;q.append((y,x))
 while q:
  y,x=q.popleft()
  for dy,dx in ((1,0),(-1,0),(0,1),(0,-1)):
    n=(y+dy,x+dx)
    if 0<=n[0]<Hh and 0<=n[1]<Ww and not A[n] and not sea[n]: sea[n]=1;q.append(n)
 return ~sea.astype(bool)
A0=A
COST_L=np.full(len(KN),4.0);COST_L[ki('path')]=1;COST_L[ki('cliff')]=40;COST_L[ki('forest')]=25;COST_L[ki('water')]=30;COST_L[ki('none')]=60;COST_L[ki('structure')]=6
COST_S=np.full(len(KN),6.0);COST_S[ki('water')]=1;COST_S[ki('none')]=2;COST_S[ki('other')]=2
def route(a,b,C):
  cost=C[G];a=(int(a[1]-minz),int(a[0]-minx));b=(int(b[1]-minz),int(b[0]-minx))
  a=(min(max(a[0],0),Hh-1),min(max(a[1],0),Ww-1));b=(min(max(b[0],0),Hh-1),min(max(b[1],0),Ww-1))
  dist={a:0};prev={};q=[(0,a)]
  h=lambda p:abs(p[0]-b[0])+abs(p[1]-b[1])
  q=[(h(a),0,a)]
  while q:
    f,g,p=heapq.heappop(q)
    if p==b: break
    if g>dist.get(p,1e18): continue
    for dy,dx,l in ((1,0,1),(-1,0,1),(0,1,1),(0,-1,1),(1,1,1.41),(1,-1,1.41),(-1,1,1.41),(-1,-1,1.41)):
      n=(p[0]+dy,p[1]+dx)
      if 0<=n[0]<Hh and 0<=n[1]<Ww:
        ng=g+l*cost[n]
        if ng<dist.get(n,1e18): dist[n]=ng;prev[n]=p;heapq.heappush(q,(ng+h(n),ng,n))
  path=[b]
  while path[-1]!=a and path[-1] in prev: path.append(prev[path[-1]])
  return [(p[1]+minx,p[0]+minz) for p in path[::-1]]
def dp(pts,eps):
  if len(pts)<3: return pts
  (x0,y0),(x1,y1)=pts[0],pts[-1];L=((x1-x0)**2+(y1-y0)**2)**.5 or 1
  dmax,i=max((abs((y1-y0)*x-(x1-x0)*y+x1*y0-y1*x0)/L,i) for i,(x,y) in enumerate(pts))
  return dp(pts[:i+1],eps)[:-1]+dp(pts[i:],eps) if dmax>eps else [pts[0],pts[-1]]
def oct(a,b):
  dx,dz=b[0]-a[0],b[1]-a[1];k=min(abs(dx),abs(dz))
  return [a,(a[0]+k*(1 if dx>0 else -1),a[1]+k*(1 if dz>0 else -1)),b]
def octi(pts):
  q=dp(pts,24)
  q=[q[0]]+[c for i,c in enumerate(q[1:-1],1) if abs(c[0]-q[i-1][0])+abs(c[1]-q[i-1][1])>10]+[q[-1]]
  o=[q[0]]
  for b in q[1:]: o+=oct(o[-1],b)[1:]
  return o
def smooth(pts,k=6):
  out=[]
  for i in range(len(pts)):
    s=pts[max(0,i-k):i+k+1];out.append((sum(p[0] for p in s)/len(s),sum(p[1] for p in s)/len(s)))
  return out[::3]+[out[-1]]
R=lambda *k:['Route %d'%x if isinstance(x,int) else x for x in k]
exec(open(os.path.join(HERE,'hoenn_lines.py')).read().split("LINES=")[1].split("\nmiss=set()")[0].join(["LINES=",""]))
LINES=[l for l in LINES if l[2]!=R('Oldale Town',103,110,'Slateport City')]+[((240,150,30),0,R('Oldale Town',103,110))]
PATHS=[]
for col,sea,seq in LINES:
  pts=[H_[n] for n in seq if n in H_]
  for i in range(len(pts)-1): PATHS.append((col,sea,octi(route(pts[i],pts[i+1],COST_S if sea else COST_L))))
B=Image.new('L',(Ww,Hh),0);bd=ImageDraw.Draw(B)
for col,sea,pth in PATHS:
  if not sea: bd.line([(x-minx,z-minz) for x,z in pth],fill=255,width=9)
# land the game has no tiles for but the roads enclose: Meteor Falls (114 to 115), Mt Chimney (Lavaridge to Route 116)
for a_,b_ in (('Route 114','Route 115'),('Lavaridge Town','Route 116')):
  bd.line([(H_[a_][0]-minx,H_[a_][1]-minz),(H_[b_][0]-minx,H_[b_][1]-minz)],fill=255,width=9)
for n,(x,z) in H_.items():
  if not n.startswith('Route'): bd.ellipse([x-minx-14,z-minz-14,x-minx+14,z-minz+14],fill=255)
LANDM=flood(A0|(np.array(B)>0))
import pickle;pickle.dump({'PATHS':[(sea,pth) for col,sea,pth in PATHS],'LINES':[(sea,seq) for col,sea,seq in LINES],'H':H_},open('map/paths.pkl','wb'))
