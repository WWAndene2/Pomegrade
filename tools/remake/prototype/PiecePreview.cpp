// scratch: several GR pieces' terrain in one glTF, each at an offset (x z), textured from an area pack
#include "Bch.h"
#include "BinLinker.h"
#include "Gltf.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
using namespace remake;
int main(int c,char**v){ std::vector<BchTexture> tex; auto ad=BinLinker::Read(ReadFile(v[1]),"AD"); for(auto&f:ad.Files) if(Bch::Is(f)) for(auto&t:Bch::Read(f).Textures) tex.push_back(t);
 std::vector<GltfPart> p; std::vector<GltfMaterial> mt;
 for(int a=3;a+2<c;a+=3){ auto gr=BinLinker::Read(ReadFile(v[a]),"GR"); Bch b=Bch::Read(gr.Files[1]);
  // shadow decals (their texture named *shadow*) left out: the preview can't blend them as the console does
  { auto& m=b.Models[0]; std::vector<BchMesh> keep; for(auto& me:m.Meshes){ const std::string& t=m.Materials[me.Material].Texture[0]; const std::string& t1=m.Materials[me.Material].Texture[1];
    if(t.find("shadow")==std::string::npos && t1.find("shadow")==std::string::npos) keep.push_back(me); } m.Meshes=keep; }
  const float o[3]={(float)atof(v[a+1]),0,(float)atof(v[a+2])}; AppendBchModel(b.Models[0],tex,o,p,mt); }
 std::ofstream(v[2])<<WriteGltf(p,mt); printf("%zu parts\n",p.size()); }
