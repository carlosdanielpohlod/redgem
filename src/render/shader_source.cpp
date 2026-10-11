#include "render/shader_source.hpp"

#include "core/config.hpp"
#include "render/shaders.hpp"

namespace redgem::shaders {
namespace {

#if defined(GRAPHICS_API_OPENGL_ES3)
constexpr const char* kHeaderOfThisGpu = kMobileHeader;
#else
constexpr const char* kHeaderOfThisGpu = kDesktopVersionLine;
#endif
constexpr const char* kDefine = "\n#define ";
constexpr const char* kNameValueSeparator = " ";

std::string define(const char* name, int value) {
  return std::string(kDefine) + name + kNameValueSeparator + std::to_string(value);
}

}  // namespace

std::string with_header(const std::string& source, const std::string& header, int max_bones, int max_point_lights) {
  const std::size_t version = source.find(kDesktopVersionLine);
  if (version == std::string::npos) return source;
  const std::string limits = define(kBoneLimitName, max_bones) + define(kPointLightLimitName, max_point_lights);
  return header + limits + source.substr(version + std::string(kDesktopVersionLine).size());
}

std::string for_this_gpu(const char* source) {
  return with_header(source, kHeaderOfThisGpu, config::kMaxSkinJoints, kMaxPointLights);
}

}  // namespace redgem::shaders
