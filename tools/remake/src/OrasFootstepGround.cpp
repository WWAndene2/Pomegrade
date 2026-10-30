#include "OrasFootstepGround.h"
#include "Bch.h"
#include "BinLinker.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>

namespace remake
{

namespace
{

constexpr int Tiles = 40;
constexpr float TileUnits = 18; // a piece spans -360..360 units: tile column = x / 18 + 20

}

std::vector<std::string> TileSurfaceTextures(const Bytes& gr)
{
    const BinLinker piece = BinLinker::Read(gr, "GR");
    const Bch bch = Bch::Read(piece.Files.at(1));
    std::vector<std::string> out(Tiles * Tiles);
    if (bch.Models.empty()) return out;
    const BchModel& model = bch.Models[0];
    std::vector<float> top(Tiles * Tiles, -std::numeric_limits<float>::infinity());
    for (const BchMesh& mesh : model.Meshes)
    {
        // opaque meshes only: the blended layers are shadows and covers over the ground, not what is walked on
        if (mesh.Material >= model.Materials.size() || mesh.Layer > 0) continue;
        // its first texture's name; the material's when it names none (rooms indoors: no texture named in the piece)
        const BchMaterial& material = model.Materials[mesh.Material];
        const std::string& texture = material.Texture[0].empty() ? material.Name : material.Texture[0];
        for (size_t t = 0; t + 2 < mesh.Triangles.size(); t += 3)
        {
            const float* a = mesh.Vertices[mesh.Triangles[t]].Position;
            const float* b = mesh.Vertices[mesh.Triangles[t + 1]].Position;
            const float* c = mesh.Vertices[mesh.Triangles[t + 2]].Position;
            // a walkable face looks up: its normal's height over its length (walls and cliff faces left out)
            const float u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
            const float n[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
            const float length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            if (length < 1e-6f || std::fabs(n[1]) < 0.7f * length) continue;
            const float area = u[0] * v[2] - u[2] * v[0];
            const int c0 = std::max(0, (int)std::floor(std::min({a[0], b[0], c[0]}) / TileUnits + Tiles / 2 - 0.5f));
            const int c1 = std::min(Tiles - 1, (int)std::ceil(std::max({a[0], b[0], c[0]}) / TileUnits + Tiles / 2 - 0.5f));
            const int r0 = std::max(0, (int)std::floor(std::min({a[2], b[2], c[2]}) / TileUnits + Tiles / 2 - 0.5f));
            const int r1 = std::min(Tiles - 1, (int)std::ceil(std::max({a[2], b[2], c[2]}) / TileUnits + Tiles / 2 - 0.5f));
            for (int r = r0; r <= r1; r++)
                for (int col = c0; col <= c1; col++)
                {
                    // the tile's centre in the triangle (barycentric weights on x, z), and the triangle's height there
                    const float px = (col - Tiles / 2 + 0.5f) * TileUnits - a[0], pz = (r - Tiles / 2 + 0.5f) * TileUnits - a[2];
                    const float wb = (px * v[2] - pz * v[0]) / area, wc = (u[0] * pz - u[2] * px) / area;
                    if (wb < 0 || wc < 0 || wb + wc > 1) continue;
                    const float y = a[1] + wb * u[1] + wc * v[1];
                    if (y <= top[r * Tiles + col]) continue;
                    top[r * Tiles + col] = y;
                    out[r * Tiles + col] = texture;
                }
        }
    }
    return out;
}

int GroundOfTexture(const std::string& name, FootstepGame game)
{
    std::string t = name;
    for (char& c : t) c = (char)std::tolower((unsigned char)c);
    auto has = [&](std::initializer_list<const char*> words) {
        for (const char* w : words) if (t.find(w) != std::string::npos) return true;
        return false;
    };
    const bool x = game == FootstepGame::X;
    // Read from the games' own tagging (remake_tool piece-grounds over every piece, 30 October): Omega Ruby's ground 4 is
    // drawn chip_kusa / *_kusa, 6 floor01 / floor02 / bridges, 8 sand / sabak_* / chip_des_01, 12 metal; X's 3 plr0201_yuka01
    // (a house's floor: X has no wood sound), 4 tex_sogen / tex_jari / tex_hanyoushizen, 8 bm_w_beach, 11 plr0201_jytan01
    // (carpet), 12 d12_ami (grating). Where the games tag wet sand 10 and tall grass 5, a texture says only sand and grass.
    // The words the names share stand for each: water, walls, roofs, sky and effects first, which stay silent.
    // Not covered: X/Y's outdoor terrain, drawn by projection (projection_dummy, no name to go by), stays at 0
    if (has({"sea", "water", "diving", "suiro", "w124", "wall", "kabe", "saku", "fence", "roof", "sky", "ceiling", "effect", "_eff",
             "dummy", "tree", "plant", "build", "shadow", "waku", "far"}))
        return 0;
    if (has({"metal", "ami0", "tetsu"})) return 12;
    if (has({"bridge", "hasi", "hashi"})) return x ? 3 : 6; // Omega Ruby's bridges are tagged 6 (wood), its ground 7 hardly used
    if (has({"jytan", "jutan", "carpet", "tatami"})) return x ? 11 : 6;
    if (has({"snow", "yuki"})) return x ? 7 : 0;
    if (has({"sand", "sabak", "beach", "suna", "chip_des"})) return 8;
    if (has({"kinoha", "leaf", "ochiba"})) return x ? 6 : 4;
    if (has({"kusa", "grass", "sogen", "shiba", "lawn", "hanyoushizen", "jari"})) return 4;
    if (has({"chip_wood"})) return 4; // Omega Ruby's forest floor
    if (has({"rock", "iwa", "isi", "ishi", "stone", "gake", "cliff", "doukutsu", "cave", "soil", "road", "miti", "michi", "ground",
             "zimen", "jimen", "jime", "town"}))
        return 3;
    if (has({"floor", "yuka", "flooring"})) return x ? 3 : 6;
    return 0;
}

Bytes WithTextureGrounds(const Bytes& gr, FootstepGame game, int* changed)
{
    BinLinker piece = BinLinker::Read(gr, "GR");
    const std::vector<std::string> textures = TileSurfaceTextures(gr);
    Bytes& tiles = piece.Files.at(0);
    int n = 0;
    for (int t = 0; t < Tiles * Tiles && 8 + (size_t)t * 4 <= tiles.size(); t++)
    {
        uint32_t word;
        std::memcpy(&word, &tiles[4 + (size_t)t * 4], 4);
        if ((word >> 12 & 0x1F) != 0) continue;
        const int ground = GroundOfTexture(textures[t], game);
        if (ground == 0) continue;
        word |= (uint32_t)ground << 12;
        std::memcpy(&tiles[4 + (size_t)t * 4], &word, 4);
        n++;
    }
    if (changed) *changed = n;
    return piece.Write();
}

}
