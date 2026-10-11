#include "render/gpu_select.hpp"

#include "core/platform.hpp"

#include <filesystem>

namespace redgem {
namespace {

constexpr const char* kNvidiaDriverInfo = "/proc/driver/nvidia/version";
// What the drivers read to pick a card.
constexpr const char* kNvidiaOffloadVariable = "__NV_PRIME_RENDER_OFFLOAD";
constexpr const char* kGlxVendorVariable = "__GLX_VENDOR_LIBRARY_NAME";
constexpr const char* kNvidiaVendor = "nvidia";
constexpr const char* kSecondCardVariable = "DRI_PRIME";
constexpr const char* kEnabled = "1";

struct NamedChoice {
  GpuChoice choice;
  const char* name;
};
constexpr NamedChoice kChoices[] = {
    {GpuChoice::Auto, "auto"}, {GpuChoice::Dedicated, "dedicated"}, {GpuChoice::Integrated, "integrated"}};

}  // namespace

const char* gpu_choice_name(GpuChoice choice) {
  for (const NamedChoice& named : kChoices) {
    if (named.choice == choice) return named.name;
  }
  return kChoices[0].name;
}

std::optional<GpuChoice> gpu_choice_from_name(const std::string& name) {
  for (const NamedChoice& named : kChoices) {
    if (name == named.name) return named.choice;
  }
  return std::nullopt;
}

std::map<std::string, std::string> discrete_gpu_environment(bool nvidia_driver_loaded, GpuChoice choice) {
  if (choice == GpuChoice::Integrated) return {};
  if (nvidia_driver_loaded) {
    return {{kNvidiaOffloadVariable, kEnabled}, {kGlxVendorVariable, kNvidiaVendor}};
  }
  return {{kSecondCardVariable, kEnabled}};
}

void prefer_discrete_gpu(GpuChoice choice) {
  std::error_code error;
  const bool nvidia = std::filesystem::exists(kNvidiaDriverInfo, error);
  for (const auto& [name, value] : discrete_gpu_environment(nvidia, choice)) {
    platform::set_environment(name, value, false);  // what the user already set stays
  }
}

}  // namespace redgem
