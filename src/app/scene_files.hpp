#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <variant>

#include "scene/scene_desc.hpp"

namespace redgem {

// Whether a folder is a game: it holds the scene a game starts with (scene.glb).
bool is_game_folder(const std::filesystem::path& folder);

// A game's scene.glb, read (what the loader has to say about it is logged); the error when it
// cannot be read.
std::variant<SceneDesc, std::string> read_game_scene(const std::filesystem::path& game_dir);

// Where a game keeps the scene a script calls `name` ("main" is scene.glb).
std::filesystem::path scene_file(const std::filesystem::path& game_dir, const std::string& name);

// Whether a script's scene name stays inside the game's folders (a warning says so when not).
bool safe_scene_name(const std::string& name);

// The scene a script asked for by name; nothing, with the reason logged, when it cannot be had.
std::optional<SceneDesc> read_scene(const std::filesystem::path& game_dir, const std::string& name);

}  // namespace redgem
