#!/bin/sh
# Builds ooz (github.com/powzix/ooz, GPL-3.0: an independent Oodle Kraken decompressor) as libooz.so for pack.py.
# ooz targets Windows: its stdafx.h is replaced by stdafx_portable.h (GCC built-ins for the MSVC intrinsics), only the
# decompressor is kept (kraken.cpp up to Kraken_Decompress; its test harness loads Oodle's DLL) and a C entry point
# ooz_decompress is added. Owner's approval to build and run ooz: 3 November.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
work=${1:-"$here/build"}
commit=0503806
mkdir -p "$work"
[ -d "$work/ooz" ] || git clone -q https://github.com/powzix/ooz.git "$work/ooz"
git -C "$work/ooz" checkout -q "$commit"
src="$work/src"; mkdir -p "$src"
cp "$here/stdafx_portable.h" "$src/stdafx.h"
cp "$work/ooz/bitknit.cpp" "$work/ooz/lzna.cpp" "$src/"
end=$(awk '/^int Kraken_Decompress/ {s=NR} s && NR>s && /^}/ {print NR; exit}' "$work/ooz/kraken.cpp")
head -n "$end" "$work/ooz/kraken.cpp" > "$src/kraken.cpp"
cat >> "$src/kraken.cpp" <<'C'

extern "C" int ooz_decompress(const byte *src, size_t src_len, byte *dst, size_t dst_len) {
  return Kraken_Decompress(src, src_len, dst, dst_len);
}
C
g++ -O2 -w -msse4.1 -fpermissive -fPIC -shared "$src/kraken.cpp" "$src/bitknit.cpp" "$src/lzna.cpp" -o "$work/libooz.so"
echo "built: $work/libooz.so"
