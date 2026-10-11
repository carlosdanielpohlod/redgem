#include "render/gpu_select.hpp"

#include "core/platform.hpp"

#include <cstdlib>
#include <filesystem>

namespace redgem {
namespace {

constexpr const char* kPreferenceVariable = "REDGEM_GPU";
constexpr const char* kIntegrated = "integrated";
constexpr const char* kNvidiaDriverInfo = "/proc/driver/nvidia/version";

}  // namespace

std::map<std::string, std::string> discrete_gpu_environment(bool nvidia_driver_loaded, const std::string& preference) {
  if (preference == kIntegrated) return {};
  if (nvidia_driver_loaded) {
    return {{"__NV_PRIME_RENDER_OFFLOAD", "1"}, {"__GLX_VENDOR_LIBRARY_NAME", "nvidia"}};
  }
  return {{"DRI_PRIME", "1"}};
}

void prefer_discrete_gpu() {
  const char* preference = std::getenv(kPreferenceVariable);
  std::error_code error;
  const bool nvidia = std::filesystem::exists(kNvidiaDriverInfo, error);
  for (const auto& [name, value] : discrete_gpu_environment(nvidia, preference != nullptr ? preference : "")) {
    platform::set_environment(name, value, false);  // what the user already set stays
  }
}

}  // namespace redgem
