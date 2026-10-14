// Building a game: the systems it can be built for, and where the player for each is found.

#include "project/game_build.hpp"

#include "core/platform.hpp"

namespace redgem {
namespace {

constexpr const char* kLinuxTarget = "linux";
constexpr const char* kWindowsTarget = "windows";
constexpr const char* kAndroidTarget = "android";
constexpr const char* kRuntimesFolder = "runtimes";  // players for other systems, next to this program
constexpr const char* kPlayerName = "redgem-player";

}  // namespace

std::optional<BuildTarget> build_target_from_name(const std::string& name) {
  if (name == kLinuxTarget) return BuildTarget::Linux;
  if (name == kWindowsTarget) return BuildTarget::Windows;
  if (name == kAndroidTarget) return BuildTarget::Android;
  return std::nullopt;
}

const char* build_target_name(BuildTarget target) {
  switch (target) {
    case BuildTarget::Windows:
      return kWindowsTarget;
    case BuildTarget::Android:
      return kAndroidTarget;
    case BuildTarget::Linux:
      break;
  }
  return kLinuxTarget;
}

BuildTarget host_build_target() { return platform::is_windows() ? BuildTarget::Windows : BuildTarget::Linux; }

std::filesystem::path runtime_location(BuildTarget target, const std::filesystem::path& self) {
  const std::filesystem::path beside = self.parent_path();
  if (target == BuildTarget::Android) return beside / kRuntimesFolder / kAndroidTarget;
  const std::string player = platform::executable_file_name(kPlayerName, target == BuildTarget::Windows);
  if (target == host_build_target()) return beside / player;
  return beside / kRuntimesFolder / build_target_name(target) / player;
}

std::optional<std::filesystem::path> runtime_for(BuildTarget target, const std::filesystem::path& self) {
  if (target == BuildTarget::Android) return std::nullopt;
  std::filesystem::path runtime = runtime_location(target, self);
  std::error_code fs_error;
  if (!std::filesystem::is_regular_file(runtime, fs_error)) return std::nullopt;
  return runtime;
}

std::string missing_runtime_message(BuildTarget target, const std::filesystem::path& looked_for) {
  const std::string name = build_target_name(target);
  // The player for the system the engine was built on comes out of that same build.
  const std::string made_by = target == host_build_target() ? std::string("building its redgem-player target")
                                                            : "running tools/" + name + "/build_runtime.sh";
  return "cannot build for " + name + ": the player for it is missing (looked for " + looked_for.string() +
         "). It comes with the editor; an engine built from its sources gets it by " + made_by;
}

}  // namespace redgem
