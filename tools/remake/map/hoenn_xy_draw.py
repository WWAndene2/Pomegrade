# Hoenn in X/Y's town map style (owner, 27-28 October): the land of the DexNav screen (ref/dexnav.jpg) widened past its
# edges, the relief from the pieces' 3D terrain (map/height.npy, hoenn_height.py) in six flat terraces with outlines, the
# places on the DexNav squares, the routes redrawn by hand over the game's own roads (map/paths.pkl, warped onto this map),
# X's icons redrawn as vectors (Mauville the capital, its Poke Ball open only where a route meets it), sea routes as dots.
# Approximate: the hand-drawn route bends, the 10 places the DexNav omits (EXTRA), the land added past the screen's edges.
# Also reads xtm/png/._timg_townmap_bg1.bclim.png (X's Kalos map, a/0/6/9 member 0 decoded by darc_png; its colours sampled
# in an earlier step of this script, the final drawing uses its own flat palette); writes hoenn_xy_map.png
# Hoenn drawn as X/Y's town map (Kalos's townmap_bg1): relief greens to browns with contours, a blue sea with a grid,
# white routes between grey stops, orange dots for places. Positions: the DexNav screen's squares (by eye, approximate)
import numpy as np,math
from PIL import Image,ImageDraw,ImageFilter
from collections import deque
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
LAND=np.isin(lab,keep)[:630]
# relief: the DexNav map's own shading of the land (lighter = higher), its route squares filled from around them
green=(g>r)&(g>b+20)&LAND[:,:] if False else None
G=ref[:630,:,1].astype(float);R_=ref[:630,:,0].astype(float)
isg=(ref[:630,:,1]>ref[:630,:,0])&(ref[:630,:,1]>ref[:630,:,2]+20)&LAND
elev=np.where(isg,(G+R_)/2,np.nan)
# fill the holes (route squares, cities) with the mean of their neighbours, repeatedly
E=elev.copy()
for it in range(40):
  nan=np.isnan(E)
  if not nan[LAND].any(): break
  P=np.pad(E,1,constant_values=np.nan);S_=np.zeros_like(E);C=np.zeros_like(E)
  for dy in (-1,0,1):
    for dx in (-1,0,1):
      v=P[1+dy:1+dy+E.shape[0],1+dx:1+dx+E.shape[1]];ok=~np.isnan(v);S_[ok]+=v[ok];C[ok]+=1
  fill=nan&(C>0);E[fill]=S_[fill]/C[fill]
E=np.nan_to_num(E,nan=np.nanmin(elev))
# a coastal falloff so the coast is low
dist=np.zeros(LAND.shape);cur=LAND.copy()
for k in range(1,30):
  cur=np.array(Image.fromarray((cur*255).astype(np.uint8)).filter(ImageFilter.MinFilter(3)))>0;dist+=cur
e8=np.clip(E,0,255).astype(np.uint8);E=np.array(Image.fromarray(e8).filter(ImageFilter.GaussianBlur(4))).astype(float)
lo,hi=np.percentile(E[LAND],5),np.percentile(E[LAND],97)
h=np.clip((E-lo)/(hi-lo),0,1)*0.7+np.clip(dist/30,0,1)*0.3

# ---- drawn at X's own size (the DexNav screen's 960 x 630 is three times it), every colour, gradient, outline and icon
# taken from X's townmap_bg1 itself
X_=np.array(Image.open('xtm/png/._timg_townmap_bg1.bclim.png').convert('RGB').crop((0,0,320,210))).astype(float)
xr,xg,xb=X_[...,0],X_[...,1],X_[...,2]
xsea=(xb>xr+40)&(xb>xg)
xicon=((abs(xr-xg)<10)&(abs(xg-xb)<12)&((xr>150)|(xr<80)))|((xr>170)&(xg<140)&(xb<80))
from PIL import ImageFilter as IF
grow=lambda m,k: np.array(Image.fromarray((m*255).astype(np.uint8)).filter(IF.MaxFilter(k)))>0
xicon=grow(xicon,5)
def distance(mask,steps):
  dd=np.full(mask.shape,steps,float);cur=mask.copy()
  for k in range(steps):
    dd[cur&(dd==steps)]=k;cur=grow(cur,3)
  return dd
