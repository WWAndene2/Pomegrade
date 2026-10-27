# (owner, 28 October: Kalos redrawn so it matches hoenn_xy_map.png; run from a work directory holding xtm/png/, X's a/0/6/9
# member 0 decoded by darc_png; writes kalos_xy_map.png)
# Kalos redrawn in the Hoenn map's style (hoenn_xy_draw.py): X's own town map image (a/0/6/9, townmap_bg1) read for the
# land, the relief (its colours, greener low, browner high), its stops, places, Lumiose and the routes joining them
import numpy as np,math
from collections import deque
from PIL import Image,ImageDraw,ImageFilter as IF
src=np.array(Image.open('xtm/png/._timg_townmap_bg1.bclim.png').convert('RGB')).astype(float)
r,g,b=src[...,0],src[...,1],src[...,2];Hs,Ws=src.shape[:2]
VX0,VX1,VY0=0,306,52   # the drawn part: right of 306 and above 52 the image's padding (its edge pixels stretched)
valid=np.zeros((Hs,Ws),bool);valid[VY0:,VX0:VX1]=True
sea=(b>r+35)&(b>g+5)
grey=(abs(r-g)<12)&(abs(g-b)<14)&(r>185)
orange=(r>170)&(g<140)&(b<80)
def blobs(m,minn):
  seen=np.zeros_like(m);out=[]
  for y,x in zip(*np.nonzero(m)):
    if seen[y,x]: continue
    q=deque([(y,x)]);seen[y,x]=1;pts=[]
    while q:
      a_,c_=q.popleft();pts.append((a_,c_))
      for dy,dx in((1,0),(-1,0),(0,1),(0,-1)):
        A,B=a_+dy,c_+dx
        if 0<=A<Hs and 0<=B<Ws and m[A,B] and not seen[A,B]: seen[A,B]=1;q.append((A,B))
    if len(pts)>=minn: out.append((len(pts),sum(p[1] for p in pts)/len(pts),sum(p[0] for p in pts)/len(pts)))
  return out
# the icons and routes read off X's image square by square (xgrid.png, its pixels): stops S, places P, Lumiose B
IC={'B':(165.5,177),'A':(71,97),'Bs':(55.5,135),'C':(29,155),'D':(48,180.5),'E':(110,192),'F':(130.5,134.5),'G':(196,136),'H':(212,99),
 'I':(212,75),'J':(239,62),'K':(235,163),'L':(271,138.5),'M':(298,159.5),'N':(280.5,175),'O':(219,192),'Pp':(164,232),
 'd1':(116,97),'d2':(70.5,135),'d3':(39.5,172),'d4':(94,162),'d5':(122,170.5),'d6':(212,121),'d7':(222,129.5),'d8':(234,140.5),
 'd9':(258,97.5),'d10':(307.5,177),'d11':(190.5,201.5),'d12':(232,207),'d13':(80,217.5),'d14':(165,244)}
kind=lambda n:'B' if n=='B' else 'P' if n.startswith('d') else 'S'
LANDE=[('A','d1'),('A','Bs'),('Bs','d2'),('d2','F'),('F','d4'),('F','B'),('Bs','C'),('C','d3'),('d3','D'),('D','E'),('E','B'),('B','G'),('G','d6'),
 ('d6','H'),('H','I'),('G','d8'),('d8','K'),('d8','L'),('L','d9'),('L','M'),('M','N'),('N','O'),('O','B'),('O','Pp'),('B','Pp'),('Pp','d14'),('O','d11')]
SEAE=[('E','d13')]
# land: X's land, cut at the padding; the cut edges carried on and rounded off as the Hoenn map's
land=(~sea)&valid
# X's image is cut at its top (y 52), right (x 306) and bottom: the land there carried on to a coast drawn past the cut,
# irregular (smoothed noise) so it reads as a coast, not a box (invented: X's image does not show it)
PAD=30
HH,WW=Hs+2*PAD,VX1+2*PAD
L2=np.zeros((HH,WW),bool);L2[PAD:PAD+Hs,PAD:PAD+VX1]=land[:,:VX1]
rng=np.random.default_rng(3)
def coast(n,depth,amp):
  v=np.cumsum(rng.normal(0,1,n));v=np.convolve(v,np.ones(15)/15,'same');v=(v-v.min())/(np.ptp(v)+1e-9)
  return (depth+amp*(v-0.5)).astype(int)
