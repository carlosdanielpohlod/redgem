#pragma once

// Which compressed texture formats a graphics card takes, and which one a KTX2 (Basis Universal)
// picture is transcoded to on it: BCn on desktop cards, ASTC or ETC2 on OpenGL ES (phones), and
// plain RGBA8 when the card takes none of them. Nothing here talks to the card: the renderer reads
// its extensions (render/compressed_textures.hpp) and hands them over.

#include <string>
#include <vector>

namespace redgem {

// What a picture's levels are made of on the graphics card.
enum class GpuBlockFormat {
  Bc1Rgb,       // BC1 (S3TC DXT1), opaque
  Bc3Rgba,      // BC3 (S3TC DXT5), with alpha
  Astc4x4Rgba,  // ASTC 4x4 (LDR), with or without alpha
  Etc1Rgb,      // ETC1, uploaded as ETC2 RGB (which reads it as it is), opaque
  Etc2Rgba,     // ETC2 with EAC alpha
  Rgba8         // not compressed: four bytes a pixel
};

// The compressed formats a graphics card takes.
struct GpuTextureSupport {
  bool bc = false;    // BC1 to BC3 (EXT_texture_compression_s3tc)
  bool astc = false;  // ASTC LDR (KHR_texture_compression_astc_ldr)
  bool etc2 = false;  // ETC2 and EAC (core in OpenGL ES 3, ARB_ES3_compatibility on desktop)
};

// What a card with these OpenGL extensions takes; OpenGL ES 3 takes ETC2 whatever it lists.
GpuTextureSupport support_from_extensions(const std::vector<std::string>& extensions, bool opengl_es);

// The format a picture is transcoded to on a card that takes `support`: BCn first (the desktop's
// own), then ASTC (the better of the two on phones), then ETC2, and RGBA8 when it takes none.
GpuBlockFormat best_block_format(const GpuTextureSupport& support, bool has_alpha);

// Whether the format is one of the compressed ones.
bool is_block_compressed(GpuBlockFormat format);

}  // namespace redgem
