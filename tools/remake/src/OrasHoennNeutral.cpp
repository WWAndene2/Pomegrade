#include "OrasHoennNeutral.h"

#include "Amx.h"
#include "Garc.h"
#include "NitroCompression.h"
#include "OrasWorkspace.h"
#include "OrasZone.h"

#include <sstream>

namespace remake
{

namespace
{
Bytes Plain(const Bytes& data) { return IsLzCompressed(data) ? LzDecompress(data) : data; }

constexpr int SceneActor = 2000, RockSmash = 10050, Cut = 10052, Strength = 10054;
}

Bytes ScriptWithoutSceneChanges(const Bytes& script, size_t& dropped)
{
    // the source's lines (AmxSource): GetSceneChangeData (hash #63F02D54) is "const.p.alt N" then the N/4-th data cell on
    // (OrasXyScript.cpp SceneChangeWorkCells, read on X/Y's zones; the same in Omega Ruby's, Zone_LoadZoneScript 0x3FF538
    // reading (case, work, value) triples until a case -1)
    dropped = 0;
    if (script.empty()) return script;
    const std::string source = AmxSource(script);
    std::vector<std::string> text;
    std::istringstream in(source);
    for (std::string l; std::getline(in, l);) text.push_back(l);
    const auto words = [&](size_t i)
    {
        std::vector<std::string> w;
        std::istringstream s(text[i].substr(0, text[i].find(';')));
        for (std::string x; s >> x;) w.push_back(x);
        return w;
    };
    std::string label;
    for (size_t i = 0; i < text.size(); i++)
    {
        const auto w = words(i);
        if (w.size() >= 3 && w[0] == "public" && w[1] == "#63F02D54") label = w[2] + ":";
    }
    if (label.empty()) return script;
    size_t first = SIZE_MAX;
    for (size_t i = 0; i + 2 < text.size(); i++)
    {
        const auto w = words(i);
        if (w.empty() || w[0] != label) continue;
        const auto proc = words(i + 1), load = words(i + 2);
        if (proc.empty() || proc[0] != "proc" || load.size() != 2 || load[0] != "const.p.alt") return script;
        first = std::stoul(load[1], nullptr, 0) / 4;
        break;
    }
    if (first == SIZE_MAX) return script;
    // the data cells, each line's place: the first triple's case cell made -1
    size_t cell = 0;
    bool data = false;
    for (size_t i = 0; i < text.size(); i++)
    {
        const auto w = words(i);
        if (!w.empty() && w[0] == "data") { data = true; continue; }
        if (!data || w.empty() || w[0] != "cell") continue;
        if (first < cell + w.size() - 1)
        {
            std::vector<std::string> cells(w.begin() + 1, w.end());
            if (cells[first - cell] == "-1") return script; // no scene
            dropped = 1;
            cells[first - cell] = "-1";
            std::string line = "    cell";
            for (const std::string& c : cells) line += " " + c;
            text[i] = line;
            std::string out;
            for (const std::string& l : text) out += l + "\n";
            return AmxAssemble(out);
        }
        cell += w.size() - 1;
    }
    return script;
}

std::vector<std::string> HoennWithoutStory(OrasWorkspace& ws, int zones)
{
    std::vector<std::string> log;
    Garc& archive = ws.Edited("a/0/1/3");
    size_t people = 0, obstacles = 0, triggers = 0, scenes = 0, changed = 0;
    for (int z = 0; z < zones && (size_t)z < archive.Count(); z++)
    {
        const Bytes raw = archive.Sub((size_t)z);
        OrasZone zone;
        try { zone = OrasZone::Read(Plain(raw)); }
        catch (const FormatError&) { continue; } // not a zone (536: the header table)
        const size_t before = zone.Characters.size() + zone.Triggers.size();
        std::vector<ZoneCharacter> kept;
        for (const ZoneCharacter& c : zone.Characters)
        {
            const int script = c.Raw[5];
            if (script == RockSmash || script == Cut || script == Strength) { obstacles++; continue; }
            if (c.Raw[4] != 0 && script <= SceneActor) { people++; continue; }
            kept.push_back(c);
        }
        zone.Characters = kept;
        triggers += zone.Triggers.size();
        zone.Triggers.clear();
        size_t dropped = 0;
        zone.Script = ScriptWithoutSceneChanges(zone.Script, dropped);
        scenes += dropped;
        if (zone.Characters.size() + zone.Triggers.size() == before && !dropped) continue;
        const Bytes written = zone.Write(Plain(raw));
        archive.Set((size_t)z, IsLzCompressed(raw) ? Lz11Compress(written) : written);
        changed++;
    }
    log.push_back("Hoenn without its story: " + std::to_string(changed) + " zone(s) changed; " + std::to_string(people) +
                  " story character(s), " + std::to_string(obstacles) + " field-move obstacle(s), " + std::to_string(triggers) +
                  " trigger(s) and " + std::to_string(scenes) + " zone scene(s) on entering left out; initialisation scripts kept");
    return log;
}

}