xland=~xsea
# only the drawn part of Kalos: the image's top and right are padding (stretched edge pixels)
valid=np.zeros(xsea.shape,bool);valid[55:,:300]=True
xland&=valid
xd_sea=distance(xland,40)          # a sea pixel's distance to the land
xd_land=distance(xsea,12)          # a land pixel's distance to the sea
seacol=[X_[(xsea)&(~xicon)&(xd_sea==k)].mean(0) if ((xsea)&(~xicon)&(xd_sea==k)).sum() else None for k in range(41)]
for k in range(41):
  if seacol[k] is None: seacol[k]=seacol[k-1]
rimcol=[X_[(xland)&(~xicon)&(xd_land==k)].mean(0) for k in range(1,5)]
# the land's gradient: X's inland colours ordered from the greenest (low) to the brownest (high), by quantile
inl=X_[(xland)&(~xicon)&(xd_land>=4)]
order=np.argsort(-(inl[:,1]-inl[:,0]))
ramp=np.array([inl[order[int(i*(len(order)-1)/255)]] for i in range(256)])
ramp=np.array(Image.fromarray(ramp[None].astype(np.uint8)).filter(IF.BoxBlur(6)))[0].astype(float)
# the whole region: the DexNav screen cuts Hoenn at its edges, so the land is given a margin of 60 px all round, the
# land touching an edge carried 30 px past it and rounded off
MG=60
LANDp=np.zeros((630+2*MG,960+2*MG),bool);LANDp[MG:MG+630,MG:MG+960]=LAND
for k in range(1,31):
  LANDp[MG:MG+630,MG-k]|=LAND[:,0];LANDp[MG:MG+630,MG+959+k]|=LAND[:,959]
  LANDp[MG-k,MG:MG+960]|=LAND[0,:];LANDp[MG+629+k,MG:MG+960]|=LAND[629,:]
sm_=np.array(Image.fromarray((LANDp*255).astype(np.uint8)).filter(IF.GaussianBlur(9)))>128
near_=np.ones(LANDp.shape,bool);near_[MG+40:MG+590,MG+40:MG+920]=False   # only the edges' strips are rounded
LANDp=np.where(near_,sm_,LANDp)
LAND=LANDp
W,H=(960+2*MG)//2,(630+2*MG)//2
Ml=np.array(Image.fromarray((LAND*255).astype(np.uint8)).resize((W,H),Image.BILINEAR))>127
# the real relief: the height of each piece's 3D terrain (map/height.npy, the world's tiles x 4), sampled through the
# map -> world transform, the gaps between the roads filled from around them, smoothed
import pickle,json as _j
Dp0=pickle.load(open('map/paths.pkl','rb'));Hw0=Dp0['H']
def _U(r): return np.where(r>0,r*r*np.log(r+1e-12),0)
def _tps(src,dst,lam):
  n=len(src);L=np.zeros((n+3,n+3));L[:n,:n]=_U(np.linalg.norm(src[:,None]-src[None],axis=2))+lam*np.eye(n)
  Pm=np.hstack([np.ones((n,1)),src]);L[:n,n:]=Pm;L[n:,:n]=Pm.T;w=np.linalg.solve(L,np.vstack([dst,np.zeros((3,2))]))
  def f(q):
    out=np.empty((len(q),2))
    for i in range(0,len(q),20000):
      s_=q[i:i+20000];out[i:i+20000]=_U(np.linalg.norm(s_[:,None]-src[None],axis=2))@w[:n]+np.hstack([np.ones((len(s_),1)),s_])@w[n:]
    return out
  return f
Cm={'Littleroot Town':(161,468),'Oldale Town':(161,402),'Petalburg City':(62,402),'Rustboro City':(29,254),'Fallarbor Town':(128,72),'Lavaridge Town':(194,204),'Verdanturf Town':(161,270),'Mauville City':(310,270),'Slateport City':(293,452),'Dewford Town':(95,567),'Fortree City':(425,72),'Lilycove City':(672,172),'Mossdeep City':(836,224),'Sootopolis City':(740,348),'Pacifidlog Town':(623,435),'Ever Grande City':(920,402)}
nm0=[n for n in Cm if n in Hw0]
toW=_tps(np.array([Cm[n] for n in nm0],float),np.array([Hw0[n] for n in nm0]),2000.0)
def gblur(a,sig):
  r=int(3*sig)+1;k=np.exp(-np.arange(-r,r+1)**2/(2*sig*sig));k/=k.sum()
  p=np.pad(a,((0,0),(r,r)),mode='edge');a=sum(k[i]*p[:,i:i+a.shape[1]] for i in range(len(k)))
  p=np.pad(a,((r,r),(0,0)),mode='edge');return sum(k[i]*p[i:i+a.shape[0],:] for i in range(len(k)))
