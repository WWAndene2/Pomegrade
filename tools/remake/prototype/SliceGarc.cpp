// scratch: a/0/3/9 with pieces 5 and 6 replaced by the slice's
#include "Garc.h"
#include "NitroCompression.h"
#include <cstdio>
using namespace remake;
int main(){ const Bytes in=ReadFile("oras/a039.garc"); Garc g(in); const Garc orig(in);
 for(auto [i,f]: {std::pair<size_t,const char*>{5,"slice/gr5_route201.bin"},{6,"slice/gr6_twinleaf3.bin"}}){ const bool lz=IsLzCompressed(orig.Sub(i)); const Bytes d=ReadFile(f); g.Set(i, lz?Lz11Compress(d):d);
  Bytes back=Garc(g.Write()).Sub(i); if(IsLzCompressed(back)) back=LzDecompress(back); printf("piece %zu (LZ %d): %zu bytes, reads back identical %s\n",i,lz,d.size(),back==d?"yes":"NO"); }
 const Bytes out=g.Write(); const Garc b(out); int same=0; for(size_t i=0;i<b.Count();i++) if(i!=5&&i!=6) same+=b.Sub(i)==orig.Sub(i);
 printf("other pieces unchanged %d of %zu; archive %zu -> %zu\n",same,b.Count()-2,in.size(),out.size()); WriteFile("slice/a039_slice.garc",out); }
