#include "BchWriter.h"
#include "PicaCommands.h"

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

// the vertices in the mesh's format; base: the buffer's file offset (alignment is the file's: an element's
// offset is base plus what is written so far)
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
                PushElement(out, base, a.Format, (k < available ? src[k] : 1.0f) / (a.Scale ? a.Scale : 1.0f));
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
        // the new buffers are in the raw section: a mesh whose buffers RawExt holds (sections 9-13) is refused
        if (vp.Target < 4 || vp.Target > 8) throw FormatError("BCH writer: mesh " + std::to_string(g.Mesh) + "'s vertex buffer is not in the raw data");
        for (const BchSubMesh& sub : mesh.SubMeshes)
        {
            const uint32_t t = pointers[pointerIndex(sub.IndexBufferWord)].Target;
            if (t < 4 || t > 8) throw FormatError("BCH writer: mesh " + std::to_string(g.Mesh) + "'s index buffer is not in the raw data");
        }
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
    // [start, end) of the blocks, and of the whole buffer they come from: a pointer anywhere in the buffer
    // (another mesh sharing it, even short of the aligned blocks) keeps them
    struct Drop { uint32_t start, end, bufferStart, bufferEnd; };
    std::vector<Drop> drop;
    auto block = [&](uint32_t start, uint32_t length) {
        const uint32_t a = (start + 0x7F) & ~0x7Fu, b = (start + length) & ~0x7Fu;
        if (a < b && a >= s.Raw && b <= insertAt) drop.push_back({a, b, start, start + length});
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
        for (auto& d : drop) if (target >= d.bufferStart && target < d.bufferEnd) d.start = d.end = 0; // still used
    }
    std::vector<std::pair<uint32_t, uint32_t>> merged;
    for (const Drop& d : drop) if (d.start < d.end) merged.push_back({d.start, d.end});
    std::sort(merged.begin(), merged.end());
    {
        std::vector<std::pair<uint32_t, uint32_t>> joined;
        for (const auto& d : merged)
        {
            if (!joined.empty() && d.first <= joined.back().second) joined.back().second = std::max(joined.back().second, d.second);
            else joined.push_back(d);
        }
        merged.swap(joined);
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

Bytes BchReplaceString(const Bytes& bch, const std::string& from, const std::string& to)
{
    if (from.size() != to.size() || from.empty()) throw FormatError("BCH writer: a string replaced by one of another length");
    const BchSections s = BchSections::Read(bch);
    const uint32_t end = s.Strings + U32(bch, HeaderLengths + Strings * 4);
    if (end > bch.size() || end > s.Commands) throw FormatError("BCH writer: the string section runs past its neighbours");
    Bytes out = bch;
    int replaced = 0;
    // whole strings, and the model part of qualified ones ("material@model"): each string starts after a zero
    // byte (or at the section's start) and ends at one
    for (uint32_t at = s.Strings; at < end;)
    {
        uint32_t stop = at;
        while (stop < end && bch[stop]) stop++;
        const uint32_t length = stop - at, n = (uint32_t)from.size();
        if (length >= n && memcmp(&bch[stop - n], from.data(), n) == 0 && (length == n || bch[stop - n - 1] == '@'))
        {
            memcpy(&out[stop - n], to.data(), n);
            replaced++;
        }
        at = stop + 1;
    }
    if (!replaced) throw FormatError("BCH writer: no string \"" + from + "\" to replace");
    return out;
}

}

