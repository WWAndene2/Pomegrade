#include "OrasSandbox.h"

#include "Amx.h"
#include "BinLinker.h"
#include "GameText.h"
#include "Bps.h"
#include "Garc.h"
#include "NitroCompression.h"
#include "OrasMatrix.h"
#include "OrasTown.h"
#include "SceneScript.h"
#include "OrasNewZone.h"
#include "OrasWorkspace.h"
#include "OrasXyPiece.h"
#include "OrasXyScript.h"
#include "XyCharacter.h"
#include "N3dsRom.h"
#include "OrasZone.h"
#include "TownLayout.h"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <tuple>

namespace remake
{

static Bytes Plain(const Bytes& data) { return IsLzCompressed(data) ? LzDecompress(data) : data; }

struct SandboxZone
{
    int Number = -1, Pack = -1, Music = -1, Encounters = -1, Lighting = -1;
    std::vector<std::string> Lines; // its text file's lines (the zone's own, header word 3)
    std::map<uint32_t, float> Camera; // the camera settings given: byte offset in preset 0 of the area pack's file 6 -> value
    std::map<uint32_t, float> Light;  // the light colours given: byte offset in the area pack's file 4 -> value
    float SpawnX = -1, SpawnZ = -1;
    int XyEvents = -1; // xy-events: the X/Y zone whose people, objects, triggers, text and scripts the zone takes
    std::string Name;
    std::vector<std::array<int, 8>> Characters; // model, x, z, facing, script, movement, kind, sight
    std::vector<std::array<int, 3>> Shared;     // per character: its two zones and its shared id ({-1, -1, -1}: its zone only)
    std::vector<std::array<int, 5>> Doors;      // x, z, the zone its house's inside is made of, 1 for an X/Y zone (door ... xy), height
    std::vector<std::array<int, 5>> Triggers;   // x, z, width, height, the zone script's event number
    std::string Script, InitScript;             // assembler source (Amx.h) of the zone's own scripts, empty: ones that do nothing
    std::string Scene;                          // the zone script as a scene (SceneScript.h), instead of Script
};

struct SandboxDescription
{
    std::vector<SandboxZone> Zones;
    int Width = 0, Height = 0;
    std::vector<std::string> Blocks; // 4 Height rows of 4 Width digits: the zone (its place in Zones) of each 10 x 10 block
    std::vector<std::string> Map;
    std::vector<std::pair<int, std::string>> Teams; // a trainer of a/0/3/6 and its team block's lines
    std::map<int, std::pair<int, std::string>> TeamNames; // a team's trainer -> its new class (-1: kept) and name ("": kept)
    std::vector<SandboxZone> Edits;                 // game zones added to (edit Z): their lines, characters, triggers, scene
    std::vector<std::array<int, 4>> XyPieces;       // piece x, y, X/Y's a/0/4/1 member, the a/0/1/3 area pack it draws from
    // stairs X/Y runs by script (xy-stairs): X/Y zone A, its tiles x z w h going up and the way they are walked into; X/Y
    // zone B, its tiles going down and their way (0 down, 1 up, 2 right, 3 left: a warp kind's low byte)
    std::vector<std::array<int, 12>> XyStairs;
};

static constexpr size_t rowBytesOf536 = 56; // a zone header row (member 536 of a/0/1/3)
static constexpr int BlockTiles = 10; // a zone block of the matrix (OrasMatrix.h)

// a zone's characters against the pool oras-engine --characters gives (OrasWorkspace::CharacterPool)
// (a game zone already over 26, as zone 451's 27, is not refused for the people it had: `had`)
static void CheckCharacterCount(const OrasWorkspace& ws, int zone, size_t count, size_t had = 0)
{
    const size_t most = std::max(ws.CharacterPool ? ws.CharacterPool - 1 : 26, had);
    if (count > most)
        throw FormatError("sandbox: zone " + std::to_string(zone) + " places " + std::to_string(count) + " characters, the pool takes " +
                          std::to_string(most) + (ws.CharacterPool ? " (--characters " + std::to_string(ws.CharacterPool) + ", one for the player)" : " (the game's; give the --characters N of oras-engine)"));
}


// A character with no script (script 0), talked to, sends the game into a wait it never leaves (run tk_o, 17 October:
// Omega Ruby's own character 284 in the demo, its zone's script the empty one, the screen black after A): its event must
// end the talk. Each such character (not a trainer: they have 3000 + id) gets an event of its own that does nothing, an
// "on N" with no statement in the zone's scene (a scene written for it when the zone has none: run tk_ee, talking then
// leaves the player free, the person turned to them), N the smallest number from 1 up no event, character or trigger of
// the description uses (9999 sent the game into its fatal-error loop, run tk_fix0: the numbers a talk accepts are bounded,
// where is not read; 9 works). A game zone's own characters' numbers are not checked (edit Z): a clash runs its event.
// A zone written in assembler (script) is left as written.
static SandboxZone WithSilentCharacters(SandboxZone z)
{
    if (!z.Script.empty()) return z;
    std::set<int> used;
    for (const auto& c : z.Characters) used.insert(c[4]);
    for (const auto& t : z.Triggers) used.insert(t[4]);
    std::istringstream scene(z.Scene);
    for (std::string l; std::getline(scene, l);)
    {
        std::istringstream words(l);
        std::string first;
        int n = 0;
        if (words >> first && first == "on" && words >> n) used.insert(n);
    }
    int silent = 1;
    while (used.count(silent)) silent++;
    bool any = false;
    for (auto& c : z.Characters)
        if (c[4] == 0 && c[6] != 1) { c[4] = silent; any = true; }
    if (any) z.Scene += (z.Scene.empty() || z.Scene.back() == '\n' ? "" : "\n") + std::string("on ") + std::to_string(silent) + "\n";
    return z;
}

static bool Solid(char c) { return c == '~' || c == 't' || c == 'T' || c == 'H' || c == 'F' || c == 'L'; }

static SandboxDescription ReadDescription(const std::string& text)
{
    SandboxDescription d;
    std::istringstream in(text);
    std::string line;
    enum { Statements, Blocks, Map } part = Statements;
    std::string* source = nullptr; // inside a script, init-script or scene block
    bool editing = false;          // the statements go to the last "edit" zone, not the last new zone
    int number = 0;
    auto fail = [&](const std::string& what) { throw FormatError("sandbox description, line " + std::to_string(number) + ": " + what); };
    while (std::getline(in, line))
    {
        number++;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (source)
        {
            // a script block's lines as they are ('#' is a hash in assembler source, ';' its comment), up to "end"
            std::istringstream first(line);
            std::string word;
            if (first >> word && word == "end") { source = nullptr; continue; }
            *source += line + "\n";
            continue;
        }
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        std::istringstream words(line);
        std::string what;
        if (!(words >> what)) continue;
        if (part == Map) { d.Map.push_back(what); continue; }
        if (what == "map") { part = Map; continue; }
        if (part == Blocks && what.find_first_not_of("0123456789") == std::string::npos) { d.Blocks.push_back(what); continue; }
        part = Statements;
        bool ok = true;
        if (what == "zone")
        {
            // "zone N", or "zone" alone: the next free number as the build stands (OrasWorkspace.h), given when it is built
            d.Zones.emplace_back();
            editing = false;
            std::string n;
            if (words >> n) { try { d.Zones.back().Number = std::stoi(n); } catch (...) { fail("zone takes a number or nothing"); } }
            continue;
        }
        if (what == "edit")
        {
            // edit Z: then lines, characters, triggers and a scene added to game zone Z (0-535), its own kept
            d.Edits.emplace_back();
            if (!(words >> d.Edits.back().Number) || d.Edits.back().Number < 0 || d.Edits.back().Number > 535) fail("edit takes a game zone, 0-535");
            editing = true;
            continue;
        }
        if (what == "team")
        {
            // team ID: then the trainer's new team, a "pokemon" line each, up to "end" (read when the mod is built)
            // team ID [class C] [name TEXT]: the class (a/0/3/7 record, the battle's portrait) and the name may change too
            int id = 0;
            if (!(words >> id) || id < 1 || id > 949) fail("team takes a trainer id, 1-949 (a/0/3/6)");
            int cls = -1;
            std::string name, key;
            while (words >> key)
            {
                if (key == "class") { if (!(words >> cls) || cls < 0 || cls > 279) fail("class takes 0-279 (a/0/3/7)"); }
                else if (key == "name") { std::getline(words >> std::ws, name); if (name.empty()) fail("name takes a text"); }
                else fail("team has no " + key);
            }
            d.TeamNames[id] = {cls, name};
            d.Teams.push_back({id, ""});
            source = &d.Teams.back().second;
            continue;
        }
        if (what == "pieces") { ok = (bool)(words >> d.Width >> d.Height); if (!ok) fail("pieces needs its numbers"); continue; }
        if (what == "xy-piece")
        {
            std::array<int, 4> x{};
            ok = (bool)(words >> x[0] >> x[1] >> x[2] >> x[3]);
            if (!ok) fail("xy-piece needs X Y MEMBER PACK");
            d.XyPieces.push_back(x);
            continue;
        }
        if (what == "xy-stairs")
        {
            // ZA X Z W H WAY ZB X Z W H WAY, a way being down, up, right or left
            std::array<int, 12> x{};
            ok = true;
            for (int k = 0; k < 12 && ok; k++)
            {
                if (k == 5 || k == 11)
                {
                    std::string way;
                    ok = (bool)(words >> way);
                    const char* ways[] = {"down", "up", "right", "left"};
                    x[k] = -1;
                    for (int w = 0; w < 4; w++) if (way == ways[w]) x[k] = w;
                    ok = ok && x[k] >= 0;
                }
                else ok = (bool)(words >> x[k]);
            }
            if (!ok) fail("xy-stairs needs ZA X Z W H WAY ZB X Z W H WAY (WAY: down, up, right or left)");
            d.XyStairs.push_back(x);
            continue;
        }
        if (what == "blocks") { part = Blocks; continue; }
        if (editing ? d.Edits.empty() : d.Zones.empty()) fail(what + " before any zone");
        SandboxZone& z = editing ? d.Edits.back() : d.Zones.back();
        if (editing && what != "line" && what != "character" && what != "trigger" && what != "scene")
            fail("an edited game zone takes line, character, trigger and scene only");
        if (what == "script" || what == "init-script" || what == "scene")
        {
            std::string& block = what == "script" ? z.Script : what == "scene" ? z.Scene : z.InitScript;
            if (!block.empty()) fail(what + " given twice");
            if (what != "init-script" && !(what == "script" ? z.Scene : z.Script).empty()) fail("a zone takes a script or a scene, not both");
            // a scene's lines are counted from its block's first (SceneScript.h names them in its errors)
            block = what == "scene" ? "" : "; " + what + " of zone " + std::to_string(z.Number) + "\n";
            source = &block;
            continue;
        }
        if (what == "template") fail("\"template\" is gone: zones are written from nothing (SINNOH_BUILD.md R2); give the area pack with \"pack P\"");
        if (what == "pack") ok = (bool)(words >> z.Pack);
        else if (what == "music") ok = (bool)(words >> z.Music);
        else if (what == "line") { std::string rest; std::getline(words, rest); z.Lines.push_back(rest.substr(rest.find_first_not_of(' ') == std::string::npos ? rest.size() : rest.find_first_not_of(' '))); }
        else if (what == "encounters") ok = (bool)(words >> z.Encounters);
        else if (what == "lighting") ok = (bool)(words >> z.Lighting);
        else if (what == "light" || what == "character-light")
        {
            // "light NAME R G B..." (OrasSandbox.h): a colour of the area pack's file 4, in each of its 12 entries. The file holds
            // its colours in planes of 12 floats (3 kinds x 4 times of day, FUN_0013D908 0x13D908 reads them, FUN_0012DDF4 blends
            // two entries and sets the field light's ambient and diffuse colours, FUN_00139124 and FUN_001391D4): a colour's red
            // at its group's offset, green 0x30 and blue 0x60 further; the direction the same way from 0x240, x y z, the way the light
            // goes, normalised by the game (FUN_0013D540; (0, -1, 0) in Littleroot, from above) (ORAS_ENGINE.md 2)
            static const std::map<std::string, uint32_t> colours = {{"ambient", 0x00}, {"diffuse", 0x90}, {"direction", 0x240}};
            std::string name; float rgb[3];
            ok = false;
            while (words >> name >> rgb[0] >> rgb[1] >> rgb[2])
            {
                const auto at = colours.find(name);
                if (at == colours.end()) fail("no light setting \"" + name + "\" (ambient, diffuse, direction)");
                // character-light: the second light set, the characters' (the same layout 0x2D0 further, read by FUN_0013DC4C)
                const uint32_t set = what == "light" ? 0 : 0x2D0;
                for (uint32_t c = 0; c < 3; c++)
                    for (uint32_t entry = 0; entry < 12; entry++) z.Light[set + at->second + c * 0x30 + entry * 4] = rgb[c];
                ok = true;
            }
        }
        else if (what == "camera")
        {
            // "camera NAME VALUE..." (OrasSandbox.h): each name a float of a camera preset, by its offset (Field_CameraApplyParams
            // 0x102CB460 and Field_CopyCameraParams 0x102CB128 read them, ORAS_ENGINE.md 2)
            static const std::map<std::string, uint32_t> settings = {{"height", 0x08}, {"pitch", 0x0C}, {"yaw", 0x10},
                {"near", 0x18}, {"far", 0x1C}, {"fov", 0x20}, {"distance", 0x24}};
            std::string name; float value;
            ok = false;
            while (words >> name >> value)
            {
                const auto at = settings.find(name);
                if (at == settings.end()) fail("no camera setting \"" + name + "\" (height, pitch, yaw, near, far, fov, distance)");
                z.Camera[at->second] = value; ok = true;
            }
        }
        else if (what == "spawn") ok = (bool)(words >> z.SpawnX >> z.SpawnZ);
        else if (what == "xy-events") ok = (bool)(words >> z.XyEvents) && z.XyEvents >= 0;
        else if (what == "name") { std::getline(words >> std::ws, z.Name); ok = !z.Name.empty(); }
        else if (what == "character" || what == "trainer")
        {
            // character MODEL X Z [face F] [script S] [move M] [share Z]; trainer ID MODEL X Z [face F] [sight N] [move M]
            // share Z: the character also belongs to the description's zone Z (one map, crossed without a load): the game keeps
            // it on screen across the border instead of swapping zones' people (ORAS_ENGINE.md 4.5b, read 17 October)
            int id = 0;
            std::array<int, 8> c{0, 0, 0, 0, 0, 0, 0, 0};
            if (what == "trainer") { ok = (bool)(words >> id); c[4] = 3000 + id; c[6] = 1; c[7] = 4; }
            ok = ok && (bool)(words >> c[0] >> c[1] >> c[2]);
            std::string key;
            int share = -1;
            while (ok && words >> key)
            {
                int v = 0;
                ok = (bool)(words >> v);
                if (key == "face") c[3] = v;
                else if (key == "share" && what == "character") share = v;
                else if (key == "script" && what == "character") c[4] = v;
                else if (key == "move") c[5] = v;
                else if (key == "sight" && what == "trainer") c[7] = v;
                else fail(what + " has no " + key);
            }
            if (what == "trainer" && (id < 1 || id > 949)) fail("trainer ids are 1-949 (a/0/3/6)");
            z.Characters.push_back(c);
            z.Shared.push_back({share < 0 ? -1 : z.Number, share, share < 0 ? -1 : 0});
        }
        else if (what == "trigger")
        {
            // trigger X Z [W H] script N: stepping on the W x H tiles from X, Z runs the zone script with g_mode N
            std::array<int, 5> t{0, 0, 1, 1, 0};
            std::vector<std::string> rest;
            for (std::string v; words >> v;) rest.push_back(v);
            ok = (rest.size() == 4 || rest.size() == 6) && rest[rest.size() - 2] == "script";
            if (ok)
            {
                try
                {
                    t[0] = std::stoi(rest[0]); t[1] = std::stoi(rest[1]);
                    if (rest.size() == 6) { t[2] = std::stoi(rest[2]); t[3] = std::stoi(rest[3]); }
                    t[4] = std::stoi(rest.back());
                }
                catch (...) { ok = false; }
            }
            if (ok && (t[2] < 1 || t[3] < 1 || t[4] < 1)) fail("a trigger's size and script are 1 or more");
            z.Triggers.push_back(t);
        }
        else if (what == "door")
        {
            // door X Z house Z2: the door tile ('D' on the map) and the game interior its house's inside is copied from;
            // door X Z xy ZX: on an xy-piece (no 'D': X's door model is the piece's), the inside made of X/Y's zone ZX
            // either may end with "height H": the warp's height in pixels (word 5; 0 by default, Littleroot's ground). X's
            // Vaniville stands at 144 (its warps' own): a warp at 0 there did not fire (run hDoor2)
            std::array<int, 5> door{0, 0, 0, 0, 0};
            std::string house, key;
            ok = (bool)(words >> door[0] >> door[1] >> house >> door[2]) && (house == "house" || house == "xy");
            door[3] = house == "xy";
            if (ok && words >> key) ok = key == "height" && (bool)(words >> door[4]);
            z.Doors.push_back(door);
        }
        else fail("unknown statement " + what);
        if (!ok) fail(what + " needs its numbers");
    }
    if (d.Zones.empty())
    {
        // edits and teams alone: no new zone, no map
        if (d.Edits.empty() && d.Teams.empty()) throw FormatError("sandbox description: a zone, an edit or a team is required");
        return d;
    }
    if (d.Width <= 0 || d.Height <= 0) throw FormatError("sandbox description: pieces are required with new zones");
    if ((int)d.Map.size() != d.Height * TownTiles)
        throw FormatError("sandbox description: the map has " + std::to_string(d.Map.size()) + " rows, pieces asks for " + std::to_string(d.Height * TownTiles));
    for (size_t r = 0; r < d.Map.size(); r++)
    {
        if ((int)d.Map[r].size() != d.Width * TownTiles)
            throw FormatError("sandbox description: map row " + std::to_string(r) + " has " + std::to_string(d.Map[r].size()) + " letters, pieces asks for " +
                              std::to_string(d.Width * TownTiles));
        for (char c : d.Map[r])
            if (std::string(".g:s*~tTHDFL").find(c) == std::string::npos)
                throw FormatError(std::string("sandbox description: map row ") + std::to_string(r) + ": letter '" + c + "' is not built yet (. g : s * ~ t T H D F L)");
    }
    const int bw = d.Width * OrasMatrix::BlocksPerPiece, bh = d.Height * OrasMatrix::BlocksPerPiece;
    if (d.Blocks.empty())
    {
        if (d.Zones.size() > 1) throw FormatError("sandbox description: several zones need a blocks grid");
        d.Blocks.assign((size_t)bh, std::string((size_t)bw, '0'));
    }
    if ((int)d.Blocks.size() != bh) throw FormatError("sandbox description: blocks needs " + std::to_string(bh) + " rows (4 a piece)");
    std::vector<bool> used(d.Zones.size(), false);
    for (const std::string& row : d.Blocks)
    {
        if ((int)row.size() != bw) throw FormatError("sandbox description: a blocks row needs " + std::to_string(bw) + " digits (4 a piece)");
        for (char c : row)
        {
            if ((size_t)(c - '0') >= d.Zones.size()) throw FormatError(std::string("sandbox description: blocks names zone ") + c + ", there are " + std::to_string(d.Zones.size()));
            used[(size_t)(c - '0')] = true;
        }
    }
    for (size_t k = 0; k < d.Zones.size(); k++)
    {
        const SandboxZone& z = d.Zones[k];
        const std::string which = "sandbox description, zone " + (z.Number >= 0 ? std::to_string(z.Number) : "#" + std::to_string(k + 1) + " (numbered when built)");
        if (z.SpawnX < 0) throw FormatError(which + ": its spawn is required");
        if ((z.Number < 0) != (d.Zones[0].Number < 0)) throw FormatError(which + ": every zone numbered, or none");
        if (&z == &d.Zones[0] && z.Pack < 0) throw FormatError(which + ": the first zone gives the area pack (pack P)");
        if (!used[k]) throw FormatError(which + ": no block of the blocks grid");
        if (k && z.Number >= 0 && z.Number != d.Zones[k - 1].Number + 1) throw FormatError(which + ": zones are numbered one after the other");
        auto check = [&](float x, float zz, const std::string& what) {
            if (x < 0 || zz < 0 || x >= d.Width * TownTiles || zz >= d.Height * TownTiles) throw FormatError(which + ": " + what + " outside the map");
            if (Solid(d.Map[(size_t)zz][(size_t)x])) throw FormatError(which + ": " + what + " on a solid tile");
            if ((size_t)(d.Blocks[(size_t)zz / BlockTiles][(size_t)x / BlockTiles] - '0') != k) throw FormatError(which + ": " + what + " on another zone's block");
        };
        check(z.SpawnX, z.SpawnZ, "the spawn tile");
        for (const auto& c : z.Characters) check((float)c[1], (float)c[2], "a character");
        for (const auto& door : z.Doors)
        {
            check((float)door[0], (float)door[1], "a door");
            if (door[3]) { if (door[2] < 0) throw FormatError(which + ": an X/Y inside is an X/Y zone number"); continue; }
            if (d.Map[(size_t)door[1]][(size_t)door[0]] != 'D') throw FormatError(which + ": a door is not on a 'D' of the map");
            if (door[2] < 0 || door[2] >= 536) throw FormatError(which + ": a house's inside is a game zone, 0-535");
        }
    }
    if (source) fail("a script block without its \"end\"");
    return d;
}

// one piece's layout from the hand-drawn roles: paths and water at half-tile precision as whole tiles, no snow, collision from the role
static TownLayout PieceLayout(const SandboxDescription& d, int px, int py)
{
    TownLayout l;
    for (int r = 0; r < TownTiles; r++) l.Vis.push_back(d.Map[(size_t)py * TownTiles + r].substr((size_t)px * TownTiles, TownTiles));
    // a door: a house tile to the builder, which puts Petalburg's house on it, as wide as the solid tiles beside it on its row
    // (BuildTown), and its door model; solid, as Littleroot's door tiles are (0x01000021 in piece 6: the player
    // enters by walking into it from the tile below, where the warp stands at the door tile)
    for (int r = 0; r < TownTiles; r++)
        for (int c = 0; c < TownTiles; c++)
            if (l.Vis[r][c] == 'D') { l.Vis[r][c] = 'H'; TownDoor door; door.Column = c; door.Row = r; l.Doors.push_back(door); }
    for (int r = 0; r < 2 * TownTiles; r++)
    {
        std::string path, water;
        for (int c = 0; c < 2 * TownTiles; c++)
        {
            const char v = l.Vis[r / 2][c / 2];
            path += v == ':' ? ':' : '.';
            water += v == '~' ? '~' : '.';
        }
        l.Path2.push_back(path);
        l.Water2.push_back(water);
        l.Snow2.push_back(std::string(2 * TownTiles, '.'));
    }
    for (const std::string& row : l.Vis)
    {
        std::string c;
        for (char v : row) c += v == '~' ? '~' : v == 't' || v == 'T' || v == 'H' || v == 'F' || v == 'D' ? '#' : v == 'g' ? 'g' : '.';
        l.Collision.push_back(c);
    }
    return l;
}

// An X/Y interior made Omega Ruby's for a door's inside (door X Z xy ZX): X's zone ZX (a/0/1/2: header word 1 its area pack,
// word 2 its matrix, as Omega Ruby's; its events' warp 0 its way out, as a sandbox inside's), its one piece converted
// (XyPieceToOras, its props as xy-piece's) on a copy of X's matrix with the piece renumbered (X's matrices are Omega Ruby's
// format: ORAS_ENGINE.md 7), on a new area pack: a copy of game zone 223's (a house's inside: its camera and light) with X's
// textures and props added
struct XyInside { int Matrix = -1, AreaPack = -1; uint16_t ExitX = 0, ExitZ = 0; std::vector<std::array<uint16_t, 12>> Warps; };
// a zone with no warp (a floor X/Y reaches by script, xy-stairs) is taken when floorAt gives the tile pixels it is entered at
static XyInside XyInsideToOras(OrasWorkspace& ws, int xZone, const std::vector<int>& models, const Garc& pieceArchive, const Garc& zoneArchive,
                               std::vector<std::string>& log, const std::array<uint16_t, 2>* floorAt = nullptr)
{
    if (ws.XyRom.empty()) throw FormatError("sandbox: door ... xy needs oras-sandbox --xy <x.3ds>");
    N3dsRom xRom(ws.XyRom);
    const Garc xZones(xRom.Read("a/0/1/2")), xMatrices(xRom.Read("a/0/4/2")), xPieces(xRom.Read("a/0/4/1")), xPacks(xRom.Read("a/0/1/3")),
        xProps(xRom.Read("a/0/2/4"));
    if (xZone >= (int)xZones.Count()) throw FormatError("sandbox: no X/Y zone " + std::to_string(xZone));
    const BinLinker zone = BinLinker::Read(Plain(xZones.Sub((size_t)xZone)), "ZO");
    const Bytes& header = zone.Files.at(0);
    const Bytes& events = zone.Files.at(1);
    const size_t pack = U16(header, 2), matrixIndex = U16(header, 4);
    // the events file: u32 size, four u8 counts (furniture, characters, warps, triggers), u32 a fifth count, the arrays (OrasZone.h)
    if (events.size() < 12 || (events[6] == 0 && !floorAt)) throw FormatError("sandbox: X/Y zone " + std::to_string(xZone) + " has no warp (no way out)");
    const size_t warp0 = 12 + 0x14 * (size_t)events[4] + 0x30 * (size_t)events[5];
    XyInside in;
    if (events[6]) { in.ExitX = U16(events, warp0 + 8); in.ExitZ = U16(events, warp0 + 12); }
    else { in.ExitX = (*floorAt)[0]; in.ExitZ = (*floorAt)[1]; }
    for (size_t k = 0; k < events[6]; k++)
    {
        std::array<uint16_t, 12> w{};
        for (size_t j = 0; j < 12; j++) w[j] = U16(events, warp0 + 0x18 * k + 2 * j);
        in.Warps.push_back(w);
    }
    OrasMatrix matrix = OrasMatrix::Read(Plain(xMatrices.Sub(matrixIndex)));
    if (matrix.Width != 1 || matrix.Height != 1) throw FormatError("sandbox: X/Y zone " + std::to_string(xZone) + "'s matrix is not one piece");
    const uint16_t xPieceIndex = matrix.Pieces.at(0);
    const Bytes xPiece = Plain(xPieces.Sub(xPieceIndex)), xPack = Plain(xPacks.Sub(pack));
    const int housePack = OrasZone::Read(Plain(zoneArchive.Sub(223))).AreaPack();
    Bytes packData = Plain(ws.Original("a/0/1/4").Sub((size_t)housePack));
    const std::map<uint32_t, uint32_t> props = XyPropsToOras(XyPieceProps(xPiece), xProps, xPack, pack, ws.Edited("a/0/2/3"), ws.Original("a/0/2/3"), packData, log);
    const Bytes kage = BinLinker::Read(Plain(pieceArchive.Sub(6)), "GR").Files.at(6);
    const Bytes piece = XyPieceToOras(xPiece, xPack, pack, kage, "", 0, 0, props, packData, log);
    matrix.Pieces.at(0) = (uint16_t)AppendMember(ws.Edited("a/0/3/9"), pieceArchive, 6, piece, "GR");
    const Bytes matrixData = matrix.Write();
    in.Matrix = (int)AppendMember(ws.Edited("a/0/4/0"), ws.Original("a/0/4/0"), 1, matrixData, "MM");
    in.AreaPack = ws.AddAreaPack(packData, housePack, models);
    log.push_back("X/Y zone " + std::to_string(xZone) + ": its piece " + std::to_string(matrix.Pieces.at(0)) + " (X's " + std::to_string(xPieceIndex) +
                  "), matrix " + std::to_string(in.Matrix) + ", area pack " + std::to_string(in.AreaPack) + " (a copy of pack " + std::to_string(housePack) + ")");
    return in;
}

// An X/Y zone's people, objects, triggers, text and scripts made Omega Ruby's (xy-events, and the insides of door ... xy):
// characters' plain models (under 0x1000) converted (XyCharacter.h) and appended to a/0/2/1 once each (modelOf), class
// models (0x1000+, 0x2000+: the two games' numbering, not compared) kept; flags (characters' word 4) and works (triggers'
// word 2) moved by `numbers`; tiles shifted by (dx, dz) from X/Y's matrix to the new one; the text, X/Y's a/0/8/0-7 member
// of the zone's header word 3, added as it is in each language; the scripts read from <scriptDir>/x<zone>.amx and
// x<zone>.init.amx when there, else translated (XyScriptToOras, XyScriptSource), and assembled against the native mask they
// need (0x243 and the bits their natives are registered under: `mask`, which oras-engine --script-mask must give the zone
// when it is not 0x243). X/Y's fifth kind of event (zone 0 has 2) is left out (logged).
struct XyEvents
{
    std::vector<ZoneFurniture> Furniture;
    std::vector<ZoneCharacter> Characters;
    std::vector<ZoneTrigger> Triggers;
    Bytes Script, InitScript;
    int Text = -1;
    uint32_t Mask = 0x243;
    std::vector<int> Models; // the Omega Ruby models of its characters (plain ones), for the area pack's list
};
static XyEvents XyEventsToOras(OrasWorkspace& ws, int xZone, int dx, int dz, const XyNumbers& numbers, std::map<int, int>& modelOf,
                               const std::string& namesFile, const std::string& scriptDir, int orasZone, std::vector<std::string>& log)
{
    N3dsRom xRom(ws.XyRom);
    const Garc xZones(xRom.Read("a/0/1/2"));
    const OrasZone x = OrasZone::Read(Plain(xZones.Sub((size_t)xZone)));
    XyEvents e;
    const auto moved = [](const std::map<int, int>& table, int v) { const auto t = table.find(v); return t == table.end() ? v : t->second; };
    for (ZoneFurniture f : x.Furniture)
    {
        f.Raw[4] = (uint16_t)(f.Raw[4] + dx); f.Raw[5] = (uint16_t)(f.Raw[5] + dz);
        e.Furniture.push_back(f);
    }
    for (ZoneCharacter c : x.Characters)
    {
        const int model = c.Raw[1];
        if (model < 0x1000)
        {
            if (!modelOf.count(model))
            {
                const Garc xCharacters(xRom.Read("a/0/2/1"));
                std::vector<std::string> report;
                const Bytes file = XyCharacterToOras(Plain(xCharacters.Sub((size_t)model)), report);
                const size_t member = AppendMember(ws.Edited("a/0/2/1"), ws.Original("a/0/2/1"), 284, file, std::string(file.begin(), file.begin() + 2));
                if (member >= 0x1000) throw FormatError("X/Y character " + std::to_string(model) + ": a/0/2/1 member " + std::to_string(member) + " past a plain model number");
                modelOf[model] = (int)member;
                log.push_back("X/Y character model " + std::to_string(model) + ": a/0/2/1 member " + std::to_string(member));
            }
            c.Raw[1] = (uint16_t)modelOf[model];
            if (std::find(e.Models.begin(), e.Models.end(), modelOf[model]) == e.Models.end()) e.Models.push_back(modelOf[model]);
        }
        c.Raw[4] = (uint16_t)moved(numbers.Flags, c.Raw[4]);
        c.Raw[20] = (uint16_t)(c.Raw[20] + dx); c.Raw[21] = (uint16_t)(c.Raw[21] + dz);
        e.Characters.push_back(c);
    }
    for (ZoneTrigger t : x.Triggers)
    {
        t.Raw[2] = (uint16_t)moved(numbers.Works, t.Raw[2]);
        t.Raw[6] = (uint16_t)(t.Raw[6] + dx); t.Raw[7] = (uint16_t)(t.Raw[7] + dz);
        e.Triggers.push_back(t);
    }
    if (!x.Others.empty()) log.push_back("X/Y zone " + std::to_string(xZone) + ": its " + std::to_string(x.Others.size()) + " event(s) of the fifth kind left out");
    // the text, each language's file as X/Y has it
    std::vector<Bytes> texts;
    for (int k = 0; k < 8; k++) texts.push_back(Plain(Garc(xRom.Read("a/0/8/" + std::to_string(k))).Sub(x.Header[3])));
    e.Text = ws.AddZoneTextFiles(texts);
    // the scripts
    for (const bool init : {false, true})
    {
        const Bytes& xScript = init ? x.InitScript : x.Script;
        const std::filesystem::path file = std::filesystem::path(scriptDir) / ("x" + std::to_string(xZone) + (init ? ".init" : "") + ".amx");
        std::string source, from;
        std::vector<std::string> notes;
        if (!scriptDir.empty() && std::filesystem::exists(file))
        {
            const Bytes text = ReadFile(file.string());
            source.assign(text.begin(), text.end());
            from = file.string();
        }
        else if (xScript.size() >= 16)
        {
            source = XyScriptSource(XyScriptToOras(xScript, namesFile, 0x243).Script, namesFile, numbers, notes);
            from = "translated";
        }
        else source = init ? EmptyInitScript : EmptyZoneScript;
        uint32_t mask = 0x243;
        Bytes assembled;
        for (int tries = 0;; tries++)
        {
            try { assembled = AmxAssemble(source, AmxNativeCheck(namesFile, mask)); break; }
            catch (const FormatError& err)
            {
                const std::string why = err.what();
                const size_t bit = why.find("registered under bit ");
                if (bit == std::string::npos || tries > 16) throw FormatError("X/Y zone " + std::to_string(xZone) + (init ? " init" : "") + " script (" + from + "): " + why);
                mask |= (uint32_t)std::stoul(why.substr(bit + 21), nullptr, 0);
            }
        }
        e.Mask |= mask;
        (init ? e.InitScript : e.Script) = assembled;
        if (!from.empty()) log.push_back("zone " + std::to_string(orasZone) + ": X/Y zone " + std::to_string(xZone) + "'s " + (init ? "init script" : "script") + ", " + from);
        for (const std::string& n : notes) log.push_back("  " + n);
    }
    if (e.Mask != 0x243)
    {
        char m[160];
        snprintf(m, sizeof m, "zone %d: its scripts need native mask 0x%X: give oras-engine --script-mask %d:0x%X", orasZone, e.Mask, orasZone, e.Mask);
        log.push_back(m);
    }
    return e;
}

std::vector<std::string> BuildOrasSandbox(OrasWorkspace& ws, const std::string& description, const std::string& outDir,
                                          const std::function<std::string(const std::string&)>& checkNative, const std::string& descriptionPath)
{
    SandboxDescription d = ReadDescription(description);
    std::vector<std::string> log;
    char line[256];

    // the trainers' new teams (a/0/3/6 and a/0/3/8, ORAS_ENGINE.md 6: Trainer_ReadData, Trainer_BuildTeams; formats read
    // on the game's 949 teams, 16 October): a team entry is u8 difficulty (the game's 0-255), u8 0, u16 level, u16 species,
    // u16 form, then u16 item for formats 2 and 3, then 4 u16 moves for formats 1 and 3; the record's byte +0 is the format,
    // +7 the count, the rest (class, AI) kept
    for (const auto& [id, block] : d.Teams)
    {
        struct Mon { int Species = 0, Level = 0, Form = 0, Item = 0, Difficulty = 0; std::vector<int> Moves; };
        std::vector<Mon> mons;
        std::istringstream lines(block);
        int number = 0;
        for (std::string line; std::getline(lines, line);)
        {
            number++;
            const size_t hash = line.find('#');
            if (hash != std::string::npos) line.erase(hash);
            std::istringstream w(line);
            std::string what;
            if (!(w >> what)) continue;
            Mon m;
            m.Difficulty = 0;
            if (what != "pokemon" || !(w >> m.Species >> m.Level))
                throw FormatError("sandbox: team " + std::to_string(id) + ", line " + std::to_string(number) + ": \"pokemon SPECIES LEVEL [form F] [item I] [moves A B C D] [difficulty D]\"");
            for (std::string key; w >> key;)
            {
                bool ok = true;
                if (key == "form") ok = (bool)(w >> m.Form);
                else if (key == "item") ok = (bool)(w >> m.Item);
                else if (key == "difficulty") ok = (bool)(w >> m.Difficulty);
                else if (key == "moves") { m.Moves.assign(4, 0); ok = (bool)(w >> m.Moves[0] >> m.Moves[1] >> m.Moves[2] >> m.Moves[3]); }
                else ok = false;
                if (!ok) throw FormatError("sandbox: team " + std::to_string(id) + ", line " + std::to_string(number) + ": \"" + key + "\" and its value(s)");
            }
            if (m.Species < 1 || m.Species > 721 || m.Level < 1 || m.Level > 100 || m.Difficulty < 0 || m.Difficulty > 255)
                throw FormatError("sandbox: team " + std::to_string(id) + ", line " + std::to_string(number) + ": species 1-721, level 1-100, difficulty 0-255");
            mons.push_back(m);
        }
        if (mons.empty() || mons.size() > 6) throw FormatError("sandbox: team " + std::to_string(id) + " has 1 to 6 Pokemon");
        bool items = false, moves = false;
        for (const Mon& m : mons) { items |= m.Item != 0; moves |= !m.Moves.empty(); }
        const int format = (moves ? 1 : 0) | (items ? 2 : 0);
        Bytes team;
        for (const Mon& m : mons)
        {
            const auto u16 = [&](int v) { team.push_back((uint8_t)v); team.push_back((uint8_t)(v >> 8)); };
            team.push_back((uint8_t)m.Difficulty); team.push_back(0);
            u16(m.Level); u16(m.Species); u16(m.Form);
            if (items) u16(m.Item);
            if (moves) for (int k = 0; k < 4; k++) u16(m.Moves.empty() ? 0 : m.Moves[k]);
        }
        Bytes record = ws.Original("a/0/3/6").Sub((size_t)id);
        if (record.size() != 24) throw FormatError("sandbox: trainer " + std::to_string(id) + "'s record is not 24 bytes");
        record[0] = (uint8_t)format;
        record[7] = (uint8_t)mons.size();
        const auto& [cls, name] = d.TeamNames.at(id);
        if (cls >= 0) { record[2] = (uint8_t)cls; record[3] = (uint8_t)(cls >> 8); }
        if (!name.empty())
            // the trainer's name: line ID of a/0/7/1-a/0/7/8 member 22, one archive per language (the 950 trainers' names,
            // found by "Dixie", trainer 10's, on line 10 of the French one, a/0/7/4; member 21 is the 280 classes'),
            // the same name in all eight
            for (int a = 1; a <= 8; a++)
            {
                const std::string path = "a/0/7/" + std::to_string(a);
                Garc& texts = ws.Edited(path);
                const Bytes was = texts.Sub(22);
                const Bytes plain = Plain(was);
                // the other names' bytes kept as they are (a/0/7/1's do not write back identical: their zero padding)
                std::vector<std::string> names = ReadGameText(plain);
                if ((size_t)id >= names.size()) throw FormatError(path + " member 22 has no line " + std::to_string(id));
                const Bytes file = ReplaceGameTextLine(plain, (size_t)id, name);
                names[(size_t)id] = name;
                if (ReadGameText(file) != names) throw FormatError(path + " member 22: the names do not read back with the new one");
                texts.Set(22, IsLzCompressed(was) ? Lz11Compress(file) : file);
            }
        ws.Edited("a/0/3/6").Set((size_t)id, record);
        ws.Edited("a/0/3/8").Set((size_t)id, team);
        snprintf(line, sizeof line, "trainer %d: a new team of %zu (format %d)%s%s", id, mons.size(), format,
                 cls >= 0 ? (", class " + std::to_string(cls)).c_str() : "", name.empty() ? "" : (", named " + name).c_str());
        log.push_back(line);
    }

    // the game zones added to (edit Z): their lines appended to the zone's text file in the eight languages (header word
    // 3, a/0/7/9-a/0/8/6), so a scene's "say L" is the zone's own count + L; their characters after the zone's own, numbered
    // from its highest + 1 (a scene's "say L by N" and "walk N" name a character by that number, as the log prints it);
    // their triggers after its own; the scene compiled on the zone's own script, chained (every event it does not give goes
    // to the zone's own main, so its people, signs and triggers keep working)
    for (const SandboxZone& edit : d.Edits)
    {
        const SandboxZone z = WithSilentCharacters(edit);
        const Bytes raw = ws.Original("a/0/1/3").Sub((size_t)z.Number);
        OrasZone zone = OrasZone::Read(Plain(raw));
        const int text = zone.Header[3];
        int lineBase = -1;
        for (int a = 9; a <= 16; a++)
        {
            const std::string path = "a/0/" + std::to_string(7 + a / 10) + "/" + std::to_string(a % 10);
            Garc& texts = ws.Edited(path);
            const Bytes was = texts.Sub((size_t)text);
            Bytes file = Plain(was);
            const int count = (int)ReadGameText(file).size();
            if (lineBase < 0) lineBase = count;
            else if (count != lineBase) throw FormatError("sandbox: zone " + std::to_string(z.Number) + "'s text has " + std::to_string(count) + " lines in " + path + ", " + std::to_string(lineBase) + " in a/0/7/9");
            for (const std::string& l : z.Lines) file = AppendGameTextLine(file, l);
            if ((int)ReadGameText(file).size() != count + (int)z.Lines.size()) throw FormatError(path + ": the added lines do not read back");
            texts.Set((size_t)text, IsLzCompressed(was) ? Lz11Compress(file) : file);
        }
        int nextId = 0;
        for (const ZoneCharacter& c : zone.Characters) nextId = std::max(nextId, (int)c.Raw[0] + 1);
        for (const auto& c : z.Characters)
        {
            zone.Characters.push_back(NewCharacter(nextId, c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[6] == 1 ? c[7] : 0));
            snprintf(line, sizeof line, "zone %d: character %d (model %d) at tile (%d, %d)", z.Number, nextId, c[0], c[1], c[2]);
            log.push_back(line);
            if (ws.Swims(c[0])) log.push_back("warning: zone " + std::to_string(z.Number) + ": model " + std::to_string(c[0]) + " is a swimmer, drawn swimming half in the ground off water (OrasWorkspace::Swims)");
            nextId++;
        }
        {
            // the models it is given added to its pack's list, which a model must be in (OrasWorkspace::CharacterList)
            std::vector<int> models;
            for (const auto& c : z.Characters) models.push_back(c[0]);
            ws.ListCharacters(zone.AreaPack(), models);
        }
        for (const auto& t : z.Triggers)
        {
            ZoneTrigger tr;
            tr.Raw[0] = (uint16_t)t[4]; tr.Raw[6] = (uint16_t)t[0]; tr.Raw[7] = (uint16_t)t[1];
            tr.Raw[8] = (uint16_t)t[2]; tr.Raw[9] = (uint16_t)t[3];
            zone.Triggers.push_back(tr);
        }
        if (!z.Scene.empty())
        {
            if (!checkNative) throw FormatError("sandbox: zone " + std::to_string(z.Number) + "'s scene: no native tables to check it against");
            std::string scene;
            try { scene = CompileScene(z.Scene, AmxSource(zone.Script), lineBase, true); }
            catch (const FormatError& e) { throw FormatError("sandbox: zone " + std::to_string(z.Number) + "'s " + e.what()); }
            zone.Script = AmxAssemble(scene, checkNative);
        }
        CheckCharacterCount(ws, z.Number, zone.Characters.size(), zone.Characters.size() - z.Characters.size());
        const Bytes written = zone.Write(Plain(raw));
        ws.Edited("a/0/1/3").Set((size_t)z.Number, IsLzCompressed(raw) ? Lz11Compress(written) : written);
        snprintf(line, sizeof line, "zone %d edited: %zu line(s) from line %d of text member %d, %zu character(s), %zu trigger(s), %s",
                 z.Number, z.Lines.size(), lineBase, text, z.Characters.size(), z.Triggers.size(), z.Scene.empty() ? "its own script" : "a scene chained to its script");
        log.push_back(line);
    }
    if (d.Zones.empty()) return log;

    // the game's archives as the build has left them (OrasWorkspace.h): what earlier steps added is kept, numbers follow it
    const Garc &pieceArchive = ws.Original("a/0/3/9"), &matrixArchive = ws.Original("a/0/4/0"), &zoneArchive = ws.Original("a/0/1/3"), &areaArchive = ws.Original("a/0/1/4");
    Garc &newPieces = ws.Edited("a/0/3/9"), &newMatrices = ws.Edited("a/0/4/0"), &newZones = ws.Edited("a/0/1/3"), &newAreas = ws.Edited("a/0/1/4");
    if (d.Zones[0].Number < 0)
        for (size_t k = 0; k < d.Zones.size(); k++) d.Zones[k].Number = (int)(newZones.Count() + k);
    if ((size_t)d.Zones[0].Number != newZones.Count())
        throw FormatError("sandbox: zone " + std::to_string(d.Zones[0].Number) + " is not the next free member of a/0/1/3 (" + std::to_string(newZones.Count()) + " as the build stands)");
    for (const SandboxZone& z : d.Zones)
        if (z.Encounters >= 536 || z.Lighting >= 536) throw FormatError("sandbox: encounters and lighting are game zones, 0-535");

    // every zone draws its textures from the first zone's area pack (a/0/1/4 member: the game's assets), which the pieces are
    // built into
    const int pack = d.Zones[0].Pack;
    if (pack >= (int)areaArchive.Count()) throw FormatError("sandbox: no area pack " + std::to_string(pack));
    Bytes packData = Plain(areaArchive.Sub((size_t)pack));

    // the matrix: one rectangle over the whole map (the player must stand in one of file 1's: OrasRegion), each block its zone
    const size_t matrixTemplate = 1;
    const OrasMatrix mm = OrasMatrix::Read(Plain(matrixArchive.Sub(matrixTemplate)));
    OrasMatrix matrix;
    matrix.Width = (uint16_t)d.Width; matrix.Height = (uint16_t)d.Height;
    matrix.Pieces.assign((size_t)d.Width * d.Height, OrasMatrix::None);
    matrix.Zones.assign((size_t)d.Width * d.Height * 16, OrasMatrix::None);
    for (size_t r = 0; r < d.Blocks.size(); r++)
        for (size_t c = 0; c < d.Blocks[r].size(); c++) matrix.Zone((int)c, (int)r) = (uint16_t)d.Zones[(size_t)(d.Blocks[r][c] - '0')].Number;
    matrix.Third.assign((size_t)d.Width * d.Height, OrasMatrix::None);
    matrix.File1 = mm.File1;
    std::fill(matrix.File1.begin(), matrix.File1.end(), 0);
    const float whole[4] = {0.0f, d.Width * 720.0f, 0.0f, d.Height * 720.0f};
    matrix.File1[0] = 1;
    std::memcpy(&matrix.File1[4], whole, sizeof whole);
    matrix.Lead[0] = mm.Lead[0]; matrix.Lead[1] = mm.Lead[1];

    OrasTownOptions to;
    to.CloseEdges = false;
    to.Ledges = true;
    to.SnowClumps = false; // no snow in a sandbox, and its clumps take the snow-band mesh the ledges use
    to.AreaPack = (size_t)pack;
    const PieceBudget budget = GamePieceBudget(pieceArchive);
    std::string layouts;
    std::map<uint32_t, uint32_t> xyPropTypes; // X/Y prop types -> the a/0/2/3 members made of them (xy-piece)
    for (int y = 0; y < d.Height; y++)
        for (int x = 0; x < d.Width; x++)
        {
            const TownLayout layout = PieceLayout(d, x, y);
            snprintf(line, sizeof line, "world16_%02d_%02d", x, y);
            const auto xy = std::find_if(d.XyPieces.begin(), d.XyPieces.end(), [&](const std::array<int, 4>& p) { return p[0] == x && p[1] == y; });
            Bytes piece;
            if (xy != d.XyPieces.end())
            {
                // an X/Y piece in place of the built one (OrasXyPiece.h): its own ground, collision and door models; the map's rows
                // for it are not used. Omega Ruby's shadow file is Littleroot's (piece 6)
                if (ws.XyRom.empty()) throw FormatError("sandbox: xy-piece needs oras-sandbox --xy <x.3ds>");
                N3dsRom xRom(ws.XyRom);
                const Garc xPieces(xRom.Read("a/0/4/1")), xPacks(xRom.Read("a/0/1/3")), xProps(xRom.Read("a/0/2/4"));
                const Bytes kage = BinLinker::Read(Plain(pieceArchive.Sub(6)), "GR").Files.at(6);
                const Bytes xPiece = Plain(xPieces.Sub((size_t)(*xy)[2])), xPack = Plain(xPacks.Sub((size_t)(*xy)[3]));
                // its doors and props: X's models appended to a/0/2/3 once for all the X pieces (their pack assumed X's same one)
                std::set<uint32_t> fresh;
                for (const uint32_t t : XyPieceProps(xPiece)) if (!xyPropTypes.count(t)) fresh.insert(t);
                for (const auto& [from, to2] : XyPropsToOras(fresh, xProps, xPack, (size_t)(*xy)[3], ws.Edited("a/0/2/3"), ws.Original("a/0/2/3"), packData, log))
                    xyPropTypes[from] = to2;
                piece = XyPieceToOras(xPiece, xPack, (size_t)(*xy)[3], kage, line, x, y, xyPropTypes, packData, log);
                log.push_back("piece (" + std::to_string(x) + ", " + std::to_string(y) + "): X/Y's a/0/4/1 member " + std::to_string((*xy)[2]));
            }
            else piece = BuildTownPiece(layout, to, pieceArchive, areaArchive, budget, x, y, line, packData, log);
            const size_t index = AppendMember(newPieces, pieceArchive, to.TargetPiece, piece, "GR");
            matrix.Piece(x, y) = (uint16_t)index;
            snprintf(line, sizeof line, "piece (%d, %d): a/0/3/9 member %zu, %zu bytes", x, y, index, piece.size());
            log.push_back(line);
            layouts += "piece " + std::to_string(x) + " " + std::to_string(y) + "\n" + layout.Text();
        }
    const Bytes matrixData = matrix.Write();
    if (OrasMatrix::Read(matrixData).Write() != matrixData) throw FormatError("the new matrix does not read back identical");
    const size_t matrixIndex = AppendMember(newMatrices, matrixArchive, matrixTemplate, matrixData, "MM");
    snprintf(line, sizeof line, "matrix: a/0/4/0 member %zu, %d x %d pieces, %zu zone(s) on its blocks", matrixIndex, d.Width, d.Height, d.Zones.size());
    log.push_back(line);

    // the place names (Navi-Map, the name shown on entering): header word 14 & 0x3FF is a line of text file 90 (Zone_GetLocationNameId
    // 0x4D86EC; checked: zone 6 and its house 223 both 170, line 170 "Bourg-en-Vol" in a/0/7/4, "Littleroot Town" in a/0/7/3).
    // Each name is added as a new line in the eight languages' files (a/0/7/1-a/0/7/8), the same text in all
    std::vector<int> nameLine(d.Zones.size(), -1);
    for (size_t k = 0; k < d.Zones.size(); k++)
        if (!d.Zones[k].Name.empty()) nameLine[k] = ws.AddPlaceName(d.Zones[k].Name);

    // the sandbox's own area packs: the game's 9 placeholder packs no zone uses (0, 1, 39-42, 88, 97, 195: twelve small files each)
    // are filled, never a pack a game zone draws from, so no Hoenn place changes: the first with the shared pack the pieces were
    // built for (the first zone's pack and the textures they show), the next ones with that pack's copy under another light.
    // A pack appended past the game's 229 is refused: its zone sent the game into its fatal-error loop even with a/1/3/7 grown
    // to match (runs light1-light4; a/1/3/7 is one cause, read under the debugger in runs apk1-apk4, another is not found)
    // its characters' list: the shared pack's, with every model the new zones show added (OrasWorkspace::CharacterList)
    std::vector<int> models;
    for (const SandboxZone& z : d.Zones)
        for (const auto& c : z.Characters) if (std::find(models.begin(), models.end(), c[0]) == models.end()) models.push_back(c[0]);
    auto takeFree = [&](const Bytes& data, const std::string& what) {
        const uint16_t slot = (uint16_t)ws.AddAreaPack(data, pack, models);
        snprintf(line, sizeof line, "area pack %u (unused by the game): %s", slot, what.c_str());
        log.push_back(line);
        return slot;
    };
    const uint16_t ownPack = takeFree(packData, "the sandbox's pack, a copy of pack " + std::to_string(pack) + " with the pieces' textures");
    // a zone given `lighting` or `camera` gets a copy of that pack with them, one copy per different pair (free slots are few)
    // `lighting`: the area pack's file 4 of game zone `lighting`. File 4 is 2,944 bytes of floats (0.1, 0.4, 1.0...; 6 different
    // ones among the game's 229 packs), copied into a graphics object's buffer when the field is set up (Field_SceneSetup_AddModels
    // 0x102DDC84, Res_CopyToCachedBuffer 0x48E390; battles fill it from a/0/5/9). What reads it is not found, and an interior's
    // (zone 216's, run light5) changed nothing seen outdoors: it is not the scene's light (ORAS_ENGINE.md 2)
    // `camera`: settings of preset 0 of the pack's file 6, a table of 0x44-byte presets from offset 0 (Zone_ApplyZonePackData
    // 0x102E22D0 copies preset N, N the low byte of a u16 at +0xE of the area pack's loader, 0 when past the file's end; it was
    // 0 in Littleroot, run cam10, and the sandbox's zones use preset 0, runs cam0 and cam3; where N comes from is not found)
    std::map<std::tuple<int, std::map<uint32_t, float>, std::map<uint32_t, float>>, uint16_t> zonePacks;
    std::vector<uint16_t> packOf(d.Zones.size(), ownPack);
    for (size_t k = 0; k < d.Zones.size(); k++)
    {
        const SandboxZone& z = d.Zones[k];
        if (z.Lighting < 0 && z.Camera.empty() && z.Light.empty()) continue;
        const auto key = std::make_tuple(z.Lighting, z.Camera, z.Light);
        if (const auto found = zonePacks.find(key); found != zonePacks.end()) { packOf[k] = found->second; continue; }
        BinLinker copy = BinLinker::Read(packData, "AD");
        std::string what = "zone " + std::to_string(z.Number) + "'s pack, a copy of the sandbox's with";
        if (z.Lighting >= 0)
        {
            const int from = OrasZone::Read(Plain(zoneArchive.Sub((size_t)z.Lighting))).AreaPack();
            const Bytes light = BinLinker::Read(Plain(areaArchive.Sub((size_t)from)), "AD").Files.at(4);
            if (copy.Files.size() < 5 || light.size() != copy.Files[4].size()) throw FormatError("sandbox: an area pack's file 4 is not the size of the shared pack's");
            copy.Files[4] = light;
            what += " the file 4 of zone " + std::to_string(z.Lighting) + " (pack " + std::to_string(from) + ")";
        }
        if (!z.Camera.empty())
        {
            Bytes& presets = copy.Files.at(6);
            for (const auto& [at, value] : z.Camera)
            {
                if (at + 4 > presets.size()) throw FormatError("sandbox: the pack's camera presets (file 6) are too short");
                uint32_t bits; std::memcpy(&bits, &value, 4);
                for (int b = 0; b < 4; b++) presets[at + b] = (uint8_t)(bits >> (8 * b));
            }
            what += std::string(z.Lighting >= 0 ? " and" : "") + " its camera";
        }
        if (!z.Light.empty())
        {
            Bytes& light = copy.Files.at(4);
            for (const auto& [at, value] : z.Light)
            {
                if (at + 4 > light.size()) throw FormatError("sandbox: the pack's file 4 is too short for the light");
                uint32_t bits; std::memcpy(&bits, &value, 4);
                for (int b = 0; b < 4; b++) light[at + b] = (uint8_t)(bits >> (8 * b));
            }
            what += std::string(z.Lighting >= 0 || !z.Camera.empty() ? " and" : "") + " its light";
        }
        packOf[k] = zonePacks[key] = takeFree(copy.Write(), what);
    }

    // the zones, each written from nothing (OrasNewZone.h): the new matrix, its area pack, its own text file, its characters
    // and doors, its scripts, the spawn tile and the name
    // every 'D' of the map is one zone's door; each door's house inside is a new zone after the described ones on a game
    // interior's map (its matrix and area pack: the game's assets), its one warp leading back out to the door
    size_t doorsGiven = 0, doorsDrawn = 0;
    for (const SandboxZone& z : d.Zones) for (const auto& door : z.Doors) doorsGiven += door[3] ? 0 : 1; // an X/Y door is X's model, no 'D'
    for (const std::string& row : d.Map) doorsDrawn += (size_t)std::count(row.begin(), row.end(), 'D');
    if (doorsGiven != doorsDrawn) throw FormatError("sandbox: the map has " + std::to_string(doorsDrawn) + " doors ('D'), the zones give " + std::to_string(doorsGiven));
    struct Inside { int Zone, Map, Outside, Warp; size_t OutsideAt; bool Xy, Extra; }; // Extra: an X/Y floor reached by stairs
    std::vector<Inside> insides;
    int nextZone = d.Zones.back().Number + 1;
    for (size_t k = 0; k < d.Zones.size(); k++)
        for (size_t w = 0; w < d.Zones[k].Doors.size(); w++)
            insides.push_back({nextZone++, d.Zones[k].Doors[w][2], d.Zones[k].Number, (int)w, k, d.Zones[k].Doors[w][3] != 0, false});
    // an X/Y inside brings the X/Y zones its other warps lead to (a house's upper floor, by its stairs: X's zones 4-5, 6-7), each a
    // zone of its own whose warps keep X's, their destinations renumbered (xyZoneOf: X/Y zone -> its new zone)
    std::map<int, int> xyZoneOf;
    std::map<int, size_t> xyWarpCount; // X/Y zone -> its warps in X/Y (an xy-stairs warp comes after them)
    for (const Inside& in : insides) if (in.Xy) xyZoneOf[in.Map] = in.Zone;
    for (size_t i = 0; i < insides.size(); i++)
    {
        if (!insides[i].Xy) continue;
        if (ws.XyRom.empty()) throw FormatError("sandbox: door ... xy needs oras-sandbox --xy <x.3ds>");
        N3dsRom xRom(ws.XyRom);
        const Garc xZones(xRom.Read("a/0/1/2"));
        const Bytes events = BinLinker::Read(Plain(xZones.Sub((size_t)insides[i].Map)), "ZO").Files.at(1);
        const size_t first = 12 + 0x14 * (size_t)events.at(4) + 0x30 * (size_t)events.at(5);
        // a door's inside keeps its warp 0, its way out, for the sandbox's own (NewExitWarp); a floor reached by stairs, every warp
        for (size_t k = insides[i].Extra ? 0 : 1; k < events.at(6); k++)
        {
            const int dest = U16(events, first + 0x18 * k);
            if (xyZoneOf.count(dest)) continue;
            xyZoneOf[dest] = nextZone;
            Inside extra = insides[i];
            extra.Zone = nextZone++; extra.Map = dest; extra.Warp = -1; extra.Extra = true;
            insides.push_back(extra);
        }
        xyWarpCount[insides[i].Map] = events.at(6);
        // the floor of stairs X/Y runs by script (xy-stairs), a zone of its own as a floor its warps reach
        for (const std::array<int, 12>& st : d.XyStairs)
        {
            if (st[0] != insides[i].Map || xyZoneOf.count(st[6])) continue;
            xyZoneOf[st[6]] = nextZone;
            Inside extra = insides[i];
            extra.Zone = nextZone++; extra.Map = st[6]; extra.Warp = -1; extra.Extra = true;
            insides.push_back(extra);
        }
    }
    for (const std::array<int, 12>& st : d.XyStairs)
        if (!xyZoneOf.count(st[0]) || !xyZoneOf.count(st[6]))
            throw FormatError("sandbox: xy-stairs from X/Y zone " + std::to_string(st[0]) + ", which no door ... xy leads into");

    // the X/Y zones taken (xy-events and the insides of door ... xy): their story numbers moved together (XyNumbersFor), as
    // remake_tool xy-script-export numbers them for the same zones, so its files and these events agree
    std::set<int> xyZones;
    for (const SandboxZone& z : d.Zones) if (z.XyEvents >= 0) xyZones.insert(z.XyEvents);
    for (const Inside& in : insides) if (in.Xy) xyZones.insert(in.Map);
    const std::filesystem::path descFile(descriptionPath);
    const std::string xyNames = descriptionPath.empty() ? "" : (descFile.parent_path() / ".." / "ghidra" / "function_names.tsv").string();
    const std::string xyScripts = descriptionPath.empty() ? "" : (descFile.parent_path() / descFile.stem()).string();
    XyNumbers xyNumbers;
    std::map<int, int> xyModelOf;
    int xyDx = 0, xyDz = 0; // X/Y's matrix tiles to the sandbox's, for an outdoor zone's events (xy-events)
    if (!xyZones.empty())
    {
        if (xyNames.empty() || !std::filesystem::exists(xyNames))
            throw FormatError("sandbox: X/Y zones' events and scripts need ghidra/function_names.tsv beside the description's folder");
        N3dsRom xRom(ws.XyRom);
        const Garc xZones(xRom.Read("a/0/1/2"));
        std::vector<Bytes> scripts;
        std::set<int> vanish, conditions;
        for (const int z : xyZones)
        {
            const OrasZone o = OrasZone::Read(Plain(xZones.Sub((size_t)z)));
            for (const ZoneCharacter& c : o.Characters) if (c.Raw[4]) vanish.insert(c.Raw[4]);
            for (const ZoneTrigger& t : o.Triggers) conditions.insert(t.Raw[2]);
            for (const Bytes* s : {&o.Script, &o.InitScript})
                if (s->size() >= 16) scripts.push_back(XyScriptToOras(*s, xyNames, 0x243).Script);
        }
        xyNumbers = XyNumbersFor(scripts, vanish, conditions);
        log.push_back("X/Y zones' story numbers: " + std::to_string(xyNumbers.Flags.size()) + " flag(s) from 0xC09, " + std::to_string(xyNumbers.Works.size()) +
                      " work(s) from 0x4100 (XyNumbersFor)");
        if (!d.XyPieces.empty())
        {
            // the first xy-piece's cell in X/Y's matrix (its file 4: row, column) against its cell here
            const BinLinker piece = BinLinker::Read(Plain(Garc(xRom.Read("a/0/4/1")).Sub((size_t)d.XyPieces[0][2])), "GR");
            xyDx = (d.XyPieces[0][0] - piece.Files.at(4).at(1)) * 40;
            xyDz = (d.XyPieces[0][1] - piece.Files.at(4).at(0)) * 40;
        }
    }

    // each new zone's text file (header word 3): a new member of the eight story text archives (a/0/7/9-a/0/8/6, one per
    // language; ORAS_ENGINE.md 6), the zone's lines in all eight, one empty line when it gives none
    const size_t zoneCount = d.Zones.size() + insides.size();
    std::vector<int> textOf(zoneCount, -1);
    for (size_t k = 0; k < zoneCount; k++)
        textOf[k] = ws.AddZoneText(k < d.Zones.size() && !d.Zones[k].Lines.empty() ? d.Zones[k].Lines : std::vector<std::string>{""});

    // a zone's scripts, assembled: its own (script / init-script), or ones that do nothing (OrasNewZone.h)
    const auto assemble = [&](const std::string& source, int number, const char* what) {
        if (!checkNative) throw FormatError("sandbox: zone " + std::to_string(number) + "'s " + what + ": no native tables to check it against");
        return AmxAssemble(source, checkNative);
    };
    const auto appendZone = [&](const OrasZone& zone, const Bytes& encounter, int number) {
        const int added = ws.AddZone(WriteNewZone(zone, encounter), encounter);
        if (added != number) throw FormatError("zone " + std::to_string(number) + " was appended as member " + std::to_string(added));
    };

    size_t insideAt = 0;
    // shared characters: each listed in both its zones' files, with the two zones at record +0x1C/+0x1E and one id at +0x22,
    // so Event_ReleaseCharactersNotInZone (0x3F6DF0) keeps it across the border and Event_CharacterPool_Acquire (0x3F54E8)
    // rebinds it to the other zone's record instead of placing a second one (read; checked headless, runs hSh and hFar2)
    std::vector<SandboxZone> zones = d.Zones;
    int sharedId = 0x200;
    for (size_t k = 0; k < d.Zones.size(); k++)
        for (size_t i = 0; i < d.Zones[k].Characters.size(); i++)
        {
            if (d.Zones[k].Shared[i][1] < 0) continue;
            const int other = d.Zones[k].Shared[i][1];
            auto to = std::find_if(zones.begin(), zones.end(), [&](const SandboxZone& o) { return o.Number == other; });
            if (to == zones.end() || other == d.Zones[k].Number)
                throw FormatError("sandbox: zone " + std::to_string(d.Zones[k].Number) + ": share " + std::to_string(other) + " is not another zone of the description");
            zones[k].Shared[i] = {d.Zones[k].Number, other, sharedId};
            to->Characters.push_back(d.Zones[k].Characters[i]);
            to->Shared.push_back({d.Zones[k].Number, other, sharedId});
            sharedId++;
        }
    for (size_t k = 0; k < d.Zones.size(); k++)
    {
        const SandboxZone z = WithSilentCharacters(zones[k]);
        OrasZone zone;
        NewZoneFields f;
        f.Number = z.Number; f.AreaPack = packOf[k]; f.Matrix = (int)matrixIndex; f.Text = textOf[k];
        f.NameLine = nameLine[k]; f.Music = z.Music; f.SpawnX = z.SpawnX; f.SpawnZ = z.SpawnZ;
        XyEvents xe;
        if (z.XyEvents >= 0)
        {
            xe = XyEventsToOras(ws, z.XyEvents, xyDx, xyDz, xyNumbers, xyModelOf, xyNames, xyScripts, z.Number, log);
            f.Text = xe.Text;
        }
        zone.Header = NewZoneHeader(f);
        for (size_t i = 0; i < z.Characters.size(); i++)
        {
            const auto& c = z.Characters[i];
            zone.Characters.push_back(NewCharacter((int)zone.Characters.size(), c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[6] == 1 ? c[7] : 0));
            if (z.Shared[i][1] >= 0)
            {
                ZoneCharacter& s = zone.Characters.back();
                s.Raw[14] = (uint16_t)z.Shared[i][0]; s.Raw[15] = (uint16_t)z.Shared[i][1]; s.Raw[17] = (uint16_t)z.Shared[i][2];
                // +0x20 names the zone whose file lists this record: Acquire compares the loaded character's +0x52 (record
                // +0x1C) with the zone entered when +0x1C == +0x20, else its +0x54 (+0x1E). Left 0, re-entering the owner
                // zone found no match and loaded every shared character again until the pool's fatal (run hFar, 17 October)
                s.Raw[16] = (uint16_t)z.Number;
            }
            // a swimmer has no standing motion and the map's water is solid, so it always stands on land here
            if (ws.Swims(c[0]))
            {
                snprintf(line, sizeof line, "warning: zone %d: model %d at tile (%d, %d) is a swimmer, drawn swimming half in the ground on land (OrasWorkspace::Swims)", z.Number, c[0], c[1], c[2]);
                log.push_back(line);
            }
        }
        for (const auto& door : z.Doors)
        {
            zone.Doors.push_back(NewDoorWarp(insides.at(insideAt++).Zone, 0, door[0], door[1]));
            zone.Doors.back().Raw[5] = (uint16_t)door[4];
        }
        for (const auto& t : z.Triggers)
        {
            // a trigger's 12 words, as the game's 361 hold them: 0 the event number, 1-2 a condition (work variable word 2
            // equal to word 1; 0 in word 2, as 134 of the game's have it, is none: inferred), 6-7 the tile, 8-9 its size
            // (width, height); the rest 0 (word 3 is 0, 5, 6, 7 or 8 in the game's, word 10 0 or a multiple of 9: not read)
            ZoneTrigger tr;
            tr.Raw[0] = (uint16_t)t[4]; tr.Raw[6] = (uint16_t)t[0]; tr.Raw[7] = (uint16_t)t[1];
            tr.Raw[8] = (uint16_t)t[2]; tr.Raw[9] = (uint16_t)t[3];
            zone.Triggers.push_back(tr);
        }
        if (z.XyEvents >= 0)
        {
            // X/Y's people keep their ids, which its scripts name: none given here beside them
            if (!zone.Characters.empty()) throw FormatError("sandbox: zone " + std::to_string(z.Number) + " takes X/Y's people (xy-events): give it no character");
            zone.Characters = xe.Characters;
            zone.Furniture.insert(zone.Furniture.end(), xe.Furniture.begin(), xe.Furniture.end());
            zone.Triggers.insert(zone.Triggers.end(), xe.Triggers.begin(), xe.Triggers.end());
            ws.ListCharacters(f.AreaPack, xe.Models);
        }
        if (z.XyEvents >= 0 && z.Scene.empty() && z.Script.empty()) zone.Script = xe.Script;
        else if (!z.Scene.empty())
        {
            // the scene on Littleroot's zone script (zone 6), whose routines it calls (SceneScript.h)
            std::string scene;
            try { scene = CompileScene(z.Scene, AmxSource(OrasZone::Read(Plain(zoneArchive.Sub(6))).Script)); }
            catch (const FormatError& e) { throw FormatError("sandbox: zone " + std::to_string(z.Number) + "'s " + e.what()); }
            zone.Script = assemble(scene, z.Number, "scene");
        }
        else zone.Script = assemble(z.Script.empty() ? EmptyZoneScript : z.Script, z.Number, "script");
        zone.InitScript = z.XyEvents >= 0 && z.InitScript.empty() ? xe.InitScript : assemble(z.InitScript.empty() ? EmptyInitScript : z.InitScript, z.Number, "init-script");
        const Bytes encounter = z.Encounters >= 0 ? BinLinker::Read(Plain(zoneArchive.Sub((size_t)z.Encounters)), "ZO").Files.at(3) : Bytes{};
        CheckCharacterCount(ws, z.Number, zone.Characters.size());
        appendZone(zone, encounter, z.Number);
        snprintf(line, sizeof line, "zone %d: written from nothing (OrasNewZone.h), area pack %d, text member %d, %zu character(s), %s scripts, spawn (%.1f, %.1f), name %s, encounters %s",
                 z.Number, f.AreaPack, f.Text, zone.Characters.size(), !z.Scene.empty() ? "a scene's" : z.Script.empty() && z.InitScript.empty() ? "empty" : "its own", z.SpawnX, z.SpawnZ,
                 nameLine[k] >= 0 ? ("\"" + z.Name + "\" (line " + std::to_string(nameLine[k]) + ")").c_str() : "none",
                 z.Encounters >= 0 ? ("of zone " + std::to_string(z.Encounters)).c_str() : "none");
        log.push_back(line);
    }
    for (size_t i = 0; i < insides.size(); i++)
    {
        // the inside: the game interior's map (its matrix and area pack) and the tile of its way out (its warp 0, a mat of kind 0:
        // where this map's exit is), read from the game zone on that map; nothing else of it
        const Inside& in = insides[i];
        if (in.Xy)
        {
            // a stairs' warp, as X/Y's own between floors (houses 2 and 3 of Vaniville, X's zones 4-5: kind 0x5WW, WW the way it
            // is walked into, 0 down, 1 up, 2 right, 3 left, read from X's mats, zone 77: 0x902 on its right edge, 0x903 on its
            // left, 0x901 on its top; height 9 going up, -9 down; pixels at the first tile's centre, span in tiles): the stairs'
            // tiles and way, its destination and the destination's warp (X/Y's warps there, then this one: so none of them may
            // be left out). With the way wrong the game set the player out on the wrong side (owner's phone, 18 October: the
            // middle house's upper floor, its stairs walked into rightward, had 0x503)
            const auto stairsWarp = [&](const int* rect, int way, int toXy, bool up) {
                ZoneDoor w;
                w.Raw = {};
                w.Raw[0] = (uint16_t)xyZoneOf.at(toXy);
                w.Raw[1] = (uint16_t)xyWarpCount[toXy];
                w.Raw[2] = (uint16_t)(0x0500 | way);
                w.Raw[4] = (uint16_t)(rect[0] * 18 + 9);
                w.Raw[5] = (uint16_t)(int16_t)(up ? 9 : -9);
                w.Raw[6] = (uint16_t)(rect[1] * 18 + 9);
                w.Raw[7] = (uint16_t)rect[2];
                w.Raw[8] = (uint16_t)rect[3];
                return w;
            };
            std::array<uint16_t, 2> floorAt{};
            bool scriptFloor = false;
            for (const std::array<int, 12>& st : d.XyStairs)
                if (st[6] == in.Map) { floorAt = {(uint16_t)(st[7] * 18 + 9), (uint16_t)(st[8] * 18 + 9)}; scriptFloor = true; }
            const XyInside x = XyInsideToOras(ws, in.Map, models, pieceArchive, zoneArchive, log, scriptFloor ? &floorAt : nullptr);
            OrasZone zone;
            NewZoneFields f;
            f.Kind = NewZoneKind::Interior; f.Number = in.Zone; f.AreaPack = x.AreaPack; f.Matrix = x.Matrix;
            f.Text = textOf[d.Zones.size() + i]; f.Overworld = in.Outside; f.NameLine = nameLine[in.OutsideAt];
            f.SpawnX = x.ExitX / 18.0f; f.SpawnZ = x.ExitZ / 18.0f;
            // its people, objects, triggers, text and scripts, X/Y's (an inside is on its own matrix: no shift)
            const XyEvents xe = XyEventsToOras(ws, in.Map, 0, 0, xyNumbers, xyModelOf, xyNames, xyScripts, in.Zone, log);
            f.Text = xe.Text;
            zone.Header = NewZoneHeader(f);
            zone.Furniture = xe.Furniture;
            zone.Characters = xe.Characters;
            zone.Triggers = xe.Triggers;
            ws.ListCharacters(f.AreaPack, xe.Models);
            for (size_t k = 0; k < x.Warps.size(); k++)
            {
                if (k == 0 && !in.Extra) { zone.Doors.push_back(NewExitWarp(in.Outside, in.Warp, x.ExitX, x.ExitZ)); continue; }
                // X's warp as it is (kind, position, height, span), into the zone made of its destination; the arrival warp index
                // is X's (each zone keeps its warps in X's order). A warp out of town (to the outdoor zone) from a floor: not kept
                const auto to = xyZoneOf.find(x.Warps[k][0]);
                const bool stairsZone = std::any_of(d.XyStairs.begin(), d.XyStairs.end(), [&](const std::array<int, 12>& st) { return st[0] == in.Map || st[6] == in.Map; });
                if (to == xyZoneOf.end() && stairsZone)
                    throw FormatError("sandbox: X/Y zone " + std::to_string(in.Map) + " of an xy-stairs has a warp left out: its stairs warp's index would be wrong");
                if (to == xyZoneOf.end()) { log.push_back("zone " + std::to_string(in.Zone) + ": X's warp " + std::to_string(k) + " to X zone " + std::to_string(x.Warps[k][0]) + " left out"); continue; }
                ZoneDoor w;
                w.Raw = x.Warps[k];
                w.Raw[0] = (uint16_t)to->second;
                zone.Doors.push_back(w);
            }
            for (const std::array<int, 12>& st : d.XyStairs)
            {
                if (st[0] == in.Map) zone.Doors.push_back(stairsWarp(&st[1], st[5], st[6], true));
                if (st[6] == in.Map) zone.Doors.push_back(stairsWarp(&st[7], st[11], st[0], false));
                if (st[0] == in.Map || st[6] == in.Map)
                    log.push_back("zone " + std::to_string(in.Zone) + ": stairs " + (st[0] == in.Map ? "up" : "down") + " to zone " +
                                  std::to_string(xyZoneOf.at(st[0] == in.Map ? st[6] : st[0])) + " (xy-stairs)");
            }
            zone.Script = xe.Script;
            zone.InitScript = xe.InitScript;
            appendZone(zone, Bytes{}, in.Zone);
            if (in.Extra) snprintf(line, sizeof line, "zone %d: X/Y's zone %d (matrix %d, area pack %d), a floor reached by its warps, %zu warp(s)",
                                   in.Zone, in.Map, f.Matrix, f.AreaPack, zone.Doors.size());
            else snprintf(line, sizeof line, "zone %d: the inside of zone %d's door %d, made of X/Y's zone %d (matrix %d, area pack %d), warp 0 back out, %zu warp(s)",
                          in.Zone, in.Outside, in.Warp, in.Map, f.Matrix, f.AreaPack, zone.Doors.size());
            log.push_back(line);
            continue;
        }
        const OrasZone map = OrasZone::Read(Plain(zoneArchive.Sub((size_t)in.Map)));
        if (map.Doors.empty() || map.Doors[0].Kind() != 0)
            throw FormatError("sandbox: zone " + std::to_string(in.Map) + " is not a house's inside (its warp 0 is not a way out, kind 0)");
        OrasZone zone;
        NewZoneFields f;
        f.Kind = NewZoneKind::Interior; f.Number = in.Zone; f.AreaPack = map.AreaPack(); f.Matrix = map.Matrix();
        f.Text = textOf[d.Zones.size() + i]; f.Overworld = in.Outside; f.NameLine = nameLine[in.OutsideAt];
        f.SpawnX = map.Doors[0].Raw[4] / 18.0f; f.SpawnZ = map.Doors[0].Raw[6] / 18.0f;
        zone.Header = NewZoneHeader(f);
        zone.Doors.push_back(NewExitWarp(in.Outside, in.Warp, map.Doors[0].Raw[4], map.Doors[0].Raw[6]));
        zone.Script = assemble(EmptyZoneScript, in.Zone, "script");
        zone.InitScript = assemble(EmptyInitScript, in.Zone, "init-script");
        appendZone(zone, Bytes{}, in.Zone);
        snprintf(line, sizeof line, "zone %d: the inside of zone %d's door %d, written from nothing on game zone %d's map (matrix %d, area pack %d), warp 0 back out",
                 in.Zone, in.Outside, in.Warp, in.Map, f.Matrix, f.AreaPack);
        log.push_back(line);
    }

    // the mod itself is written once by the build (OrasWorkspace::Write)
    const std::filesystem::path out(outDir);
    std::filesystem::create_directories(out);
    WriteFile((out / "sandbox_layout.txt").string(), Bytes(layouts.begin(), layouts.end()));
    return log;
}

}
