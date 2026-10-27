# Hoenn rail-style map: ORAS's traced roads kept true (tile coordinates); the town map's land outline bent onto them
import numpy as np,pickle,math,json
from PIL import Image,ImageDraw,ImageFilter,ImageFont
from collections import deque
D=pickle.load(open('map/paths.pkl','rb'));H=D['H']
Kj=json.load(open('map/kinds.json'));minx,minz=Kj['minx'],Kj['minz'];Hh,Ww=np.load('map/kinds.npy').shape
C={'Littleroot Town':(161,468),'Oldale Town':(161,402),'Petalburg City':(62,402),'Rustboro City':(29,254),'Fallarbor Town':(128,72),
 'Lavaridge Town':(194,204),'Verdanturf Town':(161,270),'Mauville City':(310,270),'Slateport City':(293,452),'Dewford Town':(95,567),
 'Fortree City':(425,72),'Lilycove City':(672,172),'Mossdeep City':(836,224),'Sootopolis City':(740,348),'Pacifidlog Town':(623,435),
 'Ever Grande City':(920,402)}
def U(r): return np.where(r>0,r*r*np.log(r+1e-12),0)
def tps(src,dst,lam):
  n=len(src);L=np.zeros((n+3,n+3));L[:n,:n]=U(np.linalg.norm(src[:,None]-src[None],axis=2))+lam*np.eye(n)
  P=np.hstack([np.ones((n,1)),src]);L[:n,n:]=P;L[n:,:n]=P.T
  w=np.linalg.solve(L,np.vstack([dst,np.zeros((3,2))]))
  def f(q):
    q=np.atleast_2d(q);out=np.empty((len(q),2))
    for i in range(0,len(q),20000):
      s=q[i:i+20000];out[i:i+20000]=U(np.linalg.norm(s[:,None]-src[None],axis=2))@w[:n]+np.hstack([np.ones((len(s),1)),s])@w[n:]
    return out
  return f
# world -> town map px: where each world tile's land is looked up
toMap=tps(np.array([H[n] for n in C]),np.array(list(C.values()),float),2000.0)
# town map land (route squares, spot icons, the Y button removed)
ref=np.array(Image.open('ref/dexnav.jpg').convert('RGB')).astype(int);r,g,b=ref[...,0],ref[...,1],ref[...,2]
m=~((b>g+8)&(b>r+30));m[630:]=False;m[:95,855:]=False
A=np.array(Image.fromarray((m*255).astype(np.uint8)).filter(ImageFilter.MinFilter(7)).filter(ImageFilter.MaxFilter(7)))>0
lab=np.zeros(A.shape,int);n=0;keep=[]
for y0,x0 in zip(*np.nonzero(A)):
  if lab[y0,x0]: continue
  n+=1;lab[y0,x0]=n;q=deque([(y0,x0)]);pts=[]
  while q:
    y,x=q.popleft();pts.append((y,x))
    for dy,dx in ((1,0),(-1,0),(0,1),(0,-1)):
      a_,b_=y+dy,x+dx
      if 0<=a_<A.shape[0] and 0<=b_<A.shape[1] and A[a_,b_] and not lab[a_,b_]: lab[a_,b_]=n;q.append((a_,b_))
  ys=[p[0] for p in pts];xs=[p[1] for p in pts];w_=max(xs)-min(xs)+1;h_=max(ys)-min(ys)+1
  if len(pts)>=1300 and not (w_<50 and h_<50 and len(pts)/(w_*h_)>0.88): keep.append(n)
