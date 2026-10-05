// scratch prototype: Littleroot's (zone 6) three warps moved onto Twinleaf's doors; a/0/1/3 rebuilt
// warp entry (0x18): u16 x at +0x0C and y at +0x10 in model units of the zone's matrix ((tile + 0.5) * 18)
#include "Garc.h"
#include "BinLinker.h"
#include "NitroCompression.h"
#include <cstdio>
using namespace remake;
static void Put16(Bytes& b, size_t at, uint16_t v) { b.at(at) = (uint8_t)v; b.at(at + 1) = (uint8_t)(v >> 8); }
int main(){
 const Bytes in=ReadFile("oras/a013.garc"); Garc g(in); Bytes raw=g.Sub(6); const bool lz=IsLzCompressed(raw); Bytes z=lz?LzDecompress(raw):raw;
 BinLinker zo=BinLinker::Read(z,"ZO"); printf("zone 6 LZ %d, rewritten identical %s\n",lz, zo.Write()==z?"yes":"NO");
 Bytes& e=zo.Files[1]; const int nf=e[4], nn=e[5], nw=e[6]; const size_t w0=8+nf*0x14+nn*0x30;
 // Littleroot is matrix 1's cell (2, 4): its tiles start at (80, 160)
 const int doors[3][2]={{13,19},{24,19},{14,29}}; // Twinleaf: top-left, top-right, bottom-left houses
 for(int k=0;k<nw&&k<3;k++){ const size_t at=w0+k*0x18; const int x=(80+doors[k][0])*18+9, y=(160+doors[k][1])*18+9;
  printf("warp %d to zone %u: (%u, %u) -> (%d, %d)\n",k,U16(e,at+4),U16(e,at+0xC),U16(e,at+0x10),x,y); Put16(e,at+0xC,(uint16_t)x); Put16(e,at+0x10,(uint16_t)y); }
 // the zone's area pack (header u16 at +2): Petalburg's (9), whose textures the rebuilt Twinleaf names
 printf("zone 6 area %u -> 9 (matrix %u)\n",U16(zo.Files[0],2),U16(zo.Files[0],4)); Put16(zo.Files[0],2,9);
 const Bytes nz=zo.Write(); g.Set(6, lz?Lz11Compress(nz):nz); const Bytes out=g.Write();
 const Garc back(out); int same=0; for(size_t i=0;i<back.Count();i++) if(i!=6) same+= back.Sub(i)==Garc(in).Sub(i);
 printf("other zones unchanged: %d of %zu; archive %zu -> %zu bytes\n",same,back.Count()-1,in.size(),out.size()); WriteFile("slice/a013_twinleaf.garc",out); }
