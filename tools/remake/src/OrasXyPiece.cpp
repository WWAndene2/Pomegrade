#include "OrasXyPiece.h"
#include "Bch.h"
#include "BchUpgrade.h"
#include "BchWriter.h"
#include "BinLinker.h"
#include "OrasTown.h"

#include <map>
#include <set>

namespace remake
{

static void Put32(Bytes& b, size_t at, uint32_t v) { for (int k = 0; k < 4; k++) b.at(at + k) = (uint8_t)(v >> (8 * k)); }

uint32_t XyTileValue(uint32_t value)
{
    // Vaniville's four values (pieces 12-15): 0x20, 0x01000021 and 0x28 occur in Omega Ruby's pieces (0x28 on 203 tiles);
    // 0x31000020, one tile in piece 14, in none: made plain walkable, its low bits (inferred: what it does in X is not read)
    switch (value)
    {
    case 0x31000020: return 0x00000020;
    default: return value;
    }
}

Bytes XyPieceToOras(const Bytes& xPiece, const Bytes& xAreaPack, size_t xPackIndex, const Bytes& kage, const std::string& modelName,
                    int cellX, int cellY, Bytes& orasAreaPack, std::vector<std::string>& log)
{
    BinLinker gr = BinLinker::Read(xPiece, "GR");
    if (gr.Files.size() != 6) throw FormatError("X piece: " + std::to_string(gr.Files.size()) + " files, 6 expected");

    // file 0: the tile grid (two u16 40, then 40 x 40 u32)
    Bytes& tiles = gr.Files[0];
    if (tiles.size() < 4 + 1600 * 4) throw FormatError("X piece: a tile grid of " + std::to_string(tiles.size()) + " bytes");
    std::map<uint32_t, size_t> mapped;
    for (size_t at = 4; at < 4 + 1600 * 4; at += 4)
    {
        const uint32_t v = U32(tiles, at), o = XyTileValue(v);
        if (o != v) { Put32(tiles, at, o); mapped[v]++; }
    }
    for (const auto& [v, n] : mapped)
    {
        char line[96];
        snprintf(line, sizeof line, "X tiles: 0x%08X on %zu tile(s) made 0x%08X", v, n, XyTileValue(v));
        log.push_back(line);
    }

    // file 1: the model, upgraded, renamed for its cell, its textures brought into the pack
    Bytes model = BchUpgradeTo21(gr.Files[1]);
    // renamed only when the lengths match (BchReplaceString moves nothing): X's map01_11_14 and the sandbox's world16_00_00 differ,
    // so X's name stays (whether the game reads it is not known: the pieces drawn so far kept their donor's)
    const std::string xName = Bch::Read(model).Models.at(0).Name;
    std::string name = xName;
    if (xName.size() == modelName.size()) { model = BchReplaceString(model, xName, modelName); name = modelName; }
    std::set<std::string> wanted;
    {
        const BchModel m = Bch::Read(model).Models.at(0);
        for (const BchMaterial& mat : m.Materials)
            for (const std::string& t : mat.Texture) if (!t.empty() && t != "projection_dummy") wanted.insert(t);
    }
    std::map<std::string, std::string> finalName;
    orasAreaPack = ImportTextures(orasAreaPack, xAreaPack, xPackIndex, {wanted.begin(), wanted.end()}, finalName, log, "_x" + std::to_string(xPackIndex));
    {
        const BchModel m = Bch::Read(model).Models.at(0);
        for (size_t k = 0; k < m.Materials.size(); k++)
            for (int slot = 0; slot < 3; slot++)
            {
                const std::string& t = m.Materials[k].Texture[slot];
                const auto f = finalName.find(t);
                if (f != finalName.end() && !f->second.empty() && f->second != t) model = BchSetTextureName(model, 0, k, slot, f->second);
            }
    }
    size_t missing = 0;
    for (const auto& [name, final] : finalName) if (final.empty()) missing++;
    log.push_back("X piece: model " + name + ", " + std::to_string(wanted.size()) + " texture(s) named, " + std::to_string(missing) + " not in X's pack " +
                  std::to_string(xPackIndex));
    gr.Files[1] = model;

    // file 3: the door models (u32 count, then 44 bytes each: type, scale, rotation, position), left out. With them, a zone with no
    // warp stopped in Map_PlacePropsFromContainer's fatal (DllField 0x102608AC: the field's door-model manager, [field + 0xAC] + 0x170,
    // null; run hVan3). That the game makes it only for a zone with doors is inferred; X's door types (1, 2, 5, 10 in Vaniville)
    // against Omega Ruby's are not mapped yet
    if (gr.Files[3].size() >= 4)
    {
        const uint32_t doors = U32(gr.Files[3], 0);
        if (doors) log.push_back("X piece: its " + std::to_string(doors) + " door model(s) left out");
        Put32(gr.Files[3], 0, 0);
    }

    // file 4: the cell
    if (gr.Files[4].size() < 2) throw FormatError("X piece: file 4 too short");
    gr.Files[4][0] = (uint8_t)cellY;
    gr.Files[4][1] = (uint8_t)cellX;

    // file 5: tall grass, when there is any
    if (Bch::Is(gr.Files[5])) gr.Files[5] = BchUpgradeTo21(gr.Files[5]);

    gr.Files.push_back(kage);
    return gr.Write();
}

}
