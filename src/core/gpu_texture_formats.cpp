#include "core/gpu_texture_formats.hpp"

#include <algorithm>

namespace redgem {
namespace {

constexpr const char* kS3tcExtension = "GL_EXT_texture_compression_s3tc";
constexpr const char* kAstcExtension = "GL_KHR_texture_compression_astc_ldr";
constexpr const char* kEs3CompatibilityExtension = "GL_ARB_ES3_compatibility";

bool lists(const std::vector<std::string>& extensions, const char* name) {
  return std::find(extensions.begin(), extensions.end(), name) != extensions.end();
}

GpuBlockFormat bc_format(bool has_alpha) { return has_alpha ? GpuBlockFormat::Bc3Rgba : GpuBlockFormat::Bc1Rgb; }
GpuBlockFormat etc_format(bool has_alpha) { return has_alpha ? GpuBlockFormat::Etc2Rgba : GpuBlockFormat::Etc1Rgb; }

}  // namespace

GpuTextureSupport support_from_extensions(const std::vector<std::string>& extensions, bool opengl_es) {
  GpuTextureSupport support;
  support.bc = lists(extensions, kS3tcExtension);
  support.astc = lists(extensions, kAstcExtension);
  support.etc2 = opengl_es || lists(extensions, kEs3CompatibilityExtension);
  return support;
}

GpuBlockFormat best_block_format(const GpuTextureSupport& support, bool has_alpha) {
  if (support.bc) return bc_format(has_alpha);
  if (support.astc) return GpuBlockFormat::Astc4x4Rgba;
  if (support.etc2) return etc_format(has_alpha);
  return GpuBlockFormat::Rgba8;
}

bool is_block_compressed(GpuBlockFormat format) { return format != GpuBlockFormat::Rgba8; }

}  // namespace redgem