namespace remake
{

// the 24-bit float packing of a uniform loaded in 3 words (PicaCommands' decoding, reversed): component c of the four set to f
static void SetFloat24(uint32_t w[3], int c, uint32_t f)
{
    switch (c)
    {
    case 0: w[2] = (w[2] & 0xFF000000) | f; break;
    case 1: w[2] = (w[2] & 0x00FFFFFF) | (f << 24); w[1] = (w[1] & 0xFFFF0000) | (f >> 8); break;
    case 2: w[1] = (w[1] & 0x0000FFFF) | (f << 16); w[0] = (w[0] & 0xFFFFFF00) | (f >> 16); break;
    case 3: w[0] = (w[0] & 0x000000FF) | (f << 8); break;
    }
}

Bytes BchCompactVertices(const Bytes& bch, size_t modelIndex, const std::vector<size_t>& meshes, std::vector<std::string>* log)
{
    std::vector<BchGeometry> geometry;
    const Bch parsed = Bch::Read(bch);
    if (modelIndex >= parsed.Models.size()) throw FormatError("BCH compact: no such model");
    const BchModel& model = parsed.Models[modelIndex];
    Bytes out = bch;
    std::vector<std::pair<size_t, uint32_t>> patched; // every command word changed, to apply again after the old buffers are freed
    auto patch = [&](size_t at, uint32_t value) { Put32(out, at, value); patched.push_back({at, value}); };
    auto word = [&](size_t at) { uint32_t v = 0; for (int i = 0; i < 4; i++) v |= (uint32_t)out.at(at + i) << (8 * i); return v; };
    for (size_t index : meshes)
    {
        if (index >= model.Meshes.size()) throw FormatError("BCH compact: no such mesh");
        const BchMesh& mesh = model.Meshes[index];
        if (mesh.Vertices.empty() || !mesh.CommandsAt) continue;
        // the new format of each attribute: positions and texture coordinates s16 over their largest magnitude, normals s8 over
        // 127, colours as they are; any other attribute leaves the mesh as it is
        float posMax = 0, uvMax = 0;
        for (const BchVertex& v : mesh.Vertices)
        {
            for (int k = 0; k < 3; k++) posMax = std::max(posMax, std::fabs(v.Position[k] - mesh.PositionOffset[k]));
            for (int k = 0; k < 2; k++) uvMax = std::max(uvMax, std::fabs(v.TexCoord[k]));
        }
        bool supported = true;
        for (const BchAttribute& a : mesh.Attributes)
            supported = supported && (((a.Name == 0 || a.Name == 1) && a.Format == 3 && a.Elements == 3) || (a.Name == 4 && a.Format == 3 && a.Elements == 2) ||
                                      (a.Name == 3 && a.Format < 2));
        if (!supported) { if (log) log->push_back("mesh " + std::to_string(index) + ": vertex layout kept (an attribute it does not compact)"); continue; }

        // the command list: the format words, the stride, the scale uniforms (c7 x position, y normal; c8 x texture coordinates)
        std::vector<uint32_t> words(mesh.CommandWords);
        for (size_t k = 0; k < words.size(); k++) words[k] = word(mesh.CommandsAt + 4 * k);
        const PicaCommands cmd = PicaCommands::Parse(words);
        size_t formatAt[2] = {0, 0}, strideAt = 0;
        uint64_t formats = 0, attributes = 0, permutation = 0;
        int formatCount = 0, strideCount = 0;
        uint32_t total = 0; // the vertex shader's input count, as the reader takes it
        for (const PicaCommand& c : cmd.List)
            switch (c.Register)
            {
            case 0x201: formatAt[0] = c.At; formats |= c.Params[0]; formatCount++; break;
            case 0x202: formatAt[1] = c.At; formats |= (uint64_t)c.Params[0] << 32; formatCount++; break;
            case 0x204: attributes |= c.Params[0]; break;
            case 0x205: strideAt = c.At; attributes |= (uint64_t)(c.Params[0] & 0xFFFF) << 32; strideCount++; break;
            case 0x242: total = c.Params[0] + 1; break;
            case 0x2BB: permutation |= c.Params[0]; break;
            case 0x2BC: permutation |= (uint64_t)c.Params[0] << 32; break;
            }
        if (formatCount != 2 || strideCount != 1) throw FormatError("BCH compact: mesh " + std::to_string(index) + " sets its vertex format more or less than once");
        const float scale[3] = {posMax > 0 ? posMax / 32767 : 1, 1.0f / 127, uvMax > 0 ? uvMax / 32767 : 1}; // position, normal, texture coordinates
        uint32_t stride = 0;
        for (uint32_t k = 0; k < total && k < 12; k++)
        {
            if ((formats >> (48 + k)) & 1) continue; // a fixed attribute: not in the buffer
            const int slot = (int)((attributes >> (k * 4)) & 0xF);
            const int name = (int)((permutation >> (slot * 4)) & 0xF);
            int fmt = (int)((formats >> (slot * 4)) & 0xF);
            const int elements = (fmt >> 2) + 1;
            if (name == 0 || name == 4) fmt = (fmt & ~3) | 2; // s16
            if (name == 1) fmt = (fmt & ~3) | 0;              // s8
            formats = (formats & ~((uint64_t)0xF << (slot * 4))) | ((uint64_t)fmt << (slot * 4));
            const int size = (fmt & 3) == 2 ? 2 : (fmt & 3) == 3 ? 4 : 1;
            if (size > 1) stride += stride & 1; // aligned as the reader and EncodeVertices align
            stride += size * elements;
        }
        stride += stride & 1;
        const size_t base = mesh.CommandsAt;
        patch(base + 4 * formatAt[0], (uint32_t)formats);
        patch(base + 4 * formatAt[1], (uint32_t)(formats >> 32));
        patch(base + 4 * strideAt, (word(base + 4 * strideAt) & 0xFF00FFFF) | (stride << 16));
        auto setUniform = [&](int u, int c, float value) {
            if (cmd.UniformWords[u][cmd.Uniform32[u] ? c : 0] < 0) throw FormatError("BCH compact: mesh " + std::to_string(index) + " does not load uniform c" + std::to_string(u));
            if (cmd.Uniform32[u]) { uint32_t v; memcpy(&v, &value, 4); patch(base + 4 * cmd.UniformWords[u][c], v); return; }
            uint32_t w[3];
            for (int k = 0; k < 3; k++) w[k] = word(base + 4 * cmd.UniformWords[u][k]);
            SetFloat24(w, c, ToPicaFloat24(value));
            for (int k = 0; k < 3; k++) patch(base + 4 * cmd.UniformWords[u][k], w[k]);
        };
        for (const BchAttribute& a : mesh.Attributes)
        {
            if (a.Name == 0) setUniform(7, 0, scale[0]);
            if (a.Name == 1) setUniform(7, 1, scale[1]);
            if (a.Name == 4) setUniform(8, 0, scale[2]);
        }
        geometry.push_back({index, mesh.Vertices, mesh.Triangles});
        if (log) log->push_back("mesh " + std::to_string(index) + ": " + std::to_string(mesh.Vertices.size()) + " vertices, stride " + std::to_string(mesh.Stride) + " -> " + std::to_string(stride));
    }
    if (geometry.empty()) return out;
    // BchReplaceGeometry frees a mesh's old buffer by its vertex count times the stride the file states: so the old buffers are freed
    // first, while the file still states the old strides (each mesh down to one vertex and no triangle), then the format words are
    // patched (on that file, at the same command offsets: commands lie before the raw data and do not move) and the buffers written
    // again in the new layouts
    std::vector<BchGeometry> emptied;
    for (const BchGeometry& g : geometry) emptied.push_back({g.Mesh, {g.Vertices.front()}, {}});
    Bytes freed = BchReplaceGeometry(bch, modelIndex, emptied);
    for (const auto& [at, value] : patched) Put32(freed, at, value); // command words: before the raw data, at the same offsets
    return BchReplaceGeometry(freed, modelIndex, geometry);
}

}
