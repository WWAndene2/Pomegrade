#ifndef REMAKE_ORASXYSHAREDSCRIPTS_H
#define REMAKE_ORASXYSHAREDSCRIPTS_H

// X/Y's shared scripts (the scripts a zone's events name by an id of 2000 and up, not its own) given to Omega Ruby (read 23
// October, static). Both games map such an id through a table of id ranges in the .code (16 bytes a row: the first id, the
// last, the member of the shared scripts' archive, the text's kind (1: table 8, the system text), the text file), read by
// Msg_ResolveTextSource (Omega Ruby's 0x3FB230: 86 rows at 0x587678, their archive a/0/2/9 (0x1D), 85 members; X's
// 0x3D9C64: 66 rows at 0x5457A4, its archive a/0/3/1, 59 members), which also takes, by the member, its native mask
// (Omega Ruby's 96 words at 0x587BD8, the rows' end) and its name (0x5F4704). The two games use the same ids for other
// scripts (X's objectencount.amx, member 39, the dust boxes', is ids 10480-10529, which in Omega Ruby are rows 25-29).
// So an X/Y script ported keeps its ids moved by XyScriptIdShift, past every id of both games' (2000-20010) and below
// 60000 (Msg_ResolveTextSource's own 60000-60002): X/Y's land in 32000-50010, and 20011-29999 and 50011-59999 stay free
// for new scripts. Its script is appended to a/0/2/9 (translated as a zone's, XyAssembleScript), its text file to the
// system text (X/Y's a/0/7/2-9 -> a/0/7/1-8), and oras-engine --xy-shared-scripts gives the engine its rows, masks and
// names (the tables copied longer into the room, OrasEngine.cpp). The ids X/Y's zones name (furniture word 0, character
// word 5, trigger word 0: the words holding ids of 2000 and up in X's 361 zones) are moved with it (XyScriptIdToOras).
// Not moved yet: an id a script passes to a native (GlobalCall, ReserveScript...).

#include "Bytes.h"
#include "OrasXyScript.h"

#include <set>
#include <string>
#include <vector>

namespace remake
{

class N3dsRom;
class OrasWorkspace;

constexpr int XyScriptIdShift = 30000;

// the members oras-xy-world ports: objectencount.amx, the dust boxes' (OrasXyDustBox.h); the others are the next step
inline const std::set<int> XyPortedSharedScripts = {39};

struct XyScriptRange
{
    uint16_t First = 0, Last = 0, Member = 0, TextKind = 0, Text = 0;
};

// X/Y's rows (its .code's table)
std::vector<XyScriptRange> XyScriptRanges(const Bytes& xCode);

// the ported ranges' ids (X/Y's) moved; any other id kept
class XyScriptIds
{
public:
    void Add(const XyScriptRange& r) { Ranges.push_back(r); }
    int ToOras(int id) const;
private:
    std::vector<XyScriptRange> Ranges;
};

// X/Y's shared scripts of the given members ported (their rows' ids added to `ids`): the engine's file (oras-engine
// --xy-shared-scripts), "XYSS", the rows' count and rows, then per member its number in a/0/2/9, native mask and name
Bytes AddXySharedScripts(OrasWorkspace& ws, N3dsRom& xRom, const std::set<int>& xMembers, const XyNumbers& numbers,
                         const std::string& namesFile, const struct XySounds* sounds, bool music, struct XyBackgroundNumbers* backgrounds,
                         XyScriptIds& ids, std::vector<std::string>& log);

// a shared script's source edited before it is assembled: the case values of its dispatch in its own ranges moved with its
// ids; objectencount.amx's (member 39) computed can flags moved
std::string XySharedScriptEdits(std::string source, int member, const std::vector<XyScriptRange>& rows);

// X/Y's scripts of those members, translated (for XyNumbersFor)
std::vector<Bytes> XySharedScriptsOf(N3dsRom& xRom, const std::set<int>& xMembers, const std::string& namesFile);

}

#endif // REMAKE_ORASXYSHAREDSCRIPTS_H