top=coast(WW,14,14);right=coast(HH,18,16);bot=coast(WW,14,12)
for x in range(PAD,PAD+VX1):
  if land[VY0+1,x-PAD]:
    for k in range(1,max(1,top[x])): L2[PAD+VY0-k,x]=True
  if land[Hs-1,x-PAD]:
    for k in range(1,max(1,bot[x])): L2[min(HH-1,PAD+Hs-1+k),x]=True
for y in range(PAD,PAD+Hs):
  if land[y-PAD,VX1-1]:
    for k in range(1,max(1,right[y])): L2[y,min(WW-1,PAD+VX1-1+k)]=True
# the corners joined, the new coast smoothed
sm=np.array(Image.fromarray((L2*255).astype(np.uint8)).filter(IF.GaussianBlur(4)))>110
zone=np.zeros_like(L2);zone[:PAD+VY0+6,:]=True;zone[PAD+Hs-6:,:]=True;zone[:,PAD+VX1-6:]=True
L2=np.where(zone,sm,L2)
# relief: X's colours, browner = higher; the icons and lines filled from around them
hraw=np.zeros((Hs+2*PAD,Ws+2*PAD));hraw[PAD:PAD+Hs,PAD:PAD+Ws]=np.where(valid,(r-g)*1.0+(200-b)*0.3,0)
white=(r>200)&(g>200)&(b>200)
mask_ok=np.zeros_like(L2);mo=(~grey)&(~orange)&(~white)&(~sea)&valid&((r+g+b)>120)
mo2=np.zeros((Hs+2*PAD,Ws+2*PAD),bool);mo2[PAD:PAD+Hs,PAD:PAD+Ws]=mo;mo2=mo2[:,:WW];hraw=hraw[:,:WW]
def gblur(a,sig):
  rr=int(3*sig)+1;k=np.exp(-np.arange(-rr,rr+1)**2/(2*sig*sig));k/=k.sum()
  p=np.pad(a,((0,0),(rr,rr)),mode='edge');a=sum(k[i]*p[:,i:i+a.shape[1]] for i in range(len(k)))
  p=np.pad(a,((rr,rr),(0,0)),mode='edge');return sum(k[i]*p[i:i+a.shape[0],:] for i in range(len(k)))
Ev=np.where(mo2,hraw,0.0);Wt=mo2.astype(float)
for sig in (1,2,4,8,16):
  a_=gblur(Ev*(Wt>0),sig);b_=gblur((Wt>0).astype(float),sig);fill=(Wt==0)&(b_>1e-3);Ev[fill]=a_[fill]/b_[fill];Wt[fill]=1
Ev=gblur(Ev,1.0)
# ---- drawn in the Hoenn map's style, at its scale (Kalos about as wide on the page as Hoenn)
SCL=6;WB,HB=WW*SCL,HH*SCL
mkB=Image.fromarray((L2*255).astype(np.uint8)).resize((WB,HB),Image.BILINEAR).filter(IF.GaussianBlur(4)).point(lambda v:255 if v>128 else 0)
MB=np.array(mkB)>0
e8=((Ev-Ev.min())/(Ev.max()-Ev.min()+1e-9)*255).astype(np.uint8)
hB=np.array(Image.fromarray(e8).resize((WB,HB),Image.BICUBIC)).astype(float)
qs=np.percentile(hB[MB],[30,55,72,85,94]);lev=np.digitize(hB,qs)
lev=np.array(Image.fromarray((lev*40).astype(np.uint8)).filter(IF.MedianFilter(9)))//40
FLAT=[(120,190,120),(150,200,110),(176,196,104),(168,160,92),(150,122,74),(132,96,64)]
a=np.zeros((HB,WB,3),np.uint8);a[:]=(52,140,206)
a[np.array(mkB.filter(IF.MaxFilter(41)))>0]=(92,172,226)
for i,c in enumerate(FLAT): a[MB&(lev==i)]=c
im=Image.fromarray(a);d=ImageDraw.Draw(im,'RGBA')
for gx_ in range(0,WB,256): d.line([(gx_,0),(gx_,HB)],fill=(40,110,170),width=3)
for gy_ in range(0,HB,256): d.line([(0,gy_),(WB,gy_)],fill=(40,110,170),width=3)
a=np.array(im);a[MB]=np.array(Image.fromarray(np.where(MB[...,None],a,0).astype(np.uint8)))[MB]
for i,c in enumerate(FLAT): a[MB&(lev==i)]=c
edge=np.zeros((HB,WB),bool);edge[1:,:]|=lev[1:,:]!=lev[:-1,:];edge[:,1:]|=lev[:,1:]!=lev[:,:-1];edge&=MB
edge=np.array(Image.fromarray((edge*255).astype(np.uint8)).filter(IF.MaxFilter(3)))>0;a[edge]=(a[edge]*0.72).astype(np.uint8)
a[(np.array(mkB.filter(IF.MaxFilter(7)))>0)&~MB]=(28,70,96)
im=Image.fromarray(a);d=ImageDraw.Draw(im,'RGBA')
P=lambda p:((p[0]+PAD)*SCL,(p[1]+PAD)*SCL)
SP={n:P(p) for n,p in IC.items()}
DK=(46,54,56);WH=(248,248,248)
def octi(p0,p1):
  dx,dy=p1[0]-p0[0],p1[1]-p0[1];k=min(abs(dx),abs(dy));m=(p1[0]-math.copysign(k,dx),p1[1]-math.copysign(k,dy))
  return [p0,m,p1] if math.hypot(m[0]-p0[0],m[1]-p0[1])>3 and math.hypot(m[0]-p1[0],m[1]-p1[1])>3 else [p0,p1]
