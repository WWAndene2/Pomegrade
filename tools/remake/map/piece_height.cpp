// A GR piece's terrain height for hoenn_height.py: piece_height <piece> <out.f32> [...]; 160 x 160 floats (4 a tile), the
// highest point of its ground meshes (TopView's kinds, effects, shadows, blades and water left out), NaN where none
// scratch: a GR piece's terrain height, 40x40 tiles at 4 px each (160x160 floats, max Y of the ground meshes; NaN none)
#include "Bch.h"
#include "BinLinker.h"
#include "NitroCompression.h"
#include "TopView.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
using namespace remake;
int main(int argc,char**argv){
  for(int a=1;a+1<argc;a+=2){
    std::ifstream f(argv[a],std::ios::binary);Bytes d((std::istreambuf_iterator<char>(f)),{});
    try{
      if(IsLzCompressed(d)) d=LzDecompress(d);
      const BinLinker piece=BinLinker::Read(d,"GR");const Bch bch=Bch::Read(piece.Files.at(1));
      const int N=160;std::vector<float> h(N*N,NAN);
      for(const BchModel& m:bch.Models) for(const BchMesh& me:m.Meshes){
        if(me.Material>=m.Materials.size()) continue;
        TopViewKind k=MeshKind(m,me);
        if(k==TopViewKind::Effect||k==TopViewKind::Shadow||k==TopViewKind::Blades||k==TopViewKind::Water) continue;
        for(size_t t=0;t+2<me.Triangles.size();t+=3){
          float px[3],pz[3],py[3];
          for(int i=0;i<3;i++){const BchVertex& v=me.Vertices[me.Triangles[t+i]];px[i]=(v.Position[0]/18+20)*4;pz[i]=(v.Position[2]/18+20)*4;py[i]=v.Position[1];}
          float ar=(px[1]-px[0])*(pz[2]-pz[0])-(pz[1]-pz[0])*(px[2]-px[0]);if(std::fabs(ar)<1e-6) continue;
          int x0=std::max(0,(int)std::floor(std::min({px[0],px[1],px[2]}))),x1=std::min(N-1,(int)std::ceil(std::max({px[0],px[1],px[2]})));
          int y0=std::max(0,(int)std::floor(std::min({pz[0],pz[1],pz[2]}))),y1=std::min(N-1,(int)std::ceil(std::max({pz[0],pz[1],pz[2]})));
          for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
            float X=x+.5f,Y=y+.5f;
            float w0=((px[1]-X)*(pz[2]-Y)-(pz[1]-Y)*(px[2]-X))/ar,w1=((px[2]-X)*(pz[0]-Y)-(pz[2]-Y)*(px[0]-X))/ar,w2=1-w0-w1;
            if(w0<0||w1<0||w2<0) continue;
            float hy=w0*py[0]+w1*py[1]+w2*py[2];float& c=h[y*N+x];if(std::isnan(c)||hy>c) c=hy;}
        }}
      std::ofstream o(argv[a+1],std::ios::binary);o.write((char*)h.data(),h.size()*4);
    }catch(...){}
  }
}
