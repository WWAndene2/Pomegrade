#include "N3dsWorld.h"

#include <cmath>
#include <map>
#include <sstream>

namespace remake
{

N3dsWorld N3dsWorld::Translate(const WorldMap& world, float ndsTile, const std::vector<NdsWarp>& warps)
{
    N3dsWorld out;
    out.NdsTile = ndsTile;
    const uint32_t tilesW = world.Matrix.Width * LandTiles, tilesH = world.Matrix.Height * LandTiles;
    out.Width = (tilesW + N3dsMapTiles - 1) / N3dsMapTiles;
    out.Height = (tilesH + N3dsMapTiles - 1) / N3dsMapTiles;

    std::map<std::pair<uint32_t, uint32_t>, N3dsPiece> pieces; // by (y, x): row-major order
    auto piece = [&](uint32_t px, uint32_t py) -> N3dsPiece& {
        N3dsPiece& p = pieces[{py, px}];
        if (p.Permissions.empty())
        {
            p.X = px; p.Y = py;
            p.Permissions.assign(N3dsMapTiles * N3dsMapTiles, -1);
        }
        return p;
    };

    for (uint32_t cy = 0; cy < world.Matrix.Height; cy++)
        for (uint32_t cx = 0; cx < world.Matrix.Width; cx++)
        {
            const size_t c = world.Matrix.Cell(cx, cy);
            const auto& land = world.Cells[c];
            if (!land) continue;
            // the tiles: same index in the whole world's tile grid
            for (uint32_t ty = 0; ty < LandTiles; ty++)
                for (uint32_t tx = 0; tx < LandTiles; tx++)
                {
                    const uint32_t gx = cx * LandTiles + tx, gy = cy * LandTiles + ty;
                    piece(gx / N3dsMapTiles, gy / N3dsMapTiles).Permissions[(gy % N3dsMapTiles) * N3dsMapTiles + gx % N3dsMapTiles] =
                        land->Permissions[ty * LandTiles + tx];
                }
            // the buildings: their place in the world's tile grid (from the DS cell's centre), then
            // from their ORAS piece's centre, in ORAS units
            for (const LandBuilding& b : land->Buildings)
            {
                const float gx = cx * LandTiles + LandTiles / 2.0f + b.Position[0];
                const float gz = cy * LandTiles + LandTiles / 2.0f + b.Position[2];
                const long px = (long)std::floor(gx / N3dsMapTiles), pz = (long)std::floor(gz / N3dsMapTiles);
                if (px < 0 || pz < 0 || px >= (long)out.Width || pz >= (long)out.Height) continue; // outside the world
                N3dsBuilding n;
                n.Model = b.Model;
                n.SourceCell = c;
                n.Position[0] = (gx - (px * N3dsMapTiles + N3dsMapTiles / 2.0f)) * N3dsTileUnits;
                n.Position[1] = b.Position[1] * N3dsTileUnits;
                n.Position[2] = (gz - (pz * N3dsMapTiles + N3dsMapTiles / 2.0f)) * N3dsTileUnits;
                piece((uint32_t)px, (uint32_t)pz).Buildings.push_back(n);
            }
        }

    // warps: a tile each, kept with the tile (same index in the world's grid)
    for (const NdsWarp& w : warps)
    {
        if (w.Warp.X >= tilesW || w.Warp.Z >= tilesH) continue; // outside the world
        N3dsWarp n;
        n.Tile[0] = w.Warp.X % N3dsMapTiles;
        n.Tile[1] = w.Warp.Z % N3dsMapTiles;
        n.Zone = w.Zone; n.Index = w.Index; n.DestZone = w.Warp.DestHeader; n.DestWarp = w.Warp.DestWarp;
        piece(w.Warp.X / N3dsMapTiles, w.Warp.Z / N3dsMapTiles).Warps.push_back(n);
    }

    for (auto& [key, p] : pieces) out.Pieces.push_back(std::move(p));
    return out;
}

std::string N3dsWorld::Json() const
{
    std::ostringstream o;
    o << "{\n  \"format\": \"pomegrade-remake-n3ds-world\", \"version\": 1,\n"
      << "  \"pieceTiles\": " << N3dsMapTiles << ", \"tileUnits\": " << N3dsTileUnits << ", \"ndsTileUnits\": " << NdsTile
      << ", \"scale\": " << Scale() << ",\n"
      << "  \"width\": " << Width << ", \"height\": " << Height << ",\n  \"pieces\": [";
    for (size_t i = 0; i < Pieces.size(); i++)
    {
        const N3dsPiece& p = Pieces[i];
        o << (i ? "," : "") << "\n    {\"x\": " << p.X << ", \"y\": " << p.Y << ", \"permissions\": [";
        for (size_t t = 0; t < p.Permissions.size(); t++) o << (t ? "," : "") << p.Permissions[t];
        o << "], \"buildings\": [";
        for (size_t b = 0; b < p.Buildings.size(); b++)
        {
            const N3dsBuilding& x = p.Buildings[b];
            o << (b ? ", " : "") << "{\"model\": " << x.Model << ", \"position\": [" << x.Position[0] << ", " << x.Position[1] << ", "
              << x.Position[2] << "], \"sourceCell\": " << x.SourceCell << "}";
        }
        o << "], \"warps\": [";
        for (size_t w = 0; w < p.Warps.size(); w++)
        {
            const N3dsWarp& x = p.Warps[w];
            o << (w ? ", " : "") << "{\"tile\": [" << x.Tile[0] << ", " << x.Tile[1] << "], \"zone\": " << x.Zone << ", \"index\": " << x.Index
              << ", \"destZone\": " << x.DestZone << ", \"destWarp\": " << x.DestWarp << "}";
        }
        o << "]}";
    }
    o << "\n  ]\n}\n";
    return o.str();
}

}
