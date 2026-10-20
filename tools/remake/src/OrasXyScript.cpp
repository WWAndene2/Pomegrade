#include "OrasXyScript.h"

#include "Amx.h"
#include "OrasXyNatives.h"
#include "OrasXySound.h"
#include "OrasXyTrainers.h"

#include <cstdio>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <tuple>

namespace remake
{

const std::vector<std::pair<std::string, std::string>>& XyNativeRenames()
{
    static const std::vector<std::pair<std::string, std::string>> renames = {
        {"FadeRequestIn", "_FadeRequestIn"},
        {"GetFieldTradeData", "FieldTradeGetData"},
        {"EventBGMEnd", "BGMEndEvent"},
        {"ReqLoadSpMotion", "SpMotionReqLoad"},
        {"IsEndLoadSpMotion", "SpMotionLoadIsEnd"},
        {"SetupSpMotion", "SpMotionSetup"},
        {"PlaySpMotion", "SpMotionPlay"},
        {"DeleteSpMotion", "SpMotionDelete"},
        // found on 20 October by the names of X/Y's natives (its tables' strings, hashed): the same words reordered, and the
        // same argument count in both games' scripts (X / Omega Ruby calls: 35 / 117 of none, 12 / 7 of 3, 13 / 13 of 2,
        // 7 / 8 of 3, 4 / 20 of 2, 2 / 4 of 1); PlaySpMotionMove's 7 arguments not compared (no Omega Ruby script calls it)
        {"GetRomVersion", "RomGetVersion"},
        {"GetTrainerInfo", "TrainerGetInfo"},
        {"BattleEventBGMStart", "BGMStartBattleEvent"},
        {"EventBGMChange", "BGMChangeEvent"},
        {"NowMapBGMChange", "BGMChangeNowMap"},
        {"CallPoke3Select", "_CallPoke3Select"},
        {"PlaySpMotionMove", "SpMotionPlayMove"},
        // EventBGMStart, the name of X/Y's #9AB8B7EE (no string of X/Y's: the name guessed from its siblings and checked by its
        // hash), a music id and a fade in frames, as BGMStartEvent's 2 (40 X/Y scripts, 44 Omega Ruby calls)
        {"EventBGMStart", "BGMStartEvent"},
        // X/Y's #BD56411E (X/Y zone 21's trainer, its argument TrainerGetInfo's 3rd word), named by X's DllField string:
        // its code (X 0x1028B688) is instruction for instruction _TrainerBGMPlay's (Omega Ruby 0x102975F4), the music
        // played with a 10-frame fade through the same sound function (X 0x423804, Omega Ruby 0x44D364); read 20 October
        {"BGMTrainerEyeBGM", "_TrainerBGMPlay"},
    };
    return renames;
}

static std::map<uint32_t, std::string> OrasNatives(const std::string& namesFile);

XyScriptNatives XyScriptToOras(const Bytes& xScript, const std::string& namesFile, uint32_t mask)
{
    const auto check = XyNativeCheck(namesFile, mask);
    const std::map<uint32_t, std::string> oras = OrasNatives(namesFile);
    std::map<uint32_t, std::pair<std::string, std::string>> renamed;
    for (const auto& [x, o] : XyNativeRenames()) renamed[AmxNameHash(x)] = {x, o};

    AmxScript s = AmxScript::Read(xScript);
    XyScriptNatives out;
    for (AmxScript::Entry& native : s.Natives)
    {
        std::string name, xName = "?";
        if (const auto r = renamed.find(native.Name); r != renamed.end())
        {
            xName = r->second.first;
            name = r->second.second;
            native.Name = AmxNameHash(name);
            out.Renamed.push_back(xName + " -> " + name);
        }
        else if (const auto o = oras.find(native.Name); o != oras.end()) xName = name = o->second;
        const std::string why = name.empty() ? "not in Omega Ruby's tables" : check(name);
        if (!why.empty())
        {
            char hash[16];
            snprintf(hash, sizeof hash, "#%08X", native.Name);
            out.Missing.push_back(std::string(hash) + " (" + xName + "): " + why);
        }
    }
    out.Script = s.Write();
    return out;
}

// Omega Ruby's native names by hash (function_names.tsv's Script_Native_ rows)
static std::map<uint32_t, std::string> OrasNatives(const std::string& namesFile)
{
    std::map<uint32_t, std::string> oras;
    // with X/Y's own natives the mod gives Omega Ruby (OrasXyNatives.h)
    for (const XyNative& n : XyNatives()) oras[AmxNameHash(n.Name)] = n.Name;
    const Bytes text = ReadFile(namesFile);
    const std::string all(text.begin(), text.end());
    for (size_t at = all.find("\tScript_Native_"); at != std::string::npos; at = all.find("\tScript_Native_", at + 1))
    {
        const size_t q = all.find("native", at), open = all.find('"', q), close = all.find('"', open + 1);
        if (q == std::string::npos || open == std::string::npos || close == std::string::npos) break;
        const std::string name = all.substr(open + 1, close - open - 1);
        oras[AmxNameHash(name)] = name;
    }
    return oras;
}

namespace
{
// one pass over a script's source lines: each sysreq.n's native, and the instruction before it giving its first argument
struct Line { std::string Op; std::vector<std::string> Words; };
const std::set<std::string> FlagNatives = {"FlagSet", "FlagReset", "FlagGet", "FlagSetReq"};
const std::set<std::string> WorkNatives = {"WorkGet", "WorkSet", "WorkAdd", "WorkSub"};
const std::set<std::string> Constants = {"push.c", "push.p.c", "const.pri", "const.p.pri", "const.alt", "const.p.alt"};

std::vector<Line> Split(const std::string& source)
{
    std::vector<Line> lines;
    std::istringstream in(source);
    for (std::string l; std::getline(in, l);)
    {
        Line line;
        std::istringstream w(l.substr(0, l.find(';')));
        for (std::string word; w >> word;) line.Words.push_back(word);
        if (!line.Words.empty()) line.Op = line.Words[0];
        lines.push_back(line);
    }
    return lines;
}

// The scenes a zone starts by itself: Zone_LoadZoneScript (0x3FF538) calls the script's public GetSceneChangeData with 0,
// 1, 2... and keeps (case, work, value) triples until -1, the case run while the work holds the value. In X/Y's 55 zones
// with that public it is "const.p.alt N", then the N/4 + i-th data cell (lidx.p.b 2). Returns the cell indices holding the
// triples' works, empty without the public or that shape. Their works were X/Y's own once moved elsewhere: zone 2's
// opening (case 1 while work 16514 is 0) set the moved work and ran again and again (run cam4, 20 October)
std::vector<size_t> SceneChangeWorkCells(const std::vector<Line>& lines, const std::vector<long>& cells)
{
    std::vector<size_t> out;
    const std::string hash = "#63F02D54"; // AmxNameHash("GetSceneChangeData")
    std::string label;
    for (const Line& l : lines)
        if (l.Op == "public" && l.Words.size() >= 3 && l.Words[1] == hash) label = l.Words[2] + ":";
    if (label.empty()) return out;
    for (size_t i = 0; i + 2 < lines.size(); i++)
    {
        if (lines[i].Op != label) continue;
        if (lines[i + 1].Op != "proc" || lines[i + 2].Op != "const.p.alt" || lines[i + 2].Words.size() != 2) return out;
        const size_t first = std::stoul(lines[i + 2].Words[1], nullptr, 0) / 4;
        for (size_t k = first; k + 2 < cells.size() && cells[k] != -1; k += 3) out.push_back(k + 1);
        return out;
    }
    return out;
}

// the data section's cells, in order
std::vector<long> DataCells(const std::vector<Line>& lines)
{
    std::vector<long> cells;
    bool data = false;
    for (const Line& l : lines)
    {
        if (l.Op == "data") { data = true; continue; }
        if (data && l.Op == "cell") for (size_t k = 1; k < l.Words.size(); k++) cells.push_back(std::stol(l.Words[k], nullptr, 0));
    }
    return cells;
}

// the instructions holding a flag's or work's number given to a flag or work native (its first argument): the push (or
// the constant a push.pri pushes) before the sysreq.n, or, when that is a procedure's parameter (push.s / push.p.s at 12 +
// 4 n), the constant each call of the procedure pushes for it, followed through up to four procedures. `name` gives a
// native index's name. A number computed otherwise is added to `notes` (when given), not moved. `other`: more natives, each
// with the argument (0 the first) whose constants are wanted, returned as Other
struct Argument { size_t Line; bool Flag; bool Other = false; };
std::vector<Argument> NumberArguments(const std::vector<Line>& lines, const std::function<std::string(size_t)>& name, std::vector<std::string>* notes,
                                      const std::map<std::string, size_t>& other = {})
{
    // the procedure each line is in (its label), and each procedure's calls
    std::vector<std::string> procOf(lines.size());
    std::map<std::string, std::vector<size_t>> calls;
    std::string label, proc;
    for (size_t i = 0; i < lines.size(); i++)
    {
        const Line& l = lines[i];
        if (!l.Op.empty() && l.Op.back() == ':') label = l.Op.substr(0, l.Op.size() - 1);
        if (l.Op == "proc") proc = label;
        procOf[i] = proc;
        if (l.Op == "call" && l.Words.size() >= 2) calls[l.Words[1]].push_back(i);
    }
    // the instruction giving the first argument before line `at` (a sysreq.n) or the n-th argument before a call
    const auto pushed = [&](size_t at, size_t n) -> int {
        size_t seen = 0;
        for (size_t k = at; k-- > 0 && at - k <= 24;)
        {
            const std::string& op = lines[k].Op;
            if (op.empty() || op.back() == ':') return -1;
            if (op.rfind("push", 0) != 0) continue;
            if (seen++ < n) continue;
            if (op == "push.pri")
            {
                // a base plus an index (X/Y's ranges: "load.p.s.pri -4", "add.p.c 2962", "push.pri", FlagSet): the base, which
                // XyNumbersFor's offset numbering moves with the whole range
                if (k > 0 && lines[k - 1].Op == "add.p.c" && lines[k - 1].Words.size() == 2) return (int)(k - 1);
                for (size_t j = k; j-- > 0 && k - j <= 3;)
                    if (lines[j].Op == "const.pri" || lines[j].Op == "const.p.pri") return (int)j;
                return -1;
            }
            return (int)k;
        }
        return -1;
    };
    std::vector<Argument> out;
    std::set<std::pair<size_t, bool>> done;
    // (line of an argument's instruction, flag, other, depth)
    std::vector<std::tuple<size_t, bool, bool, int>> todo;
    for (size_t i = 0; i < lines.size(); i++)
    {
        if (lines[i].Op != "sysreq.n" || lines[i].Words.size() < 2) continue;
        const std::string n = name(std::stoul(lines[i].Words[1]));
        const bool flag = FlagNatives.count(n), work = WorkNatives.count(n);
        const auto o = other.find(n);
        if (!flag && !work && o == other.end()) continue;
        const int at = pushed(i, flag || work ? 0 : o->second);
        if (at < 0) { if (notes) notes->push_back(n + " at source line " + std::to_string(i + 1) + ": its number is computed, not moved"); continue; }
        todo.emplace_back((size_t)at, flag, !flag && !work, 0);
    }
    while (!todo.empty())
    {
        const auto [at, flag, isOther, depth] = todo.back();
        todo.pop_back();
        const Line& l = lines[at];
        const std::string what = isOther ? "a number" : flag ? "a flag" : "a work";
        if (Constants.count(l.Op) || (l.Op == "add.p.c" && l.Words.size() == 2 && !isOther))
        {
            if (done.insert({at, flag}).second) out.push_back({at, flag, isOther});
            continue;
        }
        const bool param = (l.Op == "push.s" || l.Op == "push.p.s") && l.Words.size() == 2 && std::stol(l.Words[1], nullptr, 0) >= 12;
        if (!param || depth >= 4 || procOf[at].empty())
        {
            if (notes) notes->push_back(what + " at source line " + std::to_string(at + 1) + " is computed, not moved");
            continue;
        }
        const size_t n = (size_t)(std::stol(l.Words[1], nullptr, 0) - 12) / 4;
        for (const size_t call : calls[procOf[at]])
        {
            // the call's argument bytes are pushed last (push.c / push.p.c), the arguments before them
            const int arg = pushed(call, n + 1);
            if (arg < 0) { if (notes) notes->push_back(what + " passed at source line " + std::to_string(call + 1) + " is computed, not moved"); continue; }
            todo.emplace_back((size_t)arg, flag, isOther, depth + 1);
        }
    }
    return out;
}
}

XyNumbers XyNumbersFor(const std::vector<Bytes>& scripts, const std::set<int>& extraFlags, const std::set<int>& extraWorks)
{
    std::set<int> flags = extraFlags, works;
    for (int w : extraWorks) if (w >= 0x4000 && w < 0x8000) works.insert(w);
    for (const Bytes& script : scripts)
    {
        const AmxScript s = AmxScript::Read(script);
        const std::vector<Line> lines = Split(AmxSource(script));
        const auto name = [&](size_t index) -> std::string {
            if (index >= s.Natives.size()) return "";
            for (const std::string& n : FlagNatives) if (AmxNameHash(n) == s.Natives[index].Name) return n;
            for (const std::string& n : WorkNatives) if (AmxNameHash(n) == s.Natives[index].Name) return n;
            return "";
        };
        for (const Argument& a : NumberArguments(lines, name, nullptr))
        {
            const int v = (int)std::stol(lines[a.Line].Words.back(), nullptr, 0);
            // flag 0 is no flag (a character's word 4 0: always shown); numbered, it gave such people flag 0xD00 (19 October)
            if (a.Flag) { if (v != 0) flags.insert(v); }
            else if (v >= 0x4000 && v < 0x8000) works.insert(v);
        }
        const std::vector<long> cells = DataCells(lines);
        for (const size_t k : SceneChangeWorkCells(lines, cells)) if (cells[k] >= 0x4000 && cells[k] < 0x8000) works.insert((int)cells[k]);
    }
    // by an offset, not packed: X/Y's ranges (a flag or work given as a base plus an index, "add.p.c 2962") stay ranges
    // once their base is moved. X/Y's flags reach 3013 and its works 0x4166 (its whole world, 20 October), within both ranges
    XyNumbers n;
    for (int f : flags)
    {
        // a trainer's "beaten" flag (0x6CC + n in X/Y) is the one oras-engine --xy-trainers gives X/Y's trainer n
        if (f > GameTrainerFlags && f < GameTrainerFlags + XyTrainerCount) { n.Flags[f] = XyTrainerFlagBase + (f - GameTrainerFlags); continue; }
        if (f < 0 || XyFirstFlag + f > XyLastFlag) throw FormatError("X/Y scripts: flag " + std::to_string(f) + " past 0xD00-0x28FF");
        n.Flags[f] = XyFirstFlag + f;
    }
    for (int w : works)
    {
        if (XyFirstWork + (w - 0x4000) > XyLastWork) throw FormatError("X/Y scripts: event work " + std::to_string(w) + " past 0x4178-0x436B");
        n.Works[w] = XyFirstWork + (w - 0x4000);
    }
    return n;
}

const std::set<std::string>& XyDemoNatives()
{
    static const std::set<std::string> natives = {"CallFieldCaptureDemo", "CallFieldNotCaptureDemo", "CallFieldCaptureDemoSimplePoke", "CallCaptureDemo"};
    return natives;
}

std::string XyScriptSource(const Bytes& script, const std::string& namesFile, const XyNumbers& numbers, std::vector<std::string>& notes)
{
    const std::map<uint32_t, std::string> oras = OrasNatives(namesFile);
    const AmxScript s = AmxScript::Read(script);
    const std::string source = AmxSource(script);
    std::vector<Line> lines = Split(source);
    // natives whose result a script waits on: a sysreq.n followed by jnz (the loop left once it returns non-zero; house
    // 2's mirror, X/Y's #F5C84EF0, froze the game as CommandNOP, which returns 0: owner's phone, 19 October)
    std::set<size_t> waitedOn;
    for (size_t i = 0; i + 1 < lines.size(); i++)
        if (lines[i].Op == "sysreq.n" && lines[i].Words.size() >= 2 && lines[i + 1].Op == "jnz") waitedOn.insert(std::stoul(lines[i].Words[1]));
    std::vector<std::string> names; // by native index, as written in the source
    for (size_t index = 0; index < s.Natives.size(); index++)
    {
        const AmxScript::Entry& e = s.Natives[index];
        const auto o = oras.find(e.Name);
        if (o != oras.end() || e.Name == AmxNameHash("_Suspend")) { names.push_back(o != oras.end() ? o->second : "_Suspend"); continue; }
        char hash[16];
        snprintf(hash, sizeof hash, "#%08X", e.Name);
        // a wait on it ends with FadeIsEnd, which returns 1 unless a fade is running (Script_Native_FadeIsEnd 0x370774, in
        // the natives every zone has) and changes nothing; otherwise CommandNOP, which does nothing and returns 0
        const bool waited = waitedOn.count(index);
        names.push_back(waited ? "FadeIsEnd" : "CommandNOP");
        notes.push_back(std::string("native ") + hash + (waited ? ": none in Omega Ruby, FadeIsEnd in its place (a wait on it ends; it does nothing)"
                                                                : ": none in Omega Ruby, CommandNOP in its place (it does nothing)"));
    }
    std::vector<std::string> text;
    {
        std::istringstream in(source);
        for (std::string l; std::getline(in, l);) text.push_back(l);
    }
    std::map<size_t, std::string> rewrite; // line -> new text
    size_t native = 0;
    for (size_t i = 0; i < lines.size(); i++)
    {
        const Line& l = lines[i];
        if (l.Op == "native" && native < names.size())
        {
            const uint32_t h = s.Natives[native].Name;
            char hash[16];
            snprintf(hash, sizeof hash, "#%08X", h);
            const bool standIn = h != AmxNameHash(names[native]) && names[native] != "_Suspend";
            rewrite[i] = "native " + names[native] + (standIn ? std::string("   ; X/Y's ") + hash + ", none in Omega Ruby" : "");
            native++;
            continue;
        }
        if (l.Op == "sysreq.n" && l.Words.size() >= 2)
        {
            const size_t index = std::stoul(l.Words[1]);
            const std::string name = index < names.size() ? names[index] : "?";
            rewrite[i] = text[i] + "   ; " + name;
            // X/Y's cutscenes (owner's decision, 20 October): translated but skipped, their arguments released and 0 as
            // the result, the call kept in a comment; putting it back in an edited script (oras-xy-world --scripts) plays
            // it. Omega Ruby's natives of these names play its own demo of X/Y's number: X/Y zone 160's scene 4,
            // CallFieldNotCaptureDemo(45, 6, 1), loaded DllSequence and read a null effect (0x46C950; runs hK0, hK2)
            if (XyDemoNatives().count(name) && l.Words.size() >= 3)
            {
                rewrite[i] = "    stack.p " + l.Words[2] + "   ; X/Y's demo, skipped; to play it, this line and the next become:"
                             " sysreq.n " + l.Words[1] + " " + l.Words[2] + "   ; " + name + "\n    zero.pri";
                notes.push_back(name + " at source line " + std::to_string(i + 1) + ": X/Y's demo, skipped (kept in a comment)");
            }
        }
    }
    const auto nameOf = [&](size_t index) { return index < names.size() ? names[index] : std::string(); };
    for (const Argument& a : NumberArguments(lines, nameOf, &notes))
    {
        const int v = (int)std::stol(lines[a.Line].Words.back(), nullptr, 0);
        const std::map<int, int>& table = a.Flag ? numbers.Flags : numbers.Works;
        const auto to = table.find(v);
        if (to == table.end()) continue;
        rewrite[a.Line] = "    " + lines[a.Line].Op + " " + std::to_string(to->second) + "   ; X/Y's " + (a.Flag ? "flag " : "work ") + std::to_string(v);
    }
    // a work given to another native (any constant equal to a moved work)
    for (size_t i = 0; i < lines.size(); i++)
    {
        if (rewrite.count(i) || !Constants.count(lines[i].Op) || lines[i].Words.size() != 2) continue;
        const int v = (int)std::stol(lines[i].Words[1], nullptr, 0);
        const auto to = numbers.Works.find(v);
        if (to != numbers.Works.end()) rewrite[i] = "    " + lines[i].Op + " " + std::to_string(to->second) + "   ; X/Y's work " + std::to_string(v);
    }
    // the works of the scenes the zone starts by itself (GetSceneChangeData's table), the data written again, 8 cells a line
    std::vector<long> cells = DataCells(lines);
    size_t movedScenes = 0;
    for (const size_t k : SceneChangeWorkCells(lines, cells))
    {
        const auto to = numbers.Works.find((int)cells[k]);
        if (to != numbers.Works.end()) { cells[k] = to->second; movedScenes++; }
    }
    if (movedScenes)
    {
        notes.push_back("scene changes: " + std::to_string(movedScenes) + " work(s) moved");
        size_t data = 0;
        while (data < lines.size() && lines[data].Op != "data") data++;
        for (size_t i = data + 1; i < lines.size(); i++) if (lines[i].Op == "cell") rewrite[i] = "";
        std::string block;
        for (size_t k = 0; k < cells.size(); k++) block += (k % 8 ? " " : (k ? "\n    cell " : "    cell ")) + std::to_string(cells[k]);
        rewrite[data] = text[data] + "\n" + block;
    }
    std::string out;
    for (size_t i = 0; i < text.size(); i++)
    {
        if (rewrite.count(i) && rewrite[i].empty()) continue;
        out += (rewrite.count(i) ? rewrite[i] : text[i]) + "\n";
    }
    return out;
}

std::string XyMoveZones(const std::string& source, const std::map<int, XyZonePlace>& zones, std::vector<std::string>& notes)
{
    const std::vector<Line> lines = Split(source);
    std::vector<std::string> text, names;
    {
        std::istringstream in(source);
        for (std::string l; std::getline(in, l);) text.push_back(l);
    }
    for (const Line& l : lines) if (l.Op == "native" && l.Words.size() >= 2) names.push_back(l.Words[1]);
    // the instruction pushing the n-th argument of the sysreq.n at `at` (the last push is the first argument), -1 when not
    // a constant push in the same block
    const auto argument = [&](size_t at, size_t n) -> int {
        size_t seen = 0;
        for (size_t k = at; k-- > 0 && at - k <= 32;)
        {
            const std::string& op = lines[k].Op;
            if (op.empty() || op.back() == ':') return -1;
            if (op.rfind("push", 0) != 0) continue;
            if (seen++ < n) continue;
            return (op == "push.c" || op == "push.p.c") && lines[k].Words.size() == 2 ? (int)k : -1;
        }
        return -1;
    };
    const auto value = [&](int line) { return (int)std::stol(lines[(size_t)line].Words[1], nullptr, 0); };
    std::set<size_t> moved; // lines already rewritten
    const auto set = [&](int line, int v, const std::string& why) {
        text[(size_t)line] = "    " + lines[(size_t)line].Op + " " + std::to_string(v) + "   ; " + why;
        moved.insert((size_t)line);
    };
    std::vector<std::string> viaProcedure; // map changes whose zone is not a constant pushed beside them
    const std::set<std::string> withTile = {"_MapChangeCore", "_MapChangeAndDemoCall"}, zoneOnly = {"_MapChangeRailCore"},
        unread = {"EventMapReplace", "CallMapConnect", "GymEsperWarpEvent"};
    for (size_t i = 0; i < lines.size(); i++)
    {
        if (lines[i].Op != "sysreq.n" || lines[i].Words.size() < 2) continue;
        const size_t index = std::stoul(lines[i].Words[1]);
        const std::string name = index < names.size() ? names[index] : "";
        const std::string where = " at source line " + std::to_string(i + 1);
        if (unread.count(name)) { notes.push_back(name + where + ": uses X/Y's zone numbers, not moved"); continue; }
        // PlayerGetZoneID's result compared with constants right after it (X/Y's 27 calls: "eq.p.c.pri 89", "jnz"), each a
        // zone moved; a result stored or computed on is noted, not followed
        if (name == "PlayerGetZoneID")
        {
            bool compared = false;
            for (size_t k = i + 1; k < lines.size() && k <= i + 3; k++)
            {
                const std::string& op = lines[k].Op;
                const bool compare = (op.rfind("eq.", 0) == 0 || op.rfind("neq.", 0) == 0 || op.rfind("const.", 0) == 0) && lines[k].Words.size() == 2;
                if (!compare) break;
                compared = true;
                const int xZone = value((int)k);
                const auto place = zones.find(xZone);
                if (place == zones.end()) { notes.push_back(name + where + ": X/Y zone " + std::to_string(xZone) + " it is compared with is not in this mod, kept"); break; }
                set((int)k, place->second.Zone, "X/Y's zone " + std::to_string(xZone));
                if (op.rfind("const.", 0) == 0) continue; // a comparison's operand, then its compare
                break;
            }
            if (!compared) notes.push_back(name + where + ": its result is not compared at once, not moved");
            continue;
        }
        if (!withTile.count(name) && !zoneOnly.count(name)) continue;
        const int zoneLine = argument(i, 0);
        if (zoneLine < 0) { viaProcedure.push_back(name + where); continue; }
        const int xZone = value(zoneLine);
        const auto place = zones.find(xZone);
        if (place == zones.end()) { notes.push_back(name + where + ": X/Y zone " + std::to_string(xZone) + " is not in this mod, kept"); continue; }
        set(zoneLine, place->second.Zone, "X/Y's zone " + std::to_string(xZone));
        if (!withTile.count(name) || (place->second.Dx == 0 && place->second.Dz == 0)) continue;
        const int x = argument(i, 1), z = argument(i, 3);
        if (x < 0 || z < 0) { notes.push_back(name + where + ": its tile is computed, not shifted"); continue; }
        set(x, value(x) + place->second.Dx, "X/Y's x " + std::to_string(value(x)));
        set(z, value(z) + place->second.Dz, "X/Y's z " + std::to_string(value(z)));
    }
    // a zone a procedure passes on (X/Y's map-change wrappers, "push.p.s 12" ... "sysreq.n _MapChangeCore"): the constant each
    // of its calls pushes, followed as NumberArguments follows a flag (45 such calls in X/Y's world, 20 October). Its tile is
    // not shifted (the world's zones keep X/Y's tiles: Dx, Dz 0); a zone moved with a shift is noted
    if (!viaProcedure.empty())
    {
        const auto nameOf = [&](size_t index) { return index < names.size() ? names[index] : std::string(); };
        std::vector<std::string> unresolved;
        size_t passed = 0;
        for (const Argument& a : NumberArguments(lines, nameOf, &unresolved, {{"_MapChangeCore", 0}, {"_MapChangeAndDemoCall", 0}, {"_MapChangeRailCore", 0}}))
        {
            if (!a.Other || moved.count(a.Line) || lines[a.Line].Words.size() != 2 || !Constants.count(lines[a.Line].Op)) continue;
            const int xZone = value((int)a.Line);
            const auto place = zones.find(xZone);
            if (place == zones.end()) { notes.push_back("a map change's zone passed by a procedure, X/Y zone " + std::to_string(xZone) + " at source line " + std::to_string(a.Line + 1) + ", is not in this mod, kept"); continue; }
            set((int)a.Line, place->second.Zone, "X/Y's zone " + std::to_string(xZone) + ", passed on to a map change");
            if (place->second.Dx != 0 || place->second.Dz != 0) notes.push_back("a map change's zone passed by a procedure at source line " + std::to_string(a.Line + 1) + ": its tile not shifted");
            passed++;
        }
        for (const std::string& u : unresolved) notes.push_back(u);
        if (!passed) for (const std::string& v : viaProcedure) notes.push_back(v + ": its zone is computed, not moved");
        else notes.push_back(std::to_string(passed) + " zone(s) of map changes passed by procedures moved");
    }
    std::string out;
    for (const std::string& l : text) out += l + "\n";
    return out;
}

int XyMessageMember(int n)
{
    // X's MsgLoad (linked X DllField 0x10291918): "cmp r7, #0x16; ldrlo pc, [pc, r7, lsl #2]", each case a "mov r7, #member"
    static const int member[22] = {0x8E, 0x16, 0x41, 0xDB, 0x60, 0x5C, 0x5E, 0x5F, 0x5D, 0xDD, 0xDC,
                                   0x76, 0x80, 0x74, 0x73, 0x81, 0x3E, 0xDF, 0x01, 0x3D, 0x20, 0x4F};
    return n >= 0 && n < 22 ? member[n] : -1;
}

std::string XyMoveMessages(const std::string& source, const std::map<std::pair<bool, int>, int>& files, std::set<std::pair<bool, int>>& found,
                           std::vector<std::string>& notes)
{
    const std::vector<Line> lines = Split(source);
    std::vector<std::string> text, names;
    {
        std::istringstream in(source);
        for (std::string l; std::getline(in, l);) text.push_back(l);
    }
    for (const Line& l : lines) if (l.Op == "native" && l.Words.size() >= 2) names.push_back(l.Words[1]);
    for (size_t i = 0; i < lines.size(); i++)
    {
        if (lines[i].Op != "sysreq.n" || lines[i].Words.size() < 2) continue;
        const size_t index = std::stoul(lines[i].Words[1]);
        if (index >= names.size() || names[index] != "MsgLoad") continue;
        // its two arguments: the last two pushes (the last push is the first argument), constants, in the same block
        int at[2] = {-1, -1};
        size_t seen = 0;
        for (size_t k = i; k-- > 0 && i - k <= 8 && seen < 2;)
        {
            const std::string& op = lines[k].Op;
            if (op.empty() || op.back() == ':') break;
            if (op.rfind("push", 0) != 0) continue;
            if ((op == "push.c" || op == "push.p.c") && lines[k].Words.size() == 2) at[seen] = (int)k;
            seen++;
        }
        const std::string where = " at source line " + std::to_string(i + 1);
        if (at[0] < 0 || at[1] < 0) { notes.push_back("MsgLoad" + where + ": its file is computed, not moved"); continue; }
        const std::pair<bool, int> file{std::stol(lines[(size_t)at[0]].Words[1], nullptr, 0) != 0, (int)std::stol(lines[(size_t)at[1]].Words[1], nullptr, 0)};
        found.insert(file);
        const std::string name = std::string(file.first ? "story" : "system") + " text file " + std::to_string(file.second);
        const auto to = files.find(file);
        if (to == files.end()) { notes.push_back("MsgLoad" + where + ": X/Y's " + name + " not in the mod, kept"); continue; }
        text[(size_t)at[1]] = "    " + lines[(size_t)at[1]].Op + " " + std::to_string(to->second) + "   ; X/Y's " + name;
    }
    std::string out;
    for (const std::string& l : text) out += l + "\n";
    return out;
}

std::string XyMoveSounds(const std::string& source, const XySounds& sounds, bool streams, std::vector<std::string>& notes)
{
    const std::vector<Line> lines = Split(source);
    std::vector<std::string> text, names;
    {
        std::istringstream in(source);
        for (std::string l; std::getline(in, l);) text.push_back(l);
    }
    for (const Line& l : lines) if (l.Op == "native" && l.Words.size() >= 2) names.push_back(l.Words[1]);
    // the natives taking a sound id first, as X/Y's scripts give them constants (SEPlay 870 times, MEPlay 35, SEStop 6, and
    // the music natives renamed from X/Y's: BGMStartEvent's id then its fade, 0x1001E and 10 in Vaniville's)
    const std::set<std::string> withSound = {"SEPlay", "SEStop", "SEIsPlaying", "MEPlay", "BGMStartEvent", "BGMChangeEvent",
                                             "BGMStartBattleEvent", "BGMChangeNowMap",
                                             // a trainer's music on sight: its id the script's (Script_Native__TrainerBGMPlay
                                             // 0x102975F4 passes it on as it is; X/Y's BGMTrainerEyeBGM renamed to it)
                                             "_TrainerBGMPlay"};
    size_t moved = 0, kept = 0;
    for (size_t i = 0; i < lines.size(); i++)
    {
        if (lines[i].Op != "sysreq.n" || lines[i].Words.size() < 2) continue;
        const size_t index = std::stoul(lines[i].Words[1]);
        if (index >= names.size() || !withSound.count(names[index])) continue;
        // its first argument: the last push before it, a constant, in the same block
        for (size_t k = i; k-- > 0 && i - k <= 8;)
        {
            const std::string& op = lines[k].Op;
            if (op.empty() || op.back() == ':') break;
            if (op.rfind("push", 0) != 0) continue;
            if ((op == "push.c" || op == "push.p.c") && lines[k].Words.size() == 2)
            {
                const uint32_t id = (uint32_t)std::stoul(lines[k].Words[1], nullptr, 0), to = sounds.Translate(id);
                const bool stream = to != id && (to >> 16) == 0;
                if (to == id || (stream && !streams)) { kept++; break; }
                char why[48];
                snprintf(why, sizeof why, "   ; X/Y's sound 0x%X", id);
                text[k] = "    " + op + " " + std::to_string(to) + why;
                moved++;
            }
            break;
        }
    }
    if (moved || kept) notes.push_back("sounds: " + std::to_string(moved) + " of X/Y's ids made Omega Ruby's, " + std::to_string(kept) + " kept");
    std::string out;
    for (const std::string& l : text) out += l + "\n";
    return out;
}


std::string XyMoveTrainers(const std::string& source, std::vector<std::string>& notes)
{
    const std::vector<Line> lines = Split(source);
    std::vector<std::string> text, names;
    {
        std::istringstream in(source);
        for (std::string l; std::getline(in, l);) text.push_back(l);
    }
    for (const Line& l : lines) if (l.Op == "native" && l.Words.size() >= 2) names.push_back(l.Words[1]);
    const auto nameOf = [&](size_t index) { return index < names.size() ? names[index] : std::string(); };
    // _CallTrainerBattleCore(0, trainer, ...): X/Y's scripts call it through a procedure of their own, given the trainer
    // first (Shalour's gym, X/Y zone 21: Korrina 613)
    size_t moved = 0;
    for (const Argument& a : NumberArguments(lines, nameOf, &notes, {{"_CallTrainerBattleCore", 1}}))
    {
        if (!a.Other) continue;
        const int v = (int)std::stol(lines[a.Line].Words.back(), nullptr, 0);
        if (v <= 0 || v >= XyTrainerCount) continue;
        text[a.Line] = "    " + lines[a.Line].Op + " " + std::to_string(XyTrainerToOras(v)) + "   ; X/Y's trainer " + std::to_string(v);
        moved++;
    }
    if (moved) notes.push_back("trainers: " + std::to_string(moved) + " battle(s) given Omega Ruby's numbers of X/Y's trainers");
    std::string out;
    for (const std::string& l : text) out += l + "\n";
    return out;
}

}

