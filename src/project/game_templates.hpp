#pragma once

#include <filesystem>
#include <string>
#include <vector>

// New games, made from the templates built into the program. (The scene of a template is put
// together with the editor's own scene document: this is a tool, not part of what plays a game.)
namespace redgem {

// Starting points for new games: "empty", "platformer".
const std::vector<std::string>& game_templates();
// One line about what a template starts with.
std::string template_description(const std::string& template_name);
// A name usable as a game folder: letters, digits, spaces, '-' and '_' (not starting with a space).
bool valid_game_name(const std::string& name);

// Creates a playable game folder (scene.glb, scripts/, sounds/, scenes/, models/, game.json).
// Fails if the folder exists and is not empty.
bool create_game(const std::filesystem::path& game_dir, const std::string& template_name, std::string& error);

}  // namespace redgem