TM=np.isin(lab,keep)
# world canvas with a sea margin
PADX,PADZ=110,150;X0,Z0=minx-PADX,minz-PADZ;WW,HH=Ww+2*PADX,Hh+2*PADZ
zz,xx=np.mgrid[0:HH,0:WW];q=np.stack([xx.ravel()+X0+0.5,zz.ravel()+Z0+0.5],1)
mp=toMap(q);mx=np.clip(mp[:,0].astype(int),0,959);mz=np.clip(mp[:,1].astype(int),0,719)
inside=(mp[:,0]>=0)&(mp[:,0]<960)&(mp[:,1]>=0)&(mp[:,1]<630)
LANDW=(TM[mz,mx]&inside).reshape(HH,WW)
# network: stops are the places and the junctions (a route where lines meet, or a line's end); each edge between two
# stops is the traced road between them, drawn as the 1-3 segment octilinear shape (straight / 45 degrees) closest to it
G2=4
def snapg(p): return (round(p[0]/G2)*G2,round(p[1]/G2)*G2)
seqs=[(sea,[n for n in seq if n in H]) for sea,seq in D['LINES']]
# Mt. Pyre: the island on Route 122, placed from its square on the town map (565, 228), mapped back to the tiles
toWorld=tps(np.array(list(C.values()),float),np.array([H[n] for n in C]),2000.0)
H['Mt. Pyre']=tuple(toWorld(np.array([[565.0,228.0]]))[0])
it=iter(D['PATHS']);pairs=[]
for sea,seq in seqs:
  for x,y in zip(seq,seq[1:]): pairs.append((sea,x,y,next(it)[1]))
cnt={}
for sea,seq in seqs:
  for i,n in enumerate(seq): cnt[n]=cnt.get(n,0)+(2 if i in (0,len(seq)-1) else 1)
STOP={n for n in cnt if not n.startswith('Route') or cnt[n]>=2}
POS={n:snapg(H[n]) for n in STOP}
EDG={}
for sea,seq in seqs:
  cur=[seq[0]];path=[];mids=[]
  for (s_,x,y,pth) in [q for q in pairs if q[0]==sea and q[1] in seq and q[2] in seq and seq.index(q[2])==seq.index(q[1])+1 and q[1] in seq]:
    pass
  i=0
  pp=[q for q in pairs]
  start=seq[0];pts=[];mids=[]
  for x,y in zip(seq,seq[1:]):
    pth=next(q[3] for q in pairs if q[0]==sea and q[1]==x and q[2]==y)
    pts+= [tuple(t) for t in pth]
    if y in STOP:
      k=tuple(sorted([start,y]))
      if k not in EDG or (EDG[k][0] and not sea): EDG[k]=(sea,pts,mids)
      else: EDG[k][2].extend(mids)
      start=y;pts=[];mids=[]
    else: mids.append(y)
def dp(pts,eps):
  if len(pts)<3: return list(pts)
  (x0,y0),(x1,y1)=pts[0],pts[-1];L=math.hypot(x1-x0,y1-y0) or 1
  dm,i=max((abs((y1-y0)*x-(x1-x0)*y+x1*y0-y1*x0)/L,i) for i,(x,y) in enumerate(pts))
  return dp(pts[:i+1],eps)[:-1]+dp(pts[i:],eps) if dm>eps else [pts[0],pts[-1]]
def seglen(o): return sum(math.hypot(b2[0]-a2[0],b2[1]-a2[1]) for a2,b2 in zip(o,o[1:]))
def dist_pt_poly(p,o):
  best=1e18
  for a2,b2 in zip(o,o[1:]):
    vx,vy=b2[0]-a2[0],b2[1]-a2[1];L2=vx*vx+vy*vy or 1;t=max(0,min(1,((p[0]-a2[0])*vx+(p[1]-a2[1])*vy)/L2))
    best=min(best,math.hypot(p[0]-a2[0]-t*vx,p[1]-a2[1]-t*vy))
  return best
def shapes(A_,B_):
  dx,dz=B_[0]-A_[0],B_[1]-A_[1];k=min(abs(dx),abs(dz));sx,sz=math.copysign(1,dx),math.copysign(1,dz)
  diag=(k*sx,k*sz);st=(dx-diag[0],dz-diag[1])
  out=[[A_,B_]] if k==0 or abs(dx)==abs(dz) else []
  if out: return out
  out.append([A_,(A_[0]+diag[0],A_[1]+diag[1]),B_])            # diagonal first
  out.append([A_,(A_[0]+st[0],A_[1]+st[1]),B_])                # straight first
  for f in (0.25,0.5,0.75):                                     # straight, diagonal, straight
    m1=(A_[0]+st[0]*f,A_[1]+st[1]*f);out.append([A_,m1,(m1[0]+diag[0],m1[1]+diag[1]),B_])
  # diagonal, straight, diagonal
  out.append([A_,(A_[0]+diag[0]/2,A_[1]+diag[1]/2),(A_[0]+diag[0]/2+st[0],A_[1]+diag[1]/2+st[1]),B_])
  return out
