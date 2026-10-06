#undef NDEBUG // the checks below are asserts
#include "GPU3D_TextureReplacement.h"
#include "Platform.h"
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#include <cstdio>
#include <cstdarg>
#include <cassert>
#include <filesystem>
namespace melonDS::Platform { void Log(LogLevel, const char* f, ...) { va_list a; va_start(a,f); vprintf(f,a); va_end(a);} }
using namespace melonDS;
int main(){
  namespace fs=std::filesystem; fs::remove_all("root");
  u32 w=16,h=8; std::vector<u32> tex(w*h);
  for(u32 i=0;i<w*h;i++) tex[i]= (i%3==0)?0:((i&63)|((63-(i&63))<<8)|(10<<16)|(31u<<24));
  TextureReplacement r;
  assert(!r.Active());
  TextureReplacement::SetConfig({"root",true,true});
  assert(!r.Active()); // no game code
  TextureReplacement::SetGameCode("../x"); assert(!r.Active()); // unsafe
  TextureReplacement::SetGameCode("ABCE"); assert(r.Active());
  u64 hsh=TextureReplacement::HashDecoded(tex.data(),w,h);
  r.Dump(hsh,tex.data(),w,h);
  std::string name=TextureReplacement::TextureName(hsh,w,h);
  std::string dp="root/ABCE/dump/"+name+".png"; assert(fs::exists(dp));
  int x,y,c; u8* p=stbi_load(dp.c_str(),&x,&y,&c,4); assert(x==16&&y==8);
  // make 4x replacement in a subfolder
  std::vector<u8> hd(64*32*4);
  for(int yy=0;yy<32;yy++)for(int xx=0;xx<64;xx++)for(int k=0;k<4;k++) hd[(yy*64+xx)*4+k]=p[((yy/4)*16+xx/4)*4+k];
  // translucent edge pixel
  hd[3]=100;
  fs::create_directories("root/ABCE/pack");
  stbi_write_png(("root/ABCE/pack/"+name+".png").c_str(),64,32,4,hd.data(),256);
  // bad one: wrong ratio
  std::vector<u32> t2(w*h,0x1F000001); u64 h2=TextureReplacement::HashDecoded(t2.data(),w,h);
  std::vector<u8> bad(48*8*4,255); stbi_write_png(("root/ABCE/"+TextureReplacement::TextureName(h2,w,h)+".png").c_str(),48,8,4,bad.data(),48*4);
  std::vector<u32> out; u32 ow,oh;
  assert(!r.Lookup(hsh,w,h,true,4096,out,ow,oh)); // index not refreshed yet
  TextureReplacement::SetGameCode("ABCE"); r.Active();
  assert(r.Lookup(hsh,w,h,true,4096,out,ow,oh)); assert(ow==64&&oh==32);
  printf("texel0 rgba8=%08x\n",out[0]); assert(out[0]==0); // alpha 100 -> transparent in binary mode
  TextureReplacement::ConvertToRGB6A5(out);
  // round trip: HD texel equals original
  int mism=0; for(u32 yy=0;yy<32;yy++)for(u32 xx=0;xx<64;xx++){ if(yy==0&&xx==0) continue; if(out[yy*64+xx]!=tex[(yy/4)*16+xx/4]) mism++; }
  printf("mismatches %d, texel0 binary=%08x\n",mism,out[0]);
  assert(mism==0); assert(out[0]==0); // alpha 100 -> transparent in binary mode
  assert(r.Lookup(hsh,w,h,false,4096,out,ow,oh)); printf("texel0 smooth=%08x\n",out[0]); assert((out[0]>>24)==100);
  TextureReplacement::ConvertToRGB6A5(out); assert((out[0]>>24)==12);
  assert(r.Lookup(hsh,w,h,true,4096,out,ow,oh)); // from memory cache
  assert(!r.Lookup(h2,w,h,true,4096,out,ow,oh)); // rejected
  // background loading: the first lookup queues the file and says pending,
  // the next frame (Active) takes the decoded texture in, Loaded() changes
  // and the same texels come back; a rejected file is not pending again
  {
    // the synchronous result to compare with, before the (shared) settings change
    std::vector<u32> sync; u32 sw,sh; assert(r.Lookup(hsh,w,h,true,4096,sync,sw,sh));
    TextureReplacement b;
    TextureReplacement::SetConfig({"root",true,false,true}); assert(b.Active());
    bool pending=false; u32 loaded=b.Loaded();
    assert(!b.Lookup(hsh,w,h,true,4096,out,ow,oh,&pending)); assert(pending);
    assert(!b.Lookup(hsh,w,h,true,4096,out,ow,oh,&pending)); assert(pending); // queued once
    assert(!b.Lookup(h2,w,h,true,4096,out,ow,oh,&pending)); assert(pending);
    b.WaitForLoads(); b.Active(); assert(b.Loaded()!=loaded);
    assert(b.Lookup(hsh,w,h,true,4096,out,ow,oh,&pending)); assert(!pending); assert(out==sync&&ow==sw&&oh==sh);
    assert(!b.Lookup(h2,w,h,true,4096,out,ow,oh,&pending)); assert(!pending); // rejected
    // a load still queued when the settings change is dropped
    TextureReplacement::SetConfig({"root",true,false,true}); b.Active();
    assert(!b.Lookup(hsh,w,h,false,4096,out,ow,oh,&pending)); assert(pending);
    TextureReplacement::SetConfig({"root",true,false,true}); b.Active();
    b.WaitForLoads(); b.Active();
    assert(!b.Lookup(hsh,w,h,false,4096,out,ow,oh,&pending)); assert(pending); // queued again for the new settings
    b.WaitForLoads(); b.Active();
    assert(b.Lookup(hsh,w,h,false,4096,out,ow,oh,&pending)); assert(!pending);
    puts("background OK");
  }
  puts("ALL OK");
}
