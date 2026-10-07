#include "N3dsRom.h"
#include "Blz.h"

#include <functional>

namespace remake
{

static uint64_t U64(const Bytes& b, size_t at) { return U32(b, at) | (uint64_t)U32(b, at + 4) << 32; }

// UTF-16LE names (RomFS names are ASCII in practice; anything else is kept as UTF-8)
static std::string Name16(const Bytes& b, size_t at, uint32_t bytes)
{
    std::string s;
    for (uint32_t i = 0; i + 1 < bytes; i += 2)
    {
        const uint16_t c = U16(b, at + i);
        if (c < 0x80) s += (char)c;
        else if (c < 0x800) { s += (char)(0xC0 | c >> 6); s += (char)(0x80 | (c & 0x3F)); }
        else { s += (char)(0xE0 | c >> 12); s += (char)(0x80 | (c >> 6 & 0x3F)); s += (char)(0x80 | (c & 0x3F)); }
    }
    return s;
}

Bytes N3dsRom::ReadAt(uint64_t offset, uint64_t size)
{
    if (offset > ImageSize || size > ImageSize - offset) throw FormatError("3DS image: a read past its end");
    Bytes b((size_t)size);
    File.seekg((std::streamoff)offset);
    if (!File.read((char*)b.data(), (std::streamsize)size)) throw FormatError("3DS image: read failed");
    return b;
}

N3dsRom::N3dsRom(const std::string& path) : File(path, std::ios::binary)
{
    if (!File) throw FormatError("can't open " + path);
    File.seekg(0, std::ios::end);
    ImageSize = (uint64_t)File.tellg();

    // NCSD: partition 0's offset at 0x120 (in 0x200-byte units); a .cxi is the NCCH itself
    const Bytes head = ReadAt(0, 0x200);
    uint64_t ncch = 0;
    if (Text(head, 0x100, 4) == "NCSD") ncch = (uint64_t)U32(head, 0x120) * 0x200;
    Ncch = ncch;
    const Bytes n = ReadAt(ncch, 0x200);
    if (Text(n, 0x100, 4) != "NCCH") throw FormatError("not a 3DS game image (no NCCH)");
    Program = U64(n, 0x118);
    Product = Text(n, 0x150, 16);
    if (!(U8(n, 0x18F) & 0x04)) throw FormatError("the game image is encrypted: decrypt it first");
    const uint64_t romfs = ncch + (uint64_t)U32(n, 0x1B0) * 0x200;
    if (!U32(n, 0x1B4)) throw FormatError("the game image has no RomFS");

    const Bytes ivfc = ReadAt(romfs, 0x60);
    if (Text(ivfc, 0, 4) != "IVFC") throw FormatError("RomFS: no IVFC header");
    const uint32_t masterHash = U32(ivfc, 0x08), blockLog2 = U32(ivfc, 0x4C);
    if (blockLog2 > 20) throw FormatError("RomFS: level 3 block size out of range");
    const uint64_t align = 1ull << blockLog2;
    const uint64_t level3 = romfs + (0x60 + masterHash + align - 1) / align * align;
    RomfsLevel3 = level3;
    const Bytes h = ReadAt(level3, 0x28);
    if (U32(h, 0) != 0x28) throw FormatError("RomFS: unexpected level 3 header");
    const Bytes dirs = ReadAt(level3 + U32(h, 0x0C), U32(h, 0x10));
    const Bytes files = ReadAt(level3 + U32(h, 0x1C), U32(h, 0x20));
    const uint64_t data = level3 + U32(h, 0x24);

    // directories: parent, sibling, first child, first file, hash link, name length, name
    std::function<void(uint32_t, const std::string&, int)> walk = [&](uint32_t dir, const std::string& prefix, int depth) {
        if (depth > 64) throw FormatError("RomFS: directory tree too deep (a loop?)");
        for (uint32_t f = U32(dirs, dir + 0x0C), guard = 0; f != 0xFFFFFFFF; f = U32(files, f + 4))
        {
            if (++guard > 1000000) throw FormatError("RomFS: file list loops");
            FileList[prefix + Name16(files, f + 0x20, U32(files, f + 0x1C))] = {data + U64(files, f + 8), U64(files, f + 0x10)};
        }
        for (uint32_t c = U32(dirs, dir + 8), guard = 0; c != 0xFFFFFFFF; c = U32(dirs, c + 4))
        {
            if (++guard > 1000000) throw FormatError("RomFS: directory list loops");
            walk(c, prefix + Name16(dirs, c + 0x18, U32(dirs, c + 0x14)) + "/", depth + 1);
        }
    };
    walk(0, "", 0);
}

Bytes N3dsRom::Read(const std::string& path)
{
    const auto it = FileList.find(path);
    if (it == FileList.end()) throw FormatError("no " + path + " in the RomFS");
    return ReadAt(it->second.first, it->second.second);
}

Bytes N3dsRom::Code()
{
    // ExeFS at the NCCH's 0x1A0 (0x200-byte units): a 0x200-byte header of 10 entries (name[8], offset, size; offsets from the
    // header's end); the extended header follows the NCCH header at 0x200, its system control info's flags at 0x0D (bit 0: code packed)
    const Bytes n = ReadAt(Ncch, 0x200);
    const uint64_t exefs = Ncch + (uint64_t)U32(n, 0x1A0) * 0x200;
    if (!U32(n, 0x1A4)) throw FormatError("the game image has no ExeFS");
    const Bytes header = ReadAt(exefs, 0x200);
    const Bytes exheader = ReadAt(Ncch + 0x200, 0x10);
    for (int k = 0; k < 10; k++)
    {
        const size_t e = (size_t)k * 16;
        if (Text(header, e, 8).rfind(".code", 0) != 0) continue;
        const Bytes code = ReadAt(exefs + 0x200 + U32(header, e + 8), U32(header, e + 12));
        return (U8(exheader, 0x0D) & 1) ? BlzDecompress(code) : code;
    }
    throw FormatError("the ExeFS has no .code");
}

Bytes N3dsRom::ExHeader() { return ReadAt(Ncch + 0x200, 0x800); }

}
