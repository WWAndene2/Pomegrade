// scratch: BPS patch from two loose files, checked by our own applier
#include "Bps.h"
#include <cstdio>
using namespace remake;
int main(int c,char**v){ const Bytes a=ReadFile(v[1]), b=ReadFile(v[2]); const Bytes p=BpsCreate(a,b); printf("patch %zu bytes, rebuilds: %s\n", p.size(), BpsApply(a,p)==b?"yes":"NO"); WriteFile(v[3],p); }
