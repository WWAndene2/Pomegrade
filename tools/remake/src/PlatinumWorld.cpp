#include "PlatinumWorld.h"
#include "NitroCompression.h"

namespace remake
{

static Bytes Plain(const Bytes& data) { return IsLzCompressed(data) ? LzDecompress(data) : data; }

static Narc Archive(const NdsRom& rom, const char* path)
{
    const NdsFile* f = rom.Find(path);
    if (!f) throw FormatError(std::string("no ") + path + " in the cartridge (not Platinum?)");
    return Narc(Plain(rom.Read(*f)));
}

static Narc Lands(const NdsRom& rom) { return Archive(rom, "fielddata/land_data/land_data.narc"); }

static MapMatrix ReadMatrix(const Narc& matrices, size_t matrix)
{
    if (matrix >= matrices.Count()) throw FormatError("no map matrix " + std::to_string(matrix));
    return MapMatrix::Read(Plain(matrices.Member(matrix)));
}

const Tex0* PlatinumWorld::Set(const Narc& narc, uint16_t id, std::map<uint16_t, Bytes>& files, std::map<uint16_t, std::unique_ptr<Tex0>>& sets)
{
    if (!sets.count(id))
    {
        sets[id] = nullptr;
        if (id < narc.Count())
        {
            files[id] = Plain(narc.Member(id));
            const long at = Tex0::Find(files[id]);
            if (at >= 0) sets[id] = std::make_unique<Tex0>(files[id], (size_t)at);
        }
    }
    return sets[id].get();
}

PlatinumWorld::PlatinumWorld(const NdsRom& rom, size_t matrixIndex)
    : Matrix(matrixIndex),
      Areas(Archive(rom, "fielddata/areadata/area_data.narc")),
      Events(Archive(rom, "fielddata/eventdata/zone_event.narc")),
      MapTextures(Archive(rom, "fielddata/areadata/area_map_tex/map_tex_set.narc")),
      BuildingTextures(Archive(rom, "fielddata/areadata/area_build_model/areabm_texset.narc")),
      BuildingModels(Archive(rom, "fielddata/build_model/build_model.narc")),
      World(ReadMatrix(Archive(rom, "fielddata/mapmatrix/map_matrix.narc"), matrixIndex), Lands(rom))
{
    const Narc matrices = Archive(rom, "fielddata/mapmatrix/map_matrix.narc");
    Headers = FindMapHeaders(rom.Arm9(), Areas.Count(), matrices.Count(), Events.Count(), &HeaderTableAt);

    // each cell's textures: its zone's area (the cell's map header; matrices without headers: the first zone using the matrix)
    int defaultZone = -1;
    for (size_t h = 0; h < Headers.size() && defaultZone < 0; h++) if (Headers[h].Matrix == Matrix) defaultZone = (int)h;
    CellTex.resize(World.Cells.size());
    for (size_t c = 0; c < CellTex.size(); c++)
    {
        const int zone = World.Matrix.Headers[c] >= 0 ? World.Matrix.Headers[c] : defaultZone;
        if (zone < 0 || (size_t)zone >= Headers.size() || Headers[zone].Area >= Areas.Count()) continue;
        const AreaData area = AreaData::Read(Plain(Areas.Member(Headers[zone].Area)));
        CellTex[c] = {Set(MapTextures, area.MapTextures, MapFiles, MapSets), Set(BuildingTextures, area.BuildingSet, BuildingFiles, BuildingSets)};
    }

    for (size_t h = 0; h < Headers.size(); h++)
    {
        if (Headers[h].Matrix != Matrix) continue;
        const ZoneEvents ev = ZoneEvents::Read(Plain(Events.Member(Headers[h].Events)));
        for (size_t i = 0; i < ev.Warps.size(); i++) Warps.push_back({ev.Warps[i], (uint16_t)h, (uint16_t)i});
    }
}

}
