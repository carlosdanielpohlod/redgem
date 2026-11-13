#pragma once

// Compressed pictures onto the graphics card: which compressed formats it takes (read from its
// OpenGL extensions) and textures uploaded level by level as they are, with the mipmaps the
// picture brought (a compressed texture cannot have its mipmaps made by the card).

#include <raylib.h>

#include "core/gpu_texture_formats.hpp"
#include "core/ktx2_image.hpp"

namespace redgem::render_detail {

// What this graphics card takes, read once from its extensions (the window must be open).
const GpuTextureSupport& gpu_texture_support();

// A texture of the picture's levels in its compressed format, the full-size one first; an empty
// texture (id 0) when the picture is not compressed or the card refuses it.
Texture2D upload_compressed(const TranscodedPicture& picture);

}  // namespace redgem::render_detail
