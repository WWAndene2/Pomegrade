#include "OrasXyWorld.h"
#include "BinLinker.h"
#include "GameText.h"
#include "Garc.h"
#include "N3dsRom.h"
#include "NitroCompression.h"
#include "OrasMatrix.h"
#include "OrasNewZone.h"
#include "OrasTown.h"
#include "OrasXyEncounters.h"
#include "OrasXyEvents.h"
#include "OrasXyPiece.h"
#include "OrasXyScript.h"
#include "OrasZone.h"

#include <algorithm>
#include <cstdio>
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
    std::map<int, std::vector<int>> packModels; // X/Y pack -> the Omega Ruby models its zones show
    for (const int z : taken)
    {
        events[z] = XyEventsToOras(ws, z, 0, 0, numbers, modelOf, o.NamesFile, o.ScriptDir, numbers.Zones.at(z).Zone, log, o.Story);
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
        // the donor's light (file 4), camera (6) and the rest, but not its props, textures (0, 1, 11) nor material animations (2):
        // Littleroot's, beside X/Y's of the same names, left some of Santalune's meshes black (OpenGL) or noisy (software
        // renderer) and renamed X/Y's textures (run hZ12gl); on placeholder pack 0's files instead (its few textures, no
        // animation) they drew right (run hD0, 19 October). Which of the two did it is not separated
        BinLinker donor = BinLinker::Read(Plain(ws.Original("a/0/1/4").Sub((size_t)packDonor[pack])), "AD");
        const BinLinker blank = BinLinker::Read(Plain(ws.Original("a/0/1/4").Sub(0)), "AD");
        // file 0 too: its list of Littleroot's props, whose textures were in file 1, stopped the area loader (run hM2)
        for (const size_t f : {(size_t)0, (size_t)1, (size_t)2, (size_t)11}) donor.Files.at(f) = blank.Files.at(f);
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
            const Bytes piece = XyPieceToOras(xPiece, xPack, (size_t)pack, kage, "", gr.Files.at(4).at(1), gr.Files.at(4).at(0), props, packData.at(pack), log);
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

    // the zones
    std::map<int, uint32_t> masks;
    bool wildTables = false; // a zone with X/Y's encounters, which need its land tables in the engine
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
        f.SpawnX = x.Header[22] / 18.0f;
        f.SpawnZ = x.Header[26] / 18.0f;
        OrasZone zone;
        zone.Header = NewZoneHeader(f);
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
        const Bytes encounters = XyEncountersToOras(BinLinker::Read(Plain(xZones.Sub((size_t)z)), "ZO").Files.at(3), z);
        if (!encounters.empty()) wildTables = true;
        const int added = ws.AddZone(WriteNewZone(zone, encounters), encounters);
        if (added != number) throw FormatError("oras-xy-world: X/Y zone " + std::to_string(z) + " took zone " + std::to_string(added) + ", not " + std::to_string(number));
        if (e.Mask != 0x243) masks[number] = e.Mask;
    }

    // what the engine must be given (oras-engine)
    std::string engine = "oras-engine options: --zone-rows " + std::to_string(ws.NextZone()) + " --story large";
    if (std::any_of(packOf.begin(), packOf.end(), [](const auto& p) { return p.second > 255; })) engine += " --area-packs large";
    if (wildTables) engine += " --wild-tables large";
    for (const auto& [zone, mask] : masks)
    {
        snprintf(line, sizeof line, " --script-mask %d:0x%X", zone, mask);
        engine += line;
    }
    log.push_back(engine);
    return log;
}

}
