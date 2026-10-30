// Footstep grounds from textures (OrasFootstepGround.h): the names the games themselves tag with a ground get that ground or
// the surface it stands for, the others their surface by their words; water, walls and effects stay silent; each game its
// own numbers where they differ
#include "OrasFootstepGround.h"
#include "OrasXyFootsteps.h"
#include <cstdio>
#include <string>

using namespace remake;

static bool ok = true;
static void check(const std::string& texture, FootstepGame game, int want)
{
    const int got = GroundOfTexture(texture, game);
    printf("%s (%s): %d%s\n", texture.c_str(), game == FootstepGame::X ? "X" : "Omega Ruby", got, got == want ? "" : " NO");
    if (got != want) ok = false;
}

int main()
{
    // tagged so by Omega Ruby (remake_tool piece-grounds, 30 October): sand and metal kept, grass and wood as surfaces
    check("r124_sand", FootstepGame::Oras, 8);
    check("sabak_ground1", FootstepGame::Oras, 8);
    check("chip_des_01", FootstepGame::Oras, 8);
    check("floor_metal", FootstepGame::Oras, 12);
    check("chip_kusa", FootstepGame::Oras, SoftGrass);
    check("floor01", FootstepGame::Oras, Wood);
    check("chip_bridge_01", FootstepGame::Oras, Wood);
    // tagged so by X
    check("bm_w_beach01", FootstepGame::X, 8);
    check("tex_asase00", FootstepGame::X, 10);
    check("d12_ami02", FootstepGame::X, 12);
    check("plr0201_jytan01", FootstepGame::X, Carpet);
    check("tex_sogen_00", FootstepGame::X, SoftGrass);
    check("d06_snow02", FootstepGame::X, 7);
    // the surfaces by their words
    check("chip_soil_a", FootstepGame::Oras, Dirt);
    check("r02_michi01", FootstepGame::X, Dirt);
    check("kanazumi_miti01", FootstepGame::Oras, Dirt); // "umi" inside a word is no sea
    check("r13_arachi01", FootstepGame::X, Dirt);
    check("c02a03_ground00", FootstepGame::X, Paved);    // Lumiose's pavements
    check("c01_isidatami01", FootstepGame::X, Paved);
    check("t02_road_c", FootstepGame::X, Paved);
    check("chip_doukutsu02", FootstepGame::Oras, CaveRock);
    check("iwa00", FootstepGame::X, CaveRock);
    check("r06_kinoha", FootstepGame::X, ForestFloor);
    check("chip_wood_b", FootstepGame::Oras, ForestFloor);
    check("in_tree_yuka", FootstepGame::Oras, Wood);
    check("flooring01_v", FootstepGame::X, Wood);
    check("d_com_yuka01", FootstepGame::X, IndoorFloor);
    check("c103r_floor01", FootstepGame::Oras, IndoorFloor);
    // silent: water, walls, effects, props, nothing drawn
    check("chip_sea_b", FootstepGame::Oras, 0);
    check("chip_diving_ground", FootstepGame::Oras, 0);
    check("tex_umi_00", FootstepGame::X, 0);
    check("plr0201_kabe01", FootstepGame::X, 0);
    check("table01", FootstepGame::Oras, 0);
    check("Comb1", FootstepGame::X, 0);
    check("", FootstepGame::Oras, 0);
    printf("%s\n", ok ? "all good" : "FAILED");
    return ok ? 0 : 1;
}
