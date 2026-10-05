// scratch: Azahar's own BPS applier (azahar/src/core/file_sys/patch.cpp, unmodified) on our patch,
// with the buffer LayeredFS gives it (the original file)
#include "core/file_sys/patch.h"
#include <cstdio>
#include <fstream>
#include <iterator>
static std::vector<u8> Read(const char* p){ std::ifstream f(p, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }
int main(int c, char** v){
  std::vector<u8> buf = Read(v[1]); const auto patch = Read(v[2]), want = Read(v[3]);
  const auto r = FileSys::Patch::ApplyBpsPatch(patch, buf);
  printf("azahar result %d (0 = success); size %zu want %zu; identical: %s\n", (int)r, buf.size(), want.size(), buf == want ? "yes" : "NO");
  return !(r == Loader::ResultStatus::Success && buf == want); }
