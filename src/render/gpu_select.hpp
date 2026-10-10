#pragma once

#include <map>
#include <string>

namespace redgem {

// Laptops with two graphics cards start programs on the weak integrated one unless asked.
// Returns the environment variables that make OpenGL use the dedicated card:
//   NVIDIA driver loaded   -> PRIME render offload to it
//   otherwise              -> DRI_PRIME (the second card with open-source drivers; harmless with one card)
// `preference` is the REDGEM_GPU setting: "integrated" keeps the default card (empty map).
std::map<std::string, std::string> discrete_gpu_environment(bool nvidia_driver_loaded, const std::string& preference);

// Applies the above to this process, without overriding variables the user already set.
// Must run before the window is created.
void prefer_discrete_gpu();

}  // namespace redgem
