// The remake tooling's zone data: the map header table found in an ARM9
// binary, a zone's event lists, an area's texture sets, built here to the
// pokeplatinum decompilation's layouts (checked on the French Platinum: see
// MapHeaders.h, ZoneEvents.h).
#include "AreaData.h"
#include "MapHeaders.h"
#include "ZoneEvents.h"
#include "synthetic_files.h"

#include <cstdio>
#include <string>

using namespace remake;
using namespace synthetic;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

// a 24-byte map header
static Bytes Header(uint8_t area, uint16_t matrix, uint16_t events, uint8_t camera)
{
    Bytes h(24, 0);
    h[0] = area;
    Put16(h, 2, matrix);
    Put16(h, 0x10, events);
    h[0x15] = camera;
    return h;
}

int main()
{
    // an ARM9 image: noise, a run of 40 valid-looking entries (a decoy), noise, the table (150), noise
    Bytes arm9(0x40, 0xFF);
    for (int i = 0; i < 40; i++) { const Bytes h = Header(1, 1, 1, 0); arm9.insert(arm9.end(), h.begin(), h.end()); }
    arm9.insert(arm9.end(), 24, 0xFF);
    const size_t tableAt = arm9.size();
    for (int i = 0; i < 150; i++) { const Bytes h = Header((uint8_t)(i % 10), (uint16_t)(i % 7), (uint16_t)i, (uint8_t)(i % 16)); arm9.insert(arm9.end(), h.begin(), h.end()); }
    arm9.insert(arm9.end(), 24, 0xFF);
    size_t at = 0;
    const std::vector<MapHeader> headers = FindMapHeaders(arm9, 10, 7, 150, &at);
    check(at == tableAt && headers.size() == 150, "map headers: the run naming the most event files found, the decoy and zeroed fields skipped");
    check(headers[3].Area == 3 && headers[3].Matrix == 3 && headers[3].Events == 3 && headers[17].Camera == 1, "map headers: area, matrix, events, camera");
    bool refused = false;
    try { FindMapHeaders(arm9, 10, 7, 30); } catch (const FormatError&) { refused = true; } // 30 event files at most then
    check(refused, "map headers: no run naming 100 event files refused");

    // events: 1 background event, 1 object (x 5, z 7, y 2.5 tiles), 2 warps, 1 trigger
    Bytes ev;
    Push32(ev, 1); ev.insert(ev.end(), 0x14, 0);
    Push32(ev, 1); { Bytes o(0x20, 0); Put16(o, 0, 3); Put16(o, 2, 99); Put16(o, 0x0A, 1234); Put16(o, 0x18, 5); Put16(o, 0x1A, 7); Put32(o, 0x1C, 0x28000); ev.insert(ev.end(), o.begin(), o.end()); }
    Push32(ev, 2); for (uint16_t w : {0, 1}) { Bytes x(0x0C, 0); Put16(x, 0, (uint16_t)(100 + w)); Put16(x, 2, 200); Put16(x, 4, 42); Put16(x, 6, w); ev.insert(ev.end(), x.begin(), x.end()); }
    Push32(ev, 1); ev.insert(ev.end(), 0x10, 0);
    const ZoneEvents e = ZoneEvents::Read(ev);
    check(e.BgEvents == 1 && e.CoordEvents == 1 && e.Objects.size() == 1 && e.Warps.size() == 2, "zone events: the four lists counted");
    check(e.Objects[0].Id == 3 && e.Objects[0].Graphics == 99 && e.Objects[0].Script == 1234 && e.Objects[0].X == 5 && e.Objects[0].Z == 7 && e.Objects[0].Y == 2.5f,
          "zone events: an object's id, graphics, script, position");
    check(e.Warps[1].X == 101 && e.Warps[1].Z == 200 && e.Warps[1].DestHeader == 42 && e.Warps[1].DestWarp == 1, "zone events: a warp's tile and destination");
    refused = false;
    try { Bytes longer = ev; longer.push_back(0); ZoneEvents::Read(longer); } catch (const FormatError&) { refused = true; }
    check(refused, "zone events: bytes after the last list refused");
    refused = false;
    try { Bytes cut(ev.begin(), ev.end() - 4); ZoneEvents::Read(cut); } catch (const FormatError&) { refused = true; }
    check(refused, "zone events: a cut file refused");
    refused = false;
    try { Bytes huge = ev; Put32(huge, 0, 0x40000000); ZoneEvents::Read(huge); } catch (const FormatError&) { refused = true; }
    check(refused, "zone events: a count past the file refused");

    Bytes area; Push16(area, 9); Push16(area, 13); Push16(area, 7); Push16(area, 2);
    const AreaData a = AreaData::Read(area);
    check(a.BuildingSet == 9 && a.MapTextures == 13 && a.Light == 2, "area data: building set, map textures, lighting");
    refused = false;
    try { area.push_back(0); AreaData::Read(area); } catch (const FormatError&) { refused = true; }
    check(refused, "area data: not 8 bytes refused");

    printf(ok ? "ALL OK\n" : "FAILURES\n");
    return ok ? 0 : 1;
}
