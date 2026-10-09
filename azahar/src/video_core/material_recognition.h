// Copyright 2026 Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstdint>
#include <vector>

// Pomegrade: the material and relief recognition of the remake's offline renders (tools/remake/render/materials.py,
// ORAS_RENDER.md) for the engine: a texture's texels are classed from their own pixels (no names) as
// glass, foliage, wood, tile, stone, plaster, metal or matte, from the neighbourhood's colour, contrast and stripe direction
// against profiles measured in ORAS, and the class gives the physical properties; the texture's own shading, high-passed,
// gives its relief (painted outlines raised, joins between regions faded flat), its height for parallax, the volume of the
// classes that protrude, and its normals. A texture never changes, so this runs once per texture, when the game uploads it.
// The same steps as materials.py, filter for filter (scipy's box, Sobel, Gaussian with truncate 4, median, morphology and
// exact Euclidean distance, with the same edge modes), so the maps match the offline ones.
namespace VideoCore::MaterialRecognition {

enum class Class : uint8_t { Glass, Foliage, Wood, Tile, Stone, Plaster, Metal, Matte };
constexpr int ClassCount = 8;

struct ClassProperties {
    float roughness, metalness, relief, transmission, volume;
};
/// materials.py's CLASSES and VOLUME, by Class
const ClassProperties& Properties(Class c);

struct Maps {
    int width = 0, height = 0;
    std::vector<uint8_t> classes; ///< Class per texel
    std::vector<uint8_t> heights; ///< 0 deepest, 255 highest; flat surfaces at 255 (materials.py's _height)
    std::vector<uint8_t> normal;  ///< RGB per texel, tangent space, 0.5 + 0.5 n (materials.py's _normal)
    std::vector<uint8_t> volume;  ///< 0-255, how much a texel rises in the layered volume (materials.py's _volume)
    std::vector<uint8_t> outline; ///< 255 on the painted outlines (materials.py's _outline)
};

/// The maps of an RGBA8 texture (rows top to bottom, 4 bytes a texel)
Maps Recognise(const uint8_t* rgba, int width, int height);

} // namespace VideoCore::MaterialRecognition