HM=np.load('map/height.npy');HJ=_j.load(open('map/height.json'))
yy0,xx0=np.mgrid[0:H,0:W];wq=toW(np.stack([xx0.ravel()*2.0-MG,yy0.ravel()*2.0-MG],1).astype(float))
ix=((wq[:,0]-HJ['minx'])*4).astype(int);iz=((wq[:,1]-HJ['minz'])*4).astype(int)
okq=(ix>=0)&(iz>=0)&(ix<HM.shape[1])&(iz<HM.shape[0])
hv=np.full(len(wq),np.nan);
# each map pixel the highest of the world's 3 x 3 tiles around it (a map pixel covers about 2 tiles)
for dz in (-6,0,6):
  for dx in (-6,0,6):
    jx=np.clip(ix+dx,0,HM.shape[1]-1);jz=np.clip(iz+dz,0,HM.shape[0]-1);vv=np.where(okq,HM[jz,jx],np.nan)
    hv=np.fmax(hv,vv)
hv=hv.reshape(H,W)
known=~np.isnan(hv)&Ml
E_=np.where(known,hv,0.0);Wt=known.astype(float)
for sig in (2,4,8,16,32):
  a_=gblur(E_*(Wt>0),sig);b_=gblur((Wt>0).astype(float),sig)
  fill=(~known)&(b_>1e-3)&(Wt==0);E_[fill]=a_[fill]/b_[fill];Wt[fill]=0.5
E_[~Ml]=np.nanmin(hv)
hh=gblur(E_,1.5)
# h as a quantile, so Hoenn's heights spread over X's colours as Kalos's do

# ---- drawn from scratch, at 4x, flat cartoon colours: terraces of the real relief with their outlines, a flat sea,
# icons and routes as vectors (X's shapes and colours)
SCL=4;WB,HB=W*SCL,H*SCL
def up(a):
  lo_,hi_=float(np.min(a)),float(np.max(a));n_=((a-lo_)/(hi_-lo_+1e-9)*255).astype(np.uint8)
  return np.array(Image.fromarray(n_).resize((WB,HB),Image.BICUBIC)).astype(float)
mkB=Image.fromarray((Ml*255).astype(np.uint8)).resize((WB,HB),Image.BILINEAR).filter(IF.GaussianBlur(3)).point(lambda v:255 if v>128 else 0)
MB=np.array(mkB)>0
hB=up(hh)
v=hB[MB];qs=np.percentile(v,[30,55,72,85,94])
lev=np.digitize(hB,qs)
lev=np.array(Image.fromarray((lev*40).astype(np.uint8)).filter(IF.MedianFilter(9)))//40
FLAT=[(120,190,120),(150,200,110),(176,196,104),(168,160,92),(150,122,74),(132,96,64)]
img=np.zeros((HB,WB,3),np.uint8)
img[:]=(52,140,206)
shallow=np.array(mkB.filter(IF.MaxFilter(41)))>0
img[shallow]=(92,172,226)
for i,c in enumerate(FLAT): img[MB&(lev==i)]=c
im=Image.fromarray(img);d=ImageDraw.Draw(im,'RGBA')
# grid on the sea
for gx_ in range(0,WB,256): d.line([(gx_,0),(gx_,HB)],fill=(40,110,170),width=3)
for gy_ in range(0,HB,256): d.line([(0,gy_),(WB,gy_)],fill=(40,110,170),width=3)
im.paste((0,0,0),(0,0),Image.new("L",(WB,HB),0))
# relief outlines: between terraces, a darker line of the lower one
edge=np.zeros((HB,WB),bool);edge[1:,:]|=lev[1:,:]!=lev[:-1,:];edge[:,1:]|=lev[:,1:]!=lev[:,:-1];edge&=MB
edge=np.array(Image.fromarray((edge*255).astype(np.uint8)).filter(IF.MaxFilter(3)))>0
a=np.array(im);a[edge]=(a[edge]*0.72).astype(np.uint8)
# the coast: a dark outline
coast=(np.array(mkB.filter(IF.MaxFilter(7)))>0)&~MB
a[coast]=(28,70,96)
im=Image.fromarray(a);d=ImageDraw.Draw(im,'RGBA')
# ---- the DexNav screen's own squares (29 x 19, read square by square): towns T, land routes R, sea routes S, places P
PX,PY,OX,OY=33.15,32.91,26.6+MG,8.1+MG
GRID={}
def put(k,cells):
  for c in cells: GRID[c]=k
