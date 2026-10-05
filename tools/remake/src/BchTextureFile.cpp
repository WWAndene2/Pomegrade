#include "BchTextureFile.h"

#include <algorithm>
#include <cstring>

namespace remake
{

namespace
{

void Put32(Bytes& b, size_t at, uint32_t v) { for (int i = 0; i < 4; i++) b.at(at + i) = (uint8_t)(v >> (8 * i)); }
void Push32(Bytes& b, uint32_t v) { for (int i = 0; i < 4; i++) b.push_back((uint8_t)(v >> (8 * i))); }
void Push16(Bytes& b, uint16_t v) { b.push_back((uint8_t)v); b.push_back((uint8_t)(v >> 8)); }
size_t AlignUp(size_t v, size_t a) { return (v + a - 1) / a * a; }

int Bit(const std::string& name, uint32_t i)
{
    const size_t at = i >> 3;
    return at >= name.size() ? 0 : ((uint8_t)name[at] >> (i & 7)) & 1;
}

struct Node { uint32_t Ref = 0xFFFFFFFF; uint16_t Left = 0, Right = 0; std::string Name; };

// the dictionary's name tree for these names (inserted in order); node 0 is the root
std::vector<Node> BuildTree(const std::vector<std::string>& names)
{
    std::vector<Node> nodes(1);
    for (const std::string& name : names)
    {
        uint16_t prev = 0, cur = nodes[0].Left;
        while (nodes[prev].Ref > nodes[cur].Ref) { prev = cur; cur = Bit(name, nodes[cur].Ref) ? nodes[cur].Right : nodes[cur].Left; }
        const std::string& other = nodes[cur].Name;
        int diff = -1;
        for (int i = (int)std::max(name.size(), other.size()) * 8 - 1; i >= 0; i--)
            if (Bit(name, i) != Bit(other, i)) { diff = i; break; }
        if (diff < 0) throw FormatError("BCH textures: the name " + name + " appears twice");
        prev = 0; cur = nodes[0].Left;
        while (nodes[prev].Ref > nodes[cur].Ref && nodes[cur].Ref > (uint32_t)diff) { prev = cur; cur = Bit(name, nodes[cur].Ref) ? nodes[cur].Right : nodes[cur].Left; }
        const uint16_t k = (uint16_t)nodes.size();
        Node n;
        n.Ref = (uint32_t)diff; n.Name = name;
        if (Bit(name, diff)) { n.Left = cur; n.Right = k; } else { n.Left = k; n.Right = cur; }
        nodes.push_back(n);
        if (prev == 0) nodes[0].Left = k;
        else if (Bit(name, nodes[prev].Ref)) nodes[prev].Right = k;
        else nodes[prev].Left = k;
    }
    return nodes;
}

struct Relocation { uint32_t Source, Offset, Target; };

}

Bytes BchWriteTextureFile(const std::vector<BchTextureSource>& textures)
{
    const size_t n = textures.size();
    if (n == 0) throw FormatError("BCH textures: no texture");
    std::vector<std::string> names;
    for (const BchTextureSource& t : textures) names.push_back(t.Name);
    const std::vector<Node> tree = BuildTree(names);

    // strings
    Bytes strings;
    std::vector<uint32_t> nameAt;
    for (const std::string& s : names) { nameAt.push_back((uint32_t)strings.size()); strings.insert(strings.end(), s.begin(), s.end()); strings.push_back(0); }

    // contents: offsets of each part
    const uint32_t descriptors = 0, rootsA = 15 * 12, treeAt = rootsA + 3 * 12, valuesAt = treeAt + (uint32_t)(n + 1) * 12;
    const uint32_t rootsB = valuesAt + (uint32_t)n * 4, objects = rootsB + 11 * 12, contentsLength = objects + (uint32_t)n * 32;
    Bytes contents(contentsLength, 0);
    std::vector<Relocation> relocations;
    auto word = [&](uint32_t at) { return at / 4; };
    // descriptors: dictionary k at 12 * k; the textures are the fourth
    for (uint32_t k = 0; k < 15; k++)
    {
        const uint32_t d = descriptors + 12 * k;
        const uint32_t root = k < 3 ? rootsA + 12 * k : k == 3 ? treeAt : rootsB + 12 * (k - 4);
        if (k == 3) { Put32(contents, d, valuesAt); Put32(contents, d + 4, (uint32_t)n); relocations.push_back({0, word(d), 0}); }
        Put32(contents, d + 8, root);
        relocations.push_back({0, word(d + 8), 0});
    }
    // the name tree: nodes of 12 bytes (u32 reference bit, u16 left, u16 right, u32 name pointer)
    for (size_t i = 0; i < tree.size(); i++)
    {
        const uint32_t at = treeAt + (uint32_t)i * 12;
        Put32(contents, at, tree[i].Ref);
        contents[at + 4] = (uint8_t)tree[i].Left; contents[at + 5] = (uint8_t)(tree[i].Left >> 8);
        contents[at + 6] = (uint8_t)tree[i].Right; contents[at + 7] = (uint8_t)(tree[i].Right >> 8);
        if (i > 0) { Put32(contents, at + 8, nameAt[i - 1]); relocations.push_back({0, at + 8, 1}); }
        else Put32(contents, at, 0xFFFFFFFF);
    }
    for (size_t i = 0; i < n; i++) { Put32(contents, valuesAt + (uint32_t)i * 4, objects + (uint32_t)i * 32); relocations.push_back({0, word(valuesAt + (uint32_t)i * 4), 0}); }

    // commands and raw data
    Bytes commands, raw;
    for (size_t i = 0; i < n; i++)
    {
        const BchTextureSource& t = textures[i];
        const uint32_t obj = objects + (uint32_t)i * 32;
        const uint32_t dataAt = (uint32_t)raw.size();
        raw.insert(raw.end(), t.Data.begin(), t.Data.end());
        static const uint16_t regs[3][4] = {{0x82, 0x84, 0x85, 0x8E}, {0x92, 0x94, 0x95, 0x96}, {0x9A, 0x9C, 0x9D, 0x9E}};
        for (int unit = 0; unit < 3; unit++)
        {
            Put32(contents, obj + unit * 8, (uint32_t)commands.size());
            Put32(contents, obj + unit * 8 + 4, 12);
            relocations.push_back({0, word(obj + unit * 8), 2});
            Push32(commands, (t.Width << 16) | t.Height); Push32(commands, 0xF0000 | regs[unit][0]);
            Push32(commands, 0); Push32(commands, 0x40000 | regs[unit][1]);
            relocations.push_back({2, (uint32_t)commands.size() / 4, 5});
            Push32(commands, dataAt); Push32(commands, 0xF0000 | regs[unit][2]);
            Push32(commands, t.Format); Push32(commands, 0xF0000 | regs[unit][3]);
            Push32(commands, 0); Push32(commands, 0); Push32(commands, 1); Push32(commands, 0xF023D);
        }
        Put32(contents, obj + 0x18, (uint32_t)t.Format | (1u << 8));
        Put32(contents, obj + 0x1C, nameAt[i]);
        relocations.push_back({0, obj + 0x1C, 1});
    }
    raw.resize(AlignUp(raw.size(), 0x80), 0);

    // sections
    const uint32_t contentsAt = 0x44, stringsAt = contentsAt + contentsLength;
    const uint32_t commandsAt = (uint32_t)AlignUp(stringsAt + strings.size(), 16);
    const uint32_t rawAt = (uint32_t)AlignUp(commandsAt + commands.size(), 0x80);
    const uint32_t relocAt = rawAt + (uint32_t)raw.size();
    Bytes table;
    for (const Relocation& r : relocations) Push32(table, (r.Source << 29) | (r.Target << 25) | r.Offset);

    Bytes out;
    out.insert(out.end(), {'B', 'C', 'H', 0, 0x21, 0x21, 0x6F, 0xA6});
    for (uint32_t a : {contentsAt, stringsAt, commandsAt, rawAt, relocAt, relocAt}) Push32(out, a);   // contents, strings, commands, raw, raw ext, relocation
    for (uint32_t l : {contentsLength, (uint32_t)strings.size(), (uint32_t)commands.size(), (uint32_t)raw.size(), 0u, (uint32_t)table.size()}) Push32(out, l);
    Push32(out, (uint32_t)n * 12); Push32(out, 0); Push16(out, 1); Push16(out, (uint16_t)(3 * n));
    out.insert(out.end(), contents.begin(), contents.end());
    out.insert(out.end(), strings.begin(), strings.end());
    out.resize(commandsAt, 0);
    out.insert(out.end(), commands.begin(), commands.end());
    out.resize(rawAt, 0);
    out.insert(out.end(), raw.begin(), raw.end());
    out.insert(out.end(), table.begin(), table.end());
    return out;
}

}
