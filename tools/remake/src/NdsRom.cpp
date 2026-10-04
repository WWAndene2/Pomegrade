#include "NdsRom.h"

#include <algorithm>

namespace remake
{

NdsRom::NdsRom(Bytes image) : Image(std::move(image))
{
    TitleText = Text(Image, 0x00, 12);
    Code = Text(Image, 0x0C, 4);
    const uint32_t fnt = U32(Image, 0x40), fat = U32(Image, 0x48), fatSize = U32(Image, 0x4C);
    const uint32_t count = fatSize / 8;
    std::vector<std::string> names(count);
    // the root directory is 0xF000; its entry's last field is the directory count
    ReadDirectory(fnt, 0xF000, "", names, 0);
    for (uint32_t id = 0; id < count; id++)
    {
        NdsFile f;
        f.Id = (uint16_t)id;
        f.Path = names[id].empty() ? "overlay/" + std::to_string(id) : names[id];
        f.Start = U32(Image, fat + id * 8);
        f.End = U32(Image, fat + id * 8 + 4);
        if (f.End < f.Start || f.End > Image.size())
            throw FormatError("file " + std::to_string(id) + " lies outside the cartridge");
        FileList.push_back(f);
    }
}

void NdsRom::ReadDirectory(uint32_t fnt, uint16_t dir, const std::string& prefix, std::vector<std::string>& names, int depth)
{
    if (depth > 64) throw FormatError("directory tree too deep (a loop?)");
    const uint32_t entry = fnt + (dir & 0xFFF) * 8;
    uint32_t at = fnt + U32(Image, entry);
    uint16_t id = U16(Image, entry + 4);
    for (;;)
    {
        const uint8_t kind = U8(Image, at++);
        if (kind == 0) break;
        const uint8_t len = kind & 0x7F;
        const std::string name = Text(Image, at, len);
        at += len;
        if (kind & 0x80)
        {
            const uint16_t sub = U16(Image, at);
            at += 2;
            ReadDirectory(fnt, sub, prefix + name + "/", names, depth + 1);
        }
        else
        {
            if (id >= names.size()) throw FormatError("named file id " + std::to_string(id) + " past the allocation table");
            names[id++] = prefix + name;
        }
    }
}

const NdsFile* NdsRom::Find(const std::string& path) const
{
    auto it = std::find_if(FileList.begin(), FileList.end(), [&](const NdsFile& f) { return f.Path == path; });
    return it == FileList.end() ? nullptr : &*it;
}

Bytes NdsRom::Read(const NdsFile& file) const
{
    return Slice(Image, file.Start, file.Size());
}

}