R=lambda *cs:put('R',cs);S_=lambda *cs:put('S',cs);T=lambda name,*cs:[GRID.__setitem__(c,'T:'+name) for c in cs];Pl=lambda name,c:GRID.__setitem__(c,'P:'+name)
row=lambda j,i0,i1:[(i,j) for i in range(i0,i1+1)];col=lambda i,j0,j1:[(i,j) for j in range(j0,j1+1)]
R(*row(2,4,7),*col(8,2,12),*col(2,2,4),*col(0,4,6),*row(7,1,3),*row(8,5,7),*row(6,6,7),*col(0,9,12),*row(12,2,3),*row(11,4,7),(4,13),
  *col(11,2,8),*row(8,10,17),*col(13,2,5),*row(5,14,18))
S_(*col(0,13,16),*row(16,1,2),*row(17,3,8),*col(8,15,16),*row(13,9,17),*row(13,19,26),(16,6),(17,6))
# the 124-128 block, its routes as lines: 124 Lilycove to Mossdeep, 125 to Shoal Cave, 126 down to Sootopolis, 127 and
# 128 down to Ever Grande
S_(*row(5,21,23),(23,6),(23,7),(26,6),*col(21,6,10),*col(25,8,12),(26,12))
T('Fallarbor Town',(3,2));T('Fortree City',(12,2));T('Lavaridge Town',(5,6));T('Rustboro City',(0,7),(0,8));T('Verdanturf Town',(4,8))
T('Mauville City',(8,8),(9,8));T('Petalburg City',(1,12));T('Oldale Town',(4,12));T('Slateport City',(8,13),(8,14));T('Littleroot Town',(4,14))
T('Dewford Town',(2,17));T('Lilycove City',(19,5),(20,5));T('Mossdeep City',(24,7),(25,7));T('Sootopolis City',(22,10));T('Pacifidlog Town',(18,13))
T('Ever Grande City',(27,12));T('Pokemon League',(27,10))
for n,c in {'Meteor Falls':(1,4),'Mt. Chimney':(6,4),'Jagged Pass':(6,5),'Fiery Path':(7,5),'Rusturf Tunnel':(4,7),'New Mauville':(9,9),'Petalburg Woods':(0,10),
  'Granite Cave':(1,17),'Sea Mauville':(7,16),'Southern Island':(13,17),'Safari Zone':(16,4),'Mt. Pyre':(17,7),'Shoal Cave':(26,5),'Victory Road':(27,11),
  'Sky Pillar':(20,12),'Seafloor Cavern':(24,12),'Battle Resort':(22,15)}.items(): Pl(n,c)
cen=lambda c:((OX+c[0]*PX)*2,(OY+c[1]*PY)*2)
groups={}
for c,k in GRID.items():
  if k[0] in 'TP': groups.setdefault(k,[]).append(c)
spot={k:(sum(cen(c)[0] for c in v)/len(v),sum(cen(c)[1] for c in v)/len(v)) for k,v in groups.items()}
at=lambda c:spot[GRID[c]] if GRID[c][0] in 'TP' else cen(c)
# the roads: the game's own (traced over the tiles, map/paths.pkl), carried onto this map by a warp fitted on every
# town and every named place (their game positions -> their squares here), so they meet the icons where the game does
LMd=pickle.load(open('map/landmarks.pkl','rb'));LMd.update(pickle.load(open('map/landmarks2.pkl','rb')))
src=[];dst=[]
for k,xy in spot.items():
  nm_=k[2:].replace('Pokemon','Pokémon')
  if k[0]=='T' and nm_ in Hw0: src.append(Hw0[nm_]);dst.append(xy)
  if k[0]=='P' and nm_ in LMd:
    v_=LMd[nm_];src.append((sum(p[0] for p in v_)/len(v_),sum(p[1] for p in v_)/len(v_)));dst.append(xy)
