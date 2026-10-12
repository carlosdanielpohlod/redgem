#pragma once

#include <filesystem>
#include <string>
#include <vector>

// New games, made from the templates built into the program. A template is a game folder of the
// repository (templates/<name>/): a new game is a copy of it, given its own name and id. Nothing
// here knows what a template holds.
namespace redgem {

// A starting point for new games.
struct GameTemplate {
  std::string name;         // its folder's name
  std::string description;  // one line about what it starts with (its template.json)
  std::string icon;         // the name of the icon it is shown with (its template.json)
};

// What a new game starts from unless another template is asked for.
inline constexpr const char* kDefaultTemplate = "empty";

// Every template, by name.
const std::vector<GameTemplate>& game_templates();
// The names alone, as a command line lists them: "empty|platformer".
std::string template_names(const std::string& separator);
// A name usable as a game folder: letters, digits, spaces, '-' and '_' (not starting with a space).
bool valid_game_name(const std::string& name);
// An id for a new game of that name: the name in small letters and dashes, and a few digits of
// its own, so that two games given the same name are still two games ("my-game-3fa2c1b0").
std::string new_game_id(const std::string& name);

// Creates a playable game folder: the template's files, the folders every game has (scripts/,
// sounds/, scenes/, models/, images/) and a game.json with the game's title and id.
// Fails if the folder exists and is not empty.
bool create_game(const std::filesystem::path& game_dir, const std::string& template_name, std::string& error);

}  // namespace redgem
