#ifndef REMAKE_NSBMD_H
#define REMAKE_NSBMD_H

// NSBMD (BMD0): DS 3D models. An MDL0 block holds a dictionary of models;
// each model has shapes (GX command lists, GxDisplayList), materials (paired
// with textures and palettes by name), and a render program (SBC) that binds
// a material then draws a shape. Converted to glTF with ModelToGltf.
// Node transforms (the SBC's matrix commands) are not applied: map pieces,
// most of a Pokemon game's world, are drawn at their model's origin; skinned
// characters come out in their bind layout. Header and SBC layouts from the
// community documentation; to be checked on a real file.

#include "Gltf.h"
#include "Nsbtx.h"

#include <map>
#include <string>
#include <vector>

namespace remake
{

struct NsbmdShape
{
    std::string Name;
    Bytes DisplayList;
    int Material = -1; // bound by the SBC when drawn, -1 when never drawn
};

struct NsbmdMaterial
{
    std::string Name, Texture, Palette;
};

struct NsbmdModel
{
    std::string Name;
    float PosScale = 1.0f;
    std::vector<NsbmdShape> Shapes;
    std::vector<NsbmdMaterial> Materials;
};

class Nsbmd
{
public:
    explicit Nsbmd(Bytes file);
    const std::vector<NsbmdModel>& Models() const { return ModelList; }
    const Bytes& File() const { return Data; }

private:
    Bytes Data;
    std::vector<NsbmdModel> ModelList;
};

// one model as glTF; textures from the model file's own TEX0, else from tex
// (an NSBTX's TEX0), matched by name
// appends one model's parts and materials, its positions scaled by its
// PosScale and moved by offset (to place it in a scene of several)
void AppendModel(const Nsbmd& file, size_t model, const Tex0* tex, const float offset[3],
                 std::vector<GltfPart>& parts, std::vector<GltfMaterial>& materials);
std::string ModelToGltf(const Nsbmd& file, size_t model, const Tex0* tex = nullptr);

}

#endif // REMAKE_NSBMD_H
