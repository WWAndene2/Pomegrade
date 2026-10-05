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

struct BchMesh
{
    uint16_t Material = 0;
    int Layer = 0;                    // drawing layer 0-3 (opaque, translucent, subtractive, additive)
    std::vector<BchVertex> Vertices;
    std::vector<uint32_t> Triangles;  // three indices each
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
};

struct Bch
{
    uint8_t Version = 0;
    std::vector<BchModel> Models;
    std::vector<BchTexture> Textures;

    static bool Is(const Bytes& data) { return data.size() >= 0x44 && Text(data, 0, 3) == "BCH"; }
    static Bch Read(const Bytes& data);
};

// a BCH model's meshes into glTF parts, its textures (by name, from textures) decoded,
// placed at offset
void AppendBchModel(const BchModel& model, const std::vector<BchTexture>& textures, const float offset[3],
                    std::vector<GltfPart>& parts, std::vector<GltfMaterial>& materials);

}

#endif // REMAKE_BCH_H
