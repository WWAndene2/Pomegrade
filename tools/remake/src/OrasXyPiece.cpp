#include "OrasXyPiece.h"
#include "Bch.h"
#include "BchTextureFile.h"
#include "BchUpgrade.h"
#include "BchWriter.h"
#include "BinLinker.h"
#include "OrasTown.h"
#include "NitroCompression.h"
#include "XyCharacter.h"

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

std::set<uint32_t> XyPieceProps(const Bytes& xPiece)
{
    const BinLinker gr = BinLinker::Read(xPiece, "GR");
    std::set<uint32_t> types;
    if (gr.Files.size() > 3 && gr.Files[3].size() >= 4)
        for (uint32_t k = 0; k < U32(gr.Files[3], 0); k++) types.insert(U32(gr.Files[3], 4 + 44 * (size_t)k));
    return types;
}

// a model file's texture names, outside the placeholder
static std::set<std::string> TexturesNamed(const Bytes& bch)
{
    std::set<std::string> names;
    for (const BchModel& m : Bch::Read(bch).Models)
        for (const BchMaterial& mat : m.Materials)
            for (const std::string& t : mat.Texture) if (!t.empty() && t != "projection_dummy") names.insert(t);
    return names;
}

// every material slot naming a texture finalName renames, repointed
static Bytes Renamed(Bytes bch, const std::map<std::string, std::string>& finalName)
{
    const Bch read = Bch::Read(bch);
    for (size_t model = 0; model < read.Models.size(); model++)
        for (size_t k = 0; k < read.Models[model].Materials.size(); k++)
            for (int slot = 0; slot < 3; slot++)
            {
                const auto f = finalName.find(read.Models[model].Materials[k].Texture[slot]);
                if (f != finalName.end() && !f->second.empty() && f->second != f->first) bch = BchSetTextureName(bch, model, k, slot, f->second);
            }
    return bch;
}

// X's props bind their textures from the pack's file 1, which in X holds projection_dummy (the placeholder of a material's first
// slot) and in Omega Ruby does not (pack 8: only its file 11): without it X's door did not bind (Map_BuildAreaModels' fatal on
// its entry, run hVan7). The pack's own (pack 8: in its file 11) is added to file 1 under its name; X's, for a pack with none
// (zone 223's, a house's inside)
// The terrain binds its own from file 11: X's houses 2-3 (pieces 164, 166) name it in every material's first slot, and on a copy
// of house 223's pack (none in any file) their piece's model did not set up (Gfx_SetupCharacterModelInstance's fatal, DllField
// 0x102E2F30, run hTr; house 1's piece 163 names none)
static Bytes WithProjectionDummy(const Bytes& packData, size_t slot, const Bytes& xAreaPack, std::vector<std::string>& log)
{
    BinLinker pack = BinLinker::Read(packData, "AD");
    std::vector<BchTextureSource> file1;
    for (const BchTexture& t : Bch::Read(pack.Files.at(slot)).Textures)
    {
        if (t.Name == "projection_dummy") return packData;
        file1.push_back({t.Name, t.Width, t.Height, t.Format, t.Data});
    }
    // looked for in every texture file of the pack, then of X's (X's house pack 74 keeps it in file 11, Omega Ruby's house pack
    // 112 has none)
    std::vector<Bytes> sources[2];
    for (const Bytes& f : pack.Files) if (Bch::Is(f)) sources[0].push_back(f);
    for (const Bytes& f : BinLinker::Read(xAreaPack, "AD").Files) if (Bch::Is(f)) sources[1].push_back(f);
    for (int k = 0; k < 2; k++)
        for (const Bytes& source : sources[k])
        for (const BchTexture& t : Bch::Read(source).Textures)
            if (t.Name == "projection_dummy")
            {
                file1.push_back({t.Name, t.Width, t.Height, t.Format, t.Data});
                pack.Files.at(slot) = BchWriteTextureFile(file1);
                log.push_back("projection_dummy copied to the area pack's file " + std::to_string(slot) + " (X's models name it), from " + (k ? "X's pack" : "its own files"));
                return pack.Write();
            }
    throw FormatError("X props: neither the area pack nor X's has a projection_dummy");
}

