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
    const uint32_t grown = (uint32_t)extra.size();
    Put32(out, HeaderLengths + Raw * 4, s.RawExt - s.Raw + grown);
    Put32(out, HeaderAddresses + RawExt * 4, s.RawExt + grown);
    Put32(out, HeaderAddresses + Relocation * 4, s.Relocation + grown);
    return out;
}

}
