#include "TownLayout.h"

#include <algorithm>
#include <set>

namespace remake
{

// Platinum's texture names, by what they show. A texture not listed is "unknown" (reported, treated as grass).
// '\0': no role (shadows; a path's outline, drawn over the grass beside it; the base under a pond, where what lies on it decides).
static const std::map<std::string, char>& Roles()
{
    static const std::map<std::string, char> roles = {
        {"conttree_b", 'T'}, {"conttree_t", 'T'}, {"tree01", 't'}, {"tree04_2", 't'}, {"tree04", 't'},
        {"nsand", ':'}, {"hage", ':'},                // hage: a bald (dirt) patch
        {"nsandp", 0},
        {"imped", 'F'}, {"fenter", 'F'},
        {"nhana", '*'},
        {"lake", '~'}, {"sea", '~'}, {"puddle", '~'}, {"lakep", 'f'}, {"puddlep", 'f'}, {"puddle_b", 0},
        {"s_snow", 's'}, {"s_snow02", 's'}, {"s_snow03", 's'}, {"s_snow04", 's'}, {"s_sonwp", 's'}, {"s_snow_lm", 's'},
        {"ngrass", '.'}, {"nectgr", 'g'}, {"allpeak", 'L'},
        {"h_kage", 'H'}, {"t1_s01_1", 'H'}, {"t1_s01_2", 'H'}, {"t1_h01", 'H'}, {"door", 'H'}, {"light", 'H'},
        {"tshadow", 0}, {"seaside3", 0},
    };
    return roles;
}

bool TextureRole(const std::string& baseName, char& role)
{
    const auto it = Roles().find(baseName);
    if (it == Roles().end()) return false;
    role = it->second;
    return true;
}

// the first of these a tile shows wins: a house over water over ... over plain grass
static const char RolePrecedence[] = "H~fF*gL:Tts.";

// a half tile is path when its centre's colour is sand: Platinum's path outline (nsandp) is sand on its inner half,
// which texture names cannot tell
static bool Sand(const TerrainSample& s)
{
    if (!s.HasColour) return false;
    const int r = s.Rgb[0], g = s.Rgb[1], b = s.Rgb[2];
    return r > 200 && g > 160 && b < 200 && r > b + 50 && r >= g - 10;
}

// a half tile is snow when its centre's colour is white: bright and grey (Platinum's snow is white; its light grass is green)
static bool White(const TerrainSample& s)
{
    if (!s.HasColour) return false;
    const int r = s.Rgb[0], g = s.Rgb[1], b = s.Rgb[2];
    return r > 215 && g > 215 && b > 215 && std::max({r, g, b}) - std::min({r, g, b}) < 40;
}

// a grey half tile, light or dark (no hue): beside snow, it is snow under a tree's shadow (Platinum's shadows over its snow
// sample (64, 80, 64) to (96, 96, 112)); its grass and light grass are strongly green
static bool Grey(const TerrainSample& s)
{
    if (!s.HasColour) return false;
    const int r = s.Rgb[0], g = s.Rgb[1], b = s.Rgb[2];
    return std::max({r, g, b}) - std::min({r, g, b}) < 40;
}

void TownLayout::Classify(const TerrainScan& whole, const TerrainScan& half)
{
    TownLayout& out = *this;
    const int N = TownTiles;

    // each tile's role from the textures over its centre
    std::vector<std::string> grid(N, std::string(N, '.'));
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++)
        {
            std::set<char> roles;
            for (const std::string& m : whole.At(c, r).Materials)
            {
                char role;
                if (!TextureRole(m, role)) { out.UnknownTextures[m]++; continue; }
                if (role) roles.insert(role);
            }
            for (const char* p = RolePrecedence; *p; p++)
                if (roles.count(*p)) { grid[r][c] = *p; break; }
        }

