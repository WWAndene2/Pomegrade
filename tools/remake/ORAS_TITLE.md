# Omega Ruby's title screen, and Giratina in Groudon's place

Goal (owner): the title screen of the Sinnoh remake shows the game's own Giratina (Origin Forme, as on Platinum's box) in
place of Primal Groudon, with its battle idle animation, Giratina's cry and a Platinum-style logo. What follows is marked
checked (read in the game's files or seen on the phone), inferred, or unknown.

## 1. Where the title screen lives (checked)

- Code: `DllTitle.cro` (143,360 bytes). The scene is real-time 3D; the game's only video is `m/ay.moflex` (the opening).
- The cry: the word at `DllTitle.cro` 0x18498 is 383 (Groudon), loaded at 0x1818C by the function at 0x18184 (a 9-state
  switch). Mod `t1` (`cro DllTitle.cro 0x18498=0x17F:0x1E7`) made it 487: on the phone the title played Giratina's cry and
  still showed Primal Groudon.
- The model is not chosen by species: `DllTitle.cro` holds no other 383, no (383, form) pair and none of the scene's member
  numbers, and neither do `DllFieldDemo.cro` nor `DllSequence.cro`.
- Assets: `a/1/5/2` (the demo archive), by the names inside its members (`oras-find`):

| Members | Content |
|---|---|
| 72-86 | first title scene: `title_BG` / `title_chr`, effects (`title_effect_firedustA`, `flashA`, `groundM`, `jewelM`), a title Groudon `title_pm0383_00` (83-86) |
| 88-103 | the same for Alpha Sapphire (sea effects, `title_pm0382_00` at 100-103): unused by Omega Ruby (inferred) |
| 108-125 | the "battle" scene: `title_battle_bg`, black fade, smoke, `title_battle_pm0383_00` (116-121, shadow 121), **`title_battle_pm0383_51` (Primal Groudon, 122-125)** |
| 130-145 | the same for Kyogre (`title_battle_pm0382_00` / `_51`) |
| 1120-1135 | the logo layouts (`title_logo_ruby01.bclim` ... and `title_loop.bclan`), one per language; 1128-1135 sapphire |

Each title Pokémon is 4 members, bare BCH files holding animations and no model (`oras-members`, checked): 122 (764
bytes, a material animation of `BodyBInc`), 123 (896 bytes, the skins `pm0383_51_BodySkin` ...), 124 (`_conv`, 92,380
bytes, material constants animated), 125 (153,672 bytes, the skeleton animation: bones `Hips`, `Jaw`, `LFeeler...`). They
are made for Groudon's skeleton, so Giratina's model cannot take them.

- The game's own Pokémon models: `a/0/0/8`, 8 members a form: Primal Groudon `pm0383_51` 4179-4184, Giratina Altered
  `pm0487_11` 5427-5433, Origin `pm0487_12` 5435-5441. Animations by name: `ba10_waitA01` (battle idle), `ba20_buturi`,
  `kw..` (Pokémon-Amie) ...

- Mod `t2` (`copy a/0/0/8 4178-4185=5434-5441`, Giratina Origin's eight members over Primal Groudon's, plus `t1`'s cry), on
  the phone: **the title shows Giratina Origin**, deformed: the title's animations (122-125, made for Groudon's bones) drive
  its bones. So the title takes its model from `a/0/0/8` (checked) and its animations from `a/1/5/2`. A form's block is 8
  members, its first a small `PC` container (88 bytes); Groudon 4170-4177, Primal Groudon 4178-4185, Giratina Altered
  5426-5433, Origin 5434-5441. The swap also changes Primal Groudon in battle: fine for a test, not for the Hoenn story.

- Mod `t4a` + `t4b` (in `all`, with s2; built, **not run on the phone**): `t3`'s models (Giratina Altered over Groudon
  4170-4177, Origin over Primal Groudon 4178-4185) and the title's 12 animation members replaced by Giratina's own BCH
  animation files from its PB packs (Altered 5432, Origin 5440; files 38 `$FormatType ..._ba10_waitA01`, 748 bytes, into
  the material members 83/85, 116/118, 122/124; file 2, the skin animation, into 84, 117, 123; file 20 `..._ba10_waitA01`,
  748 bytes, into the skeleton members 86, 119, 125). Expected (inferred): no deformation, Giratina still, since the
  skeleton's motion is in the PB pack's file 0 (160,964 bytes, a format the tool does not read yet), not in file 20.
  a/1/5/2's member 125 is padded with zeros after its LZ data so the archive keeps its size.

## 1b. The Pokémon animation pack's compact motions (being decoded)

`a/0/0/8`'s PB pack (Giratina Origin: member 5440) holds, as its file 0, the motions in a format of their own (not BCH),
read so far from `oras-hex` dumps (checked on that file, the rest inferred):

- `u32` count (0x1D = 29 motion slots), then 29 `u32` offsets from the file's start, 0 for an absent slot (Giratina's
  first at 0x12E0, 0x32F0, ...), then `u32` 0x69 (105, the bone count).
- The skeleton, from 0x7C: a byte list (parent and child indices, inferred), the bone names as NUL-ended text (`Waist`,
  `Hips`, `LThighC`, ... `RFeelerA12`, up to 0x575), then from 0x580 each bone's rest transform as `f32` (scale 1.0,
  rotation, translation).
