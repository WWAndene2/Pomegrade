import os
HERE=os.path.dirname(os.path.abspath(__file__))
exec(open(os.path.join(HERE,'hoenn_assemble.py')).read().split("im=Image.new")[0])
from PIL import ImageFilter,ImageFont
import re
raw=Image.open('hoenn_assembled.png').convert('RGB')
S=4  # tiles per output px... output scale
W,H=(maxx-minx),(maxz-minz)
# land mask: tiles that are not sea-blue/background
sm=raw.resize((W//2,H//2))
mask=Image.new('L',sm.size,0);mp=mask.load();px=sm.load()
for y in range(sm.size[1]):
  for x in range(sm.size[0]):
    r,g,b=px[x,y]
    if (r,g,b)!=(16,32,70) and not (b>r+20 and b>g+10): mp[x,y]=255
K=3
mask=mask.resize((W*K,H*K)).filter(ImageFilter.GaussianBlur(24)).point(lambda v:255 if v>120 else 0)
SEAc=(169,214,240);LAND=(236,240,226);EDGE=(255,255,255)
im=Image.new("RGB",(W*K+400,H*K),SEAc)
im.paste((255,255,255),(0,0),mask.filter(ImageFilter.MaxFilter(9)))
im.paste(LAND,(0,0),mask)
d=ImageDraw.Draw(im)
T=lambda x,z:((x-minx)*K,(z-minz)*K)
# hubs and crossings
SEAR={105,106,107,108,109,122,124,125,126,127,128,129,130,131,132,133,134}
def issea(n): m=re.match(r'Route (\d+)$',n); return bool(m and int(m.group(1)) in SEAR)
drawn=set(ms)
hub={};cross=[]
for z in Z:
  m=M(z);n=nm(z)
  if m not in drawn or not out(z) or not Z[z][1]: continue
  ws=Z[z][1];pts=[(pos[m][0]+w[4]/18,pos[m][1]+w[6]/18) for w in ws]
  hub.setdefault(n,[]).extend(pts)
  for w in ws:
    k=w[2]&0xffff;dz=w[0]&0xffff
    if k in step and dz in Z and out(dz) and nm(dz)!=n: cross.append((n,nm(dz),(pos[m][0]+w[4]/18,pos[m][1]+w[6]/18)))
H_={n:(sum(p[0] for p in v)/len(v),sum(p[1] for p in v)/len(v)) for n,v in hub.items()}
for m,(x,z,t) in AP.items():
  w,h,_=mat(m);H_[t.split(' (')[0]]=(x+w*20,z+h*20)
def oct(a,b):  # octilinear: diagonal then straight
  dx,dz=b[0]-a[0],b[1]-a[1];k=min(abs(dx),abs(dz))
  mid=(a[0]+k*(1 if dx>0 else -1),a[1]+k*(1 if dz>0 else -1))
  return [a,mid,b]
LW=16
R=lambda *k:['Route %d'%x if isinstance(x,int) else x for x in k]
LINES=[((228,52,64),0,R('Littleroot Town',101,'Oldale Town',102,'Petalburg City',104,'Rustboro City',116,'Verdanturf Town',117,'Mauville City',118,119,'Fortree City',120,121,'Lilycove City')),
((240,150,30),0,R('Oldale Town',103,110,'Slateport City')),
((240,150,30),0,R('Slateport City',110,'Mauville City',111,113,'Fallarbor Town',114)),
((60,160,80),0,R('Rustboro City',115)),((60,160,80),0,R(111,112,'Lavaridge Town')),((60,160,80),0,R(118,123,121)),
((40,120,200),1,R('Petalburg City',104,105,106,'Dewford Town',107,108,109,'Slateport City',134,133,132,'Pacifidlog Town',131,130,129,128,'Ever Grande City','Pokemon League')),
((40,120,200),1,R(121,122,123)),((40,120,200),1,R('Lilycove City',124,'Mossdeep City',125)),((40,120,200),1,R(124,126,'Sootopolis City')),((40,120,200),1,R(126,127,128)),((40,120,200),1,R('Mossdeep City',127))]
miss=set()
for col,sea,seq in LINES:
  pts=[H_[n] for n in seq if n in H_];miss|={n for n in seq if n not in H_}
  path=[]
  for i in range(len(pts)-1): path+= [T(*q) for q in oct(pts[i],pts[i+1])]
  if sea:
    for i in range(len(path)-1):
      (x0,y0),(x1,y1)=path[i],path[i+1];L=max(1,int(((x1-x0)**2+(y1-y0)**2)**.5/24))
      for j in range(L+1): X=x0+(x1-x0)*j/L;Y=y0+(y1-y0)*j/L;d.ellipse([X-6,Y-6,X+6,Y+6],fill=col)
  else: d.line(path,fill=col,width=LW,joint='curve')
print('missing',miss)
try: F=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',30);f2=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',20)
except: F=f2=ImageFont.load_default()
for n,(x,z) in H_.items():
  X,Y=T(x,z)
  if re.match(r'Route (\d+)',n):
    r=16;d.ellipse([X-r,Y-r,X+r,Y+r],fill=(255,255,255),outline=(60,60,60),width=3);d.text((X,Y),n.split()[1],fill=(40,40,40),font=f2,anchor='mm')
  else:
    r=26;d.rounded_rectangle([X-r,Y-r,X+r,Y+r],radius=10,fill=(255,255,255),outline=(30,30,30),width=6)
    d.text((X+r+8,Y),n,fill=(20,20,20),font=F,anchor='lm',stroke_width=4,stroke_fill=(255,255,255))
im.save('hoenn_style2.png');print(im.size,len(H_))
