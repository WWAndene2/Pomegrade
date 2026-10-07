#ifndef REMAKE_ASSETEDIT_H
#define REMAKE_ASSETEDIT_H

// Asset customization: a model's textures changed on the owner's instructions (DEVELOPMENT_NOTE.md 3c,
// "Asset customization"), the edits done by Claude through remake_tool oras-asset.
//
// A BCH model file keeps its textures' data unrelocated (BchTexture::DataOffset), so an edited texture
// is decoded, changed and encoded back in its own format and size (PicaTextureEncode) over its old
// bytes: nothing else in the file moves, the model, its materials and the textures' names stay as the
// game made them.

#include <functional>
#include <string>
#include <vector>
#include "Bytes.h"

namespace remake
{

// what a texture edit receives and changes: RGBA, rows top to bottom, width x height
using TextureEditFn = std::function<void(Bytes& rgba, uint32_t width, uint32_t height)>;

// bch with texture `name` edited in place; throws FormatError when it has no such texture
Bytes BchEditTexture(const Bytes& bch, const std::string& name, const TextureEditFn& edit);

// colours turned round the colour wheel by `hue` degrees, saturation and brightness multiplied (HSV),
// alpha kept. Only pixels whose hue lies within `range` degrees of `around` change when range < 180
// (a colour of the model, its other colours kept: "the red parts blue")
void Recolour(Bytes& rgba, double hue, double saturation, double brightness, double around = 0, double range = 180);

// an image resized to width x height (bilinear), to replace a texture with a picture of another size
Bytes Resample(const Bytes& rgba, uint32_t fromWidth, uint32_t fromHeight, uint32_t width, uint32_t height);

}

#endif // REMAKE_ASSETEDIT_H
