#include "OrasXyEvents.h"
#include "OrasXyMoveCodes.h"
#include "Amx.h"
#include "Garc.h"
#include "N3dsRom.h"
#include "NitroCompression.h"
#include "OrasNewZone.h"
#include "OrasTown.h"
#include "OrasWorkspace.h"
#include "OrasXyNatives.h"
#include "OrasXySharedScripts.h"
#include "OrasXySound.h"
#include "OrasXyTrainers.h"
#include "XyCharacter.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <sstream>

namespace remake
{

static Bytes Plain(const Bytes& data) { return IsLzCompressed(data) ? LzDecompress(data) : data; }

// the function a case of the zone script's main dispatch calls (XyScriptSource's text: "Lc:", "push.p.c 0", "call Lf"), its
// body from "Lf:" to its first retn; empty when the case or the shape is not there
static std::pair<size_t, size_t> CaseFunction(const std::string& source, int value)
{
    size_t table = std::string::npos, best = 0;
    for (size_t at = source.find("casetbl "); at != std::string::npos; at = source.find("casetbl ", at + 1))
    {
        const size_t end = source.find('\n', at);
        if (end - at > best) { best = end - at; table = at; }
    }
    if (table == std::string::npos) return {0, 0};
    std::istringstream entries(source.substr(table, source.find('\n', table) - table));
    std::string word, label, count, fallback;
    entries >> word >> count >> fallback;
    for (std::string c, l; entries >> c >> l;) if (c == std::to_string(value)) label = l;
    if (label.empty()) return {0, 0};
    const size_t at = source.find("\n" + label + ":\n");
    if (at == std::string::npos) return {0, 0};
    const size_t call = source.find("call ", at), next = source.find(":\n", at + label.size() + 3);
    if (call == std::string::npos || call > next) return {0, 0};
    const std::string target = source.substr(call + 5, source.find('\n', call) - call - 5);
    const size_t start = source.find("\n" + target + ":\n");
    if (start == std::string::npos) return {0, 0};
    const size_t retn = source.find("    retn\n", start);
    return retn == std::string::npos ? std::pair<size_t, size_t>{0, 0} : std::pair<size_t, size_t>{start + 1, retn};
}

// X/Y's step events whose case only asks its parent flag (ParentFlagSetReq) open what the next case does when that flag is
// set: Vaniville's gate (zone 0, case 12 asks, case 13 tests ParentFlagGet, opens the gate and sets work 16496, ending the
// event). How X/Y then runs case 13 is not read; Omega Ruby does not (run hGate, 20 October: the gate stayed shut). So the
// trigger runs the next case, its flag test removed. Triggers from `first` on are the step events
static std::string XyParentFlagSteps(std::string source, std::vector<ZoneTrigger>& triggers, size_t first, int xZone, std::vector<std::string>& log)
{
    for (size_t k = first; k < triggers.size(); k++)
    {
        ZoneTrigger& t = triggers[k];
        const auto [askAt, askEnd] = CaseFunction(source, t.Raw[0]);
        if (askEnd == 0) continue;
        const std::string ask = source.substr(askAt, askEnd - askAt);
        if (ask.find("; ParentFlagSetReq") == std::string::npos) continue;
        bool onlyAsks = true;
        for (size_t at = ask.find("sysreq"); at != std::string::npos; at = ask.find("sysreq", at + 1))
        {
            const std::string line = ask.substr(at, ask.find('\n', at) - at);
            if (line.find("; ParentFlagSetReq") == std::string::npos && line.find("; CommandNOP") == std::string::npos) onlyAsks = false;
        }
        const auto [openAt, openEnd] = CaseFunction(source, t.Raw[0] + 1);
        if (!onlyAsks || openEnd == 0) continue;
        // the test: "push.p.c F", "sysreq.n N 4   ; ParentFlagGet", "eq.p.c.pri 1", "jzer L" right after "proc"
        const std::string open = source.substr(openAt, openEnd - openAt);
        const std::string test = "; ParentFlagGet\n    eq.p.c.pri 1\n    jzer ";
        const size_t get = open.find(test);
        const size_t proc = open.find("    proc\n");
        if (get == std::string::npos || proc == std::string::npos) continue;
        const size_t testStart = proc + 9, testEnd = open.find('\n', get + test.size()) + 1;
        if (open.substr(testStart, 9) != "    push.") continue;
        source.erase(openAt + testStart, testEnd - testStart);
        char line[160];
        snprintf(line, sizeof line, "X/Y zone %d: step event of case %d (asks its parent flag) runs case %d, its flag test removed", xZone, t.Raw[0], t.Raw[0] + 1);
        log.push_back(line);
        t.Raw[0] = (uint16_t)(t.Raw[0] + 1);
    }
    return source;
}

Bytes XyAssembleScript(OrasWorkspace& ws, N3dsRom& xRom, std::string source, const XyNumbers& numbers, const std::string& namesFile,
                       const XySounds* sounds, bool music, XyBackgroundNumbers* backgrounds, const XyScriptIds* scriptIds,
                       const std::function<std::string(const std::string&)>& edit, const std::string& what, uint32_t& mask,
                       std::vector<std::string>& notes, std::vector<std::string>& log)
{
    source = XyMoveZones(source, numbers.Zones, notes);
    if (sounds) source = XyMoveSounds(source, *sounds, music, notes);
    source = XyMoveTrainers(source, notes);
    if (backgrounds) source = XyMoveBackgrounds(source, *backgrounds, notes);
    source = XyMoveBadges(source, notes);
    source = XyMoveWildBattles(source, notes);
    if (scriptIds) source = XyMoveScriptIds(source, [&](int id) { return scriptIds->ToOras(id); }, notes);
    // the message files it loads (MsgLoad's (table, n), XyMessageMember): X/Y's member added once in its eight languages
    // (system text: X/Y's a/0/7/2-9 -> a/0/7/1-8; story text: X/Y's a/0/8/0-7 -> a/0/7/9-a/0/8/6), named 0x100 + the new
    // member (oras-engine --xy-messages on). B's nickname scene read Omega Ruby's file of the same n: an empty nickname
    // (run nk7, 20 October)
    {
        static std::map<std::pair<bool, int>, int> messageOf; // X/Y's file -> MsgLoad's argument; one X/Y game and workspace per run
        std::set<std::pair<bool, int>> found;
        std::vector<std::string> ignored;
        XyMoveMessages(source, messageOf, found, ignored);
        for (const auto& file : found)
        {
            const int xMember = XyMessageMember(file.second);
            if (messageOf.count(file) || xMember < 0) continue;
            std::vector<Bytes> files;
            for (int k = 0; k < 8; k++)
                files.push_back(Plain(Garc(xRom.Read(file.first ? "a/0/8/" + std::to_string(k) : "a/0/7/" + std::to_string(k + 2))).Sub((size_t)xMember)));
            const int member = file.first ? ws.AddZoneTextFiles(files) : ws.AddSystemTextFiles(files);
            messageOf[file] = 0x100 + member;
            char line[160];
            snprintf(line, sizeof line, "X/Y message file %d of its %s text (member 0x%X): member %d, MsgLoad's 0x%X", file.second,
                     file.first ? "story" : "system", xMember, member, 0x100 + member);
            log.push_back(line);
        }
        found.clear();
        source = XyMoveMessages(source, messageOf, found, notes);
    }
    source = edit(source);
    Bytes assembled;
    for (int tries = 0;; tries++)
    {
        try { assembled = AmxAssemble(source, XyNativeCheck(namesFile, mask)); break; }
        catch (const FormatError& err)
        {
            const std::string why = err.what();
            const size_t bit = why.find("registered under bit ");
            if (bit == std::string::npos || tries > 16) throw FormatError(what + ": " + why);
            mask |= (uint32_t)std::stoul(why.substr(bit + 21), nullptr, 0);
        }
    }
    return assembled;
}

XyEvents XyEventsToOras(OrasWorkspace& ws, int xZone, int dx, int dz, const XyNumbers& numbers, std::map<int, int>& modelOf,
                               const std::string& namesFile, const std::string& scriptDir, int orasZone, std::vector<std::string>& log, bool story,
                               const XySounds* sounds, bool music, XyBackgroundNumbers* backgrounds, const XyScriptIds* scriptIds)
{
    N3dsRom xRom(ws.XyRom);
    const Garc xZones(xRom.Read("a/0/1/2"));
    const OrasZone x = OrasZone::Read(Plain(xZones.Sub((size_t)xZone)));
    XyEvents e;
    const auto moved = [](const std::map<int, int>& table, int v) { const auto t = table.find(v); return t == table.end() ? v : t->second; };
    for (ZoneFurniture f : x.Furniture)
    {
        f.Raw[4] = (uint16_t)(f.Raw[4] + dx); f.Raw[5] = (uint16_t)(f.Raw[5] + dz);
        if (scriptIds) f.Raw[0] = (uint16_t)scriptIds->ToOras(f.Raw[0]);
        e.Furniture.push_back(f);
    }
    size_t storyPeople = 0;
    for (ZoneCharacter c : x.Characters)
    {
        // without the story: a character with a vanish flag is one the story shows or hides (zone 2's rivals, 838-842)
        if (!story && c.Raw[4] != 0) { storyPeople++; continue; }
        // X/Y's model number -> its a/0/2/1 member (XyModelMembers: the class models 0x1000 and up, the rivals and the player's
        // mother in zone 2, through X/Y's table; kept as they were, the game stopped entering the zone, run hDoorA, 19 October)
        static std::map<int, int> xyMembers; // one X/Y game per run of the tool
        if (xyMembers.empty()) xyMembers = XyModelMembers(xRom.Read("DllField.cro"));
        const auto numbered = xyMembers.find(c.Raw[1]);
        if (c.Raw[1] >= 0x1000 && numbered == xyMembers.end())
        {
            char m[96];
            snprintf(m, sizeof m, "X/Y zone %d: model 0x%X is in none of X/Y's model numbers", xZone, (int)c.Raw[1]);
            throw FormatError(m);
        }
        const int model = numbered == xyMembers.end() ? c.Raw[1] : numbered->second;
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
        c.Raw[4] = (uint16_t)moved(numbers.Flags, c.Raw[4]);
        // its move code (word 2) Omega Ruby's number of X/Y's class (OrasXyMoveCodes.h: X's 0x38-0x40 differ; the engine
        // needs --xy-move-codes on for the seven past 0x3E)
        c.Raw[2] = (uint16_t)XyMoveCodeToOras(c.Raw[2]);
        // a trainer's script (3000 + its id, 5000 + id for a double's second: ZoneCharacter::TrainerId) names Omega Ruby's
        // number of X/Y's trainer (OrasXyTrainers.h)
        for (const int base : {3000, 5000})
            if ((c.Raw[3] == 1 || c.Raw[3] == 3 || c.Raw[3] == 9) && c.Raw[5] > base && c.Raw[5] < base + XyTrainerCount)
                c.Raw[5] = (uint16_t)(base + XyTrainerToOras(c.Raw[5] - base));
        c.Raw[20] = (uint16_t)(c.Raw[20] + dx); c.Raw[21] = (uint16_t)(c.Raw[21] + dz);
        if (scriptIds) c.Raw[5] = (uint16_t)scriptIds->ToOras(c.Raw[5]);
        e.Characters.push_back(c);
    }
    if (!story && (!x.Triggers.empty() || storyPeople))
        log.push_back("X/Y zone " + std::to_string(xZone) + ": without its story, " + std::to_string(storyPeople) + " character(s) of a vanish flag and " +
                      std::to_string(x.Triggers.size()) + " trigger(s) left out, its init script empty");
    for (ZoneTrigger t : x.Triggers)
    {
        if (!story) break;
        t.Raw[2] = (uint16_t)moved(numbers.Works, t.Raw[2]);
        t.Raw[6] = (uint16_t)(t.Raw[6] + dx); t.Raw[7] = (uint16_t)(t.Raw[7] + dz);
        if (scriptIds) t.Raw[0] = (uint16_t)scriptIds->ToOras(t.Raw[0]);
        e.Triggers.push_back(t);
    }
    // X/Y's fifth kind: step events that run the zone script's case [0] while work [2] equals [1] (zone 57's mirror stripes,
    // 1 SetMirror and 2 ResetMirror; Vaniville's gate, case 12), the layout of a trigger, which Omega Ruby's fifth kind
    // is not (its popup messages, read only by PopupMsgStartEvent). Made triggers: they are not the story, kept without it
    const size_t firstStep = e.Triggers.size();
    for (const ZoneOther& o : x.Others)
    {
        ZoneTrigger t;
        t.Raw = o.Raw;
        t.Raw[2] = (uint16_t)moved(numbers.Works, t.Raw[2]);
        t.Raw[6] = (uint16_t)(t.Raw[6] + dx); t.Raw[7] = (uint16_t)(t.Raw[7] + dz);
        e.Triggers.push_back(t);
    }
    if (!x.Others.empty()) log.push_back("X/Y zone " + std::to_string(xZone) + ": its " + std::to_string(x.Others.size()) + " step event(s) of the fifth kind made triggers");
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
        if (init && !story) source = EmptyInitScript; // the story's scenes on entering a zone
        else if (!scriptDir.empty() && std::filesystem::exists(file))
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
        const auto steps = [&](const std::string& moved) {
            return !init && from == "translated" ? XyParentFlagSteps(moved, e.Triggers, firstStep, xZone, log) : moved;
        };
        uint32_t mask = 0x243;
        const Bytes assembled = XyAssembleScript(ws, xRom, source, numbers, namesFile, sounds, music, backgrounds, scriptIds, steps,
                                                 "X/Y zone " + std::to_string(xZone) + (init ? " init" : "") + " script (" + from + ")", mask, notes, log);
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
}
