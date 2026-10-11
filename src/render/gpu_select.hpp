#pragma once

#include <map>
#include <optional>
#include <string>

namespace redgem {

// Which graphics card draws the editor and the games on machines that have two.
enum class GpuChoice {
  Auto,        // the dedicated card when there is one (the default)
  Dedicated,   // same as Auto today; kept apart so the choice survives a change of default
  Integrated,  // the low-power card: cooler and quieter, much slower
};

const char* gpu_choice_name(GpuChoice choice);  // "auto", "dedicated", "integrated"
std::optional<GpuChoice> gpu_choice_from_name(const std::string& name);

// Laptops with two graphics cards start programs on the weak integrated one unless asked.
// Returns the environment variables that make OpenGL use the dedicated card:
//   NVIDIA driver loaded   -> PRIME render offload to it
//   otherwise              -> DRI_PRIME (the second card with open-source drivers; harmless with one card)
// GpuChoice::Integrated keeps the default card (empty map).
std::map<std::string, std::string> discrete_gpu_environment(bool nvidia_driver_loaded, GpuChoice choice);

// Applies the above to this process, without overriding variables the user already set (they
// are what the graphics drivers read: there is no other way to tell them). Must run before the
// window is created; whoever opens a window is given the choice and calls this first.
void prefer_discrete_gpu(GpuChoice choice);

}  // namespace redgem
