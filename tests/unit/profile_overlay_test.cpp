#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "app/profile_overlay.hpp"
#include "script/script_profile.hpp"

using namespace redgem;

namespace {
constexpr std::size_t kFrameLines = 2 + kProfilePhaseCount;  // FPS, frame time, then each phase

ProfileSummary summary() {
  ProfileSummary stats;
  stats.frames_per_second = 60.0;
  stats.mean.has_interval = true;
  stats.mean.frame_ms = 16.5;
  return stats;
}
}  // namespace

TEST_CASE("the overlay shows only the frame's phases when nothing else was measured") {
  const std::vector<std::string> lines = profile_overlay_lines(summary(), {});
  REQUIRE(lines.size() == kFrameLines);
  CHECK(lines[0] == "FPS: 60.00");
  CHECK(lines[1] == "Frame: 16.50 ms (wall time)");
}

TEST_CASE("the graphics card's passes follow the phases once it measured them") {
  ProfileOverlayExtras extras;
  extras.gpu.pass_ms = {0.25, 3.5, 1.0};
  CHECK(profile_overlay_lines(summary(), extras).size() == kFrameLines);  // not measured yet
  extras.gpu.measured = true;
  const std::vector<std::string> lines = profile_overlay_lines(summary(), extras);
  REQUIRE(lines.size() == kFrameLines + kGpuPassCount);
  CHECK(lines[kFrameLines] == "GPU prepare: 0.25 ms");
  CHECK(lines[kFrameLines + 1] == "GPU scene: 3.50 ms");
  CHECK(lines[kFrameLines + 2] == "GPU post: 1.00 ms");
}

TEST_CASE("the scripts' busiest methods close the overlay, at most the overlay's rows of them") {
  ScriptProfile scripts;
  ProfileOverlayExtras extras;
  extras.scripts = &scripts;
  CHECK(profile_overlay_lines(summary(), extras).size() == kFrameLines);  // no samples yet
  for (int i = 0; i < 8; ++i) scripts.record({"Game#update", "Method#n" + std::to_string(i)});
  scripts.record({"Game#update", "Method#n0"});
  const std::vector<std::string> lines = profile_overlay_lines(summary(), extras);
  REQUIRE(lines.size() == kFrameLines + 1 + kOverlayScriptRows);
  CHECK(lines[kFrameLines] == "Scripts (9 samples):");
  CHECK(lines[kFrameLines + 1] == "Method#n0 22.2% self, 22.2% total");
}
