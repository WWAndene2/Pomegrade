import os
HERE=os.path.dirname(os.path.abspath(__file__))
exec(open(os.path.join(HERE,'hoenn_place.py')).read().split("print('--- residuals")[0].replace("for c in conf: print","for c in []: print").replace("for c in comps: print","for c in []: print"))
import pickle
names_out={nm(z) for z in Z if out(z)}
hits={}
for z in Z:
  if not out(z) or M(z) not in pos: continue
  for w in Z[z][1]:
    dz=w[0]&0xffff
    if dz in Z and not out(dz):
      n=nm(dz)
      if n and n not in names_out: hits.setdefault(n,[]).append((pos[M(z)][0]+w[4]/18,pos[M(z)][1]+w[6]/18,nm(z)))
for n,v in sorted(hits.items()):
  print('%-28s %3d  from %s'%(n,len(v),sorted(set(x[2] for x in v))[:4]))
pickle.dump(hits,open('map/landmarks.pkl','wb'))
print('--- two steps')
two={}
for z in Z:
  if not out(z) or M(z) not in pos: continue
  for w in Z[z][1]:
    mz=w[0]&0xffff
    if mz not in Z or out(mz): continue
    for w2 in Z[mz][1]:
      dz=w2[0]&0xffff
      if dz in Z and not out(dz):
        n=nm(dz)
        if n and n not in names_out and n not in hits: two.setdefault(n,[]).append((pos[M(z)][0]+w[4]/18,pos[M(z)][1]+w[6]/18,nm(z)+' via '+nm(mz)))
for n,v in sorted(two.items()): print('%-28s %3d  %s'%(n,len(v),sorted(set(x[2] for x in v))[:3]))
pickle.dump(two,open('map/landmarks2.pkl','wb'))
