#include "OrasFootstepGround.h"
#include "Bch.h"
#include "BinLinker.h"
#include "OrasXyFootsteps.h"

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
        // its first texture's name, past X/Y's projection_dummy (the ground's own texture in the next unit: kusa01 after it);
        // the material's when it names none (rooms indoors: no texture named in the piece)
        const BchMaterial& material = model.Materials[mesh.Material];
        std::string texture = material.Name;
        for (const std::string& t : material.Texture)
            if (!t.empty() && t != "projection_dummy") { texture = t; break; }
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
    // a word standing alone between underscores, its digits aside (umi in tex_umi_00, not in kanazumi; mat in mat01)
    std::vector<std::string> tokens(1);
    for (char c : t)
        if (c == '_') tokens.emplace_back();
        else if (!std::isdigit((unsigned char)c)) tokens.back() += c;
    auto token = [&](std::initializer_list<const char*> words) {
        for (const char* w : words) for (const std::string& k : tokens) if (k == w) return true;
        return false;
    };
    const bool x = game == FootstepGame::X;
    // Read from the walked tiles' textures of every piece of both games (remake_tool piece-grounds, 30 October) and from
    // what the games tag themselves: Omega Ruby's ground 4 is drawn chip_kusa / *_kusa, 6 floor01 / floor02 / its
    // bridges, 8 sand / sabak_* / chip_des_01, 12 floor_metal; X's 3 plr0201_yuka01, 4 tex_sogen / tex_jari /
    // tex_hanyoushizen, 8 bm_w_beach, 10 shallows, 11 plr0201_jytan01, 12 d12_ami. The other names by their words
    // (Japanese ones: kusa grass, michi / miti path, zimen / jimen ground, arachi / arechi wasteland, hatake field, jari
    // gravel, iwa / isi rock, gake cliff, doukutsu cave, yuka floor, jyutan carpet, kaidan stairs, hasi bridge, kanpan
    // deck, isidatami paving, asase shallows, kinoha leaves, hana flowers, sora sky (Sky Pillar), dan ledge); in that order, the first that matches. Water, walls, roofs,
    // props and effects stay silent; so does what matches nothing
    if (has({"asase"})) return 10;                                    // shallows, both games' ground 10
    if (has({"in_tree_yuka", "woodfloor", "wood_floor"})) return Wood; // before "tree" and "floor"
    // Omega Ruby's floor01, floor02... in its houses: the game tags them 6, wood
    if (!x && t.rfind("floor0", 0) == 0) return Wood;
    if (has({"wall_top", "walltop"})) return Paved;                   // the walked tops of walls
    if (has({"sea", "water", "diving", "suiro", "kawa", "river", "w124", "wall", "kabe", "saku", "fence", "roof", "sky", "ceiling",
             "effect", "_eff", "dummy", "shadow", "waku", "far", "comb", "lambert", "_bg", "table", "counter", "chair", "box", "obj",
             "broom", "warp", "mado", "exit", "tree", "plant", "build", "line", "other", "komono"}) || token({"umi"}))
        return 0;
    if (has({"metal", "iron", "tetsu", "plate"}) || token({"ami"})) return 12;
    if (has({"jytan", "jyutan", "jutan", "carpet", "tatami"}) || token({"mat"})) return Carpet;
    if (has({"snow", "yuki"})) return x ? 7 : Paved;
    if (has({"sand", "sabak", "beach", "suna", "chip_des"})) return 8;
    if (has({"kinoha", "leaf", "ochiba", "chip_wood"})) return ForestFloor; // chip_wood: Omega Ruby's forest floor
    if (has({"bridge", "hasi", "hashi", "kanpan", "flooring", "wood", "harbor", "deck"})) return Wood;
    if (has({"kusa", "grass", "sogen", "shiba", "sibafu", "lawn", "hanyoushizen", "general_green", "hana", "flower"})) return SoftGrass;
    if (has({"isidatami", "road", "load", "tile", "brock", "brick", "renga", "town", "kaidan", "stairs", "steps", "prom", "plaza"}))
        return Paved;
    if (has({"rock", "iwa", "isi", "ishi", "stone", "gake", "cliff", "doukutsu", "cave", "gansho", "hono", "kanashida", "soratop", "_dan"})) return CaveRock;
    // Lumiose City's (c02) ground textures are its pavements
    if (t.rfind("c02", 0) == 0 && has({"ground", "zimen"})) return Paved;
    if (has({"soil", "michi", "miti", "zimen", "jimen", "jime", "ground", "arachi", "arechi", "hatake", "jari", "rail", "top0", "dirt", "tsuchi",
             "hibi", "entotsu", "kureta"}))
        return Dirt;
    if (has({"floor", "foor", "yuka", "_yk", "flr", "gym", "factory"})) return IndoorFloor;
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
        // ground 0 only, on the tiles walked on: kind (bits 24-31) 0, or 0x24 / 0x25 (caves' floors, read from the textures
        // they show: chip_doukutsu02, d108_yuka1); the others are water to surf (0x3D, 0x41, 0x42, 0x95-0x97), counters (0xD4),
        // walls and the void around a map (1)
        const uint32_t kind = word >> 24;
        if ((word >> 12 & 0x1F) != 0 || (kind != 0 && kind != 0x24 && kind != 0x25)) continue;
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
