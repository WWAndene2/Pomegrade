#include "TownCheck.h"

#include <map>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace remake
{

// every tile value that at least 5 of the game's 857 pieces use, as `remake_tool oras-catalog` lists them in tiles.tsv
static const uint32_t EstablishedTiles[] = {
    0x01000021, 0x00000020, 0x3D1A0006, 0xD40E0001, 0x070AA020, 0x020A8020, 0x24000024, 0x40180023, 0x411A0006, 0x20004004, 0xE90E0021, 0xE70E0021, 0x00000120,
    0x000001A0, 0x01000121, 0x75020021, 0xDD0E0001, 0xFC0001A1, 0xD4000001, 0xEA0E0021, 0x00006020, 0xEF0E0021, 0x00000060, 0x3D180006, 0xEE0E0021, 0x01000061,
    0x01180021, 0xD7000001, 0x00000030, 0x73020021, 0x27005004, 0x00000028, 0xEA000021, 0x42000006, 0xD70E0001, 0xDE0E0001, 0xEC0E0021, 0xDA000001, 0xE9000021,
    0xD90E0001, 0xE80E0021, 0x000C0020, 0xEF000021, 0x951A0002, 0x72020021, 0xDA0E0001, 0xED0E0021, 0x01000031, 0xDE000001, 0x6C000020, 0x6F000021, 0x961A0002,
    0x20084004, 0x00000004, 0x971A0002, 0x401A0023, 0xE7000021, 0x070AA030, 0x25000024, 0x30020021, 0x61000020, 0xD4000041, 0xD50E0001, 0xFE0001A1, 0x01680021,
    0x1F004004, 0x3D520006, 0x3D680006, 0x3F180003, 0x40680023, 0xD80E0001, 0xD9000001, 0x02568020, 0x0A00B000, 0xD9000041, 0xDD000001, 0xDF000041, 0xFD0001A1,
    0x00006028, 0x00006060, 0x01520021, 0x2B568024, 0x3D6A0006, 0x616C0020, 0x6E000021, 0xD8000001, 0x09009000, 0x3D1A000E, 0x69000020, 0x6A000020, 0xD40E0041,
    0xDF000001, 0xE40E0021, 0xE50E0021};

bool TileValueEstablished(uint32_t value)
{
    return std::find(std::begin(EstablishedTiles), std::end(EstablishedTiles), value) != std::end(EstablishedTiles);
}

std::vector<TownIssue> CheckLayout(const Bytes& tiles, const Bytes& doors)
{
    std::vector<TownIssue> issues;
    if (tiles.size() < 4 + 1600 * 4 || U16(tiles, 0) != 40 || U16(tiles, 2) != 40)
        issues.push_back({true, "the tile block is not 40 x 40 tiles (every one of the game's 857 pieces is)"});
    else
    {
        std::map<uint32_t, int> unseen;
        for (int i = 0; i < 1600; i++) { const uint32_t v = U32(tiles, 4 + i * 4); if (!TileValueEstablished(v)) unseen[v]++; }
        for (const auto& [value, n] : unseen)
        {
            char text[160];
            snprintf(text, sizeof text, "tile value 0x%08X (%d tiles) is not one of the 94 values the game uses in at least 5 pieces: its effect is unknown", value, n);
            issues.push_back({false, text});
        }
    }
    if (doors.size() >= 4)
    {
        const uint32_t count = U32(doors, 0);
        if (4 + (size_t)count * 44 > doors.size()) issues.push_back({true, "the door block holds " + std::to_string(count) + " doors but is only " + std::to_string(doors.size()) + " bytes"});
        else
            for (uint32_t k = 0; k < count; k++)
            {
                const size_t o = 4 + k * 44;
                auto f = [&](size_t at) { const uint32_t v = U32(doors, at); float x; memcpy(&x, &v, 4); return x; };
                if (f(o + 4) != 1.0f || f(o + 8) != 1.0f || f(o + 12) != 1.0f)
                    issues.push_back({false, "door model " + std::to_string(k) + " is scaled (" + std::to_string(f(o + 4)) + "): the game's 368 door models are all at scale 1"});
                const float rotation = f(o + 20);
                if (std::fmod(std::fabs(rotation), 90.0f) > 0.01f) issues.push_back({false, "door model " + std::to_string(k) + " is turned " + std::to_string(rotation) + " degrees, not a multiple of 90"});
                const float x = f(o + 28), z = f(o + 36);
                if (std::fabs(std::fmod(std::fabs(x), 18.0f) - 9.0f) > 0.01f || std::fabs(std::fmod(std::fabs(z), 18.0f) - 9.0f) > 0.01f)
                    issues.push_back({false, "door model " + std::to_string(k) + " is not at a tile's centre (x " + std::to_string(x) + ", z " + std::to_string(z) + ")"});
            }
    }
    return issues;
}

std::vector<TownIssue> CheckMaterials(const BchModel& model, const std::set<std::string>& available)
{
    std::vector<TownIssue> issues;
    std::set<std::string> reported;
    for (const BchMesh& mesh : model.Meshes)
    {
        if (mesh.Triangles.empty() || mesh.Material >= model.Materials.size()) continue; // draws nothing
        const BchMaterial& material = model.Materials[mesh.Material];
        for (const std::string& name : material.Texture)
        {
            if (name.empty() || name == "projection_dummy" || available.count(name)) continue;
            if (!reported.insert(material.Name + "/" + name).second) continue;
            issues.push_back({true, "material " + material.Name + " names texture " + name + ", which the area pack does not hold"});
        }
    }
    return issues;
}

std::vector<TownIssue> CheckBudget(const BchModel& model, size_t fileBytes, const PieceBudget& original)
{
    std::vector<TownIssue> issues;
    size_t vertices = 0;
    for (size_t i = 0; i < model.Meshes.size(); i++)
    {
        const BchMesh& mesh = model.Meshes[i];
        vertices += mesh.Vertices.size();
        if (mesh.Vertices.size() > 65536) issues.push_back({true, "mesh " + std::to_string(i) + " has " + std::to_string(mesh.Vertices.size()) + " vertices, more than 16-bit indices reach"});
    }
    if (original.MaxVertices && vertices > original.MaxVertices)
        issues.push_back({false, std::to_string(vertices) + " vertices, more than the game's largest piece (" + std::to_string(original.MaxVertices) + "): memory use is untested"});
    if (original.MaxFileBytes && fileBytes > original.MaxFileBytes)
        issues.push_back({false, "piece of " + std::to_string(fileBytes) + " bytes, larger than the game's largest (" + std::to_string(original.MaxFileBytes) + ")"});
    return issues;
}

}
