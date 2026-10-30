// Footstep grounds from textures (OrasFootstepGround.h): the names the games themselves tag with a ground get that ground,
// water, walls and effects stay silent, and each game gets its own numbers
#include "OrasFootstepGround.h"
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
    // tagged so by Omega Ruby (remake_tool piece-grounds, 30 October)
    check("chip_kusa", FootstepGame::Oras, 4);
    check("r113_kusa", FootstepGame::Oras, 4);
    check("floor01", FootstepGame::Oras, 6);
    check("chip_bridge_01", FootstepGame::Oras, 6);
    check("r124_sand", FootstepGame::Oras, 8);
    check("sabak_ground1", FootstepGame::Oras, 8);
    check("chip_des_01", FootstepGame::Oras, 8);
    check("floor_metal", FootstepGame::Oras, 12);
    // tagged so by X
    check("plr0201_yuka01", FootstepGame::X, 3);
    check("tex_sogen_00", FootstepGame::X, 4);
    check("bm_w_beach01", FootstepGame::X, 8);
    check("plr0201_jytan01", FootstepGame::X, 11);
    check("d12_ami02", FootstepGame::X, 12);
    // the same surface in each game's numbers
    check("chip_hasi01b", FootstepGame::X, 3);
    check("floor02", FootstepGame::X, 3);
    // silent: water, walls, effects, the projected terrain, nothing drawn
    check("chip_sea_b", FootstepGame::Oras, 0);
    check("chip_diving_ground", FootstepGame::Oras, 0);
    check("plr0201_kabe01", FootstepGame::X, 0);
    check("projection_dummy", FootstepGame::X, 0);
    check("", FootstepGame::Oras, 0);
    printf("%s\n", ok ? "all good" : "FAILED");
    return ok ? 0 : 1;
}
