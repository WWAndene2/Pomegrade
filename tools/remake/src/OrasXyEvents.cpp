#include "OrasXyEvents.h"
#include "Amx.h"
#include "Garc.h"
#include "N3dsRom.h"
#include "NitroCompression.h"
#include "OrasNewZone.h"
#include "OrasTown.h"
#include "OrasWorkspace.h"
#include "XyCharacter.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace remake
{

static Bytes Plain(const Bytes& data) { return IsLzCompressed(data) ? LzDecompress(data) : data; }

XyEvents XyEventsToOras(OrasWorkspace& ws, int xZone, int dx, int dz, const XyNumbers& numbers, std::map<int, int>& modelOf,
                               const std::string& namesFile, const std::string& scriptDir, int orasZone, std::vector<std::string>& log, bool story)
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
        c.Raw[20] = (uint16_t)(c.Raw[20] + dx); c.Raw[21] = (uint16_t)(c.Raw[21] + dz);
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
        source = XyMoveZones(source, numbers.Zones, notes);
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
}
