# Hoenn map (ORAS)

`hoenn_map.png`: a Sword / Shield style map of Hoenn drawn from Omega Ruby's own data (owner's request, 27 October): the
roads as the tiles lay them, the places where the overworld enters them, the land outline of the game's town map.
No game data is in this folder; the scripts read it from a work directory made from your own dump.

## What is data and what is not

- **Read from the game:** each map matrix's place (from the edge crossings, warp kinds 0x500-0x503), the roads (a
  path traced over the terrain each piece's model shows: paths first, grass, never cliffs, forest or water), the
  places' positions (their doors and warps), 22 landmarks (where an outdoor zone warps into them).
- **Chosen:** ORAS's world does not close into one grid (some loops miss by up to 85 tiles); each gap is put
  between two sea routes, in open water, with Slateport - Route 109 and Pacifidlog - Route 131 kept exact. Which
  place each line joins follows Hoenn's route order (routes in one matrix have no warp between them).
- **From the town map (DexNav screen, `ref/dexnav.jpg`, not committed):** the land outline, bent (thin-plate
  spline) so each city's square lands on the city; approximate: Safari Zone, Shoal Cave, Seafloor Cavern, Cave of
  Origin; Sootopolis, Pokémon League, Southern Island, Battle Resort placed by hand. Victory Road's stop is halfway
  between Ever Grande and the League.
- **Missing:** Route 125's number, Weather Institute, Trick House, Sealed Chamber, Nameless Cavern, the Mirage spots.

## Run

From a work directory holding `map/names.txt` (a/0/7/3 member 90, one `line<TAB>text` per name), `wat/oz/` (ORAS
zones), `map/a_0_4_0/` (matrices), `map/a_0_3_9/` (pieces) and `ref/dexnav.jpg`, with `REMAKE_TOOL` naming
`remake_tool`, in order:

```
python3 hoenn_terrain.py    # map/kinds.npy: the terrain kind of each tile of the assembled world
python3 hoenn_roads.py      # map/paths.pkl: the traced roads and the places' positions
python3 hoenn_landmarks.py  # map/landmarks*.pkl: where the overworld enters each landmark
python3 hoenn_draw.py       # hoenn_map.png
```

`hoenn_place.py` (matrix placement), `hoenn_assemble.py` (the tile render, `hoenn_assembled.png`) and
`hoenn_lines.py` (the lines' place order) are read by the others.