SCH={}
for k,(sea,pts,mids) in EDG.items():
  A_,B_=POS[k[0]],POS[k[1]]
  if math.hypot(pts[0][0]-A_[0],pts[0][1]-A_[1])>math.hypot(pts[0][0]-B_[0],pts[0][1]-B_[1]): pts=pts[::-1]
  dense=[]
  for a2,b2 in zip(pts,pts[1:]):
    L=max(1,int(math.hypot(b2[0]-a2[0],b2[1]-a2[1])/4))
    dense+=[(a2[0]+(b2[0]-a2[0])*t/L,a2[1]+(b2[1]-a2[1])*t/L) for t in range(L)]
  dense=dense or [A_,B_]
  cands=shapes(A_,B_)
  # a finer octilinear shape that follows the road: its bends simplified (8 tiles), each run at least 10 tiles
  q_=dp(dense,8);q_[0]=A_;q_[-1]=B_;o_=[A_]
  for b2 in q_[1:]:
    a2=o_[-1];dx,dz=b2[0]-a2[0],b2[1]-a2[1];kk=min(abs(dx),abs(dz))
    if math.hypot(dx,dz)<10 and b2!=B_: continue
    mid=(a2[0]+math.copysign(kk,dx),a2[1]+math.copysign(kk,dz))
    o_+=[mid,b2] if mid not in (a2,b2) else [b2]
  if o_[-1]!=B_: o_.append(B_)
  cands.append(o_)
  best=min(cands,key=lambda o:sum(dist_pt_poly(q,o) for q in dense)/len(dense)+1.2*len(o))
  SCH[k]=(sea,best,mids)
