#ifndef REMAKE_GLTF_H
#define REMAKE_GLTF_H

// glTF 2.0 output (Khronos's open 3D format: Blender, viewers, engines),
// one self-contained .gltf: geometry and PNG images embedded as base64 data
// URIs. One primitive per material.

#include "GxDisplayList.h"

#include <string>
#include <vector>

namespace remake
{

struct GltfMaterial
{
    std::string Name;
    Bytes Png;                       // empty: untextured
    uint32_t TexWidth = 1, TexHeight = 1; // texel coordinates are divided by these
    bool RepeatS = true, RepeatT = true, MirrorS = false, MirrorT = false;
    bool AlphaBlend = false;
};

struct GltfPart
{
    GxMesh Mesh;
    int Material = -1; // index into the materials, -1: none
};

std::string WriteGltf(const std::vector<GltfPart>& parts, const std::vector<GltfMaterial>& materials, float scale = 1.0f);

}

#endif // REMAKE_GLTF_H
