#pragma once

// `redgem scene`: a game's scenes read and changed from the command line, as short text, through
// the same document the editor edits and saves them with (so a change made here is saved as the
// editor would save it).
//
//   redgem scene list   <game> [--scene name] [--depth N] [--limit N]
//   redgem scene diff   <game> --against <before.glb> [--scene name] [--limit N]
//   redgem scene get    <game> <object> [--scene name]
//   redgem scene set    <game> <object> key=value... [--scene name]
//   redgem scene add    <game> <name> [--from <object>] [--parent <object>] [key=value...] [--scene name]
//   redgem scene remove <game> <object> [--scene name]
//
// <object> is an id (#12), a path (Level/Wall) or a name that one object alone has.

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "project/tool_line.hpp"

namespace redgem::tools {

// The word that starts the command.
inline constexpr const char* kSceneCommand = "scene";

// Runs `redgem scene ...`; `args` are the words after "scene".
ToolResult run_scene_command(const std::vector<std::string>& args);

// The names of a game's scenes: "main" (its scene.glb), then those of scenes/, sorted.
std::vector<std::string> scene_names(const std::filesystem::path& game_dir);

// The file of the scene a command line names (`--scene`, "main" when none is named). Nothing,
// with `error` listing the scenes there are, when the game has no scene of that name.
std::optional<std::filesystem::path> named_scene_file(const std::filesystem::path& game_dir, const std::string& name,
                                                      std::string& error);

}  // namespace redgem::tools