land_q=[octi(SP[a_],SP[b_]) for a_,b_ in LANDE];sea_q=[[SP[a_],SP[b_]] for a_,b_ in SEAE]
for q in land_q:
  d.line(q,fill=DK,width=22)
  for X,Y in q: d.ellipse([X-11,Y-11,X+11,Y+11],fill=DK)
for q in land_q:
  d.line(q,fill=WH,width=9)
  for X,Y in q: d.ellipse([X-4.5,Y-4.5,X+4.5,Y+4.5],fill=WH)
for q in sea_q:
  for p0,p1 in zip(q,q[1:]):
    Lg=math.hypot(p1[0]-p0[0],p1[1]-p0[1]);k=max(1,round(Lg/30))
    for t in range(1,k): X=p0[0]+(p1[0]-p0[0])*t/k;Y=p0[1]+(p1[1]-p0[1])*t/k;d.ellipse([X-6,Y-6,X+6,Y+6],fill=WH)
def stop(X,Y,r_):
  d.ellipse([X-r_-14,Y-r_-14,X+r_+14,Y+r_+14],fill=DK);d.ellipse([X-r_-5,Y-r_-5,X+r_+5,Y+r_+5],fill=WH);d.ellipse([X-r_,Y-r_,X+r_,Y+r_],fill=(196,198,204))
def place(X,Y):
  d.ellipse([X-26,Y-26,X+26,Y+26],fill=DK);d.ellipse([X-17,Y-17,X+17,Y+17],fill=WH);d.ellipse([X-11,Y-11,X+11,Y+11],fill=(226,108,30))
def ball(X,Y,dirs):
  R_=60;d.ellipse([X-R_,Y-R_,X+R_,Y+R_],fill=DK)
  for ux,uy in dirs: d.line([(X+ux*40,Y+uy*40),(X+ux*(R_+6),Y+uy*(R_+6))],fill=WH,width=9)
  d.ellipse([X-44,Y-44,X+44,Y+44],fill=WH);d.ellipse([X-37,Y-37,X+37,Y+37],fill=(206,206,212))
  d.rectangle([X-37,Y-4,X+37,Y+4],fill=(160,160,168))
  d.ellipse([X-19,Y-19,X+19,Y+19],fill=(160,160,168));d.ellipse([X-12,Y-12,X+12,Y+12],fill=WH);d.ellipse([X-7,Y-7,X+7,Y+7],fill=(206,206,212))
for n,xy in SP.items():
  if kind(n)=='P': place(*xy)
for n,xy in SP.items():
  if kind(n)=='S': stop(*xy,21)
bx=SP['B'];dirs=[]
for q in land_q:
  for p0,p1 in ((q[0],q[1]),(q[-1],q[-2])):
    if p0==bx: l=math.hypot(p1[0]-p0[0],p1[1]-p0[1]);dirs.append(((p1[0]-p0[0])/l,(p1[1]-p0[1])/l))
ball(*bx,dirs)
im.save('kalos_xy_map.png');print(im.size)
