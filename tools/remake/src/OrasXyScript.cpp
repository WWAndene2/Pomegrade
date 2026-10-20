#include "OrasXyScript.h"

#include "Amx.h"
#include "OrasXyNatives.h"
#include "OrasXySound.h"

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

// the instructions holding a flag's or work's number given to a flag or work native (its first argument): the push (or
// the constant a push.pri pushes) before the sysreq.n, or, when that is a procedure's parameter (push.s / push.p.s at 12 +
// 4 n), the constant each call of the procedure pushes for it, followed through up to four procedures. `name` gives a
// native index's name. A number computed otherwise is added to `notes` (when given), not moved.
struct Argument { size_t Line; bool Flag; };
std::vector<Argument> NumberArguments(const std::vector<Line>& lines, const std::function<std::string(size_t)>& name, std::vector<std::string>* notes)
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
    // (line of an argument's instruction, flag, depth)
    std::vector<std::tuple<size_t, bool, int>> todo;
    for (size_t i = 0; i < lines.size(); i++)
    {
        if (lines[i].Op != "sysreq.n" || lines[i].Words.size() < 2) continue;
        const std::string n = name(std::stoul(lines[i].Words[1]));
        const bool flag = FlagNatives.count(n), work = WorkNatives.count(n);
        if (!flag && !work) continue;
        const int at = pushed(i, 0);
        if (at < 0) { if (notes) notes->push_back(n + " at source line " + std::to_string(i + 1) + ": its number is computed, not moved"); continue; }
        todo.emplace_back((size_t)at, flag, 0);
    }
    while (!todo.empty())
    {
        const auto [at, flag, depth] = todo.back();
        todo.pop_back();
        const Line& l = lines[at];
        if (Constants.count(l.Op))
        {
            if (done.insert({at, flag}).second) out.push_back({at, flag});
            continue;
        }
        const bool param = (l.Op == "push.s" || l.Op == "push.p.s") && l.Words.size() == 2 && std::stol(l.Words[1], nullptr, 0) >= 12;
        if (!param || depth >= 4 || procOf[at].empty())
        {
            if (notes) notes->push_back(std::string(flag ? "a flag" : "a work") + " at source line " + std::to_string(at + 1) + " is computed, not moved");
            continue;
        }
        const size_t n = (size_t)(std::stol(l.Words[1], nullptr, 0) - 12) / 4;
        for (const size_t call : calls[procOf[at]])
        {
            // the call's argument bytes are pushed last (push.c / push.p.c), the arguments before them
            const int arg = pushed(call, n + 1);
            if (arg < 0) { if (notes) notes->push_back(std::string(flag ? "a flag" : "a work") + " passed at source line " + std::to_string(call + 1) + " is computed, not moved"); continue; }
            todo.emplace_back((size_t)arg, flag, depth + 1);
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
    }
    XyNumbers n;
    int next = XyFirstFlag;
    for (int f : flags) { if (next > XyLastFlag) throw FormatError("X/Y scripts: more flags than 0xD00-0x2C3F holds"); n.Flags[f] = next++; }
    next = XyFirstWork;
    for (int w : works) { if (next > XyLastWork) throw FormatError("X/Y scripts: more event works than 0x4178-0x436B holds"); n.Works[w] = next++; }
    return n;
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
    std::string out;
    for (size_t i = 0; i < text.size(); i++) out += (rewrite.count(i) ? rewrite[i] : text[i]) + "\n";
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
    const auto set = [&](int line, int v, const std::string& why) { text[(size_t)line] = "    " + lines[(size_t)line].Op + " " + std::to_string(v) + "   ; " + why; };
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
        if (zoneLine < 0) { notes.push_back(name + where + ": its zone is computed, not moved"); continue; }
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
                                             "BGMStartBattleEvent", "BGMChangeNowMap"};
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

}

