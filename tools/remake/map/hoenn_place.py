import struct,os,collections,json
names={}
for l in open('map/names.txt',encoding='utf-8',errors='replace'):
  k,_,v=l.rstrip('\n').partition('\t');names[int(k)]=v
def zone(z):
  d=open('wat/oz/%d.unknown'%z,'rb').read();n=struct.unpack_from('<H',d,2)[0];o=struct.unpack_from('<%dI'%(n+1),d,4)
  h=struct.unpack_from('<28H',d,o[0]);e=d[o[1]:o[2]];nf,nn,nw=e[4],e[5],e[6];at=12+nf*0x14+nn*0x30
  return h,[struct.unpack_from('<12h',e,at+k*0x18) for k in range(nw)]
Z={z:zone(z) for z in range(536) if os.path.exists('wat/oz/%d.unknown'%z)}
SKIP={348,349}
step={0x500:(0,1),0x501:(0,-1),0x502:(1,0),0x503:(-1,0)}
M=lambda z:Z[z][0][2]
nm=lambda z:names.get(Z[z][0][14]&0x3ff,'')
import re
first={}
for z in Z:
  n=nm(z)
  if re.match(r'(Route \d+|.* Town|.* City|Pokémon League)$',n): first[n]=min(first.get(n,9999),M(z))
UNDER={29,30,31,413,426}
OUT={z for z in Z if nm(z) in first and M(z) not in UNDER}
out=lambda z:z in OUT
def links(z):  # (dest zone, offset of dest matrix origin relative to z's matrix origin)
  h,ws=Z[z]
  for w in ws:
    k=w[2]&0xffff;dz=w[0]&0xffff
    if k in step and dz in Z and M(dz)!=M(z) and M(dz) not in SKIP and w[1]<len(Z[dz][1]):
      d=Z[dz][1][w[1]];sx,sz=step[k]
      yield dz,(round(w[4]/18+sx-d[4]/18),round(w[6]/18+sz-d[6]/18))
SEA={105,106,107,108,109,122,124,125,126,127,128,129,130,131,132,133,134}
def sea(n):
  m=re.match(r'Route (\d+)$',n);return 1 if m and int(m.group(1)) in SEA else 0
adj=collections.defaultdict(list)
for z in Z:
  if not out(z) or M(z) in SKIP: continue
  for dz,off in links(z):
    if out(dz): adj[M(z)].append((M(dz),off,(0 if {nm(z),nm(dz)} in ({'Slateport City','Route 109'},{'Pacifidlog Town','Route 131'}) else sea(nm(z))+sea(nm(dz))),'%s -> %s'%(names.get(Z[z][0][14]&0x3ff),names.get(Z[dz][0][14]&0x3ff))))
    else:
      # pass-through: an indoor map whose other crossings lead to another outdoor matrix
      for ez,off2 in links(dz):
        if out(ez) and M(ez)!=M(z):
          adj[M(z)].append((M(ez),(off[0]+off2[0],off[1]+off2[1]),10,'%s -> %s -> %s'%(names.get(Z[z][0][14]&0x3ff),names.get(Z[dz][0][14]&0x3ff),names.get(Z[ez][0][14]&0x3ff))))
pos={};comps=[];conf=[]
for start in [1]+sorted(adj):
  if start in pos: continue
  comp=[start];pos[start]=(0,0);q=[(0,start)]
  E=[(e[2],start,e) for e in adj[start]]
  while E:
    E.sort(key=lambda t:t[0]);pr,a,(b,(ox,oz),_,why)=E.pop(0)
    p=(pos[a][0]+ox,pos[a][1]+oz)
    if b not in pos:
      pos[b]=p;comp.append(b);E+=[(e[2],b,e) for e in adj[b]]
  comps.append(comp)
for c in conf: print('conflict m%d -> m%d have %s got %s via %s'%c)
for c in comps: print('group',sorted(c))
json.dump({'pos':{str(k):v for k,v in pos.items()},'groups':comps},open('map/place.json','w'))
print('--- residuals of direct links')
seen=set()
for a in adj:
  for b,(ox,oz),pr,why in adj[a]:
    if pr>=10 or a not in pos or b not in pos or (b,a,why) in seen: continue
    seen.add((a,b,why))
    rx=pos[b][0]-pos[a][0]-ox;rz=pos[b][1]-pos[a][1]-oz
    if abs(rx)>2 or abs(rz)>2: print('m%d -> m%d off by (%d, %d): %s'%(a,b,rx,rz,why))
