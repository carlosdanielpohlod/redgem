#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/frame_profiler.hpp"
#include "render/gpu_timers.hpp"

namespace redgem {
struct ScriptContext;
class ScriptProfile;

// The scripts' methods the overlay lists, most self time first.
inline constexpr std::size_t kOverlayScriptRows = 5;

// What the overlay shows besides the frame's phases: the graphics card's time over each pass (left
// out when it measured none) and the scripts' sampled methods (left out without samples).
struct ProfileOverlayExtras {
  GpuTimings gpu;
  const ScriptProfile* scripts = nullptr;
};

// The overlay's lines, top to bottom.
std::vector<std::string> profile_overlay_lines(const ProfileSummary& stats, const ProfileOverlayExtras& extras);
// Uses the completed frame's statistics, shared by player, editor and recorded frames.
bool shows_profile_stats(const ScriptContext& context);
void append_profile_overlay(ScriptContext& context, const ProfileOverlayExtras& extras = {});
}  // namespace redgem
