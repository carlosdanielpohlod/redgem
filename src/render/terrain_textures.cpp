#include "render/terrain_textures.hpp"

#include <rlgl.h>

#include <algorithm>
#include <cstring>
#include <vector>

#include "scene/ground/splat_map.hpp"

#if defined(GRAPHICS_API_OPENGL_ES3)
#include <GLES3/gl3.h>
#else
#include "external/glad.h"  // raylib's loader: its functions are loaded once raylib opens a window
#endif

namespace redgem::render_detail {
namespace {

// Anisotropic filtering (EXT_texture_filter_anisotropic, core in OpenGL 4.6): ground seen at a
// low angle stays sharp. Asked for, and its errors forgotten, where the card does not have it.
constexpr GLenum kMaxAnisotropy = 0x84FE;
constexpr GLenum kLargestAnisotropy = 0x84FF;
constexpr GLfloat kWantedAnisotropy = 8.0f;

constexpr const char* kAnisotropyExtension = "GL_EXT_texture_filter_anisotropic";
constexpr const char* kCoreAnisotropyExtension = "GL_ARB_texture_filter_anisotropic";

bool named_anisotropy(const char* name) {
  return name != nullptr &&
         (std::strcmp(name, kAnisotropyExtension) == 0 || std::strcmp(name, kCoreAnisotropyExtension) == 0);
}

// Whether the card filters anisotropically (asked once: the answer never changes).
bool has_anisotropy() {
  static const bool found = [] {
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (GLint i = 0; i < count; ++i) {
      if (named_anisotropy(reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i)))))
        return true;
    }
    return false;
  }();
  return found;
}

void sharpen_at_low_angles() {
  if (!has_anisotropy()) return;
  GLfloat largest = 0.0f;
  glGetFloatv(kLargestAnisotropy, &largest);
  if (largest > 1.0f) glTexParameterf(GL_TEXTURE_2D_ARRAY, kMaxAnisotropy, std::min(largest, kWantedAnisotropy));
}

// How the bound array is sampled: pictures repeat, with smaller copies; weights stop at the edge.
void sample_array(bool smaller_copies) {
  const GLint wrap = smaller_copies ? GL_REPEAT : GL_CLAMP_TO_EDGE;
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, wrap);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, wrap);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, smaller_copies ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
  if (!smaller_copies) return;
  glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
  sharpen_at_low_angles();
}

// An RGBA8 array of `layers` pictures `side` x `side`, from `pixels` (the layers one after another).
unsigned int texture_array(int side, int layers, const std::uint8_t* pixels, bool smaller_copies) {
  unsigned int texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, side, side, layers, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  sample_array(smaller_copies);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
  return texture;
}

// The weights as the two RGBA pictures of the splat array, one after the other.
std::vector<std::uint8_t> splat_layers(const SplatMap& splat) {
  std::vector<std::uint8_t> pixels;
  for (int picture = 0; picture < kSplatPictures; ++picture) {
    const std::vector<std::uint8_t> layer = splat_picture(splat, picture);
    pixels.insert(pixels.end(), layer.begin(), layer.end());
  }
  return pixels;
}

constexpr std::size_t kChannels = 4;  // RGBA

// Whether an array of the pictures' side and layers holds exactly their bytes.
bool whole(const std::vector<std::uint8_t>& pixels, const TerrainPictures& pictures) {
  const auto side = static_cast<std::size_t>(std::max(pictures.side, 0));
  return pixels.size() == side * side * kChannels * static_cast<std::size_t>(pictures.layers);
}

bool has_what_is_drawn(const TerrainLayers& layers) {
  const TerrainPictures& pictures = layers.pictures;
  const bool pictures_whole =
      !pictures.empty() && pictures.side > 0 && whole(pictures.color, pictures) && whole(pictures.surface, pictures);
  return pictures_whole && well_formed(layers.splat);
}

}  // namespace

TerrainTextures upload_terrain_textures(const TerrainLayers& layers) {
  TerrainTextures textures;
  if (!has_what_is_drawn(layers)) return textures;
  const TerrainPictures& pictures = layers.pictures;
  textures.colors = texture_array(pictures.side, pictures.layers, pictures.color.data(), true);
  textures.surfaces = texture_array(pictures.side, pictures.layers, pictures.surface.data(), true);
  textures.splat = texture_array(layers.splat.size, kSplatPictures, splat_layers(layers.splat).data(), false);
  textures.layers = pictures.layers;
  textures.splat_size = layers.splat.size;
  textures.splat_version = layers.splat.version;
  return textures;
}

void update_terrain_splat(TerrainTextures& textures, const SplatMap& splat) {
  const bool same = splat.version == textures.splat_version && splat.size == textures.splat_size;
  if (same || !textures.loaded() || !well_formed(splat)) return;
  if (splat.size != textures.splat_size) {
    glDeleteTextures(1, &textures.splat);
    textures.splat = texture_array(splat.size, kSplatPictures, splat_layers(splat).data(), false);
  } else {
    glBindTexture(GL_TEXTURE_2D_ARRAY, textures.splat);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, splat.size, splat.size, kSplatPictures, GL_RGBA, GL_UNSIGNED_BYTE,
                    splat_layers(splat).data());
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
  }
  textures.splat_size = splat.size;
  textures.splat_version = splat.version;
}

void release(TerrainTextures& textures) {
  for (unsigned int* texture : {&textures.colors, &textures.surfaces, &textures.splat}) {
    if (*texture != 0) glDeleteTextures(1, texture);
  }
  textures = {};
}

bool has_texture_units(int units) {
  static const GLint available = [] {
    GLint found = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &found);
    return found;
  }();
  return available >= units;
}

void bind_terrain_textures(const TerrainTextures& textures, int first_slot) {
  const unsigned int arrays[] = {textures.colors, textures.surfaces, textures.splat};
  for (int i = 0; i < static_cast<int>(std::size(arrays)); ++i) {
    glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(first_slot + i));
    glBindTexture(GL_TEXTURE_2D_ARRAY, arrays[i]);
  }
  glActiveTexture(GL_TEXTURE0);
}

}  // namespace redgem::render_detail
