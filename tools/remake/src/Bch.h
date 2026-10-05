#ifndef REMAKE_BCH_H
#define REMAKE_BCH_H

// BCH (H3D): the 3DS model format Pokemon X/Y and Omega Ruby / Alpha
// Sapphire use (a map piece's terrain is one, part 1 of its GR container).
// Read: its models (meshes, materials and their texture names) and its
// textures. Not read: skeletons, animations, shaders, lights, cameras.
// Layout from SPICA (public domain; its CtrH3D classes and their serializer:
// fields in order, references as pointers, lists as pointer + count, the
// meshes, sub-meshes and materials inline). A file starts with a header of
// section addresses and lengths, then a relocation table turns the sections'
// relative pointers into absolute ones before anything is read.
// Vertices: the mesh's PICA commands give the vertex buffer, its stride and
// each attribute's format, count and order; scales and the position offset
// are vertex shader uniforms 7, 8 and 6. Sub-meshes: index buffer (8 or 16
// bits), count, triangles / strip / fan.
// Checked on Omega Ruby (version 0x21): map pieces read (19, 23 and 5 meshes,
// their terrain within or just past the piece's -360..360), their materials'
// texture names all found in one area pack each (a/0/1/4, "AD" containers:
// 18 of 18, 23 of 23, 5 of 5), and rendered: sea, cliffs, grass, trees,
// houses where the game has them. Skinned meshes are read in their bind
// pose (bones are not applied).

#include "Bytes.h"
#include "Gltf.h"

#include <string>
#include <vector>

namespace remake
{

struct BchVertex
{
    float Position[3] = {}, Normal[3] = {0, 1, 0}, TexCoord[2] = {}, Colour[4] = {1, 1, 1, 1};
};

// how a mesh stores its vertices: attributes in buffer order, each scaled by its uniform (the
// position also offset), every attribute 2-byte aligned unless bytes
struct BchAttribute
{
    int Name = 0;                     // PICA attribute: 0 position, 1 normal, 3 colour, 4 texture coordinates, ...
    int Format = 0;                   // 0 s8, 1 u8, 2 s16, 3 float
    int Elements = 0;
    float Scale = 1;
};

struct BchSubMesh
{
    uint32_t IndexBuffer = 0, Count = 0, Mode = 0; // file offset, index count, 0 triangles / 1 strip / 2 fan
    bool Wide = false;                             // 16-bit indices
    // file offsets of the command words a writer changes: the index buffer's address and the count
    uint32_t IndexBufferWord = 0, CountWord = 0;
};

struct BchMesh
{
    uint16_t Material = 0;
    int Layer = 0;                    // drawing layer 0-3 (opaque, translucent, subtractive, additive)
    std::vector<BchVertex> Vertices;
    std::vector<uint32_t> Triangles;  // three indices each

    std::vector<BchAttribute> Attributes;
    float PositionOffset[3] = {};
    uint32_t VertexBuffer = 0, Stride = 0;
    uint32_t VertexBufferWord = 0;    // file offset of the command word holding the vertex buffer's address
    std::vector<BchSubMesh> SubMeshes;
};

struct BchMaterial
{
    std::string Name, Texture[3];
};

struct BchModel
{
    std::string Name;
    std::vector<BchMaterial> Materials;
    std::vector<BchMesh> Meshes;
};

struct BchTexture
{
    std::string Name;
    uint32_t Width = 0, Height = 0;
    uint8_t Format = 0;               // PICA texture format, see PicaTexture.h
    Bytes Data;
    size_t DataOffset = 0;            // where Data lies in the BCH file (texture data isn't relocated)
};

struct Bch
{
    uint8_t Version = 0;
    std::vector<BchModel> Models;
    std::vector<BchTexture> Textures;

    static bool Is(const Bytes& data) { return data.size() >= 0x44 && Text(data, 0, 3) == "BCH"; }
    static Bch Read(const Bytes& data);
};

// the file's sections, from its header (addresses and lengths)
struct BchSections
{
    uint32_t Contents = 0, Strings = 0, Commands = 0, Raw = 0, RawExt = 0, Relocation = 0, RelocationLength = 0;
    // the address a relocation's section number (SPICA's numbering, 0-14) adds; 16-bit index buffers
    // (sections 7 and 12) are flagged in bit 31
    uint32_t Base(uint32_t section) const;
    static BchSections Read(const Bytes& data);
};

// a pointer the relocation table fixes: its file offset and the section number whose address is added
struct BchPointer
{
    uint32_t At = 0, Target = 0;
};
std::vector<BchPointer> BchPointers(const Bytes& data, const BchSections& sections);

// a BCH model's meshes into glTF parts, its textures (by name, from textures) decoded,
// placed at offset
void AppendBchModel(const BchModel& model, const std::vector<BchTexture>& textures, const float offset[3],
                    std::vector<GltfPart>& parts, std::vector<GltfMaterial>& materials);

}

#endif // REMAKE_BCH_H