    // flower beds: the tiles a fence encloses (not reachable from the window's edge without crossing one) hold flowers
    std::vector<std::string> beds = grid;
    std::vector<std::vector<bool>> seen(N, std::vector<bool>(N, false));
    std::vector<std::pair<int, int>> todo;
    for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) if (r == 0 || r == N - 1 || c == 0 || c == N - 1) todo.push_back({r, c});
    while (!todo.empty())
    {
        const auto [r, c] = todo.back();
        todo.pop_back();
        if (r < 0 || c < 0 || r >= N || c >= N || seen[r][c] || grid[r][c] == 'F') continue;
        seen[r][c] = true;
        todo.insert(todo.end(), {{r + 1, c}, {r - 1, c}, {r, c + 1}, {r, c - 1}});
    }
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++)
            if (!seen[r][c] && grid[r][c] != 'F') { beds[r][c] = '*'; out.BedTiles++; }
    out.Vis = beds;

    // paths and water at half-tile precision
    out.Path2.assign(2 * N, std::string(2 * N, '.'));
    out.Water2.assign(2 * N, std::string(2 * N, '.'));
    for (int r = 0; r < 2 * N; r++)
        for (int c = 0; c < 2 * N; c++)
        {
            // a flower bed's flowers are partly sand-coloured: no path inside a bed
            if (Sand(half.At(c, r)) && out.Vis[r / 2][c / 2] != '*') out.Path2[r][c] = ':';
            if (out.Vis[r / 2][c / 2] == '~') out.Water2[r][c] = '~';
        }

    // Platinum rounds a crossroads' inner corners with a little sand that reaches one half tile into the grass's
    // corner; drawn in square half tiles, that became a step the grass seemed to lack. Such a half tile has grass on
    // two adjacent sides and path on the other two, with path on both diagonals that lie between a grass side and
    // a path side (one arm of the crossing each); a path's outer corner has grass there. All are found before any
    // is cleared, so one pass removes the rounding and nothing more.
    std::vector<std::pair<int, int>> rounding;
    auto path = [&](int c, int r) { return c >= 0 && r >= 0 && c < 2 * N && r < 2 * N && out.Path2[r][c] == ':'; };
    for (int r = 0; r < 2 * N; r++)
        for (int c = 0; c < 2 * N; c++)
            for (int dr : {-1, 1})
                for (int dc : {-1, 1})
                    if (path(c, r) && !path(c, r + dr) && !path(c + dc, r) && path(c, r - dr) && path(c - dc, r) &&
                        path(c - dc, r + dr) && path(c + dc, r - dr))
                        rounding.push_back({r, c});
    for (const auto& [r, c] : rounding) out.Path2[r][c] = '.';

    // snow at half-tile precision, from the colour Platinum shows (its snow patches are round blobs, which whole tiles turn to
    // squares): white half tiles, not on a house, fence, flower bed or water; snow grows into grey half tiles (shadows over
    // it) with snow on at least two sides, a few passes; then a half tile with no snow beside it (a speck of a blob's edge) is
    // dropped and one with snow on at least three sides (a pinhole) filled, in one pass each
    out.Snow2.assign(2 * N, std::string(2 * N, '.'));
    for (int r = 0; r < 2 * N; r++)
        for (int c = 0; c < 2 * N; c++)
            if (White(half.At(c, r)) && std::string("HF*~f").find(out.Vis[r / 2][c / 2]) == std::string::npos) out.Snow2[r][c] = '#';
    auto snowAt = [&](const std::vector<std::string>& g, int c, int r) { return c >= 0 && r >= 0 && c < 2 * N && r < 2 * N && g[r][c] == '#'; };
    auto neighbours = [&](const std::vector<std::string>& g, int c, int r) { return snowAt(g, c - 1, r) + snowAt(g, c + 1, r) + snowAt(g, c, r - 1) + snowAt(g, c, r + 1); };
    for (int pass = 0; pass < 4; pass++)
    {
        std::vector<std::string> grown = out.Snow2;
        for (int r = 0; r < 2 * N; r++)
            for (int c = 0; c < 2 * N; c++)
                if (out.Snow2[r][c] != '#' && Grey(half.At(c, r)) && neighbours(out.Snow2, c, r) >= 2 &&
                    std::string("HF*~f").find(out.Vis[r / 2][c / 2]) == std::string::npos)
                    grown[r][c] = '#';
        out.Snow2 = grown;
    }
    std::vector<std::string> cleaned = out.Snow2;
    for (int r = 0; r < 2 * N; r++)
        for (int c = 0; c < 2 * N; c++)
        {
            if (out.Snow2[r][c] == '#' && neighbours(out.Snow2, c, r) == 0) cleaned[r][c] = '.';
            if (out.Snow2[r][c] != '#' && neighbours(out.Snow2, c, r) >= 3 && std::string("HF*~f").find(out.Vis[r / 2][c / 2]) == std::string::npos) cleaned[r][c] = '#';
        }
    out.Snow2 = cleaned;
    // under the trees: Platinum shows the trees there, not the ground, so the white stops at the forest's edge; the snow goes on
    // under it (the owner: it stopped straight instead of passing under the trees), into the tree and forest half tiles beside
    // it, two passes (a tile), so its end lies under the canopies
    for (int pass = 0; pass < 2; pass++)
    {
        std::vector<std::string> grown = out.Snow2;
        for (int r = 0; r < 2 * N; r++)
            for (int c = 0; c < 2 * N; c++)
                if (out.Snow2[r][c] != '#' && std::string("tT").find(out.Vis[r / 2][c / 2]) != std::string::npos && neighbours(out.Snow2, c, r) >= 1)
                    grown[r][c] = '#';
        out.Snow2 = grown;
    }
}

