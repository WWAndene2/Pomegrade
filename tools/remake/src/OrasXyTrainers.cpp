#include "OrasXyTrainers.h"

#include "Bch.h"
#include "BchUpgrade.h"
#include "Darc.h"
#include "GameText.h"
#include "Garc.h"
#include "N3dsRom.h"
#include "NitroCompression.h"
#include "OrasXySequence.h"
#include "OrasXySound.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>

namespace remake
{

namespace
{
Bytes Plain(const Bytes& data) { return IsLzCompressed(data) ? LzDecompress(data) : data; }
// a text member's file written back as the archive held it (compressed or not)
Bytes AsWas(const Bytes& was, const Bytes& file) { return IsLzCompressed(was) ? Lz11Compress(file) : file; }
int Picture(const Bytes& classRecord) { return classRecord.at(4) | classRecord.at(5) << 8; }

// a battle layout's resource member (a/0/3/3, a/2/1/4: read 22 October on 7 of them in both games): u32 count, that many
// file names in 64 bytes each (the DARC's files, in its order), u32 count and names in 32 bytes each (the layout's
// panes), zeros to a multiple of 0x80, then the DARC
struct LayoutResource { std::vector<std::string> Files; Bytes Panes; std::vector<DarcFile> Darc; };
LayoutResource ReadLayoutResource(const Bytes& b)
{
    LayoutResource r;
    const uint32_t files = U32(b, 0);
    for (uint32_t i = 0; i < files; i++) r.Files.push_back(std::string((const char*)&b.at(4 + 64 * i)));
    const size_t panes = 4 + 64 * (size_t)files, panesEnd = panes + 4 + 32 * (size_t)U32(b, panes);
    r.Panes.assign(b.begin() + panes, b.begin() + panesEnd);
    const size_t darc = (panesEnd + 0x7F) & ~(size_t)0x7F;
    if (b.size() < darc + 4 || std::string(b.begin() + darc, b.begin() + darc + 4) != "darc") throw FormatError("a battle layout resource: no DARC where its names end");
    r.Darc = ReadDarc(Bytes(b.begin() + darc, b.end()));
    if (r.Darc.size() != files) throw FormatError("a battle layout resource: its names do not count its DARC's files");
    return r;
}
Bytes WriteLayoutResource(const LayoutResource& r)
{
    Bytes out(4 + 64 * r.Files.size(), 0);
    for (int k = 0; k < 4; k++) out[k] = (uint8_t)(r.Files.size() >> (8 * k));
    for (size_t i = 0; i < r.Files.size(); i++)
    {
        if (r.Files[i].size() >= 64) throw FormatError("a battle layout resource: a file name past 63 characters");
        std::copy(r.Files[i].begin(), r.Files[i].end(), out.begin() + 4 + 64 * i);
    }
    out.insert(out.end(), r.Panes.begin(), r.Panes.end());
    out.resize((out.size() + 0x7F) & ~(size_t)0x7F, 0);
    const Bytes darc = WriteDarc(r.Darc);
    out.insert(out.end(), darc.begin(), darc.end());
    return out;
}
}

// X/Y's battle cameras in Aim view made LookAt, as Omega Ruby remade its own (read 23 October, ORAS_ENGINE.md 0.1): Omega
// Ruby's wild-intro camera setup (DllBattle FUN_1005C418) keeps the camera's up vector (+0x59 of its camera object) and
// fills it only for a LookAt view (FUN_004CB6A4, the view type; FUN_004CB63C, the up vector); X/Y's (FUN_10058438) has
// neither, and its intro camera (a/0/3/4 513, poke_y_intro_01) is an Aim view, so in Omega Ruby the up vector stays unset
// and the intro is black. Omega Ruby's own version of it (a/0/3/2 1016, poke_y_intro_01_CAM) is LookAt. An Aim view
// whose twist is 0 and not animated (no ViewUpdater.Twist element) is exactly a LookAt view with the up vector (0, 1, 0):
// the view is appended in that form (32 bytes after the file) and the camera pointed at it; the old view is left unused.
// CGFX layout (SPICA's CtrGfx, checked on 513, 874, 1016): DATA's 16 (count, DICT) pairs from 0x1C, cameras the 6th;
// a camera's view type at +0xB4 (0 Aim, 1 LookAt), its view at +0xBC (self-relative); an Aim view {0x80000000, flags,
// target xyz, twist}, a LookAt view {0x40000000, flags, target xyz, up xyz}; file size at 0x0C, DATA's at 0x18
static Bytes AimCamerasAsLookAt(const Bytes& file)
{
    if (file.size() < 0x9C || std::string((const char*)file.data(), 4) != "CGFX") return file;
    Bytes d = file;
    const auto u32 = [&](size_t at) { return (uint32_t)d.at(at) | (uint32_t)d.at(at + 1) << 8 | (uint32_t)d.at(at + 2) << 16 | (uint32_t)d.at(at + 3) << 24; };
    const auto set = [&](size_t at, uint32_t v) { for (int b = 0; b < 4; b++) d.at(at + b) = (uint8_t)(v >> (8 * b)); };
    const auto f32 = [&](size_t at) { uint32_t v = u32(at); float f; memcpy(&f, &v, 4); return f; };
    const auto dict = [&](int section) {
        std::vector<size_t> objects;
        const size_t pair = 0x1C + 8 * (size_t)section;
        if (!u32(pair)) return objects;
        const size_t at = pair + 4 + u32(pair + 4);
        if (std::string((const char*)&d.at(at), 4) != "DICT") throw FormatError("a CGFX camera file: no DICT");
        for (uint32_t i = 0; i < u32(at + 8); i++) { const size_t e = at + 0x1C + 16 * i + 12; objects.push_back(e + u32(e)); }
        return objects;
    };
    // a twist animated by one of the file's camera animations (an element named so; the name alone is in every camera
    // file's strings): not this simple case. An animation's elements: a DICT at +0x1C (self-relative), names at its nodes' +8
    for (const size_t animation : dict(12))
    {
        const size_t elements = animation + 0x1C + u32(animation + 0x1C);
        for (uint32_t i = 0; i < u32(elements + 8); i++)
        {
            const size_t node = elements + 0x1C + 16 * i + 8, name = node + u32(node);
            if (std::string((const char*)&d.at(name)) == "ViewUpdater.Twist") return file;
        }
    }
    for (const size_t camera : dict(5))
    {
        if (u32(camera + 0xB4) != 0) continue;
        const size_t view = camera + 0xBC + u32(camera + 0xBC);
        if (u32(view) != 0x80000000) throw FormatError("a CGFX camera: view type 0 but not an Aim view");
        if (f32(view + 20) != 0.0f) return file; // a twist: not this simple case
        const uint32_t flags = u32(view + 4), lookAtFlags = (flags & 1) << 1 | (flags & 2) << 1; // inherit target rotation, translation
        while (d.size() % 4) d.push_back(0);
        const size_t at = d.size();
        const float up[3] = {0.0f, 1.0f, 0.0f};
        d.resize(at + 32);
        set(at, 0x40000000);
        set(at + 4, lookAtFlags);
        memcpy(&d[at + 8], &d[view + 8], 12);
        memcpy(&d[at + 20], up, 12);
        set(camera + 0xB4, 1);
        set(camera + 0xBC, (uint32_t)(at - (camera + 0xBC)));
    }
    set(0x0C, (uint32_t)d.size());
    set(0x18, (uint32_t)d.size() - 0x14);
    return d;
}

std::vector<std::string> AddXyTrainers(OrasWorkspace& ws, const XySounds* music, bool streams, Bytes* battleTypes, Bytes* intros,
                                       std::pair<uint32_t, uint32_t>* bandCamera)
{
    std::vector<std::string> log;
    char line[192];
    N3dsRom xy(ws.XyRom);
    const Garc xRecords(xy.Read("a/0/3/8")), xTeams(xy.Read("a/0/4/0")), xClasses(xy.Read("a/0/3/9"));
    const Garc& oClasses = ws.Original("a/0/3/7");
    Garc& records = ws.Edited("a/0/3/6");
    Garc& teams = ws.Edited("a/0/3/8");
    if (xRecords.Count() != (size_t)XyTrainerCount || xTeams.Count() != (size_t)XyTrainerCount)
        throw FormatError("X/Y trainers: a/0/3/8 and a/0/4/0 do not hold " + std::to_string(XyTrainerCount) + " members");
    if (records.Count() != (size_t)XyTrainerFirst || teams.Count() != (size_t)XyTrainerFirst)
        throw FormatError("X/Y trainers: Omega Ruby's a/0/3/6 and a/0/3/8 do not hold " + std::to_string(XyTrainerFirst) + " members (added to already?)");

    // X/Y's 178 trainer classes, ported whole (owner, 22 October): appended to a/0/3/7 after Omega Ruby's 280, X/Y's class c as
    // 280 + c (oras-engine --xy-trainers makes the game load and accept 460: Trainer_ReadClass and the class table, read 22
    // October). A class is 16 bytes in both games, read into a structure after the class's number (+0), so the record's +n
    // is the structure's +4 + n: +2 the battle type (made X/Y's own, 73 + X/Y's, below); +4 a group of three drawings
    // (small, large, silhouette; Trainer_Class_GetIndexedEntry 0x4E79A4 reads the structure's +8: X/Y's a/1/7/8 members
    // 3n..3n+2, Omega Ruby's a/1/6/0, kind 0xA0 in a battle sequence; 79 of the 109 classes both games share name the same
    // three files), each group with a 28-byte record of the same number (X/Y's a/1/2/7, Omega Ruby's a/1/2/1: 103 of 109
    // the same; 496 of Omega Ruby's 500 a blank one); +8 the same in both games for every shared class, kept. A group the
    // same as one of Omega Ruby's (the three drawings and the record) is Omega Ruby's; else X/Y's is appended, the record
    // written at its number (a blank there). Omega Ruby's 228 groups end at the reader's bound (drawing 0x2AB): oras-engine
    // --xy-trainers raises it to 0x3FF (1024 drawings)
    const std::vector<std::string> xNames = ReadGameText(Plain(Garc(xy.Read("a/0/7/4")).Sub(20)));
    if (xNames.size() != xClasses.Count()) throw FormatError("X/Y trainers: X/Y's class names do not match its class archive");
    constexpr int gameClasses = 280;
    if (oClasses.Count() != (size_t)gameClasses) throw FormatError("X/Y trainers: a/0/3/7 does not hold the game's 280 classes (added to already?)");
    Garc& classes = ws.Edited("a/0/3/7");
    {
        const Garc xRecordsOfGroups(xy.Read("a/1/2/7")), xDrawings(xy.Read("a/1/7/8"));
        Garc& groupRecords = ws.Edited("a/1/2/1");
        Garc& drawings = ws.Edited("a/1/6/0");
        if (drawings.Count() % 3) throw FormatError("X/Y trainers: a/1/6/0 is not in groups of three");
        std::vector<Bytes> gameDrawings, gameRecords;
        for (size_t i = 0; i < drawings.Count(); i++) gameDrawings.push_back(Plain(drawings.Sub(i)));
        for (size_t i = 0; i < groupRecords.Count(); i++) gameRecords.push_back(Plain(groupRecords.Sub(i)));
        std::map<uint32_t, uint32_t> groupOf;
        size_t sharedGroups = 0, addedGroups = 0;
        const auto group = [&](uint32_t x) {
            // X/Y's own reader (FUN_004AE54C, X's 0x4AE54C) takes a drawing past member 0x158 as the variant alone: a group
            // past 114 shows group 0 in X/Y, and so here
            if (3 * x + 2 > 0x158) x = 0;
            const auto known = groupOf.find(x);
            if (known != groupOf.end()) return known->second;
            if (3 * (size_t)x + 2 >= xDrawings.Count() || x >= xRecordsOfGroups.Count()) throw FormatError("X/Y trainers: X/Y has no drawings " + std::to_string(x));
            Bytes three[3];
            for (int v = 0; v < 3; v++) three[v] = Plain(xDrawings.Sub(3 * (size_t)x + (size_t)v));
            const Bytes record = Plain(xRecordsOfGroups.Sub(x));
            for (size_t g = 0; 3 * g + 2 < gameDrawings.size() && g < gameRecords.size(); g++)
                if (gameDrawings[3 * g] == three[0] && gameDrawings[3 * g + 1] == three[1] && gameDrawings[3 * g + 2] == three[2] && gameRecords[g] == record)
                { sharedGroups++; return groupOf[x] = (uint32_t)g; }
            const uint32_t g = (uint32_t)(drawings.Count() / 3);
            if (g >= gameRecords.size()) throw FormatError("X/Y trainers: a/1/2/1 has no record for drawing group " + std::to_string(g));
            if (gameRecords[g] != gameRecords.back()) throw FormatError("X/Y trainers: a/1/2/1 member " + std::to_string(g) + " is not a blank record");
            for (int v = 0; v < 3; v++) { drawings.Set(drawings.Count(), xDrawings.Sub(3 * (size_t)x + (size_t)v)); gameDrawings.push_back(three[v]); }
            groupRecords.Set(g, AsWas(groupRecords.Sub(g), record));
            gameRecords[g] = record;
            addedGroups++;
            return groupOf[x] = g;
        };
        for (size_t c = 0; c < xClasses.Count(); c++)
        {
            Bytes r = Plain(xClasses.Sub(c));
            if (r.size() != 16) throw FormatError("X/Y trainer class " + std::to_string(c) + ": not 16 bytes");
            const uint32_t g = group((uint32_t)Picture(r));
            r[4] = (uint8_t)g; r[5] = (uint8_t)(g >> 8);
            classes.Set((size_t)gameClasses + c, AsWas(oClasses.Sub(0), r));
        }
        // the game loads every class at boot, as many as oras-engine --xy-trainers makes it count (460: 458 is no ARM
        // immediate): the two past X/Y's are copies of class 0, named by no trainer (with 458 the boot froze on reading a
        // member past the archive, run xc_951)
        while (classes.Count() < 460) classes.Set(classes.Count(), oClasses.Sub(0));
        if (drawings.Count() > 0x400) throw FormatError("X/Y trainers: a/1/6/0 grows past the 1024 drawings oras-engine lets the game read (" + std::to_string(drawings.Count()) + ")");
        snprintf(line, sizeof line, "trainer classes: X/Y's %zu as 280-%zu; drawing groups (a/1/6/0, a/1/2/1): %zu shared, %zu added",
                 xClasses.Count(), (size_t)gameClasses + xClasses.Count() - 1, sharedGroups, addedGroups);
        log.push_back(line);
    }
    // the class names, one line per class in each of the eight languages (Omega Ruby's a/0/7/1-8 member 21, X/Y's a/0/7/2-9
    // member 20), read by class number (Trainer_LoadInfoAndName, message archive 0x15)
    for (int a = 1; a <= 8; a++)
    {
        const std::string path = "a/0/7/" + std::to_string(a);
        Garc& texts = ws.Edited(path);
        const Bytes was = texts.Sub(21);
        Bytes text = Plain(was);
        if (ReadGameText(text).size() != (size_t)gameClasses) throw FormatError(path + " member 21 does not hold 280 class names");
        const std::vector<std::string> names = ReadGameText(Plain(Garc(xy.Read("a/0/7/" + std::to_string(a + 1))).Sub(20)));
        for (size_t c = 0; c < xClasses.Count(); c++) text = AppendGameTextLine(text, c < names.size() ? names[c] : "");
        for (size_t c = 280 + xClasses.Count(); c < 460; c++) text = AppendGameTextLine(text, ReadGameText(Plain(was)).at(0));
        texts.Set(21, AsWas(was, text));
    }

    // X/Y's 64 battle types, ported whole (owner, 22 October: not identical = ported; ORAS_ENGINE.md 0.1): appended after Omega
    // Ruby's 73 (OrasEngine.cpp), X/Y's type t as 73 + t, and each X/Y class given its own. A type is 6 words, copied whole
    // into a battle's settings (+0x2C) in both games by the same code: +0 the encounter transition (DllField's table of 10,
    // the same in both, its effects the same files: X's a/2/6/2 and Omega Ruby's a/2/3/3, read 22 October), +4 the intro, +8
    // 0, +0xC read by no code at a battle's start in either game (runs bc7-bc10), +0x10 the music and +0x14 the victory
    // jingle (made the mod's sound ids). The intro (an index into DllBattle's table of sequences) is X/Y's own sequence,
    // ported below with `intros`; without, the intro word is Omega Ruby's same number (not X/Y's intro)
    if (music && battleTypes)
    {
        constexpr uint32_t xTable = 0x53C010, gameTypes = 73, xTypes = 64;
        const Bytes xCode = xy.Code();
        for (uint32_t t = 0; t < xTypes; t++)
        {
            const size_t xAt = xTable - 0x100000 + 24 * (size_t)t;
            Bytes r(xCode.begin() + (long)xAt, xCode.begin() + (long)xAt + 24);
            for (const size_t w : {0x10u, 0x14u})
            {
                const uint32_t id = U32(r, w);
                if ((id >> 16) != 1) throw FormatError("X/Y battle type " + std::to_string(t) + ": no music id at +" + std::to_string(w));
                // X/Y's own track (a stream appended to the archive, category 0) only with X/Y's music in the mod; else the id
                // as it is, which names Omega Ruby's sound of that number (its own trainer theme for X/Y's 0x10019: the PSS one)
                const uint32_t translated = music->Translate(id), to = (translated >> 16) == 0 && !streams ? id : translated;
                for (int b = 0; b < 4; b++) r[w + b] = (uint8_t)(to >> (8 * b));
            }
            battleTypes->insert(battleTypes->end(), r.begin(), r.end());
        }
        for (size_t c = 0; c < xClasses.Count(); c++)
        {
            const Bytes was = classes.Sub((size_t)gameClasses + c);
            Bytes r = Plain(was);
            r.at(2) = (uint8_t)(gameTypes + Plain(xClasses.Sub(c)).at(2));
            classes.Set((size_t)gameClasses + c, AsWas(was, r));
        }
        snprintf(line, sizeof line, "battle types: X/Y's %u as 73-%u, named by X/Y's classes", xTypes, gameTypes + xTypes - 1);
        log.push_back(line);

        // X/Y's battle intros, each X/Y's own sequence (read 22 October, ORAS_ENGINE.md 0.1): every intro X/Y's types name gets
        // an intro of its own past Omega Ruby's 162 (oras-engine --xy-intros copies DllBattle's table longer), playing X/Y's
        // sequence (X's a/0/3/6 member at X's DllBattle table, file offset 0xD69B0: intro 0x11 -> 831, checked by the name each
        // Kalos one shows) made Omega Ruby's (OrasXySequence.h) with what it loads made the mod's:
        //  - its screen layout (command 0xA0's first argument, resource kind 0x21: X's a/0/3/5, the same names as Omega Ruby's
        //    a/0/3/3 but other files) appended to a/0/3/3;
        //  - a model or camera (kind 0x20: X's a/0/3/4, Omega Ruby's a/0/3/2; commands 0x1F, 0x20, 0x3F, 0x50, 0x52-0x54, 0x89,
        //    0x8A, 0xB1, their first argument; the archives found by a run's RomFS reads, btr) and a motion (kind 0x1F: X's
        //    a/0/3/3, Omega Ruby's a/0/3/1; command 0x2B): Omega Ruby's member when it is the same file, else X/Y's appended
        //    (a BCH made 0x21). None of X/Y's ids is in its model variants table (FUN_10090120), so each loads as given;
        //  - a sound (command 0x5D's first argument: (category << 16) | n) made the mod's id (OrasXySound.h);
        //  - Kalos's drawn names (command 0xA9: a pane, then a texture of a/2/1/4, 0x18 bytes each) X/Y's under new names
        //    (below); champ_font, another file in X/Y, as champ_font02.
        // Command 0x40 names a slot (the ball thrown), 0xA8 a trainer slot: nothing to make.
        if (intros)
        {
            constexpr uint32_t gameIntros = 162;
            struct Family
            {
                uint32_t XFirst, XLast;
                const char* Prefix; // a numbered family's names: Prefix + 2 digits + ".bclim"; else Prefix + ".bclim"
                bool Numbered;
                uint32_t FirstNew;  // the first new name's number
            };
            static const Family families[] = {{0x11, 0x18, "gym_name", true, 9}, {0x19, 0x1C, "big_four_name", true, 5}, {0x1D, 0x1D, "champion_name", false, 2}};
            const auto nameOf = [](const Family& f, uint32_t number, bool numbered) {
                char n[32];
                if (numbered) snprintf(n, sizeof n, "%s%02u.bclim", f.Prefix, number);
                else snprintf(n, sizeof n, "%s.bclim", f.Prefix);
                return std::string(n);
            };
            // where a sequence names one of the family's game names (a field of 0x18 bytes, FUN_100AF3E4)
            const auto names = [](const Family& f, const Bytes& seq) {
                std::vector<size_t> at;
                const std::string prefix = f.Prefix;
                for (size_t i = 0; i + prefix.size() + 9 <= seq.size(); i++)
                {
                    if (!std::equal(prefix.begin(), prefix.end(), seq.begin() + (long)i)) continue;
                    const size_t rest = i + prefix.size();
                    const bool numbered = std::isdigit(seq[rest]) && std::isdigit(seq[rest + 1]) && std::equal(seq.begin() + (long)rest + 2, seq.begin() + (long)rest + 8, ".bclim");
                    const bool plain = std::equal(seq.begin() + (long)rest, seq.begin() + (long)rest + 6, ".bclim");
                    if ((f.Numbered && numbered) || (!f.Numbered && plain)) at.push_back(i);
                }
                return at;
            };
            const Bytes dllBattle = ws.Rom().Read("DllBattle.cro");
            Garc& sequences = ws.Edited("a/0/3/4");
            const uint32_t gameSequences = (uint32_t)sequences.Count();
            if (gameSequences != 0x3D0) throw FormatError("X/Y intros: a/0/3/4 does not hold the game's 976 sequences (added to already?)");
            const Garc xSequences(xy.Read("a/0/3/6")), xLayouts(xy.Read("a/0/3/5")), xModels(xy.Read("a/0/3/4")), xMotions(xy.Read("a/0/3/3"));
            const Bytes xBattle = xy.Read("DllBattle.cro");
            const Bytes xSizes(xBattle.begin() + 0xD91F8, xBattle.begin() + 0xD91F8 + 4 * 186), orasSizes(dllBattle.begin() + 0xE3800, dllBattle.begin() + 0xE3800 + 4 * 213);
            Garc& layouts = ws.Edited("a/0/3/3");
            Garc& models = ws.Edited("a/0/3/2");
            Garc& motions = ws.Edited("a/0/3/1");
            std::map<uint32_t, uint32_t> layoutOf; // X's layout -> Omega Ruby's member
            // X's member of a kind's archive as Omega Ruby's: the same file there, or X's appended (a BCH made version 0x21)
            struct Kind { const Garc& X; Garc& Oras; const char* Name; std::map<uint32_t, uint32_t> Of; std::vector<Bytes> Game; size_t Shared = 0, Added = 0; };
            Kind modelKind{xModels, models, "a/0/3/2", {}, {}}, motionKind{xMotions, motions, "a/0/3/1", {}, {}};
            const auto port = [&](Kind& k, uint32_t xMember) {
                const auto known = k.Of.find(xMember);
                if (known != k.Of.end()) return known->second;
                if (xMember >= k.X.Count()) throw FormatError(std::string("X/Y intros: X's archive for ") + k.Name + " has no member " + std::to_string(xMember));
                if (k.Game.empty()) for (size_t i = 0; i < k.Oras.Count(); i++) k.Game.push_back(Plain(k.Oras.Sub(i)));
                const Bytes& raw = k.X.Sub(xMember);
                Bytes plain = IsLzCompressed(raw) ? LzDecompress(raw) : raw;
                if (Bch::Is(plain)) plain = BchUpgradeTo21(plain);
                if (&k == &modelKind) plain = AimCamerasAsLookAt(plain);
                const auto same = std::find(k.Game.begin(), k.Game.end(), plain);
                uint32_t at;
                if (same != k.Game.end()) { at = (uint32_t)(same - k.Game.begin()); k.Shared++; }
                else
                {
                    // stored as the game's archive stores its members, not as X's: Omega Ruby's battle loader decompresses every
                    // member of a/0/3/2 and a/0/3/1 (all LZ11 in the game; run k1, 23 October: X's camera 513, plain in X,
                    // appended plain, left the loader's thread reading it for ever and the intro black)
                    const bool gameCompressed = k.Oras.Count() && IsLzCompressed(k.Oras.Sub(0));
                    at = (uint32_t)k.Oras.Count();
                    k.Oras.Set(at, gameCompressed ? Lz11Compress(plain) : plain);
                    k.Game.push_back(plain);
                    k.Added++;
                }
                return k.Of[xMember] = at;
            };
            const auto setU32 = [](Bytes& a, size_t at, uint32_t v) { for (int b = 0; b < 4; b++) a[at + b] = (uint8_t)(v >> (8 * b)); };
            std::map<uint32_t, uint32_t> introOf, sequenceOf; // X/Y's intro -> the mod's; X's sequence -> a/0/3/4's member (X
                                                               // plays one sequence for several intros: 824, 845 three times each)
            for (uint32_t t = 0; t < xTypes; t++)
            {
                const uint32_t xIntro = U32(*battleTypes, 24 * (size_t)t + 4);
                auto it = introOf.find(xIntro);
                if (it == introOf.end())
                {
                    const uint32_t id = gameIntros + (uint32_t)(intros->size() / 4);
                    if (xIntro >= 0x24) throw FormatError("X/Y intros: X's intro " + std::to_string(xIntro) + " past X's table");
                    const uint32_t xSequence = U32(xBattle, 0xD69B0 + 4 * (size_t)xIntro);
                    if (xSequence >= xSequences.Count()) throw FormatError("X/Y intros: X's intro " + std::to_string(xIntro) + " has no sequence");
                    const auto played = sequenceOf.find(xSequence);
                    const uint32_t sequence = played != sequenceOf.end() ? played->second : (uint32_t)sequences.Count();
                    const Bytes xs = Plain(xSequences.Sub(xSequence));
                    const Family* f = nullptr;
                    for (const Family& g : families) if (xIntro >= g.XFirst && xIntro <= g.XLast) f = &g;
                    std::string xName, name;
                    if (f)
                    {
                        const uint32_t k = xIntro - f->XFirst + 1;
                        xName = nameOf(*f, k, f->Numbered); name = nameOf(*f, f->FirstNew + k - 1, true);
                        if (names(*f, xs).size() != 1 || !std::equal(xName.begin(), xName.end(), xs.begin() + (long)names(*f, xs).front()))
                            throw FormatError("X/Y intros: X's sequence " + std::to_string(xSequence) + " does not show " + xName);
                    }
                    const auto field = [](Bytes& a, size_t at, const std::string& from, const std::string& to) {
                        if (std::string((const char*)&a[at]) != from) return false;
                        if (to.size() >= 0x18) throw FormatError("X/Y intros: " + to + " is too long a name");
                        std::fill(a.begin() + (long)at, a.begin() + (long)at + 0x18, 0);
                        std::copy(to.begin(), to.end(), a.begin() + (long)at);
                        return true;
                    };
                    int named = 0;
                    if (played == sequenceOf.end())
                    {
                        const Bytes converted = XySequenceToOras(xs, xSizes, orasSizes, [&](uint16_t command, Bytes& a) {
                            switch (command)
                            {
                            case 0xA0:
                            {
                                const uint32_t xLayout = U32(a, 0);
                                auto l = layoutOf.find(xLayout);
                                if (l == layoutOf.end())
                                {
                                    if (xLayout >= xLayouts.Count()) throw FormatError("X/Y intros: X's a/0/3/5 has no layout " + std::to_string(xLayout));
                                    l = layoutOf.emplace(xLayout, (uint32_t)layouts.Count()).first;
                                    layouts.Set(layouts.Count(), xLayouts.Sub(xLayout));
                                }
                                setU32(a, 0, l->second);
                                break;
                            }
                            case 0xA9:
                                if (a.size() >= 0x30)
                                {
                                    if (f && field(a, 0x18, xName, name)) named++;
                                    field(a, 0x18, "champ_font.bclim", "champ_font02.bclim");
                                }
                                break;
                            case 0x1F: case 0x20: case 0x3F: case 0x50: case 0x52: case 0x53: case 0x54: case 0x89: case 0x8A: case 0xB1:
                                setU32(a, 0, port(modelKind, U32(a, 0)));
                                break;
                            case 0x2B:
                                setU32(a, 0, port(motionKind, U32(a, 0)));
                                break;
                            case 0x5D:
                                if (U32(a, 0)) setU32(a, 0, music->Translate(U32(a, 0)));
                                break;
                            }
                        });
                        if (f && named != 1) throw FormatError("X/Y intros: X's sequence " + std::to_string(xSequence) + " names " + xName + " " + std::to_string(named) + " times");
                        sequences.Set(sequence, converted);
                        sequenceOf[xSequence] = sequence;
                    }
                    for (int b = 0; b < 4; b++) intros->push_back((uint8_t)(sequence >> (8 * b)));
                    it = introOf.emplace(xIntro, id).first;
                    snprintf(line, sizeof line, "X/Y intro 0x%X as intro %u, a/0/3/4 %u (X's sequence %u%s%s)", xIntro, id, sequence, xSequence, f ? ", naming " : "", name.c_str());
                    log.push_back(line);
                }
                setU32(*battleTypes, 24 * (size_t)t + 4, it->second);
            }
            // X/Y's wild intros' camera (X's a/0/3/4 0x201) has a horde version (0x202) no sequence names: in a horde battle the
            // game takes it instead (X/Y's FUN_10090120 when the battle mode is 4, as Omega Ruby's FUN_1009905C does with its
            // own pair 0x3F8 / 0x3F9, read 23 October). Ported with it, and handed to oras-engine --xy-band-camera
            if (modelKind.Of.count(0x201))
            {
                const uint32_t camera = modelKind.Of.at(0x201), band = port(modelKind, 0x202);
                if (bandCamera) *bandCamera = {camera, band};
                snprintf(line, sizeof line, "X/Y intros: the wild intros' camera as a/0/3/2 %u, its horde version %u", camera, band);
                log.push_back(line);
            }
            for (const Kind* k : {&modelKind, &motionKind})
            {
                snprintf(line, sizeof line, "X/Y intros: %zu of X/Y's files for %s shared with Omega Ruby's, %zu added", k->Shared, k->Name, k->Added);
                log.push_back(line);
            }
            // the names: X/Y's a/2/4/1 and Omega Ruby's a/2/1/4 each hold one member, a language per sub-file; each family's
            // new names after its last game name, the DARC's files and the names in the same (alphabetical) order
            const Garc xNamesArchive(xy.Read("a/2/4/1"));
            Garc& namesArchive = ws.Edited("a/2/1/4");
            size_t languages = 0;
            for (size_t sub = 0; sub < 32; sub++)
            {
                if (!namesArchive.Has(0, sub)) continue;
                if (!xNamesArchive.Has(0, sub)) throw FormatError("X/Y intros: X/Y's a/2/4/1 lacks language " + std::to_string(sub));
                const Bytes was = namesArchive.Sub(0, sub);
                LayoutResource res = ReadLayoutResource(Plain(was));
                if (WriteLayoutResource(res) != Plain(was)) throw FormatError("X/Y intros: a/2/1/4 does not write back as read");
                const LayoutResource xRes = ReadLayoutResource(Plain(xNamesArchive.Sub(0, sub)));
                for (const Family& f : families)
                {
                    const uint32_t count = f.XLast - f.XFirst + 1;
                    const std::string last = f.Numbered ? nameOf(f, count, true) : nameOf(f, 0, false);
                    const auto after = std::find(res.Files.begin(), res.Files.end(), last);
                    if (after == res.Files.end()) throw FormatError("X/Y intros: a/2/1/4 has no " + last);
                    size_t insertAt = (size_t)(after - res.Files.begin()) + 1;
                    const std::string& path = res.Darc.at(insertAt - 1).Path;
                    if (path.substr(path.rfind('/') + 1) != last) throw FormatError("X/Y intros: a/2/1/4's names and DARC are not in the same order");
                    const std::string folder = path.substr(0, path.rfind('/') + 1);
                    for (uint32_t k = 1; k <= count; k++, insertAt++)
                    {
                        const std::string from = nameOf(f, k, f.Numbered), to = nameOf(f, f.FirstNew + k - 1, true);
                        if (std::find(res.Files.begin(), res.Files.end(), to) != res.Files.end()) throw FormatError("X/Y intros: a/2/1/4 has " + to + " already");
                        const auto x = std::find_if(xRes.Darc.begin(), xRes.Darc.end(), [&](const DarcFile& d) { return d.Path.size() >= from.size() && d.Path.substr(d.Path.size() - from.size()) == from && d.Path[d.Path.size() - from.size() - 1] == '/'; });
                        if (x == xRes.Darc.end()) throw FormatError("X/Y intros: X/Y's a/2/4/1 lacks " + from);
                        const Bytes& data = x->Data;
                        res.Files.insert(res.Files.begin() + (long)insertAt, to);
                        res.Darc.insert(res.Darc.begin() + (long)insertAt, DarcFile{folder + to, data});
                    }
                }
                {
                    // the Champion's title font, X/Y's (another file than Omega Ruby's champ_font), after Omega Ruby's
                    const auto after = std::find(res.Files.begin(), res.Files.end(), std::string("champ_font.bclim"));
                    const auto x = std::find_if(xRes.Darc.begin(), xRes.Darc.end(), [](const DarcFile& d) { return d.Path.size() > 17 && d.Path.substr(d.Path.size() - 17) == "/champ_font.bclim"; });
                    if (after == res.Files.end() || x == xRes.Darc.end()) throw FormatError("X/Y intros: champ_font.bclim missing from a/2/1/4 or X/Y's a/2/4/1");
                    const size_t insertAt = (size_t)(after - res.Files.begin()) + 1;
                    const std::string& path = res.Darc.at(insertAt - 1).Path;
                    res.Files.insert(res.Files.begin() + (long)insertAt, "champ_font02.bclim");
                    res.Darc.insert(res.Darc.begin() + (long)insertAt, DarcFile{path.substr(0, path.rfind('/') + 1) + "champ_font02.bclim", x->Data});
                }
                namesArchive.Set(0, AsWas(was, WriteLayoutResource(res)), sub);
                languages++;
            }
            snprintf(line, sizeof line, "X/Y intros: %zu ported, Kalos's drawn names as a/2/1/4's gym_name09-16, big_four_name05-08, champion_name02 and champ_font02 in %zu languages", introOf.size(), languages);
            log.push_back(line);
        }
    }

    // the records and teams; X/Y's trainer 0 (16 bytes, no team) is Omega Ruby's empty trainer 0
    static const size_t EntrySize[4] = {8, 16, 10, 18}; // by the format byte: moves (1), item (2)
    for (int n = 0; n < XyTrainerCount; n++)
    {
        const int id = XyTrainerFirst + n;
        if (n == 0) { records.Set((size_t)id, ws.Original("a/0/3/6").Sub(0)); teams.Set((size_t)id, ws.Original("a/0/3/8").Sub(0)); continue; }
        const Bytes x = Plain(xRecords.Sub((size_t)n)), team = Plain(xTeams.Sub((size_t)n));
        if (x.size() != 20) throw FormatError("X/Y trainer " + std::to_string(n) + ": its record is not 20 bytes");
        if (x[0] > 3 || x[1] >= xClasses.Count() || team.size() != EntrySize[x[0]] * x[3])
            throw FormatError("X/Y trainer " + std::to_string(n) + ": its format, class or team size does not read");
        Bytes r(24, 0);
        r[0] = x[0];
        r[2] = (uint8_t)(gameClasses + x[1]); r[3] = (uint8_t)((gameClasses + x[1]) >> 8); // X/Y's own class, ported above
        r[6] = x[2]; r[7] = x[3];
        for (int k = 0; k < 8; k++) r[8 + k] = x[4 + k];   // items
        for (int k = 0; k < 4; k++) r[0x10 + k] = x[0xC + k]; // AI
        r[0x14] = x[0x10]; r[0x15] = x[0x11]; r[0x16] = x[0x12]; r[0x17] = x[0x13];
        records.Set((size_t)id, r);
        teams.Set((size_t)id, team);
    }

    // the names, one line per trainer in each of the eight languages
    size_t named = 0;
    for (int a = 1; a <= 8; a++)
    {
        const std::string path = "a/0/7/" + std::to_string(a);
        Garc& texts = ws.Edited(path);
        const Bytes was = texts.Sub(22);
        Bytes file = Plain(was);
        if (ReadGameText(file).size() != (size_t)XyTrainerFirst) throw FormatError(path + " member 22 does not hold " + std::to_string(XyTrainerFirst) + " trainer names");
        const std::vector<std::string> names = ReadGameText(Plain(Garc(xy.Read("a/0/7/" + std::to_string(a + 1))).Sub(21)));
        for (int n = 0; n < XyTrainerCount; n++) file = AppendGameTextLine(file, (size_t)n < names.size() ? names[(size_t)n] : "");
        texts.Set(22, AsWas(was, file));
        named = names.size();
    }
    // the battle lines (before a battle, on losing...): Omega Ruby's a/1/3/5 (archive 0x87, Res_FindIndexInArchive135
    // 0x4533B0) member 0 a u16 per trainer, the byte offset of its first entry in member 1; member 1 a u16 per line of
    // a/0/7/1-8 member 23, its kind + 1 (0 none), the line the entry's index; the reader scans up to 20 entries from the
    // trainer's for the kind asked (2,849 entries for 2,848 lines, the last 0). X/Y's a/1/4/2 member 1: (trainer, kind) per
    // line of a/0/7/2-9 member 22 (1,352), kinds 0, 1, 2 as Omega Ruby's 1, 2, 3 (inferred: the same order of entries per
    // trainer). Appended past the 0, an empty line for it in the text, each X/Y trainer's lines together, then a 0; a
    // trainer of no line points at that last 0 (the scan ends at the member's end), so it finds none, not another's
    const Bytes xTable = Plain(Garc(xy.Read("a/1/4/2")).Sub(1));
    Garc& lineTable = ws.Edited("a/1/3/5");
    Bytes firsts = Plain(lineTable.Sub(0)), kinds = Plain(lineTable.Sub(1));
    if (firsts.size() != 2 * (size_t)XyTrainerFirst) throw FormatError("a/1/3/5 member 0 does not hold " + std::to_string(XyTrainerFirst) + " trainers");
    std::map<int, std::vector<std::pair<size_t, int>>> linesOf; // X/Y trainer -> (X/Y line, kind)
    for (size_t at = 0; at + 4 <= xTable.size(); at += 4)
        linesOf[xTable[at] | xTable[at + 1] << 8].push_back({at / 4, xTable[at + 2] | xTable[at + 3] << 8});
    const size_t firstEntry = kinds.size() / 2;
    std::vector<size_t> order; // the X/Y lines in their new order
    std::vector<uint16_t> trainerOffset(XyTrainerCount, 0);
    for (const auto& [n, lines] : linesOf)
    {
        if (n <= 0 || n >= XyTrainerCount) continue;
        trainerOffset[(size_t)n] = (uint16_t)(2 * (firstEntry + order.size()));
        for (const auto& [l, kind] : lines)
        {
            order.push_back(l);
            kinds.push_back((uint8_t)(kind + 1)); kinds.push_back((uint8_t)((kind + 1) >> 8));
        }
    }
    const size_t end = firstEntry + order.size();
    kinds.push_back(0); kinds.push_back(0);
    if (2 * end > 0xFFFF) throw FormatError("trainer lines: past a u16 offset");
    for (int n = 0; n < XyTrainerCount; n++)
    {
        const uint16_t o = trainerOffset[(size_t)n] ? trainerOffset[(size_t)n] : (uint16_t)(2 * end);
        firsts.push_back((uint8_t)o); firsts.push_back((uint8_t)(o >> 8));
    }
    lineTable.Set(0, firsts);
    lineTable.Set(1, kinds);
    for (int a = 1; a <= 8; a++)
    {
        const std::string path = "a/0/7/" + std::to_string(a);
        Garc& texts = ws.Edited(path);
        const Bytes was = texts.Sub(23);
        Bytes file = Plain(was);
        if (ReadGameText(file).size() + 1 != firstEntry) throw FormatError(path + " member 23: its lines and a/1/3/5's entries do not pair");
        const std::vector<std::string> xLines = ReadGameText(Plain(Garc(xy.Read("a/0/7/" + std::to_string(a + 1))).Sub(22)));
        file = AppendGameTextLine(file, ""); // the game's last entry, 0
        for (const size_t l : order) file = AppendGameTextLine(file, l < xLines.size() ? xLines[l] : "");
        texts.Set(23, AsWas(was, file));
    }
    snprintf(line, sizeof line, "trainer lines: %zu of X/Y's after Omega Ruby's %zu, for %zu trainers", order.size(), firstEntry, linesOf.size());
    log.push_back(line);

    snprintf(line, sizeof line, "trainers: X/Y's %d as Omega Ruby's %d-%d (%zu named), their classes X/Y's own (280 + X/Y's)",
             XyTrainerCount, XyTrainerFirst, XyTrainerFirst + XyTrainerCount - 1, named);
    log.push_back(line);
    return log;
}

}
