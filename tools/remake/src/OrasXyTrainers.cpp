#include "OrasXyTrainers.h"

#include "GameText.h"
#include "Garc.h"
#include "N3dsRom.h"
#include "NitroCompression.h"

#include <cstdio>
#include <map>

namespace remake
{

namespace
{
Bytes Plain(const Bytes& data) { return IsLzCompressed(data) ? LzDecompress(data) : data; }
// a text member's file written back as the archive held it (compressed or not)
Bytes AsWas(const Bytes& was, const Bytes& file) { return IsLzCompressed(was) ? Lz11Compress(file) : file; }
int Picture(const Bytes& classRecord) { return classRecord.at(4) | classRecord.at(5) << 8; }
}

std::vector<std::string> AddXyTrainers(OrasWorkspace& ws)
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
