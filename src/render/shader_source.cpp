#include "render/shader_source.hpp"

#include "render/gpu_profile.hpp"
#include "render/shaders/uniforms.hpp"

namespace redgem::shaders {
namespace {

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
  return with_header(source, gpu_profile::kShaderHeader, gpu_profile::kMaxSkinJoints, kMaxPointLights);
}

}  // namespace redgem::shaders