warp=_tps(np.array(src,float),np.array(dst,float),50.0)
res=np.abs(warp(np.array(src,float))-np.array(dst)).max();print('warp control residual px',round(res,1),len(src),'points')
ICONS=list(spot.values())
def dp2(pts,eps):
  if len(pts)<3: return list(pts)
  (x0,y0),(x1,y1)=pts[0],pts[-1];L_=math.hypot(x1-x0,y1-y0) or 1
  dm,i=max((abs((y1-y0)*x-(x1-x0)*y+x1*y0-y1*x0)/L_,i) for i,(x,y) in enumerate(pts))
  return dp2(pts[:i+1],eps)[:-1]+dp2(pts[i:],eps) if dm>eps else [pts[0],pts[-1]]
seg=[];segS=[];ROUTEQ=[]
for sea_,pth in Dp0['PATHS']:
  q=[tuple(p) for p in warp(np.array(pth,float))]
  for i in (0,-1):
    t=min(ICONS,key=lambda t:math.hypot(t[0]-q[i][0],t[1]-q[i][1]))
    if math.hypot(t[0]-q[i][0],t[1]-q[i][1])<70: q[i]=t
  q=dp2(q,30.0 if sea_ else 22.0)
  o=[q[0]]
  for b2 in q[1:]:
    a2=o[-1];dx,dy=b2[0]-a2[0],b2[1]-a2[1];k_=min(abs(dx),abs(dy));m=(b2[0]-math.copysign(k_,dx),b2[1]-math.copysign(k_,dy))
    if math.hypot(m[0]-a2[0],m[1]-a2[1])>2 and math.hypot(m[0]-b2[0],m[1]-b2[1])>2: o.append(m)
    o.append(b2)
  if math.hypot(o[0][0]-o[-1][0],o[0][1]-o[-1][1])<10: continue
  ROUTEQ.append((sea_,o))
# cleaning: route ends near each other joined at one point; a route with an end that meets nothing dropped when short
ends=[]
for i,(sea_,o) in enumerate(ROUTEQ): ends+=[(i,0),(i,-1)]
pt=lambda e:ROUTEQ[e[0]][1][e[1]]
for e in ends:
  for f in ends:
    if e<f and e[0]!=f[0] and pt(e) not in ICONS and math.hypot(pt(e)[0]-pt(f)[0],pt(e)[1]-pt(f)[1])<45:
      ROUTEQ[e[0]][1][e[1]]=pt(f)
def attached(e):
  p=pt(e)
  return p in ICONS or any(f[0]!=e[0] and pt(f)==p for f in ends) or any(f!=e[0] and any(math.hypot(p[0]-x,p[1]-y)<12 for x,y in o2) for f,(s2,o2) in enumerate(ROUTEQ))
lenq=lambda o:sum(math.hypot(b[0]-a[0],b[1]-a[1]) for a,b in zip(o,o[1:]))
ROUTEQ=[(s_,o) for i,(s_,o) in enumerate(ROUTEQ) if (attached((i,0)) and attached((i,-1))) or lenq(o)>260]
# a loose end that still meets nothing: cut back to its last bend
for s_,o in ROUTEQ:
  for e in (0,-1):
    if o[e] not in ICONS and len(o)>2 and not any(o[e] in o2 for s2,o2 in ROUTEQ if o2 is not o): o.pop(e)