- A motion: `u16` 0x01F7 (meaning unknown), `u16` frame count (70 for `ba10_waitA01` at 0x12E0, 100 at 0x32F0), a bit
  field (`40 92 FF FF 13 FC 49 AF ...`, one code per bone and axis, inferred), then key lists, each a `u8` count and that
  many `u8` frame numbers (`02 08 2B`: two keys, frames 8 and 43; every frame below the count), then the values (`f32`,
  not yet read).

- The pack's header (checked on two files): `u32` count, `count` `u32` offsets (0 for an absent slot), then the end
  offset; data from `4 + 4 * (count + 1)`. Slot 0 is the skeleton (`u32` bone count, the hierarchy bytes, the names, the
  rest transforms), slot k a motion: in a Pokémon's pack slot k matches the PB pack's BCH file k+1 (slot 1 =
  `ba10_waitA01`, 70 frames). Motion values come as (`f32` value, `f32` slope) pairs per key (Hermite), the last key of a
  looping track equal to the first (seen, inferred).
- **The title has packs of this format of its own** (checked): `a/1/5/2` members 87 (first scene), 120 (the battle
  scene's Groudon, 58 bones) and 126 (Primal Groudon, 61 bones), each with count 2: Groudon's skeleton and one motion.
  Members 104-107, 113 and 127-129 are `CGFX` files (effects, inferred). So Giratina's own skeleton and idle motion can be
  put in their place as they are, with no conversion: `oras-copy ... <member>=motion:a/0/0/8:<PB member>:0:1`.

The title's skeleton animations are standard BCH (H3D) skeletal animations: member 125 holds 61 bone entries of 0x30
bytes (name, flags `0x00040000`, per-axis keys), the layout documented by the 3DS community's tools.

- How the title picks its model: not by a number in `DllTitle.cro` (its only 383, as a word or an unaligned `u16`, is
  the cry). Next suspect: a function of the main program giving the version's Pokémon (Groudon in Omega Ruby, Kyogre in
  Alpha Sapphire).

- `all` (t4, run 97) on the phone: Giratina Altered in Groudon's place shows right; Giratina Origin is still deformed, as member 126 (Primal Groudon's motion pack) was not yet replaced. `all2` (run 106) replaces 87, 120 and 126 with Giratina's packs: not yet run.

- `all3` (run 108) on the phone: Giratina Origin shows right (its own skeleton in member 126 ended the deformation), with no motion: the title's motion comes from elsewhere or from another slot (to find).

- `c2` (run 110) on the phone: the title as in all3, Giratina Origin still without motion (confirmed).

- Run 118 dumps (checked): the title's BCH skeleton animations are long, made for the whole title loop: member 125
  (Primal Groudon) has 61 bones and a frame count of 3500.0 (`f32` 0x455AC000 at 0x1C4), member 119 (Groudon) 58 bones
  and 3080 frames (960,744 bytes). The motion packs 120 and 126 hold the same bones (count 2: skeleton, one motion of
  64,936 bytes in 126 against 8,208 bytes for Giratina's 70-frame idle). Every mod so far put the PB pack's file 20 (748
  bytes, an empty shell: a Pokémon's motion lives in the compact pack) in 125, so if the title drives the bones from 125,
  Origin has nothing to play (inferred). `r8` (run 119) restores the original 125 with Giratina's skeleton in 126: if
  Origin then moves (likely deformed), 125 drives the motion and the fix is a BCH skeletal animation written from the
  compact idle; if it stays still, the motion comes from 126.

- `r8` (run 119) on the phone: Giratina Origin still does not move with the original member 125. The owner's observation
  (seen, unmodified game): **Primal Groudon has no skeleton animation on the title, only texture animation**, while
  Groudon has one. So the game never plays a skeleton motion on the Primal slot, whatever 125 and 126 hold: Origin standing
  still there is the game's own behaviour. Moving it needs either the animated (Groudon) slot or a `DllTitle.cro` change
  that plays a motion on the Primal slot (code not found).

- Run 120: `DllTitle.cro` holds neither the title motions' frame counts (3080.0, 3500.0 as `f32`) nor the title members'
  numbers (108-145) as immediates or words (checked): which slot moves is not decided by such a constant in the module.
- The owner (seen): the title alternates the two forms, and Giratina Altered in Groudon's slot plays its idle **in a loop**
  for its whole turn.
- Run 121, the motions' first words (checked): a motion starts `u16` flags, `u16` frame count: 87 `0x0104`, 500 frames;
  120 `0x0104`, 3080 frames; 126 (Primal Groudon) `0x0117`, 3500 frames; Giratina's idle `0x01F7`, 70 frames. In the title
  motions (over 255 frames) the key lists are `u16` (count, then frames): 126 has tracks of 93 keys about 36 frames apart,
  so Primal Groudon's motion does move bones in the data, if slightly (the owner sees none on the phone). Lead (inferred,
  untested): the Primal slot plays its motion on the title's own clock without looping, so a 70-frame motion stays on its
  last frame; then Giratina's idle unrolled to 3500 frames (the format written, not only copied) would move.

## 2. Plan

1. Giratina Origin with its battle idle animation (`pm0487_12_ba10_waitA01`) in the four members 122-125, converted from
   its `a/0/0/8` members to the title members' layout.
2. The cry: `t1`'s patch.
3. The logo: a Platinum-style logo in members 1120-1127's `.bclim` images.
4. Later: an animation made for the title.