std::map<uint32_t, uint32_t> XyPropsToOras(const std::set<uint32_t>& types, const Garc& xProps, const Bytes& xAreaPack, size_t xPackIndex,
                                           Garc& orasProps, const Garc& orasPropsOriginal, Bytes& orasAreaPack, std::vector<std::string>& log)
{
    std::map<uint32_t, uint32_t> to;
    if (types.empty()) return to;
    orasAreaPack = WithProjectionDummy(orasAreaPack, 1, xAreaPack, log);
    const Bytes xTable = BinLinker::Read(xAreaPack, "AD").Files.at(0);
    BinLinker pack = BinLinker::Read(orasAreaPack, "AD");
    Bytes table = pack.Files.at(0);
    uint32_t count = U32(table, 0);
    table.resize(4 + 0x50 * (size_t)count);
    for (const uint32_t type : types)
    {
        // X's entry for the type
        const uint32_t xCount = U32(xTable, 0);
        size_t entry = SIZE_MAX;
        for (uint32_t k = 0; k < xCount; k++) if (U16(xTable, 4 + 0x50 * (size_t)k) == type) entry = 4 + 0x50 * (size_t)k;
        if (entry == SIZE_MAX) throw FormatError("X props: type " + std::to_string(type) + " has no entry in X's area pack " + std::to_string(xPackIndex));
        if (type >= xProps.Count()) throw FormatError("X props: no a/0/2/4 member " + std::to_string(type));
        // its model, made Omega Ruby's, its textures brought into the pack
        std::vector<std::string> report;
        const Bytes raw = xProps.Sub(type);
        const Bytes plainMember = IsLzCompressed(raw) ? LzDecompress(raw) : raw;
        BinLinker bm = BinLinker::Read(XyCharacterToOras(plainMember, report), "BM");
        // files the upgrade could not carry stay BCH 0x07: X's skeletal animations (a door's opening: "dictionary 8 is not
        // empty"), which the game cannot bind (Map_BuildAreaModels' fatal, DllField 0x10269594, run hVan5). The entry's
        // references to them are cleared below: the prop shows, unanimated. Converting them (or rebuilding them on an Omega
        // Ruby template, spica anim) is left to do
        std::set<uint16_t> unconverted;
        for (size_t f = 1; f < bm.Files.size(); f++)
            if (Bch::Is(bm.Files[f]) && bm.Files[f].size() > 4 && bm.Files[f][4] == 0x07) unconverted.insert((uint16_t)f);
        std::set<std::string> wanted;
        for (const Bytes& f : bm.Files) if (Bch::Is(f)) for (const std::string& t : TexturesNamed(f)) wanted.insert(t);
        std::map<std::string, std::string> finalName;
        pack.Files.at(0) = table; // ImportTextures reads and writes the whole pack
        orasAreaPack = pack.Write();
        // into the pack's file 1, where Omega Ruby keeps its doors' textures (pack 8: t101_door, door_light, chip_mado), not the
        // main file 11: with them in 11 the prop's model did not bind (Map_BuildAreaModels' fatal, run hVan6; inferred)
        orasAreaPack = ImportTextures(orasAreaPack, xAreaPack, xPackIndex, {wanted.begin(), wanted.end()}, finalName, log, "_x" + std::to_string(xPackIndex), 1);
        pack = BinLinker::Read(orasAreaPack, "AD");
        for (Bytes& f : bm.Files) if (Bch::Is(f)) f = Renamed(f, finalName);
        const uint32_t member = (uint32_t)AppendMember(orasProps, orasPropsOriginal, 2, bm.Write(), "BM");
        if (member > 0xFFFF) throw FormatError("X props: a/0/2/3 member " + std::to_string(member) + " past a u16 type");
        // its entry, under the new number (the type is the entry's first two u16)
        Bytes e(xTable.begin() + (long)entry, xTable.begin() + (long)entry + 0x50);
        e[0] = e[2] = (uint8_t)member; e[1] = e[3] = (uint8_t)(member >> 8);
        size_t cleared = 0;
        for (size_t r = 0; r < 16; r++)
        {
            const size_t at = 0x0C + 2 * r;
            if (unconverted.count((uint16_t)(e[at] | e[at + 1] << 8))) { e[at] = e[at + 1] = 0xFF; cleared++; }
        }
        table.insert(table.end(), e.begin(), e.end());
        count++;
        to[type] = member;
        log.push_back("X prop type " + std::to_string(type) + ": a/0/2/3 member " + std::to_string(member) + ", " + std::to_string(wanted.size()) + " texture(s) named" +
                      (cleared ? ", " + std::to_string(cleared) + " animation(s) left unbound (X's skeletal animations are not converted)" : ""));
    }
    Put32(table, 0, count);
    table.resize((table.size() + 0x7F) & ~(size_t)0x7F, 0); // the game's files are padded to 0x80 (pack 8's: 4 + 7 x 0x50 in 640 bytes)
    pack.Files.at(0) = table;
    orasAreaPack = pack.Write();
    return to;
}

Bytes XyPieceToOras(const Bytes& xPiece, const Bytes& xAreaPack, size_t xPackIndex, const Bytes& kage, const std::string& modelName,
                    int cellX, int cellY, const std::map<uint32_t, uint32_t>& propTypes, Bytes& orasAreaPack, std::vector<std::string>& log)
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
    {
        bool dummy = false;
        const Bch read = Bch::Read(model); // kept alive while its materials are walked (.at() would end a temporary's life)
        for (const BchMaterial& mat : read.Models.at(0).Materials)
            for (const std::string& t : mat.Texture) dummy = dummy || t == "projection_dummy";
        if (dummy) orasAreaPack = WithProjectionDummy(orasAreaPack, 11, xAreaPack, log);
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

    // file 3: the door and prop models (u32 count, then 44 bytes each: type, scale, rotation, position): each type renumbered
    // through propTypes, the members the caller added to a/0/2/3 for X's (XyPieceProps); a type it has none for is an error
    if (gr.Files[3].size() >= 4)
    {
        const uint32_t count = U32(gr.Files[3], 0);
        for (uint32_t k = 0; k < count; k++)
        {
            const size_t at = 4 + 44 * (size_t)k;
            const auto to = propTypes.find(U32(gr.Files[3], at));
            if (to == propTypes.end()) throw FormatError("X piece: prop type " + std::to_string(U32(gr.Files[3], at)) + " has no Omega Ruby member");
            Put32(gr.Files[3], at, to->second);
        }
        if (count) log.push_back("X piece: " + std::to_string(count) + " door and prop model(s), renumbered");
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