TownLayout TownLayout::Read(const PlatinumWorld& plat, int left, int top)
{
    TownLayout out;
    out.Left = left; out.Top = top;
    const int N = TownTiles;
    out.Classify(TerrainScan::Run(plat, left, top, N, 1), TerrainScan::Run(plat, left, top, N, 2));

    // collision: Platinum's own permissions (no map there: solid); the pond is water, tall grass has encounters
    const WorldMap& world = plat.World;
    out.Collision.assign(N, std::string(N, '#'));
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++)
        {
            const int gx = left + c, gy = top + r;
            if (gx < 0 || gy < 0 || gx / (int)LandTiles >= (int)world.Matrix.Width || gy / (int)LandTiles >= (int)world.Matrix.Height) continue;
            const auto& cell = world.Cells[world.Matrix.Cell(gx / LandTiles, gy / LandTiles)];
            if (!cell) continue;
            const uint16_t permission = cell->Permissions[(gy % LandTiles) * LandTiles + gx % LandTiles];
            char ch = LandData::Solid(permission) ? '#' : '.';
            if (ch == '.' && out.Vis[r][c] == 'g') ch = 'g';
            if (out.Vis[r][c] == '~') ch = '~';
            out.Collision[r][c] = ch;
        }

    // doors: the warps of every zone on the matrix that lie in the window
    for (const NdsWarp& w : plat.Warps)
    {
        const int c = (int)w.Warp.X - left, r = (int)w.Warp.Z - top;
        if (c < 0 || r < 0 || c >= N || r >= N) continue;
        out.Doors.push_back({c, r, w.Zone, w.Index, w.Warp.DestHeader, w.Warp.DestWarp});
    }
    // north to south, west to east: the same layout whatever order the zones list their warps in
    std::sort(out.Doors.begin(), out.Doors.end(), [](const TownDoor& a, const TownDoor& b) { return a.Row != b.Row ? a.Row < b.Row : a.Column < b.Column; });
    return out;
}

std::string TownLayout::Text() const
{
    std::string s = "window " + std::to_string(Left) + "," + std::to_string(Top) + "\nroles:\n";
    for (const std::string& r : Vis) s += r + "\n";
    s += "collision:\n";
    for (const std::string& r : Collision) s += r + "\n";
    for (const TownDoor& d : Doors)
        s += "door " + std::to_string(d.Column) + " " + std::to_string(d.Row) + " zone " + std::to_string(d.Zone) + " warp " + std::to_string(d.Warp) +
             " to zone " + std::to_string(d.DestZone) + "\n";
    for (const auto& [name, count] : UnknownTextures) s += "unknown texture " + name + " x" + std::to_string(count) + "\n";
    return s;
}

}
