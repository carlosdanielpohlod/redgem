#include "render/gpu_select.hpp"

#include "core/platform.hpp"

#include <cstdlib>
#include <filesystem>

namespace redgem {
namespace {

constexpr const char* kPreferenceVariable = "REDGEM_GPU";
constexpr const char* kIntegrated = "integrated";
constexpr const char* kNvidiaDriverInfo = "/proc/driver/nvidia/version";
// What the drivers read to pick a card.
constexpr const char* kNvidiaOffloadVariable = "__NV_PRIME_RENDER_OFFLOAD";
constexpr const char* kGlxVendorVariable = "__GLX_VENDOR_LIBRARY_NAME";
constexpr const char* kNvidiaVendor = "nvidia";
constexpr const char* kSecondCardVariable = "DRI_PRIME";
constexpr const char* kEnabled = "1";
constexpr const char* kNoPreference = "";

}  // namespace

std::map<std::string, std::string> discrete_gpu_environment(bool nvidia_driver_loaded, const std::string& preference) {
  if (preference == kIntegrated) return {};
  if (nvidia_driver_loaded) {
    return {{kNvidiaOffloadVariable, kEnabled}, {kGlxVendorVariable, kNvidiaVendor}};
  }
  return {{kSecondCardVariable, kEnabled}};
}

void prefer_discrete_gpu() {
  const char* preference = std::getenv(kPreferenceVariable);
  std::error_code error;
  const bool nvidia = std::filesystem::exists(kNvidiaDriverInfo, error);
  for (const auto& [name, value] :
       discrete_gpu_environment(nvidia, preference != nullptr ? preference : kNoPreference)) {
    platform::set_environment(name, value, false);  // what the user already set stays
  }
}

}  // namespace redgem
