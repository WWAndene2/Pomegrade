// darc_png <file> <offset> <out dir>: every BCLIM of a DARC (at an offset of a file) as PNG
#include "Bclim.h"
#include "Darc.h"
#include "Png.h"
#include "Bytes.h"
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace remake;
int main(int argc,char**argv){
  std::ifstream f(argv[1],std::ios::binary);Bytes d((std::istreambuf_iterator<char>(f)),{});
  size_t off=strtoul(argv[2],0,0);Bytes darc(d.begin()+off,d.end());
  for(auto& x:ReadDarc(darc)){
    try{ClimImage c=ClimImage::Read(x.Data);c.Width=c.StoredWidth;c.Height=c.StoredHeight;Bytes png=EncodePng(c.Width,c.Height,c.Rgba());
      std::string n=x.Path;for(char&ch:n) if(ch=='/') ch='_';
      std::ofstream o(std::string(argv[3])+"/"+n+".png",std::ios::binary);o.write((char*)png.data(),png.size());
      printf("%s %ux%u %s\n",x.Path.c_str(),c.Width,c.Height,ClimFormatName(c.Format));}catch(...){printf("%s (%zu bytes)\n",x.Path.c_str(),x.Data.size());}
  }
}
