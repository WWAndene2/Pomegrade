#include "OrasXySharedScripts.h"

#include "CodePatch.h"
#include "Garc.h"
#include "N3dsRom.h"
#include "NitroCompression.h"
#include "OrasWorkspace.h"
#include "OrasXyEvents.h"

#include <cstdio>
#include <map>
#include <sstream>

namespace remake
{

namespace
{
Bytes Plain(const Bytes& data) { return IsLzCompressed(data) ? LzDecompress(data) : data; }
// a script member: a Pawn image (its magic 0xF1E0 at +4) is plain whatever its first byte (X's member 41, 0x610 bytes,
// begins with 0x10, LZ10's mark); anything else decompressed
Bytes Script(const Bytes& data) { return data.size() >= 6 && data[4] == 0xE0 && data[5] == 0xF1 ? data : Plain(data); }
uint32_t Word(const Bytes& b, size_t at) { return (uint32_t)(b.at(at) | b.at(at + 1) << 8 | b.at(at + 2) << 16 | (uint32_t)b.at(at + 3) << 24); }
uint16_t Half(const Bytes& b, size_t at) { return (uint16_t)(b.at(at) | b.at(at + 1) << 8); }
void Put16(Bytes& b, uint32_t v) { b.push_back((uint8_t)v); b.push_back((uint8_t)(v >> 8)); }
void Put32(Bytes& b, uint32_t v) { for (int k = 0; k < 4; k++) b.push_back((uint8_t)(v >> (8 * k))); }

// X's table and its scripts' names (X's 0x3D9C64: its literals 0x5457A4 and 0x5AE760, its bound `cmp r5, #0x42`)
constexpr uint32_t XRows = 0x5457A4, XRowCount = 66, XNames = 0x5AE760;
constexpr size_t NameSize = 32;
}

std::vector<XyScriptRange> XyScriptRanges(const Bytes& xCode)
{
    std::vector<XyScriptRange> rows;
    for (uint32_t k = 0; k < XRowCount; k++)
    {
        const size_t at = XRows - CodeBase + 16 * k;
        rows.push_back({Half(xCode, at), Half(xCode, at + 2), Half(xCode, at + 4), Half(xCode, at + 6), Half(xCode, at + 8)});
        if (rows.back().First < 2000 || rows.back().Last < rows.back().First) throw FormatError("X/Y's script ranges: a row not of an id range (not X/Y's code)");
    }
    return rows;
}

std::string XySharedScriptEdits(std::string source, int member, const std::vector<XyScriptRange>& rows)
{
    // its dispatch: a shared script picks its case by the id it is run for (objectencount.amx: `casetbl 11 ... 10480 L ...
    // 10489 L`), so the case values in its own ranges move with its ids
    for (size_t at = source.find("    casetbl "); at != std::string::npos; at = source.find("    casetbl ", at + 1))
    {
        const size_t end = source.find('\n', at);
        std::istringstream in(source.substr(at + 12, end - at - 12));
        std::string count, fallback, out;
        in >> count >> fallback;
        out = "    casetbl " + count + " " + fallback;
        for (std::string value, label; in >> value >> label;)
        {
            int v = std::stoi(value);
            for (const XyScriptRange& r : rows)
                if (r.Member == member && v >= r.First && v <= r.Last) { v += XyScriptIdShift; break; }
            out += " " + std::to_string(v) + " " + label;
        }
        source.replace(at, end - at, out);
    }
    // its table lookup: a script that reads a table by its id takes its range's first off it (fld_item.amx: `load.p.pri 24`,
    // the id, `add.p.c -7000`, then 3 cells a case), so that first moves too; left as X's, the ids moved by the shift read
    // 30000 x 3 cells past the table and the game hung on picking up an item (owner's phone, 29 October: Shalour)
    for (const char* op : {"    add.p.c -", "    add.c -"})
        for (size_t at = source.find(op); at != std::string::npos; at = source.find(op, at + 1))
        {
            const size_t value = at + std::string(op).size(), end = source.find_first_not_of("0123456789", value);
            const int v = std::stoi(source.substr(value, end - value));
            for (const XyScriptRange& r : rows)
                if (r.Member == member && v == r.First)
                {
                    // the packed form holds 16 bits: the moved first (37000) takes the plain one
                    source.replace(at, end - at, "    add.c -" + std::to_string(v + XyScriptIdShift));
                    break;
                }
        }
    if (member == 17 || member == 26)
    {
        // fld_item.amx and hide_item.amx: an item's flag is its case's cell + 1306 (items on the ground) or + 1100 (hidden),
        // computed, so not moved by XyScriptSource; the zones' item characters vanish by the moved flag (XyFirstFlag + it:
        // Shalour's item 198 has flag 4832 = 0xD00 + 1306 + 198), so the script sets the moved one too. Left as X's, an
        // item picked up stayed and could be picked up again (owner's phone, 29 October)
        const std::string from = member == 17 ? "    add.p.c 1306\n" : "    add.p.c 1100\n";
        const size_t at = source.find(from);
        if (at == std::string::npos || source.find(from, at + 1) != std::string::npos)
            throw FormatError("X/Y's shared script " + std::to_string(member) + ": not its one item flag base");
        source.replace(at, from.size(), "    add.p.c " + std::to_string((member == 17 ? 1306 : 1100) + XyFirstFlag) + "\n");
    }
    if (member == 39)
    {
        // objectencount.amx: a can's flag is its spot + 3152 (0xC50, X's 0xC50-0xC59: OrasXyDustBox.h), computed, so not
        // moved by XyScriptSource; the same value is the can prop's id (BuildMdlGetHandleByUniqID), which the zones keep, so
        // only what FlagGet and FlagReset are given moves (XyFirstFlag + it)
        size_t flags = 0;
        for (const char* native : {"FlagGet", "FlagReset"})
        {
            const std::string from = std::string("    push.p.s -4\n    sysreq.n ");
            for (size_t at = source.find(from); at != std::string::npos; at = source.find(from, at + 1))
            {
                const size_t line = at + 16, end = source.find('\n', line);
                if (source.substr(line, end - line).find(std::string("; ") + native) == std::string::npos) continue;
                source.replace(at, 16, "    load.p.s.pri -4\n    add.p.c " + std::to_string(XyFirstFlag) + "\n    push.pri\n");
                flags++;
            }
        }
        if (flags != 2) throw FormatError("X/Y's objectencount.amx: not its two flag calls (" + std::to_string(flags) + ")");
    }
    return source;
}

int XyScriptIds::ToOras(int id) const
{
    for (const XyScriptRange& r : Ranges)
        if (id >= r.First && id <= r.Last) return id + XyScriptIdShift;
    return id;
}

std::set<int> XySharedScriptMembers(const Bytes& xCode)
{
    std::set<int> members;
    for (const XyScriptRange& r : XyScriptRanges(xCode))
        if (!(r.First >= 3000 && r.Last <= 6999)) members.insert(r.Member);
    return members;
}

std::vector<Bytes> XySharedScriptsOf(N3dsRom& xRom, const std::set<int>& xMembers, const std::string& namesFile)
{
    const Garc scripts(xRom.Read("a/0/3/1"));
    std::vector<Bytes> out;
    for (const int m : xMembers) out.push_back(XyScriptToOras(Script(scripts.Sub((size_t)m)), namesFile, 0x243).Script);
    return out;
}

Bytes AddXySharedScripts(OrasWorkspace& ws, N3dsRom& xRom, const std::set<int>& xMembers, const XyNumbers& numbers,
                         const std::string& namesFile, const XySounds* sounds, bool music, XyBackgroundNumbers* backgrounds,
                         XyScriptIds& ids, std::vector<std::string>& log)
{
    const Bytes xCode = xRom.Code();
    const std::vector<XyScriptRange> rows = XyScriptRanges(xCode);
    const Garc scripts(xRom.Read("a/0/3/1"));
    std::map<std::pair<int, int>, int> textOf; // X/Y's text file (kind, member) -> the mod's
    Bytes rowsOut, members;
    uint32_t rowCount = 0, memberCount = 0;
    // every range first: a shared script may name another's ids
    for (const XyScriptRange& r : rows) if (xMembers.count(r.Member)) ids.Add(r);
    for (const int m : xMembers)
    {
        if ((size_t)m >= scripts.Count()) throw FormatError("X/Y's shared script " + std::to_string(m) + ": no such member of a/0/3/1");
        std::vector<std::string> notes;
        const std::string source = XyScriptSource(XyScriptToOras(Script(scripts.Sub((size_t)m)), namesFile, 0x243).Script, namesFile, numbers, notes);
        uint32_t mask = 0x243;
        const auto edit = [&](const std::string& moved) { return XySharedScriptEdits(moved, m, rows); };
        const Bytes assembled = XyAssembleScript(ws, xRom, source, numbers, namesFile, sounds, music, backgrounds, &ids, edit,
                                                 "X/Y shared script " + std::to_string(m), mask, notes, log);
        // the same script as one of Omega Ruby's: merged, its rows naming Omega Ruby's member (CLAUDE.md 8); else appended
        const Garc& orasScripts = ws.Original("a/0/2/9");
        size_t member = orasScripts.Count();
        bool merged = false;
        for (size_t k = 0; k < orasScripts.Count() && !merged; k++)
            if (Script(orasScripts.Sub(k)) == Script(scripts.Sub((size_t)m))) { member = k; merged = true; }
        if (!merged)
        {
            Garc& archive = ws.Edited("a/0/2/9");
            member = archive.Count();
            archive.Set(member, assembled);
            if (Garc(archive.Write()).Sub(member) != assembled) throw FormatError("X/Y's shared script " + std::to_string(m) + ": its member does not read back identical");
        }
        const uint32_t nameAt = Word(xCode, XNames - CodeBase + 4 * (size_t)m);
        std::string name;
        for (size_t c = nameAt - CodeBase; c < xCode.size() && xCode[c] && name.size() < NameSize - 1; c++) name.push_back((char)xCode[c]);
        size_t ranges = 0;
        for (const XyScriptRange& r : rows)
        {
            if (r.Member != m) continue;
            // the text: kind 1 the story text (Text_GetLanguageTable 0x4191B8: archives 79-86, a/0/7/9-a/0/8/6; X/Y's
            // a/0/8/0-7: X's member 142, its "obtained" lines), kind 0 the system text (Sys_GetTableEntryByIndex 0x11FB9C:
            // archives 71-78, a/0/7/1-8; X/Y's a/0/7/2-9)
            if (r.TextKind > 1) throw FormatError("X/Y's shared script " + std::to_string(m) + ": a range of text kind " + std::to_string(r.TextKind));
            const std::pair<int, int> text{r.TextKind, r.Text};
            if (!textOf.count(text))
            {
                std::vector<Bytes> files;
                for (int k = 0; k < 8; k++)
                    files.push_back(Plain(Garc(xRom.Read(r.TextKind == 1 ? "a/0/8/" + std::to_string(k) : "a/0/7/" + std::to_string(k + 2))).Sub(r.Text)));
                textOf[text] = r.TextKind == 1 ? ws.AddZoneTextFiles(files) : ws.AddSystemTextFiles(files);
            }
            for (const uint32_t v : {(uint32_t)r.First + XyScriptIdShift, (uint32_t)r.Last + XyScriptIdShift, (uint32_t)member, (uint32_t)r.TextKind, (uint32_t)textOf[text], 0u, 0u, 0u}) Put16(rowsOut, v);
            rowCount++;
            ranges++;
            char line[192];
            snprintf(line, sizeof line, "X/Y shared script %d (%s): a/0/2/9 member %zu, ids %d-%d (X/Y's %d-%d), %s text file %d, mask 0x%X", m, name.c_str(),
                     member, r.First + XyScriptIdShift, r.Last + XyScriptIdShift, r.First, r.Last, r.TextKind ? "story" : "system", textOf[text], mask);
            log.push_back(line);
        }
        if (!ranges) throw FormatError("X/Y's shared script " + std::to_string(m) + ": no range names it");
        for (const std::string& n : notes) log.push_back("  " + n);
        if (merged) { log.push_back("  the same as Omega Ruby's member " + std::to_string(member) + ": merged"); continue; }
        memberCount++;
        Put16(members, (uint32_t)member);
        Put16(members, 0);
        Put32(members, mask);
        for (size_t c = 0; c < NameSize; c++) members.push_back(c < name.size() ? (uint8_t)name[c] : 0);
    }
    Bytes file = {'X', 'Y', 'S', 'S'};
    Put32(file, rowCount);
    file.insert(file.end(), rowsOut.begin(), rowsOut.end());
    Put32(file, memberCount);
    file.insert(file.end(), members.begin(), members.end());
    return file;
}

}
