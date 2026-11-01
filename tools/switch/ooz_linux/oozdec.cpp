// local: decompress one Oodle (Kraken/Mermaid/Selkie/Leviathan) block. Usage: oozdec <in> <out> <decompressed size>
#include <cstdio>
#include <cstdlib>
#include <vector>
typedef unsigned char byte;
int Kraken_Decompress(const byte *src, size_t src_len, byte *dst, size_t dst_len);
int main(int c, char **v) {
  FILE *f = fopen(v[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  std::vector<byte> in(n + 64); fread(in.data(), 1, n, f); fclose(f);
  size_t out_size = strtoull(v[3], 0, 10); std::vector<byte> out(out_size + 64);
  int r = Kraken_Decompress(in.data(), n, out.data(), out_size);
  if (r != (int)out_size) { fprintf(stderr, "decompress failed: %d\n", r); return 1; }
  f = fopen(v[2], "wb"); fwrite(out.data(), 1, out_size, f); fclose(f); return 0;
}
