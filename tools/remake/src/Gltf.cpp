#include "Gltf.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>

namespace remake
{

static std::string Base64(const Bytes& b)
{
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o;
    for (size_t i = 0; i < b.size(); i += 3)
    {
        const uint32_t v = b[i] << 16 | (i + 1 < b.size() ? b[i + 1] << 8 : 0) | (i + 2 < b.size() ? b[i + 2] : 0);
        o += t[v >> 18 & 63]; o += t[v >> 12 & 63];
        o += i + 1 < b.size() ? t[v >> 6 & 63] : '=';
        o += i + 2 < b.size() ? t[v & 63] : '=';
    }
    return o;
}

static void PutF(Bytes& b, float f) { uint8_t x[4]; memcpy(x, &f, 4); b.insert(b.end(), x, x + 4); }
static void PutU32(Bytes& b, uint32_t v) { for (int i = 0; i < 4; i++) b.push_back((v >> (8 * i)) & 0xFF); }

std::string WriteGltf(const std::vector<GltfPart>& parts, const std::vector<GltfMaterial>& materials, float scale)
{
    Bytes buffer;
    std::ostringstream views, accessors, meshes, mats, textures, images, samplers;
    int viewCount = 0, accessorCount = 0;
    auto addView = [&](size_t offset, size_t length, int target) {
        views << (viewCount ? "," : "") << "{\"buffer\":0,\"byteOffset\":" << offset << ",\"byteLength\":" << length << ",\"target\":" << target << "}";
        return viewCount++;
    };
    auto addAccessor = [&](int view, int componentType, size_t count, const char* type, const std::string& extra = "") {
        accessors << (accessorCount ? "," : "") << "{\"bufferView\":" << view << ",\"componentType\":" << componentType
                  << ",\"count\":" << count << ",\"type\":\"" << type << "\"" << extra << "}";
        return accessorCount++;
    };

    meshes << "{\"primitives\":[";
    bool firstPrim = true;
    for (const GltfPart& part : parts)
    {
        const GxMesh& m = part.Mesh;
        if (m.Triangles.empty()) continue;
        const GltfMaterial* mat = part.Material >= 0 ? &materials.at(part.Material) : nullptr;
        // positions, with their bounds (glTF requires min/max)
        float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
        size_t off = buffer.size();
        for (const GxVertex& v : m.Vertices)
            for (int k = 0; k < 3; k++) { const float p = v.Position[k] * scale; PutF(buffer, p); lo[k] = std::min(lo[k], p); hi[k] = std::max(hi[k], p); }
        std::ostringstream bounds;
        bounds << ",\"min\":[" << lo[0] << "," << lo[1] << "," << lo[2] << "],\"max\":[" << hi[0] << "," << hi[1] << "," << hi[2] << "]";
        const int posAcc = addAccessor(addView(off, buffer.size() - off, 34962), 5126, m.Vertices.size(), "VEC3", bounds.str());
        off = buffer.size();
        for (const GxVertex& v : m.Vertices)
        {
            const float len = std::sqrt(v.Normal[0] * v.Normal[0] + v.Normal[1] * v.Normal[1] + v.Normal[2] * v.Normal[2]);
            for (int k = 0; k < 3; k++) PutF(buffer, len > 0 ? v.Normal[k] / len : (k == 2 ? 1.0f : 0.0f));
        }
        const int nrmAcc = addAccessor(addView(off, buffer.size() - off, 34962), 5126, m.Vertices.size(), "VEC3");
        off = buffer.size();
        for (const GxVertex& v : m.Vertices)
        {
            PutF(buffer, mat ? v.TexCoord[0] / mat->TexWidth : 0.0f);
            PutF(buffer, mat ? v.TexCoord[1] / mat->TexHeight : 0.0f);
        }
        const int uvAcc = addAccessor(addView(off, buffer.size() - off, 34962), 5126, m.Vertices.size(), "VEC2");
        off = buffer.size();
        for (const GxVertex& v : m.Vertices)
        {
            uint8_t rgb[3];
            for (int k = 0; k < 3; k++) { const uint8_t c = (v.Colour >> (5 * k)) & 31; rgb[k] = (uint8_t)(c << 3 | c >> 2); }
            for (int k = 0; k < 3; k++) PutF(buffer, rgb[k] / 255.0f);
        }
        const int colAcc = addAccessor(addView(off, buffer.size() - off, 34962), 5126, m.Vertices.size(), "VEC3");
        off = buffer.size();
        for (uint32_t i : m.Triangles) PutU32(buffer, i);
        const int idxAcc = addAccessor(addView(off, buffer.size() - off, 34963), 5125, m.Triangles.size(), "SCALAR");
        meshes << (firstPrim ? "" : ",") << "{\"attributes\":{\"POSITION\":" << posAcc << ",\"NORMAL\":" << nrmAcc << ",\"TEXCOORD_0\":" << uvAcc
               << ",\"COLOR_0\":" << colAcc << "},\"indices\":" << idxAcc << (part.Material >= 0 ? ",\"material\":" + std::to_string(part.Material) : "") << "}";
        firstPrim = false;
    }
    meshes << "]}";

    int textureCount = 0;
    for (size_t i = 0; i < materials.size(); i++)
    {
        const GltfMaterial& m = materials[i];
        mats << (i ? "," : "") << "{\"name\":\"" << m.Name << "\",\"doubleSided\":true,\"pbrMetallicRoughness\":{\"metallicFactor\":0,\"roughnessFactor\":1";
        if (!m.Png.empty())
        {
            auto wrap = [](bool repeat, bool mirror) { return repeat ? (mirror ? 33648 : 10497) : 33071; };
            samplers << (textureCount ? "," : "") << "{\"magFilter\":9728,\"minFilter\":9728,\"wrapS\":" << wrap(m.RepeatS, m.MirrorS) << ",\"wrapT\":" << wrap(m.RepeatT, m.MirrorT) << "}";
            images << (textureCount ? "," : "") << "{\"uri\":\"data:image/png;base64," << Base64(m.Png) << "\"}";
            textures << (textureCount ? "," : "") << "{\"source\":" << textureCount << ",\"sampler\":" << textureCount << "}";
            mats << ",\"baseColorTexture\":{\"index\":" << textureCount << "}";
            textureCount++;
        }
        mats << "}" << (m.AlphaBlend ? ",\"alphaMode\":\"BLEND\"" : (!m.Png.empty() ? ",\"alphaMode\":\"MASK\"" : "")) << "}";
    }

    std::ostringstream o;
    o << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"Pomegrade remake_tool\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
      << "\"nodes\":[{\"mesh\":0}],\"meshes\":[" << meshes.str() << "],"
      << "\"buffers\":[{\"byteLength\":" << buffer.size() << ",\"uri\":\"data:application/octet-stream;base64," << Base64(buffer) << "\"}],"
      << "\"bufferViews\":[" << views.str() << "],\"accessors\":[" << accessors.str() << "]";
    if (!materials.empty()) o << ",\"materials\":[" << mats.str() << "]";
    if (textureCount) o << ",\"textures\":[" << textures.str() << "],\"images\":[" << images.str() << "],\"samplers\":[" << samplers.str() << "]";
    o << "}\n";
    return o.str();
}

}
