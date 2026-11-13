#include "render/compressed_textures.hpp"

#include <iterator>
#include <string>
#include <vector>

#if defined(GRAPHICS_API_OPENGL_ES3)
#include <GLES3/gl3.h>
#else
#include "external/glad.h"  // raylib's loader: its functions are loaded once raylib opens a window
#endif

#include "render/gl_extras.hpp"

namespace redgem::render_detail {
namespace {

#if defined(GRAPHICS_API_OPENGL_ES3)
constexpr bool kOpenGlEs = true;
#else
constexpr bool kOpenGlEs = false;
#endif

// How OpenGL and raylib name each compressed format, in GpuBlockFormat's order. (The extensions'
// names for them, which not every header defines.)
struct GlFormat {
  GLenum internal_format;
  int pixel_format;
};
constexpr GLenum kRgbS3tcDxt1 = 0x83F0;
constexpr GLenum kRgbaS3tcDxt5 = 0x83F3;
constexpr GLenum kRgbaAstc4x4 = 0x93B0;
constexpr GLenum kRgb8Etc2 = 0x9274;
constexpr GLenum kRgba8Etc2Eac = 0x9278;
constexpr GlFormat kGlFormats[] = {{kRgbS3tcDxt1, PIXELFORMAT_COMPRESSED_DXT1_RGB},
                                   {kRgbaS3tcDxt5, PIXELFORMAT_COMPRESSED_DXT5_RGBA},
                                   {kRgbaAstc4x4, PIXELFORMAT_COMPRESSED_ASTC_4x4_RGBA},
                                   {kRgb8Etc2, PIXELFORMAT_COMPRESSED_ETC2_RGB},
                                   {kRgba8Etc2Eac, PIXELFORMAT_COMPRESSED_ETC2_EAC_RGBA}};
// One for each compressed GpuBlockFormat, in its order (RGBA8, the last, is not uploaded here).
static_assert(std::size(kGlFormats) == static_cast<std::size_t>(GpuBlockFormat::Rgba8),
              "kGlFormats follows GpuBlockFormat");

std::vector<std::string> gl_extensions() {
  GLint count = 0;
  glGetIntegerv(GL_NUM_EXTENSIONS, &count);
  std::vector<std::string> names;
  for (GLint i = 0; i < count; ++i) {
    const auto* name = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i)));
    if (name != nullptr) names.emplace_back(name);
  }
  return names;
}

void upload_level(const TranscodedLevel& level, GLint index, GLenum format) {
  glCompressedTexImage2D(GL_TEXTURE_2D, index, format, static_cast<GLsizei>(level.size.width),
                         static_cast<GLsizei>(level.size.height), 0, static_cast<GLsizei>(level.bytes.size()),
                         level.bytes.data());
}

// Sampled from the levels the picture brought (and no others), repeating as glTF's default does.
void use_levels(GLint count) {
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, count - 1);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

// The texture's name on the card, its levels uploaded; 0 when the card refused any of it.
unsigned int uploaded_texture(const TranscodedPicture& picture, GLenum format) {
  forget_errors();
  unsigned int id = 0;
  glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_2D, id);
  const auto count = static_cast<GLint>(picture.levels.size());
  for (GLint level = 0; level < count; ++level)
    upload_level(picture.levels[static_cast<std::size_t>(level)], level, format);
  use_levels(count);
  glBindTexture(GL_TEXTURE_2D, 0);
  if (glGetError() == GL_NO_ERROR) return id;
  glDeleteTextures(1, &id);
  return 0;
}

bool uploadable(const TranscodedPicture& picture) {
  return is_block_compressed(picture.format) && !picture.levels.empty();
}

}  // namespace

const GpuTextureSupport& gpu_texture_support() {
  static const GpuTextureSupport support = support_from_extensions(gl_extensions(), kOpenGlEs);
  return support;
}

Texture2D upload_compressed(const TranscodedPicture& picture) {
  if (!uploadable(picture)) return Texture2D{};
  const GlFormat format = kGlFormats[static_cast<std::size_t>(picture.format)];
  const unsigned int id = uploaded_texture(picture, format.internal_format);
  if (id == 0) return Texture2D{};
  const ImageSize size = picture.levels.front().size;
  return Texture2D{id, static_cast<int>(size.width), static_cast<int>(size.height),
                   static_cast<int>(picture.levels.size()), format.pixel_format};
}

}  // namespace redgem::render_detail