# the roads redrawn by hand over the game's own (the warped tiles' roads), each one tidied: straight and 45-degree
# legs, its bends where the game's road bends, starting and ending on its icons (positions read in this map's frame)
F=1920/1500
def SP(n): return spot[n]
def V(*pts): return [p if isinstance(p,tuple) and len(p)==2 and not isinstance(p[0],str) else p for p in pts]
def at_(k): return spot[k]
W_=lambda x,y:(x*F+2*MG,y*F+2*MG)
spot['P:New Mauville']=(spot['P:New Mauville'][0]+44,spot['P:New Mauville'][1]+34)
LANDP=[
 [at_('T:Fallarbor Town'),W_(245,160),W_(300,160),W_(330,190),W_(390,190),W_(435,235),W_(435,275),W_(485,275),W_(458,300),W_(458,350),at_('T:Mauville City')],
 [W_(435,275),W_(405,270),at_('P:Fiery Path')],[at_('P:Fiery Path'),at_('P:Jagged Pass')],[at_('P:Jagged Pass'),at_('T:Lavaridge Town')],[at_('P:Mt. Chimney'),W_(350,240),at_('P:Jagged Pass')],
 [at_('T:Fallarbor Town'),at_('P:Meteor Falls')],
 [at_('T:Rustboro City'),W_(20,295),W_(20,285),W_(65,285),at_('P:Meteor Falls')],
 [at_('T:Rustboro City'),W_(110,352),W_(235,352),at_('P:Rusturf Tunnel')],[at_('P:Rusturf Tunnel'),at_('T:Verdanturf Town')],
 [at_('T:Verdanturf Town'),W_(385,423),at_('T:Mauville City')],
 [at_('T:Rustboro City'),at_('P:Petalburg Woods'),W_(40,560),W_(93,600),at_('T:Petalburg City')],
 [at_('T:Petalburg City'),at_('T:Oldale Town')],[at_('T:Oldale Town'),at_('T:Littleroot Town')],
 [at_('T:Oldale Town'),W_(285,575),W_(405,575),W_(440,540),W_(455,540)],
 [at_('T:Mauville City'),W_(455,540),at_('T:Slateport City')],
 [at_('T:Mauville City'),at_('P:New Mauville')],
 [at_('T:Mauville City'),W_(570,423),W_(650,343),W_(650,270),W_(632,252),W_(632,140),at_('T:Fortree City')],
 [at_('T:Mauville City'),W_(570,423),W_(735,423),W_(735,355),W_(915,355),at_('P:Mt. Pyre')],
 [at_('T:Fortree City'),W_(738,215),W_(738,320),W_(770,355)],
 [W_(915,355),W_(965,355),at_('T:Lilycove City')],
 [W_(870,355),at_('P:Safari Zone')],
 [at_('T:Ever Grande City'),at_('P:Victory Road'),at_('T:Pokemon League')]]
SEAP=[
 [at_('T:Petalburg City'),W_(10,600),W_(10,795),W_(55,840),at_('T:Dewford Town')],[at_('T:Dewford Town'),at_('P:Granite Cave')],
 [at_('T:Dewford Town'),W_(350,885),at_('P:Sea Mauville'),W_(485,835),W_(485,735),at_('T:Slateport City')],
 [at_('T:Slateport City'),W_(545,688),at_('T:Pacifidlog Town')],[W_(545,688),W_(545,780),at_('P:Southern Island')],
 [at_('T:Pacifidlog Town'),W_(1030,640),at_('P:Sky Pillar')],[at_('P:Sky Pillar'),W_(1170,700),W_(1170,805),W_(1360,805),W_(1410,755),W_(1410,650),at_('T:Ever Grande City')],
 [W_(1170,805),at_('P:Battle Resort')],
 [at_('T:Lilycove City'),W_(1165,343),W_(1290,343),at_('T:Mossdeep City')],[at_('T:Mossdeep City'),W_(1388,300),at_('P:Shoal Cave')],
 [W_(1165,343),W_(1165,520),at_('T:Sootopolis City')],[at_('T:Sootopolis City'),at_('P:Seafloor Cavern')],
 [at_('T:Mossdeep City'),W_(1310,430),W_(1240,500),W_(1240,520)],[at_('P:Seafloor Cavern'),W_(1285,690),W_(1360,760),W_(1360,805)]]
seg=[];segS=[]
for o in LANDP:
  for p0,p1 in zip(o,o[1:]): seg.append((p0,p1))
for o in SEAP:
  for p0,p1 in zip(o,o[1:]): segS.append((p0,p1))

# the places the DexNav screen leaves out, each put on its route (the nearest point of the route's line; positions read
# on this map from Hoenn's geography: approximate)
T2=lambda x,y:(x*1.44,y*1.44)
EXTRA={'Weather Institute':(T2(740,300),0),'Desert Ruins':(T2(492,385),0),'Scorched Slab':(T2(700,320),0),'Ancient Tomb':(T2(790,395),0),
 'Trick House':(T2(490,640),0),'Island Cave':(T2(92,740),1),'Sealed Chamber':(T2(780,690),1),'Secret Islet':(T2(1120,500),1),
 'Secret Meadow':(T2(1080,690),1),'Secret Shore':(T2(1200,800),1)}
def onroute(p,lst):
  best=None
  for p0,p1 in lst:
    vx,vy=p1[0]-p0[0],p1[1]-p0[1];L2=vx*vx+vy*vy or 1;t=max(0,min(1,((p[0]-p0[0])*vx+(p[1]-p0[1])*vy)/L2))
    q=(p0[0]+t*vx,p0[1]+t*vy);dd=math.hypot(q[0]-p[0],q[1]-p[1])
    if best is None or dd<best[0]: best=(dd,q)
  return best[1]
