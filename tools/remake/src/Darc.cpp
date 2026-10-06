#include "Darc.h"

#include <map>

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
    // nodes: root, then each folder before the files under it, in the files' order
    struct Node { std::string Name; bool Folder; uint32_t Parent; size_t File; uint32_t End; };
    std::vector<Node> nodes = {{"", true, 0, 0, 0}};
    std::map<std::string, size_t> folderOf = {{"", 0}};
    std::vector<std::string> open;
    for (size_t i = 0; i < files.size(); i++)
    {
        std::vector<std::string> parts;
        for (size_t at = 0, next; at <= files[i].Path.size(); at = next + 1)
        {
            next = files[i].Path.find('/', at);
            if (next == std::string::npos) next = files[i].Path.size();
            parts.push_back(files[i].Path.substr(at, next - at));
        }
        std::string dir;
        for (size_t k = 0; k + 1 < parts.size(); k++)
        {
            const std::string parent = dir;
            dir += (dir.empty() ? "" : "/") + parts[k];
            if (!folderOf.count(dir)) { folderOf[dir] = nodes.size(); nodes.push_back({parts[k], true, (uint32_t)folderOf[parent], 0, 0}); }
        }
        nodes.push_back({parts.back(), false, 0, i, 0});
        for (std::string d = dir;; d = d.substr(0, d.rfind('/') == std::string::npos ? 0 : d.rfind('/')))
        {
            nodes[folderOf[d]].End = (uint32_t)nodes.size();
            if (d.empty()) break;
        }
    }
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
