#include "OrasXyWorld.h"
#include "BinLinker.h"
#include "GameText.h"
#include "Garc.h"
#include "N3dsRom.h"
#include "NitroCompression.h"
#include "OrasMatrix.h"
#include "OrasNewZone.h"
#include "OrasTown.h"
#include "OrasXyBackgrounds.h"
#include "OrasXyEncounters.h"
#include "OrasXyEvents.h"
#include "OrasXyGymModules.h"
#include "OrasXyLight.h"
#include "OrasXyPiece.h"
#include "OrasXySound.h"
#include "OrasXyScript.h"
#include "OrasXySharedScripts.h"
#include "OrasXyTrainers.h"
#include "OrasZone.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <map>

namespace remake
{

static Bytes Plain(const Bytes& data) { return IsLzCompressed(data) ? LzDecompress(data) : data; }

std::vector<std::string> BuildXyWorld(OrasWorkspace& ws, const XyWorldOptions& o)
{
    if (ws.XyRom.empty()) throw FormatError("oras-xy-world needs X/Y's .3ds");
    std::vector<std::string> log;
    char line[256];
    N3dsRom xRom(ws.XyRom);
    const Garc xZones(xRom.Read("a/0/1/2")), xMatrices(xRom.Read("a/0/4/2")), xPieces(xRom.Read("a/0/4/1")), xPacks(xRom.Read("a/0/1/3")),
        xProps(xRom.Read("a/0/2/4"));

    // X/Y's trainers, before the zones' scripts name them (OrasXyTrainers.h)
    Bytes battleTypes, intros;
    std::pair<uint32_t, uint32_t> bandCamera{0, 0};
    for (const std::string& l : AddXyTrainers(ws, o.Sounds, o.Music, o.Sounds && !o.OutDir.empty() ? &battleTypes : nullptr, &intros, &bandCamera)) log.push_back(l);

    // every X/Y zone (the archive's last member is its header table, not a zone), and those taken
    std::map<int, OrasZone> all;
    for (size_t z = 0; z < xZones.Count(); z++)
    {
        const Bytes plain = Plain(xZones.Sub(z));
        if (plain.size() < 2 || plain[0] != 'Z' || plain[1] != 'O') continue;
        all.emplace((int)z, OrasZone::Read(plain));
    }
    std::vector<int> taken;
    for (const auto& [z, zone] : all)
        if (o.Matrices.empty() || o.Matrices.count(zone.Matrix())) taken.push_back(z);
    if (taken.empty()) throw FormatError("oras-xy-world: no X/Y zone on the matrices given");

    // X/Y's story numbers over all its zones (a flag keeps its number in any build), and where each taken zone will be: the
    // zones are appended in X/Y's order, so their numbers are known before any is written
    XyNumbers numbers;
    {
        std::vector<Bytes> scripts;
        std::set<int> vanish, conditions;
        for (const auto& [z, zone] : all)
        {
            for (const ZoneCharacter& c : zone.Characters) if (c.Raw[4]) vanish.insert(c.Raw[4]);
            for (const ZoneTrigger& t : zone.Triggers) conditions.insert(t.Raw[2]);
            for (const ZoneOther& t : zone.Others) conditions.insert(t.Raw[2]);
            for (const Bytes* s : {&zone.Script, &zone.InitScript})
                if (s->size() >= 16) scripts.push_back(XyScriptToOras(*s, o.NamesFile, 0x243).Script);
        }
        // and the X/Y shared scripts ported (OrasXySharedScripts.h), whose flags are the same story's
        for (const Bytes& shared : XySharedScriptsOf(xRom, XySharedScriptMembers(xRom.Code()), o.NamesFile)) scripts.push_back(shared);
        numbers = XyNumbersFor(scripts, vanish, conditions);
    }
    const int firstZone = ws.NextZone();
    for (size_t k = 0; k < taken.size(); k++) numbers.Zones[taken[k]] = {firstZone + (int)k, 0, 0};
    snprintf(line, sizeof line, "X/Y: %zu of its %zu zones taken, as zones %d-%d; its story: %zu flags from 0x%X, %zu works from 0x%X",
             taken.size(), all.size(), firstZone, firstZone + (int)taken.size() - 1, numbers.Flags.size(), XyFirstFlag, numbers.Works.size(), XyFirstWork);
    log.push_back(line);

    // the zones' events first: they convert and append the characters' models, which the area packs must list
    std::map<int, int> modelOf;
    std::map<int, XyEvents> events;
    XyBackgroundNumbers backgroundOf; // X/Y's battle backgrounds met, Omega Ruby's numbers (OrasXyBackgrounds.h)
    // X/Y's shared scripts, before the zones' events name their ids (OrasXySharedScripts.h)
    XyScriptIds scriptIds;
    const Bytes sharedScripts = AddXySharedScripts(ws, xRom, XySharedScriptMembers(xRom.Code()), numbers, o.NamesFile, o.Sounds, o.Music, &backgroundOf, scriptIds, log);
    std::map<int, std::vector<int>> packModels; // X/Y pack -> the Omega Ruby models its zones show
    for (const int z : taken)
    {
        events[z] = XyEventsToOras(ws, z, 0, 0, numbers, modelOf, o.NamesFile, o.ScriptDir, numbers.Zones.at(z).Zone, log, o.Story, o.Sounds, o.Music, &backgroundOf, &scriptIds);
        std::vector<int>& models = packModels[all.at(z).AreaPack()];
        for (const int m : events[z].Models) if (std::find(models.begin(), models.end(), m) == models.end()) models.push_back(m);
    }

    // one Omega Ruby pack per X/Y pack, on the donor of the kind of the first zone using it
    const Garc& orasZones = ws.Original("a/0/1/3");
    const int outdoorDonor = 8, interiorDonor = OrasZone::Read(Plain(orasZones.Sub(223))).AreaPack();
    std::map<int, Bytes> packData;
    std::map<int, int> packDonor;
    for (const int z : taken)
    {
        const int pack = all.at(z).AreaPack();
        if (packData.count(pack)) continue;
        packDonor[pack] = all.at(z).Header[0] == 3 ? interiorDonor : outdoorDonor;
        // the donor's camera (6) and the rest, but not its light (4, X/Y's below), props, textures (0, 1, 11) nor material animations (2):
        // Littleroot's, beside X/Y's of the same names, left some of Santalune's meshes black (OpenGL) or noisy (software
        // renderer) and renamed X/Y's textures (run hZ12gl); on placeholder pack 0's files instead (its few textures, no
        // animation) they drew right (run hD0, 19 October). Which of the two did it is not separated
        BinLinker donor = BinLinker::Read(Plain(ws.Original("a/0/1/4").Sub((size_t)packDonor[pack])), "AD");
        const BinLinker blank = BinLinker::Read(Plain(ws.Original("a/0/1/4").Sub(0)), "AD");
        // file 0 too: its list of Littleroot's props, whose textures were in file 1, stopped the area loader (run hM2)
        for (const size_t f : {(size_t)0, (size_t)1, (size_t)2, (size_t)11}) donor.Files.at(f) = blank.Files.at(f);
        // X/Y's own light (OrasXyLight.h), where the donor's was kept before
        donor.Files.at(4) = XyLightToOras(BinLinker::Read(Plain(xPacks.Sub((size_t)pack)), "AD").Files.at(4), donor.Files.at(4));
        packData[pack] = donor.Write();
    }

    // one matrix per (X/Y matrix, X/Y pack) its zones pair, its pieces converted for that pack at their own cells
    const Garc& orasPieces = ws.Original("a/0/3/9");
    const Bytes kage = BinLinker::Read(Plain(orasPieces.Sub(6)), "GR").Files.at(6);
    std::map<std::pair<int, int>, int> matrixOf;
    for (const int z : taken)
    {
        const std::pair<int, int> group{all.at(z).Matrix(), all.at(z).AreaPack()};
        if (matrixOf.count(group)) continue;
        const auto [xMatrix, pack] = group;
        const Bytes xPack = Plain(xPacks.Sub((size_t)pack));
        OrasMatrix matrix = OrasMatrix::Read(Plain(xMatrices.Sub((size_t)xMatrix)));
        std::set<uint32_t> types;
        for (const uint16_t p : matrix.Pieces)
            if (p != OrasMatrix::None) for (const uint32_t t : XyPieceProps(Plain(xPieces.Sub(p)))) types.insert(t);
        const std::map<uint32_t, uint32_t> props = XyPropsToOras(types, xProps, xPack, (size_t)pack, ws.Edited("a/0/2/3"), ws.Original("a/0/2/3"), packData.at(pack), log);
        for (uint16_t& p : matrix.Pieces)
        {
            if (p == OrasMatrix::None) continue;
            const Bytes xPiece = Plain(xPieces.Sub(p));
            const BinLinker gr = BinLinker::Read(xPiece, "GR");
            const Bytes piece = XyPieceToOras(xPiece, xPack, (size_t)pack, kage, "", gr.Files.at(4).at(1), gr.Files.at(4).at(0), props, packData.at(pack), log, &backgroundOf);
            p = (uint16_t)AppendMember(ws.Edited("a/0/3/9"), orasPieces, 6, piece, "GR");
        }
        for (uint16_t& zone : matrix.Zones)
            if (zone != OrasMatrix::None)
            {
                const auto place = numbers.Zones.find(zone);
                zone = place == numbers.Zones.end() ? OrasMatrix::None : (uint16_t)place->second.Zone;
            }
        const Bytes data = matrix.Write();
        matrixOf[group] = (int)AppendMember(ws.Edited("a/0/4/0"), ws.Original("a/0/4/0"), 1, data, "MM");
        snprintf(line, sizeof line, "X/Y matrix %d with pack %d: a/0/4/0 member %d, %d x %d pieces", xMatrix, pack, matrixOf[group], matrix.Width, matrix.Height);
        log.push_back(line);
    }
    std::map<int, int> packOf;
    for (auto& [pack, data] : packData)
    {
        packOf[pack] = ws.AddAreaPack(data, packDonor.at(pack), packModels[pack]);
        snprintf(line, sizeof line, "X/Y pack %d: area pack %d (a copy of pack %d with its textures and props)", pack, packOf[pack], packDonor.at(pack));
        log.push_back(line);
    }

    // the place names X/Y's zones give, each added once in its eight languages (X/Y's a/0/7/2-9 are Omega Ruby's a/0/7/1-8)
    std::vector<std::vector<std::string>> xNames;
    for (int a = 2; a <= 9; a++) xNames.push_back(ReadGameText(Plain(Garc(xRom.Read("a/0/7/" + std::to_string(a))).Sub(72))));
    std::map<int, int> nameLineOf;

    // their battle backgrounds, X/Y's own (OrasXyBackgrounds.h): each value the taken zones use (header halfword 15, bits 7-13;
    // 0, none given, keeps Littleroot's), the scripts' battles and the pieces' tiles name
    for (const int z : taken) XyBackgroundValue(backgroundOf, (all.at(z).Header[15] >> 7) & 0x7F, true);
    // X/Y's 13 scripted wild battles (CallWildBattle's table, X's DllField 0xEE46C, its literal at 0x1029A7D8 read 23 October:
    // OrasEngine.cpp --xy-static-wild), their backgrounds (+8, +9; 0 none) and battle type (+10) made the mod's
    Bytes staticWild;
    if (!battleTypes.empty())
    {
        const Bytes cro = xRom.Read("DllField.cro");
        constexpr size_t xTable = 0xEE46C, xEntries = 13;
        if (cro.size() < xTable + xEntries * 12) throw FormatError("X/Y's DllField.cro too short for its scripted wild battles");
        staticWild.assign(cro.begin() + (long)xTable, cro.begin() + (long)(xTable + xEntries * 12));
        for (size_t e = 0; e < xEntries; e++)
        {
            uint8_t* entry = &staticWild[12 * e];
            for (const size_t b : {8u, 9u})
                if (entry[b]) entry[b] = (uint8_t)XyBackgroundValue(backgroundOf, entry[b], false);
            if ((size_t)entry[10] * 24 >= battleTypes.size()) throw FormatError("X/Y's scripted wild battle " + std::to_string(e) + ": battle type " + std::to_string(entry[10]) + " not ported");
            entry[10] = (uint8_t)(73 + entry[10]);
        }
    }
    const XyBackgrounds backgrounds = AddXyBackgrounds(ws, xRom, backgroundOf, log);

    // the zones
    std::map<int, uint32_t> masks;
    bool wildTables = false; // a zone with X/Y's encounters, which need its land tables in the engine
    bool largeEvents = false; // a zone whose events file needs oras-engine --events large
    for (const int z : taken)
    {
        const OrasZone& x = all.at(z);
        const XyEvents& e = events.at(z);
        const int number = numbers.Zones.at(z).Zone;
        NewZoneFields f;
        f.Kind = x.Header[0] == 3 ? NewZoneKind::Interior : NewZoneKind::Outdoor;
        f.Number = number;
        f.AreaPack = packOf.at(x.AreaPack());
        f.Matrix = matrixOf.at({x.Matrix(), x.AreaPack()});
        f.Text = e.Text;
        if (f.Kind == NewZoneKind::Interior)
        {
            const auto outside = numbers.Zones.find(x.Header[13]);
            f.Overworld = outside != numbers.Zones.end() ? outside->second.Zone : number;
        }
        const int name = x.Header[14] & 0x3FF;
        if (name && (size_t)name < xNames[0].size())
        {
            if (!nameLineOf.count(name))
            {
                std::vector<std::string> perLanguage;
                for (const auto& lines : xNames) perLanguage.push_back(lines.at((size_t)name));
                nameLineOf[name] = ws.AddPlaceNames(perLanguage);
            }
            f.NameLine = nameLineOf[name];
        }
        if (o.Music && o.Sounds)
        {
            // its music (header words 4-5, an id: OrasXySound.h), X/Y's track through category 0
            const uint32_t xMusic = (uint32_t)x.Header[4] | (uint32_t)x.Header[5] << 16, music = o.Sounds->Translate(xMusic);
            if (music != xMusic) { f.Music = (int)(music & 0xFFFF); f.MusicCategory = (int)(music >> 16); }
            else
            {
                snprintf(line, sizeof line, "zone %d (X/Y's %d): its music 0x%X is none of X/Y's sounds, Omega Ruby's default kept", number, z, xMusic);
                log.push_back(line);
            }
        }
        if (const int field = (x.Header[15] >> 7) & 0x7F) f.BattleField = XyBackgroundValue(backgroundOf, field, true);
        f.SpawnX = x.Header[22] / 18.0f;
        f.SpawnZ = x.Header[26] / 18.0f;
        OrasZone zone;
        zone.Header = NewZoneHeader(f);
        // its area kind (word 19, the module DllField loads for it): Omega Ruby's for a gym whose module is ported
        // (OrasXyGymModules.h; loaded with oras-engine --xy-gyms), none otherwise
        zone.Header[19] = XyGymAreaKind(x.Header[19]);
        zone.Furniture = e.Furniture;
        zone.Characters = e.Characters;
        zone.Triggers = e.Triggers;
        for (size_t k = 0; k < x.Doors.size(); k++)
        {
            const auto to = numbers.Zones.find(x.Doors[k].DestZone());
            if (to == numbers.Zones.end())
            {
                snprintf(line, sizeof line, "zone %d (X/Y's %d): its warp %zu to X/Y's zone %d left out (not taken)", number, z, k, x.Doors[k].DestZone());
                log.push_back(line);
                continue;
            }
            ZoneDoor w = x.Doors[k];
            w.Raw[0] = (uint16_t)to->second.Zone;
            zone.Doors.push_back(w);
        }
        zone.Script = e.Script;
        zone.InitScript = e.InitScript;
        const Bytes encounters = XyEncountersToOras(BinLinker::Read(Plain(xZones.Sub((size_t)z)), "ZO").Files.at(3), z, !battleTypes.empty());
        if (!encounters.empty()) wildTables = true;
        // a zone of many people and triggers (with its story) outgrows the game's events buffer: oras-engine --events large
        OrasZone::EventsLimit = 0x1910;
        if (zone.EventsFile().size() >= OrasZone::EventsBudget) largeEvents = true;
        const int added = ws.AddZone(WriteNewZone(zone, encounters), encounters);
        if (added != number) throw FormatError("oras-xy-world: X/Y zone " + std::to_string(z) + " took zone " + std::to_string(added) + ", not " + std::to_string(number));
        if (e.Mask != 0x243) masks[number] = e.Mask;
    }

    // what the engine must be given (oras-engine)
    std::string engine = "oras-engine options: --zone-rows " + std::to_string(ws.NextZone()) + " --story large";
    // X/Y's characters keep X/Y's move classes (OrasXyMoveCodes.h: the zone port gives the X-only ones codes past 0x3E)
    engine += " --xy-move-codes on";
    if (std::any_of(packOf.begin(), packOf.end(), [](const auto& p) { return p.second > 255; })) engine += " --area-packs large";
    if (wildTables) engine += " --wild-tables large";
    if (wildTables && !battleTypes.empty())
    {
        // X/Y's wild encounter effect entries (OrasXyEncounters.h): its DllField's 12 at 0xEF7D0 (Map_GetAttributeEffectEntry's
        // table, X's literal 0x102E87D0 read 22 October), their battle types (+4, +5 a horde) made the mod's (73 + X/Y's)
        const Bytes cro = xRom.Read("DllField.cro");
        constexpr size_t xTable = 0xEF7D0;
        if (cro.size() < xTable + 72) throw FormatError("X/Y's DllField.cro too short for its encounter effect entries");
        Bytes effects(cro.begin() + (long)xTable, cro.begin() + (long)xTable + 72);
        for (size_t e = 0; e < 12; e++)
            for (const size_t b : {4u, 5u})
            {
                uint8_t& t = effects[6 * e + b];
                if ((size_t)t * 24 >= battleTypes.size()) throw FormatError("X/Y's encounter effect entry " + std::to_string(e) + ": battle type " + std::to_string(t) + " not ported");
                t = (uint8_t)(73 + t);
            }
        const std::string file = (std::filesystem::path(o.OutDir) / "xy_wild_effects.bin").string();
        std::filesystem::create_directories(o.OutDir);
        WriteFile(file, effects);
        engine += " --xy-wild-effects " + file;
    }
    if (!staticWild.empty())
    {
        const std::string file = (std::filesystem::path(o.OutDir) / "xy_static_wild.bin").string();
        std::filesystem::create_directories(o.OutDir);
        WriteFile(file, staticWild);
        engine += " --xy-static-wild " + file;
        // DllField's room for caves (OrasEngine.cpp GrowCroCode): 128 KB, the owner's margin for the ports to come (23 October)
        engine += " --field-room 0x20000";
    }
    if (largeEvents) engine += " --events large";
    // the move codes' caves need DllField's room (the same 128 KB, once)
    if (engine.find(" --field-room ") == std::string::npos) engine += " --field-room 0x20000";
    engine += " --xy-trainers on";
    // X/Y's badges are Kalos's 8-15 in its scripts (XyMoveBadges), whether or not a zone given gives one
    engine += " --badges 16";
    if (!battleTypes.empty())
    {
        const std::string file = (std::filesystem::path(o.OutDir) / "xy_battle_types.bin").string();
        std::filesystem::create_directories(o.OutDir);
        WriteFile(file, battleTypes);
        engine += " --xy-battle-types " + file;
    }
    if (!intros.empty())
    {
        // Kalos's leaders' intros (OrasXyTrainers.h): their types name intros 162 up, which the engine's table must hold
        const std::string file = (std::filesystem::path(o.OutDir) / "xy_intros.bin").string();
        WriteFile(file, intros);
        engine += " --xy-intros " + file;
        if (bandCamera.first) engine += " --xy-band-camera " + std::to_string(bandCamera.first) + ":" + std::to_string(bandCamera.second);
    }
    if (!backgroundOf.Empty())
    {
        // their tables, longer, placed by the engine (OrasEngine.cpp)
        const std::string file = (std::filesystem::path(o.OutDir) / "xy_backgrounds.bin").string();
        std::filesystem::create_directories(o.OutDir);
        WriteFile(file, backgrounds.Tables());
        engine += " --xy-backgrounds " + file;
    }
    // with its story, a new game starts in X/Y zone 2, the player's bedroom in Vaniville (its mirror scene), at its header's
    // spawn (OrasEngine.h StartZone); that X/Y's own new game starts there is inferred, not read in X/Y's code
    if (o.Story && numbers.Zones.count(2)) engine += " --start-zone " + std::to_string(numbers.Zones.at(2).Zone);
    {
        const std::string file = (std::filesystem::path(o.OutDir) / "xy_shared_scripts.bin").string();
        std::filesystem::create_directories(o.OutDir);
        WriteFile(file, sharedScripts);
        engine += " --xy-shared-scripts " + file;
    }
    // the zones' scripts name X/Y's message files as 0x100 + their member (XyMoveMessages)
    engine += " --xy-messages on";
    if (o.Music && o.Sounds)
    {
        snprintf(line, sizeof line, " --xy-sounds %zu:%zu", o.Sounds->First, o.Sounds->Streams.size());
        engine += line;
    }
    for (const auto& [zone, mask] : masks)
    {
        snprintf(line, sizeof line, " --script-mask %d:0x%X", zone, mask);
        engine += line;
    }
    log.push_back(engine);
    return log;
}

}
