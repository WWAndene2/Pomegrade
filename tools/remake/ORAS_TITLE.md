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

## 2. Plan

1. Giratina Origin with its battle idle animation (`pm0487_12_ba10_waitA01`) in the four members 122-125, converted from
   its `a/0/0/8` members to the title members' layout.
2. The cry: `t1`'s patch.
3. The logo: a Platinum-style logo in members 1120-1127's `.bclim` images.
4. Later: an animation made for the title.
