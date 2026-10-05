#include "BchWriter.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace remake
{

namespace
{

constexpr uint32_t Wide16 = 7;            // relocation section of 16-bit index buffers in the raw data
constexpr size_t HeaderAddresses = 8, HeaderLengths = 0x20; // version 0x21: 6 addresses, then 6 lengths
enum Section { Contents, Strings, Commands, Raw, RawExt, Relocation };

void Put32(Bytes& b, size_t at, uint32_t v) { for (int i = 0; i < 4; i++) b.at(at + i) = (uint8_t)(v >> (8 * i)); }
void Pad(Bytes& b, size_t base, size_t align) { while ((base + b.size()) % align) b.push_back(0); }

// one element in an attribute's format, after its scale
void PushElement(Bytes& out, size_t base, int format, float value)
{
    switch (format)
    {
    case 0: out.push_back((uint8_t)(int8_t)std::clamp(std::lround(value), -128L, 127L)); break;
    case 1: out.push_back((uint8_t)std::clamp(std::lround(value), 0L, 255L)); break;
    case 2:
    {
        Pad(out, base, 2);
        const uint16_t v = (uint16_t)(int16_t)std::clamp(std::lround(value), -32768L, 32767L);
        out.push_back((uint8_t)v); out.push_back((uint8_t)(v >> 8));
        break;
    }
    case 3:
    {
        Pad(out, base, 2);
        uint32_t v; memcpy(&v, &value, 4);
        for (int k = 0; k < 4; k++) out.push_back((uint8_t)(v >> (8 * k)));
        break;
    }
    }
}

// the vertices in the mesh's format; base: the buffer's file offset (alignment is the file's)
Bytes EncodeVertices(const BchMesh& mesh, const std::vector<BchVertex>& vertices, size_t base)
{
    Bytes out;
    for (const BchVertex& v : vertices)
    {
        const size_t start = out.size();
        for (const BchAttribute& a : mesh.Attributes)
        {
            const float* src = nullptr;
            float position[3];
            switch (a.Name)
            {
            case 0: for (int k = 0; k < 3; k++) position[k] = v.Position[k] - mesh.PositionOffset[k]; src = position; break;
            case 1: src = v.Normal; break;
            case 3: src = v.Colour; break;
            case 4: case 5: case 6: src = v.TexCoord; break; // texture coordinate sets 0-2
            default: throw FormatError("BCH writer: a vertex attribute it can't fill (" + std::to_string(a.Name) + ")");
            }
            const int available = a.Name == 0 || a.Name == 1 ? 3 : a.Name == 3 ? 4 : 2;
            for (int k = 0; k < a.Elements; k++)
                PushElement(out, base + start, a.Format, (k < available ? src[k] : 1.0f) / (a.Scale ? a.Scale : 1.0f));
        }
        // a vertex is padded to its stride (2 bytes when it ends with byte attributes)
        while (out.size() - start < mesh.Stride && out.size() - start + 4 > mesh.Stride) out.push_back(0);
        if (out.size() - start != mesh.Stride)
            throw FormatError("BCH writer: a vertex written as " + std::to_string(out.size() - start) + " bytes, the mesh's stride is " + std::to_string(mesh.Stride));
    }
    return out;
}

}

Bytes BchReplaceGeometry(const Bytes& bch, size_t modelIndex, const std::vector<BchGeometry>& meshes)
{
    const Bch parsed = Bch::Read(bch);
    if (parsed.Version < 0x21) throw FormatError("BCH writer: only version 0x21 files (Omega Ruby / Alpha Sapphire)");
    if (modelIndex >= parsed.Models.size()) throw FormatError("BCH writer: no such model");
    const BchModel& model = parsed.Models[modelIndex];
    const BchSections s = BchSections::Read(bch);
    const std::vector<BchPointer> pointers = BchPointers(bch, s);
    // the raw section is padded to 0x80 before RawExt (its length leaves the padding out): the new
    // buffers go after the padding, and the raw section's length then covers it
    const uint32_t rawLength = U32(bch, HeaderLengths + Raw * 4);
    const uint32_t insertAt = s.RawExt;
    if (s.Raw + rawLength > s.RawExt || s.RawExt - (s.Raw + rawLength) >= 0x80 || s.RawExt > s.Relocation)
        throw FormatError("BCH writer: the raw data section is not followed by RawExt and the relocation table");
    for (const BchPointer& p : pointers)
        if (p.Target == 14 && U32(bch, p.At) >= insertAt) throw FormatError("BCH writer: a pointer from the file's start past the raw data would move");

    auto pointerIndex = [&](uint32_t at) {
        for (size_t i = 0; i < pointers.size(); i++) if (pointers[i].At == at) return i;
        throw FormatError("BCH writer: a buffer address has no relocation");
    };

    Bytes extra;     // appended to the raw section
    Bytes out = bch; // command words patched in place (they lie before the insertion)
    std::vector<std::pair<size_t, uint32_t>> wideRelocations; // relocation entries to name the 16-bit index section
    uint32_t degenerate = 0;                                   // three zero indices, for unused sub-meshes
    for (const BchGeometry& g : meshes)
    {
        if (g.Mesh >= model.Meshes.size()) throw FormatError("BCH writer: no such mesh");
        const BchMesh& mesh = model.Meshes[g.Mesh];
        if (!mesh.Stride || mesh.SubMeshes.empty() || !mesh.VertexBufferWord) throw FormatError("BCH writer: mesh " + std::to_string(g.Mesh) + " has no vertex buffer to replace");
        if (g.Vertices.empty() || g.Vertices.size() > 65536) throw FormatError("BCH writer: 1 to 65536 vertices a mesh");
        if (g.Triangles.size() % 3) throw FormatError("BCH writer: triangles are three indices each");
        for (uint32_t i : g.Triangles) if (i >= g.Vertices.size()) throw FormatError("BCH writer: an index past the vertices");

        Pad(extra, insertAt, 16);
        const uint32_t vertexAt = insertAt + (uint32_t)extra.size();
        const Bytes vb = EncodeVertices(mesh, g.Vertices, vertexAt);
        extra.insert(extra.end(), vb.begin(), vb.end());
        const BchPointer& vp = pointers[pointerIndex(mesh.VertexBufferWord)];
        Put32(out, vp.At, vertexAt - (s.Base(vp.Target) & 0x7FFFFFFF));

        std::vector<uint32_t> indices = g.Triangles;
        if (indices.empty()) indices = {0, 0, 0};
        Pad(extra, insertAt, 16);
        const uint32_t indexAt = insertAt + (uint32_t)extra.size();
        for (uint32_t i : indices) { extra.push_back((uint8_t)i); extra.push_back((uint8_t)(i >> 8)); }
        for (size_t k = 0; k < mesh.SubMeshes.size(); k++)
        {
            const BchSubMesh& sub = mesh.SubMeshes[k];
            if (!sub.IndexBufferWord || !sub.CountWord) throw FormatError("BCH writer: a sub-mesh without index buffer commands");
            if (sub.Mode != 0) throw FormatError("BCH writer: a sub-mesh drawn as strips or fans (the count would mean another thing)");
            if (k && !degenerate)
            {
                Pad(extra, insertAt, 16);
                degenerate = insertAt + (uint32_t)extra.size();
                extra.insert(extra.end(), 6, 0);
            }
            const uint32_t at = k ? degenerate : indexAt;
            const size_t ip = pointerIndex(sub.IndexBufferWord);
            Put32(out, sub.IndexBufferWord, at - s.Raw);
            wideRelocations.push_back({ip, Wide16});
            Put32(out, sub.CountWord, k ? 3 : (uint32_t)indices.size());
        }
    }
    Pad(extra, insertAt, 0x80); // the sections after it keep their alignment

    // relocation entries: the index buffers' section number (bits 25-28), in the table before it moves
    for (const auto& [index, target] : wideRelocations)
    {
        const size_t at = s.Relocation + index * 4;
        Put32(out, at, (U32(out, at) & ~(0xFu << 25)) | target << 25);
    }
    out.insert(out.begin() + insertAt, extra.begin(), extra.end());

    // the replaced meshes' old buffers, in whole 0x80 blocks (what stays keeps its alignment), unless a
    // pointer still points into them (a buffer another mesh shares)
    std::vector<std::pair<uint32_t, uint32_t>> drop; // [start, end), file offsets, sorted, disjoint
    auto block = [&](uint32_t start, uint32_t length) {
        const uint32_t a = (start + 0x7F) & ~0x7Fu, b = (start + length) & ~0x7Fu;
        if (a < b && a >= s.Raw && b <= insertAt) drop.push_back({a, b});
    };
    for (const BchGeometry& g : meshes)
    {
        const BchMesh& mesh = model.Meshes[g.Mesh];
        block(mesh.VertexBuffer, (uint32_t)(mesh.Vertices.size() * mesh.Stride));
        for (const BchSubMesh& sub : mesh.SubMeshes) block(sub.IndexBuffer, sub.Count * (sub.Wide ? 2 : 1));
    }
    auto rawTarget = [](uint32_t section) { return section >= 4 && section <= 8; };
    for (const BchPointer& p : pointers)
    {
        if (p.At >= s.Raw) throw FormatError("BCH writer: a pointer stored in the raw data");
        if (p.Target == 14 && U32(out, p.At) >= s.Raw) throw FormatError("BCH writer: a pointer from the file's start into the raw data");
        if (!rawTarget(p.Target)) continue;
        const uint32_t target = U32(out, p.At) + s.Raw;
        for (auto& d : drop) if (target >= d.first && target < d.second) d = {0, 0}; // still used
    }
    std::sort(drop.begin(), drop.end());
    std::vector<std::pair<uint32_t, uint32_t>> merged;
    for (const auto& d : drop)
    {
        if (d.first == d.second) continue;
        if (!merged.empty() && d.first <= merged.back().second) merged.back().second = std::max(merged.back().second, d.second);
        else merged.push_back(d);
    }
    auto removedBefore = [&](uint32_t at) {
        uint32_t n = 0;
        for (const auto& d : merged) if (d.second <= at) n += d.second - d.first;
        return n;
    };
    for (const BchPointer& p : pointers)
        if (rawTarget(p.Target)) Put32(out, p.At, U32(out, p.At) - removedBefore(U32(out, p.At) + s.Raw));
    for (auto d = merged.rbegin(); d != merged.rend(); ++d) out.erase(out.begin() + d->first, out.begin() + d->second);

    const uint32_t grown = (uint32_t)extra.size(), removed = removedBefore(insertAt);
    Put32(out, HeaderLengths + Raw * 4, s.RawExt - s.Raw + grown - removed);
    Put32(out, HeaderAddresses + RawExt * 4, s.RawExt + grown - removed);
    Put32(out, HeaderAddresses + Relocation * 4, s.Relocation + grown - removed);
    return out;
}

Bytes BchSetTextureName(const Bytes& bch, size_t modelIndex, size_t material, int slot, const std::string& name)
{
    const Bch parsed = Bch::Read(bch);
    if (parsed.Version < 0x21) throw FormatError("BCH writer: only version 0x21 files (Omega Ruby / Alpha Sapphire)");
    if (modelIndex >= parsed.Models.size() || material >= parsed.Models[modelIndex].Materials.size() || slot < 0 || slot > 2)
        throw FormatError("BCH writer: no such material or texture slot");
    if (name.empty() || name.size() > 255 || name.find('\0') != std::string::npos) throw FormatError("BCH writer: a texture name of 1-255 characters");
    const BchMaterial& mat = parsed.Models[modelIndex].Materials[material];
    if (mat.Texture[slot].empty()) throw FormatError("BCH writer: the slot names no texture (it has no pointer to repoint)");
    const BchSections s = BchSections::Read(bch);
    bool relocated = false;
    for (const BchPointer& p : BchPointers(bch, s))
    {
        if (p.At == mat.TextureNameWord[slot]) relocated = p.Target == 1;
        if (p.Target == 14 && U32(bch, p.At) >= s.Commands) throw FormatError("BCH writer: a pointer from the file's start past the strings would move");
    }
    if (!relocated) throw FormatError("BCH writer: the texture name pointer is not a relocated string pointer");
    if (!(s.Strings < s.Commands && s.Commands <= s.Raw && s.Raw <= s.RawExt && s.RawExt <= s.Relocation))
        throw FormatError("BCH writer: the sections are not in the expected order");

    // the name at the end of the strings, before the commands; padded to 0x80
    const uint32_t insertAt = s.Commands;
    Bytes text(name.begin(), name.end());
    text.push_back(0);
    while (text.size() % 0x80) text.push_back(0);
    Bytes out = bch;
    Put32(out, mat.TextureNameWord[slot], insertAt - s.Strings);
    out.insert(out.begin() + insertAt, text.begin(), text.end());
    const uint32_t grown = (uint32_t)text.size();
    Put32(out, HeaderLengths + Strings * 4, insertAt - s.Strings + (uint32_t)name.size() + 1);
    for (Section k : {Commands, Raw, RawExt, Relocation}) Put32(out, HeaderAddresses + k * 4, U32(bch, HeaderAddresses + k * 4) + grown);
    return out;
}

}
