#include "Darc.h"

#include <cstdint>
#include <functional>

namespace remake
{

static std::string Utf16Name(const Bytes& b, size_t at)
{
    std::string s;
    for (;; at += 2)
    {
        const uint16_t c = U16(b, at);
        if (!c) return s;
        s += c < 0x80 ? (char)c : '?';
    }
}

std::vector<DarcFile> ReadDarc(const Bytes& f)
{
    if (Text(f, 0, 4) != "darc" || U16(f, 4) != 0xFEFF) throw FormatError("DARC: no darc header");
    const uint32_t table = U32(f, 0x10);
    const uint32_t count = U32(f, table + 8);
    const size_t names = table + 12 * count;
    std::vector<DarcFile> out;
    std::vector<std::pair<uint32_t, std::string>> folders; // (end index, path) of the open folders
    for (uint32_t i = 1; i < count; i++)
    {
        while (!folders.empty() && i >= folders.back().first) folders.pop_back();
        const size_t node = table + 12 * i;
        const uint32_t nameWord = U32(f, node);
        const std::string name = Utf16Name(f, names + (nameWord & 0xFFFFFF));
        std::string path = folders.empty() ? "" : folders.back().second;
        if (!path.empty() && path.back() != '/') path += '/';
        path += name;
        if (nameWord & 0x01000000) folders.push_back({U32(f, node + 8), path});
        else out.push_back({path, Slice(f, U32(f, node + 4), U32(f, node + 8))});
    }
    return out;
}

Bytes WriteDarc(const std::vector<DarcFile>& files)
{
    // the folder tree, children in order of first appearance; the nodes are laid out depth first, so that each folder's
    // nodes follow it contiguously (its End) whatever order the files come in
    struct Dir { std::string Name; std::vector<size_t> Dirs, Files; };
    std::vector<Dir> dirs = {{"", {}, {}}};
    for (size_t i = 0; i < files.size(); i++)
    {
        size_t d = 0;
        for (size_t at = 0, slash; (slash = files[i].Path.find('/', at)) != std::string::npos; at = slash + 1)
        {
            const std::string part = files[i].Path.substr(at, slash - at);
            size_t found = SIZE_MAX;
            for (size_t c : dirs[d].Dirs) if (dirs[c].Name == part) found = c;
            if (found == SIZE_MAX) { found = dirs.size(); dirs.push_back({part, {}, {}}); dirs[d].Dirs.push_back(found); }
            d = found;
        }
        dirs[d].Files.push_back(i);
    }
    struct Node { std::string Name; bool Folder; uint32_t Parent; size_t File; uint32_t End; };
    std::vector<Node> nodes;
    std::function<void(size_t, uint32_t)> lay = [&](size_t d, uint32_t parent) {
        const size_t self = nodes.size();
        nodes.push_back({dirs[d].Name, true, parent, 0, 0});
        for (size_t f : dirs[d].Files) nodes.push_back({files[f].Path.substr(files[f].Path.rfind('/') + 1), false, 0, f, 0});
        for (size_t c : dirs[d].Dirs) lay(c, (uint32_t)self);
        nodes[self].End = (uint32_t)nodes.size();
    };
    lay(0, 0);
    Bytes nameTable;
    std::vector<uint32_t> nameAt;
    for (const Node& n : nodes)
    {
        nameAt.push_back((uint32_t)nameTable.size());
        for (char c : n.Name) { nameTable.push_back((uint8_t)c); nameTable.push_back(0); }
        nameTable.push_back(0); nameTable.push_back(0);
    }
    const uint32_t table = 0x1C, tableSize = (uint32_t)(12 * nodes.size() + nameTable.size());
    uint32_t data = (table + tableSize + 0x7F) & ~0x7Fu;
    Bytes out(data, 0);
    std::vector<uint32_t> fileAt(files.size());
    for (size_t i = 0; i < files.size(); i++)
    {
        while (out.size() % 0x80) out.push_back(0);
        fileAt[i] = (uint32_t)out.size();
        out.insert(out.end(), files[i].Data.begin(), files[i].Data.end());
    }
    auto put32 = [&](size_t at, uint32_t v) { for (int k = 0; k < 4; k++) out[at + k] = (uint8_t)(v >> (8 * k)); };
    out[0] = 'd'; out[1] = 'a'; out[2] = 'r'; out[3] = 'c';
    out[4] = 0xFF; out[5] = 0xFE; out[6] = 0x1C; out[7] = 0;
    put32(8, 0x01000000); put32(12, (uint32_t)out.size()); put32(16, table); put32(20, tableSize); put32(24, data);
    for (size_t i = 0; i < nodes.size(); i++)
    {
        const Node& n = nodes[i];
        const size_t at = table + 12 * i;
        put32(at, nameAt[i] | (n.Folder ? 0x01000000u : 0));
        put32(at + 4, n.Folder ? n.Parent : fileAt[n.File]);
        put32(at + 8, n.Folder ? n.End : (uint32_t)files[n.File].Data.size());
    }
    std::copy(nameTable.begin(), nameTable.end(), out.begin() + table + 12 * nodes.size());
    return out;
}

}
