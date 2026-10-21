#include "OrasXyTrainers.h"

#include "Darc.h"
#include "GameText.h"
#include "Garc.h"
#include "N3dsRom.h"
#include "NitroCompression.h"
#include "OrasXySound.h"

#include <algorithm>
#include <cstdio>
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

std::vector<std::string> AddXyTrainers(OrasWorkspace& ws, const XySounds* music, bool streams, Bytes* battleTypes, Bytes* intros)
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

    // the classes, by their English names (X/Y's a/0/7/4 member 20, Omega Ruby's a/0/7/3 member 21) and pictures
    const std::vector<std::string> xNames = ReadGameText(Plain(Garc(xy.Read("a/0/7/4")).Sub(20)));
    const std::vector<std::string> oNames = ReadGameText(Plain(ws.Original("a/0/7/3").Sub(21)));
    if (xNames.size() != xClasses.Count() || oNames.size() != oClasses.Count()) throw FormatError("X/Y trainers: the class names do not match the class archives");
    // the picture offset: the one most classes of the same name at the same index give (113)
    std::map<int, int> offsets;
    for (size_t c = 0; c < xClasses.Count() && c < oClasses.Count(); c++)
        if (xNames[c] == oNames[c]) offsets[Picture(Plain(oClasses.Sub(c))) - Picture(Plain(xClasses.Sub(c)))]++;
    int offset = 0, votes = 0;
    for (const auto& [o, n] : offsets) if (n > votes) offset = o, votes = n;
    std::vector<int> classOf(xClasses.Count(), -1);
    size_t byName = 0;
    for (size_t c = 0; c < xClasses.Count(); c++)
    {
        const int picture = Picture(Plain(xClasses.Sub(c))) + offset;
        int named = -1, pictured = -1;
        for (size_t o = 0; o < oClasses.Count(); o++)
        {
            if (Picture(Plain(oClasses.Sub(o))) != picture) continue;
            if (pictured < 0) pictured = (int)o;
            if (oNames[o] == xNames[c] && (named < 0 || o == c)) named = (int)o;
        }
        if (named >= 0) { classOf[c] = named; byName++; continue; }
        if (pictured < 0) throw FormatError("X/Y trainer class " + std::to_string(c) + " (" + xNames[c] + "): no Omega Ruby class has its picture");
        classOf[c] = pictured;
        snprintf(line, sizeof line, "trainer class: X/Y's %zu (%s) as Omega Ruby's %d (%s), its picture; no class of its name", c, xNames[c].c_str(), pictured, oNames[pictured].c_str());
        log.push_back(line);
    }

    // the battle types X/Y's classes name (their byte +2), with X/Y's music: appended after Omega Ruby's 73 (OrasEngine.cpp).
    // Each is Omega Ruby's record of the type its own copy of the class names (Omega Ruby keeps X/Y's classes 0-108 and
    // gives each one of its types: class 39, Grant's, X/Y's 34 -> Omega Ruby's 45), with X/Y's music and victory jingle
    // (+0x10, +0x14) in place of its own; the other words are not X/Y's: X/Y's type 34 has 0x12 at +4 where Omega Ruby's
    // leader types have 0x27-0x2C, one each, and Grant's battle so read stopped in its intro on null reads from
    // Res_DictFindIndex (0x4CC890; run hG2). What +4 picks is inferred (a resource of the type's intro), not read
    if (music && battleTypes)
    {
        constexpr uint32_t xTable = 0x53C010, oTable = 0x57A014, gameTypes = 73;
        const Bytes xCode = xy.Code(), oCode = ws.Rom().Code();
        std::map<std::pair<int, int>, int> numberOf; // (X/Y's type, Omega Ruby's type of the class) -> the new type
        std::vector<std::pair<int, int>> typeOf(xClasses.Count());
        for (size_t c = 0; c < xClasses.Count(); c++)
        {
            const std::pair<int, int> key{Plain(xClasses.Sub(c)).at(2), Plain(oClasses.Sub((size_t)classOf[c])).at(2)};
            typeOf[c] = key;
            if (numberOf.count(key)) continue;
            const size_t xAt = xTable - 0x100000 + 24 * (size_t)key.first, oAt = oTable - 0x100000 + 24 * (size_t)key.second;
            Bytes r(oCode.begin() + oAt, oCode.begin() + oAt + 24);
            for (const size_t w : {0x10u, 0x14u})
            {
                const uint32_t id = xCode[xAt + w] | xCode[xAt + w + 1] << 8 | xCode[xAt + w + 2] << 16 | (uint32_t)xCode[xAt + w + 3] << 24;
                if ((id >> 16) != 1) throw FormatError("X/Y battle type " + std::to_string(key.first) + ": no music id at +" + std::to_string(w));
                // X/Y's own track (a stream appended to the archive, category 0) only with X/Y's music in the mod; else the id
                // as it is, which names Omega Ruby's sound of that number (its own trainer theme for X/Y's 0x10019: the PSS one)
                const uint32_t translated = music->Translate(id), to = (translated >> 16) == 0 && !streams ? id : translated;
                for (int b = 0; b < 4; b++) r[w + b] = (uint8_t)(to >> (8 * b));
            }
            numberOf[key] = (int)(gameTypes + battleTypes->size() / 24);
            battleTypes->insert(battleTypes->end(), r.begin(), r.end());
        }
        std::set<int> orasUsed; // the classes Omega Ruby's own trainers use
        for (size_t t = 0; t < (size_t)XyTrainerFirst; t++) { const Bytes r = Plain(ws.Original("a/0/3/6").Sub(t)); if (r.size() >= 4) orasUsed.insert(r[2] | r[3] << 8); }
        Garc& classes = ws.Edited("a/0/3/7");
        std::map<int, int> given; // Omega Ruby class -> the battle type it was given
        size_t set = 0;
        for (size_t c = 0; c < xClasses.Count(); c++)
        {
            const int o = classOf[c], type = numberOf.at(typeOf[c]);
            if (orasUsed.count(o)) { snprintf(line, sizeof line, "battle type: X/Y's class %zu's Omega Ruby class %d is its own trainers': kept", c, o); log.push_back(line); continue; }
            const auto g = given.find(o);
            if (g != given.end()) { if (g->second != type) { snprintf(line, sizeof line, "battle type: Omega Ruby class %d keeps X/Y's %d (X/Y's class %zu names another)", o, g->second, c); log.push_back(line); } continue; }
            const Bytes was = classes.Sub((size_t)o);
            Bytes r = Plain(was);
            r.at(2) = (uint8_t)type;
            classes.Set((size_t)o, AsWas(was, r));
            given[o] = type;
            set++;
        }
        snprintf(line, sizeof line, "battle types: %zu (Omega Ruby's of the class, X/Y's music) as 73-%zu, given to %zu classes", numberOf.size(), 72 + numberOf.size(), set);
        log.push_back(line);

        // Kalos's leaders' own names in their battle intro (read 22 October, ORAS_ENGINE.md 0.1). X/Y's leader types have
        // the intros 0x11-0x18 at +4, Viola's to Wulfric's, in the order of X/Y's drawn names (a/2/4/1: gym_name01 Violette
        // ... gym_name08 Urup); Omega Ruby's record kept for the class shows a Hoenn leader's intro, whose a/0/3/4 sequence
        // names that leader's drawn name (gym_name0N, from a/2/1/4). Each Kalos leader gets an intro of its own past the
        // game's 162 (oras-engine --xy-intros copies DllBattle's table longer), its sequence a copy of the one it showed
        // with only the name changed, gym_name09-16: X/Y's eight names added to a/2/1/4 under those names, every language
        if (intros)
        {
            constexpr uint32_t introTable = 0x10105CDC - 0x10025000, gameIntros = 162, xFirst = 0x11, xLast = 0x18;
            const Bytes dllBattle = ws.Rom().Read("DllBattle.cro");
            Garc& sequences = ws.Edited("a/0/3/4");
            const uint32_t gameSequences = (uint32_t)sequences.Count();
            if (gameSequences != 0x3D0) throw FormatError("X/Y leader intros: a/0/3/4 does not hold the game's 976 sequences (added to already?)");
            std::map<uint32_t, std::pair<int, uint32_t>> leaders; // X/Y intro -> (new battle type, Omega Ruby's intro it showed)
            for (const auto& [key, type] : numberOf)
            {
                const size_t xAt = xTable - 0x100000 + 24 * (size_t)key.first;
                const uint32_t xIntro = U32(xCode, xAt + 4);
                if (xIntro < xFirst || xIntro > xLast) continue;
                const size_t at = 24 * (size_t)(type - gameTypes) + 4;
                if (!leaders.emplace(xIntro, std::pair{type, U32(*battleTypes, at)}).second)
                    throw FormatError("X/Y leader intros: two battle types show X/Y intro " + std::to_string(xIntro));
            }
            for (const auto& [xIntro, leader] : leaders)
            {
                const uint32_t id = gameIntros + (uint32_t)(intros->size() / 4), sequence = gameSequences + (uint32_t)(intros->size() / 4);
                if (leader.second >= gameIntros) throw FormatError("X/Y leader intros: Omega Ruby's intro past its table");
                const uint32_t shown = U32(dllBattle, introTable + 4 * leader.second);
                const Bytes was = sequences.Sub(shown);
                Bytes copy = Plain(was);
                char name[24];
                snprintf(name, sizeof name, "gym_name%02u.bclim", 8 + xIntro - xFirst + 1);
                const std::string pattern = "gym_name0";
                size_t found = std::string::npos, count = 0;
                for (size_t i = 0; i + 16 <= copy.size(); i++)
                    if (std::equal(pattern.begin(), pattern.end(), copy.begin() + i) && std::equal(name + 10, name + 16, copy.begin() + i + 10)) found = i, count++;
                if (count != 1) throw FormatError("X/Y leader intros: sequence " + std::to_string(shown) + " does not name one gym_name0N.bclim");
                std::copy(name, name + 16, copy.begin() + found);
                sequences.Set(sequence, AsWas(was, copy));
                const size_t at = 24 * (size_t)(leader.first - gameTypes) + 4;
                for (int b = 0; b < 4; b++) (*battleTypes)[at + b] = (uint8_t)(id >> (8 * b));
                for (int b = 0; b < 4; b++) intros->push_back((uint8_t)(sequence >> (8 * b)));
                snprintf(line, sizeof line, "leader intro: X/Y's 0x%X as intro %u, a/0/3/4 %u (a copy of %u naming %s), battle type %d", xIntro, id, sequence, shown, name, leader.first);
                log.push_back(line);
            }
            // the names: X/Y's a/2/4/1 and Omega Ruby's a/2/1/4 each hold one member, a language per sub-file
            const Garc xNamesArchive(xy.Read("a/2/4/1"));
            Garc& namesArchive = ws.Edited("a/2/1/4");
            size_t languages = 0;
            for (size_t sub = 0; sub < 32; sub++)
            {
                if (!namesArchive.Has(0, sub)) continue;
                if (!xNamesArchive.Has(0, sub)) throw FormatError("X/Y leader intros: X/Y's a/2/4/1 lacks language " + std::to_string(sub));
                const Bytes was = namesArchive.Sub(0, sub);
                LayoutResource names = ReadLayoutResource(Plain(was));
                if (WriteLayoutResource(names) != Plain(was)) throw FormatError("X/Y leader intros: a/2/1/4 does not write back as read");
                const LayoutResource xNames = ReadLayoutResource(Plain(xNamesArchive.Sub(0, sub)));
                if (std::any_of(names.Files.begin(), names.Files.end(), [](const std::string& f) { return f == "gym_name09.bclim"; }))
                    throw FormatError("X/Y leader intros: a/2/1/4 has gym_name09 already");
                // after gym_name08, the DARC's files and the names in the same order
                const auto after = std::find(names.Files.begin(), names.Files.end(), "gym_name08.bclim");
                if (after == names.Files.end()) throw FormatError("X/Y leader intros: a/2/1/4 has no gym_name08");
                size_t insertAt = (size_t)(after - names.Files.begin()) + 1;
                if (names.Darc.at(insertAt - 1).Path.substr(names.Darc.at(insertAt - 1).Path.rfind('/') + 1) != "gym_name08.bclim")
                    throw FormatError("X/Y leader intros: a/2/1/4's names and DARC are not in the same order");
                const std::string folder = names.Darc.at(insertAt - 1).Path.substr(0, names.Darc.at(insertAt - 1).Path.rfind('/') + 1);
                for (uint32_t k = 1; k <= 8; k++, insertAt++)
                {
                    char from[24], to[24];
                    snprintf(from, sizeof from, "gym_name%02u.bclim", k);
                    snprintf(to, sizeof to, "gym_name%02u.bclim", 8 + k);
                    const auto x = std::find_if(xNames.Darc.begin(), xNames.Darc.end(), [&](const DarcFile& f) { return f.Path.size() >= 16 && f.Path.substr(f.Path.size() - 16) == from; });
                    if (x == xNames.Darc.end()) throw FormatError(std::string("X/Y leader intros: X/Y's a/2/4/1 lacks ") + from);
                    names.Files.insert(names.Files.begin() + (long)insertAt, to);
                    names.Darc.insert(names.Darc.begin() + (long)insertAt, DarcFile{folder + to, x->Data});
                }
                namesArchive.Set(0, AsWas(was, WriteLayoutResource(names)), sub);
                languages++;
            }
            snprintf(line, sizeof line, "leader intros: %zu, X/Y's eight drawn names as a/2/1/4's gym_name09-16 in %zu languages", leaders.size(), languages);
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
        if (x[0] > 3 || x[1] >= classOf.size() || team.size() != EntrySize[x[0]] * x[3])
            throw FormatError("X/Y trainer " + std::to_string(n) + ": its format, class or team size does not read");
        Bytes r(24, 0);
        r[0] = x[0];
        r[2] = (uint8_t)classOf[x[1]]; r[3] = (uint8_t)(classOf[x[1]] >> 8);
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

    snprintf(line, sizeof line, "trainers: X/Y's %d as Omega Ruby's %d-%d (%zu named), their classes Omega Ruby's (%zu of %zu by name and picture)",
             XyTrainerCount, XyTrainerFirst, XyTrainerFirst + XyTrainerCount - 1, named, byName, xClasses.Count());
    log.push_back(line);
    return log;
}

}