for n,(p,sea_) in EXTRA.items():
  q=onroute(p,segS if sea_ else seg)
  if all(math.hypot(q[0]-x,q[1]-y)>56 for x,y in spot.values()): spot['P:'+n]=q
  else: print('no room for',n)
DK=(46,54,56);WH=(248,248,248)
for p0,p1 in seg: d.line([p0,p1],fill=DK,width=22)
for p0,p1 in seg:
  for X,Y in (p0,p1): d.ellipse([X-11,Y-11,X+11,Y+11],fill=DK)
for p0,p1 in seg: d.line([p0,p1],fill=WH,width=9)
for p0,p1 in seg:
  for X,Y in (p0,p1): d.ellipse([X-4.5,Y-4.5,X+4.5,Y+4.5],fill=WH)
# sea routes: evenly spaced round dots, each leg's dots spread to fit it exactly, one shared dot at every bend and junction
# (no dash laid over another)
nodes_=set()
for p0,p1 in segS: nodes_.add(p0);nodes_.add(p1)
done_=set()
for p0,p1 in segS:
  if (p1,p0) in done_ or (p0,p1) in done_: continue
  done_.add((p0,p1));Lg=math.hypot(p1[0]-p0[0],p1[1]-p0[1]);k=max(1,round(Lg/30))
  for t in range(1,k):
    X=p0[0]+(p1[0]-p0[0])*t/k;Y=p0[1]+(p1[1]-p0[1])*t/k;d.ellipse([X-6,Y-6,X+6,Y+6],fill=WH)

deg_={}
for p0,p1 in segS: deg_[p0]=deg_.get(p0,0)+1;deg_[p1]=deg_.get(p1,0)+1
for X,Y in nodes_:
  if deg_[(X,Y)]<3 and not any(math.hypot(X-x,Y-y)<30 for x,y in spot.values()): d.ellipse([X-6,Y-6,X+6,Y+6],fill=WH)
  if deg_[(X,Y)]>=3 and not any(math.hypot(X-x,Y-y)<30 for x,y in spot.values()): d.ellipse([X-11,Y-11,X+11,Y+11],fill=DK);d.ellipse([X-7,Y-7,X+7,Y+7],fill=WH)
def stop(X,Y,r):
  d.ellipse([X-r-14,Y-r-14,X+r+14,Y+r+14],fill=DK);d.ellipse([X-r-5,Y-r-5,X+r+5,Y+r+5],fill=WH);d.ellipse([X-r,Y-r,X+r,Y+r],fill=(196,198,204))
def place(X,Y):
  d.ellipse([X-26,Y-26,X+26,Y+26],fill=DK);d.ellipse([X-17,Y-17,X+17,Y+17],fill=WH);d.ellipse([X-11,Y-11,X+11,Y+11],fill=(226,108,30))
def ball(X,Y,dirs):
  R_=60;d.ellipse([X-R_,Y-R_,X+R_,Y+R_],fill=DK)
  for ux,uy in dirs: d.line([(X+ux*40,Y+uy*40),(X+ux*(R_+6),Y+uy*(R_+6))],fill=WH,width=9)
  d.ellipse([X-44,Y-44,X+44,Y+44],fill=WH);d.ellipse([X-37,Y-37,X+37,Y+37],fill=(206,206,212))
  d.rectangle([X-37,Y-4,X+37,Y+4],fill=(160,160,168))
  d.ellipse([X-19,Y-19,X+19,Y+19],fill=(160,160,168));d.ellipse([X-12,Y-12,X+12,Y+12],fill=WH);d.ellipse([X-7,Y-7,X+7,Y+7],fill=(206,206,212))
for k,(X,Y) in spot.items():
  if k[0]=='P': place(X,Y)
for k,(X,Y) in spot.items():
  if k[0]=='T' and k!='T:Mauville City': stop(X,Y,24 if (k.endswith('City') or k.endswith('League')) else 18)
mX,mY=spot['T:Mauville City'];dirs=[]
for p0,p1 in seg:
  for a_,b_ in ((p0,p1),(p1,p0)):
    if a_==(mX,mY): l=math.hypot(b_[0]-mX,b_[1]-mY);dirs.append(((b_[0]-mX)/l,(b_[1]-mY)/l))
ball(mX,mY,dirs)
im.save('hoenn_xy_map.png');print(im.size)
