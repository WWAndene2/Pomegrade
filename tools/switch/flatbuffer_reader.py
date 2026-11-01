# Minimal FlatBuffers table reader (no schema), used by trpfs_extract.py.
import struct
class T:
    def __init__(s,b,pos): s.b=b; s.pos=pos; s.vt=pos-struct.unpack_from('<i',b,pos)[0]; s.vn=struct.unpack_from('<H',b,s.vt)[0]
    def off(s,i):
        o=4+2*i
        return struct.unpack_from('<H',s.b,s.vt+o)[0] if o<s.vn else 0
    def scalar(s,i,fmt='<Q',d=0):
        o=s.off(i); return struct.unpack_from(fmt,s.b,s.pos+o)[0] if o else d
    def ref(s,i):
        o=s.off(i)
        if not o: return None
        p=s.pos+o; return p+struct.unpack_from('<I',s.b,p)[0]
    def vec(s,i):
        p=s.ref(i)
        if p is None: return (0,0)
        return (struct.unpack_from('<I',s.b,p)[0],p+4)
    def scalars(s,i,fmt='Q'):
        n,p=s.vec(i); return list(struct.unpack_from('<%d%s'%(n,fmt),s.b,p))
    def tables(s,i):
        n,p=s.vec(i); return [T(s.b,p+4*k+struct.unpack_from('<I',s.b,p+4*k)[0]) for k in range(n)]
    def strings(s,i):
        n,p=s.vec(i); r=[]
        for k in range(n):
            q=p+4*k+struct.unpack_from('<I',s.b,p+4*k)[0]; l=struct.unpack_from('<I',s.b,q)[0]; r.append(s.b[q+4:q+4+l].decode())
        return r
    def bytes_(s,i):
        n,p=s.vec(i); return s.b[p:p+n]
def root(b,base=0): return T(b,base+struct.unpack_from('<I',b,base)[0])
