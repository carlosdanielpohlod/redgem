#pragma once

// The scenes of a game, as the editor opens, makes and saves them. A scene is known by the name a
// script loads it with (Game.load_scene): "main" is the game's scene.glb, the one it starts with;
// "level2" is scenes/level2.glb.

#include <filesystem>
#include <optional>
#include <string>

#include "editor/document/scene_document.hpp"

namespace redgem {

// Whether a scene made in the editor can be called `name`: lowercase letters, digits, '_' and '-',
// starting with a letter, and not "main" (the game's scene.glb).
bool valid_scene_name(const std::string& name);
// The name a scene file of the game is known by: "main" for its scene.glb, "level2" for
// scenes/level2.glb.
std::string scene_name_of(const std::filesystem::path& game_dir, const std::filesystem::path& file);
// Where a new scene called `name` goes (scenes/<name>.glb); nothing, and why in `error`, when no
// scene can be called that or the game already has one called that.
std::optional<std::filesystem::path> new_scene_file(const std::filesystem::path& game_dir, const std::string& name,
                                                    std::string& error);
// What a new scene starts with, to be saved to `file`: the ground, a sun and a camera looking at
// them, like the scene of a new game.
SceneDocument starting_scene(const std::filesystem::path& file);
// Makes scenes/<name>.glb with what a new scene starts with. Returns its file; nothing, and why in
// `error`, when it cannot be made.
std::optional<std::filesystem::path> create_scene(const std::filesystem::path& game_dir, const std::string& name,
                                                  std::string& error);

}  // namespace redgem