EDG.pop(('Route 121','Route 123'),None) if EDG.get(('Route 121','Route 123'),(0,))[0] else None
CITY={n:POS[n] for n in STOP if not n.startswith('Route')}
JUNC={n:POS[n] for n in STOP if n.startswith('Route')}
# Ever Grande -> Victory Road -> Pokemon League (a cave), Meteor Falls between Routes 114 and 115
EG=CITY['Ever Grande City'];LG=(EG[0],snapg(H['Pokémon League'])[1]);CITY['Pokémon League']=LG;VR=(EG[0],(EG[1]+LG[1])//2);CITY['Victory Road']=VR
for n in ('Southern Island','Battle Resort'): CITY[n]=snapg(H[n])
CITY['Mt. Pyre']=snapg(H['Mt. Pyre']);SCH[('Mt. Pyre','Route 121')]=(1,shapes(POS['Route 121'],CITY['Mt. Pyre'])[-1] if POS['Route 121'][0]!=CITY['Mt. Pyre'][0] and POS['Route 121'][1]!=CITY['Mt. Pyre'][1] else [POS['Route 121'],CITY['Mt. Pyre']],[])
CAVE=[(EG,VR),(VR,LG),(POS.get('Route 114',snapg(H['Route 114'])),snapg(H['Route 115']))]
# landmarks: where the overworld enters them (warps), else their square on the town map (approximate)
import pickle
LM1=pickle.load(open('map/landmarks.pkl','rb'));LM1.update(pickle.load(open('map/landmarks2.pkl','rb')))
SKIPL={'Team Aqua Hideout','Team Magma Hideout','Rusturf Tunnel','Victory Road'}
LMK={n:snapg((sum(v[0] for v in L)/len(L),sum(v[1] for v in L)/len(L))) for n,L in LM1.items() if n not in SKIPL}
APPROX={'Safari Zone':(540,140),'Shoal Cave':(877,177),'Seafloor Cavern':(825,400),'Cave of Origin':(740,330)}
for n,pq in APPROX.items(): LMK[n]=snapg(tuple(toWorld(np.array([pq],float))[0]))
LMK['Southern Island']=CITY.pop('Southern Island');LMK['Battle Resort']=CITY.pop('Battle Resort');LMK['Mt. Pyre']=CITY.pop('Mt. Pyre')
CAVES={'Meteor Falls','Granite Cave','Fiery Path','Island Cave','Seafloor Cavern','Cave of Origin','Shoal Cave','Scorched Slab','Ancient Tomb','Desert Ruins','New Mauville','Sky Pillar','Jagged Pass','Mt. Chimney','Petalburg Woods'}
# ---- drawing, in the look of Sword / Shield's town map
S=3
def W2(p): return ((p[0]-X0)*S,(p[1]-Z0)*S)
SEA=(84,190,216);SEA2=(120,210,230);COAST=(38,112,146);LAND=(176,216,98);LAND2=(150,198,82);RED=(236,70,86);NAVY=(40,52,120)
im=Image.new('RGB',(WW*S,HH*S),SEA)
mk=Image.fromarray((LANDW*255).astype(np.uint8)).resize((WW*S,HH*S),Image.BILINEAR).filter(ImageFilter.GaussianBlur(6)).point(lambda v:255 if v>128 else 0)
im.paste(SEA2,(0,0),mk.filter(ImageFilter.MaxFilter(41)))           # shallows
im.paste(COAST,(0,0),mk.filter(ImageFilter.MaxFilter(13)))          # thick coast line
im.paste(LAND2,(0,0),mk)
im.paste(LAND,(0,0),mk.filter(ImageFilter.MinFilter(25)))            # lighter inland
d=ImageDraw.Draw(im,'RGBA')
def run(q,dotted):
  if not dotted:
    d.line(q,fill=RED,width=22,joint='curve')
    for X,Y in q: d.ellipse([X-11,Y-11,X+11,Y+11],fill=RED)
    return
  tot=seglen(q);nd=max(1,int(tot/34))
  for j in range(nd+1):
    t=tot*j/nd
    for a2,b2 in zip(q,q[1:]):
      L=math.hypot(b2[0]-a2[0],b2[1]-a2[1])
      if t<=L+1e-6: X,Y=a2[0]+(b2[0]-a2[0])*t/(L or 1),a2[1]+(b2[1]-a2[1])*t/(L or 1);d.ellipse([X-9,Y-9,X+9,Y+9],fill=RED);break
      t-=L
for k,(sea,o,_) in SCH.items(): run([W2(p) for p in o],sea)
for a,b_ in CAVE:
  (x0,y0),(x1,y1)=W2(a),W2(b_);L=math.hypot(x1-x0,y1-y0);k=max(2,int(L/30))
  for t in range(0,k,2): d.line([(x0+(x1-x0)*t/k,y0+(y1-y0)*t/k),(x0+(x1-x0)*(t+1)/k,y0+(y1-y0)*(t+1)/k)],fill=RED,width=16)
occ=Image.new('L',im.size,0);od=ImageDraw.Draw(occ)
for k,(sea,o,_) in SCH.items(): od.line([W2(p) for p in o],fill=255,width=40)
FB=lambda n:ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',n)
F,FS,FM=FB(34),FB(21),FB(24)
# route numbers: red discs with white numbers, on the line
def nearest_on(p,o):
  best=None
  for a2,b2 in zip(o,o[1:]):
    vx,vy=b2[0]-a2[0],b2[1]-a2[1];L2=vx*vx+vy*vy or 1;t=max(0,min(1,((p[0]-a2[0])*vx+(p[1]-a2[1])*vy)/L2))
    c=(a2[0]+t*vx,a2[1]+t*vy);dd=math.hypot(p[0]-c[0],p[1]-c[1])
    if best is None or dd<best[0]: best=(dd,c)
  return best
def along(o,f):
  t=seglen(o)*f
  for a2,b2 in zip(o,o[1:]):
    L=math.hypot(b2[0]-a2[0],b2[1]-a2[1])
    if t<=L: return (a2[0]+(b2[0]-a2[0])*t/(L or 1),a2[1]+(b2[1]-a2[1])*t/(L or 1))
    t-=L
  return o[-1]
labs={}
for n,pq in JUNC.items(): labs[n]=pq
for k,(sea,o,mids) in SCH.items():
  for n in set(mids): labs.setdefault(n,nearest_on(H[n],o)[1])
k=tuple(sorted(['Littleroot Town','Oldale Town']));labs['Route 101']=along(SCH[k][1],0.5)
labs['Route 122']=along(SCH[('Mt. Pyre','Route 121')][1],0.5)
if 'Route 134' in labs and 'Route 132' in labs: labs['Route 133']=((labs['Route 134'][0]+labs['Route 132'][0])/2,(labs['Route 134'][1]+labs['Route 132'][1])/2)
EDGE_OF={}
for k,(sea,o,mids) in SCH.items():
  for n in list(mids)+[x for x in k if x.startswith('Route')]: EDGE_OF.setdefault(n,o)
EDGE_OF['Route 101']=SCH[tuple(sorted(['Littleroot Town','Oldale Town']))][1];EDGE_OF['Route 122']=SCH[('Mt. Pyre','Route 121')][1]
EDGE_OF['Route 133']=next(o for k,(sea,o,m) in SCH.items() if 'Route 134' in m or 'Route 134' in k)
MARK=[pq for pq in list(CITY.values())+list(LMK.values())]
def clear(pq): return min(math.hypot(pq[0]-m[0],pq[1]-m[1]) for m in MARK)
for n in list(labs):
  o=EDGE_OF.get(n)
  if not o: continue
  base=nearest_on(labs[n],o)[1];L=seglen(o)
  # position along the edge
  acc=0;f0=0
  for a2,b2 in zip(o,o[1:]):
    l=math.hypot(b2[0]-a2[0],b2[1]-a2[1]);vx,vy=b2[0]-a2[0],b2[1]-a2[1]
    t=max(0,min(1,((base[0]-a2[0])*vx+(base[1]-a2[1])*vy)/((l*l) or 1)))
    if math.hypot(a2[0]+t*vx-base[0],a2[1]+t*vy-base[1])<0.5: f0=(acc+t*l)/(L or 1);break
    acc+=l
  if clear(base)>=14: labs[n]=base;continue
  best=max((along(o,min(1,max(0,f0+df/(L or 1)))) for df in range(-60,61,4)),key=lambda q:min(clear(q),14)-abs(math.hypot(q[0]-base[0],q[1]-base[1]))*0.01)
  labs[n]=best
for n,pq in labs.items():
  X,Y=W2(pq);t=n.split()[1];r=24
  d.ellipse([X-r,Y-r,X+r,Y+r],fill=RED,outline=(255,255,255),width=4);d.text((X,Y),t,fill=(255,255,255),font=FS,anchor='mm')
  od.ellipse([X-r-6,Y-r-6,X+r+6,Y+r+6],fill=255)
# places: Sword / Shield's blue diamond with a yellow heart for cities and towns; caves purple squares; landmarks white dots
def diamond(X,Y,big):
  r=30 if big else 24;d.polygon([(X,Y-r),(X+r,Y),(X,Y+r),(X-r,Y)],fill=(30,70,200),outline=(255,255,255))
  d.polygon([(X,Y-r),(X+r,Y),(X,Y+r),(X-r,Y)],outline=(255,255,255),width=5)
  q=r*0.5;d.polygon([(X,Y-q),(X+q,Y),(X,Y+q),(X-q,Y)],fill=(255,214,40))
def cavebox(X,Y): d.rounded_rectangle([X-16,Y-16,X+16,Y+16],radius=5,fill=(120,60,200),outline=(255,255,255),width=4)
def spot(X,Y): d.ellipse([X-13,Y-13,X+13,Y+13],fill=(255,255,255),outline=RED,width=6)
P=[]
for n,pq in CITY.items(): P.append((n,pq,'diamond'))
for n,pq in LMK.items(): P.append((n,pq,'cave' if n in CAVES or n=='Victory Road' else 'spot'))
P=[x for x in P if x[0]!='Victory Road']+[('Victory Road',CITY['Victory Road'],'cave')]
for n,pq,k in P: X,Y=W2(pq);od.rectangle([X-32,Y-32,X+32,Y+32],fill=255)
for n,pq,k in P:
  X,Y=W2(pq)
  if k=='diamond': diamond(X,Y,n.endswith('City') or n=='Pokémon League')
  elif k=='cave': cavebox(X,Y)
  else: spot(X,Y)
oa=np.array(occ)
for n,pq,k in sorted(P,key=lambda x:x[2]!='diamond'):
  X,Y=W2(pq);f=F if k=='diamond' else FM;best=None
  for ax,ay,anc,pen in ((40,0,'lm',0),(-40,0,'rm',0),(0,-40,'mb',4),(0,40,'mt',4),(34,-34,'lb',8),(34,34,'lt',8),(-34,-34,'rb',8),(-34,34,'rt',8)):
    x0,y0,x1,y1=[int(v) for v in d.textbbox((X+ax,Y+ay),n,font=f,anchor=anc)];x0-=14;x1+=14;y0-=8;y1+=8
    if x0<0 or y0<0 or x1>=im.width or y1>=im.height: continue
    c=oa[y0:y1,x0:x1].mean()+pen
    if best is None or c<best[0]: best=(c,(X+ax,Y+ay),anc,(x0,y0,x1,y1))
  _,xy,anc,(x0,y0,x1,y1)=best
  d.rounded_rectangle([x0,y0,x1,y1],radius=12,fill=(48,48,40,215))
  d.text(xy,n,fill=(255,255,255),font=f,anchor=anc);oa[y0:y1,x0:x1]=255
# title and key, Sword / Shield style banners

kx,ky=im.width-760,im.height-470
d.rounded_rectangle([kx,ky,kx+720,ky+430],radius=26,fill=(48,48,40,225))
rows=[('Route',lambda x,y:d.line([(x,y),(x+90,y)],fill=RED,width=22)),('Sea route (Surf)',lambda x,y:[d.ellipse([x+i-9,y-9,x+i+9,y+9],fill=RED) for i in range(0,91,30)]),
 ('Cave or tunnel',lambda x,y:[d.line([(x+i,y),(x+i+22,y)],fill=RED,width=16) for i in range(0,80,44)]),('City / Town',lambda x,y:diamond(x+45,y,True)),
 ('Cave',lambda x,y:cavebox(x+45,y)),('Landmark',lambda x,y:spot(x+45,y)),('Route number',lambda x,y:(d.ellipse([x+21,y-24,x+69,y+24],fill=RED,outline=(255,255,255),width=4),d.text((x+45,y),'101',fill=(255,255,255),font=FS,anchor='mm')))]
for i,(t,f) in enumerate(rows): y=ky+50+i*56;f(kx+40,y);d.text((kx+170,y),t,fill=(255,255,255),font=FM,anchor='lm')
TOP=230;big=Image.new('RGB',(im.width,im.height+TOP),SEA);big.paste(im,(0,TOP));bd=ImageDraw.Draw(big,'RGBA')
bd.rounded_rectangle([40,40,860,190],radius=30,fill=(48,48,40,230));bd.text((80,115),'HOENN',fill=(255,255,255),font=FB(90),anchor='lm')
bd.text((470,115),'Omega Ruby / Alpha Sapphire',fill=(255,214,40),font=FB(30),anchor='lm')
big.save('hoenn_swsh.png')
off=[n for n,pq,k in P if not LANDW[int(pq[1]-Z0),int(pq[0]-X0)]]
print('places:',len(P),'off land:',off)
# the empty sea between the title band and the land, trimmed
a=np.array(Image.open('hoenn_swsh.png'))[:,:,:3].astype(int)
seam=(np.abs(a-np.array(SEA)).sum(2)<12)|(np.abs(a-np.array(SEA2)).sum(2)<12)
top=np.nonzero((~seam[TOP:]).sum(1)>5)[0][0]+TOP-60
if top>TOP:
  big=Image.open('hoenn_swsh.png');out=Image.new('RGB',(big.width,big.height-(top-TOP)));out.paste(big.crop((0,0,big.width,TOP)),(0,0));out.paste(big.crop((0,top,big.width,big.height)),(0,TOP));out.save('hoenn_map.png')
