#ifndef REMAKE_GAME_TEXT_H
#define REMAKE_GAME_TEXT_H

// The game text files of the Gen 6 Pokemon games (X/Y, ORAS: one GARC member a text file): a u16 section count (1),
// u16 line count, u32 section length, u32 initial key (0), u32 section offset; the section starts with its u32 length,
// then a table of (u32 offset from the section, u16 length in UTF-16 units, u16 0) per line, then the lines. Each line's
// UTF-16 units are XORed with a key starting at 0x7C89 + 0x2983 x line index, rotated left by 3 bits after each unit.
// In a line, 0x10 starts a variable (u16 count of the units that follow, u16 code, then count - 1 arguments); 0xE07F is
// a non-breaking space, 0xE08D and 0xE08E the male and female symbols. The layout is the one the community's text
// editors read; checked on the game's files (run 125: a/0/7/1 member 0, place names, a/0/7/3 member 2, species
// categories, read as text). Trailing zero units (padding in some files) are dropped on reading.

#include "Bytes.h"

#include <string>
#include <vector>

namespace remake
{

// One line as UTF-8, a variable written [VAR XXXX(a,b,...)] in hexadecimal, a lone code unit outside the printable
// range \xXXXX, newlines \n; Write accepts the same notation back
std::vector<std::string> ReadGameText(const Bytes& file);
Bytes WriteGameText(const std::vector<std::string>& lines);
// the file with one line added at its end, every existing line's bytes kept as they are (their offsets moved by the grown
// table): for files that do not write back identical (a/0/7/1's zero padding)
// `flags`: the line's table word after its length, its grammar (3 November, read on the trainer class names, Omega Ruby's
// a/0/7/4 member 21 and X's a/0/7/5 member 20, French: 0x1 feminine, "la Championne"; 0x4 elided, "l'Écolier"; 0
// masculine, "le"), which the game's text expansion picks the article by
Bytes AppendGameTextLine(const Bytes& file, const std::string& line, uint16_t flags = 0);
// each line's grammar word (see AppendGameTextLine)
std::vector<uint16_t> GameTextLineFlags(const Bytes& file);
// the file with line `index` replaced, every other line's bytes kept as they are (for the same files)
Bytes ReplaceGameTextLine(const Bytes& file, size_t index, const std::string& line);

}

#endif // REMAKE_GAME_TEXT_H
