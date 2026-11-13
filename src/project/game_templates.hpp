#pragma once

#include <filesystem>
#include <string>
#include <vector>

// New games, made from the templates built into the program. A template is a game folder of the
// repository (templates/<name>/, with a template.json): a new game is a copy of it, given its own
// name and id. Every new game also gets the files of templates/_shared/ (a CI workflow that runs
// its tests, .github/workflows/test.yml). Nothing here knows what a template holds.
namespace redgem {

// A starting point for new games.
struct GameTemplate {
  std::string name;         // its folder's name
  std::string description;  // one line about what it starts with (its template.json)
  std::string icon;         // the name of the icon it is shown with (its template.json)
};

// What a new game starts from unless another template is asked for.
inline constexpr const char* kDefaultTemplate = "empty";
// The studio a new game says it is made by, unless its template says another: what its developer
// puts their own name in place of before the first release (it names the folder its players'
// saves are kept in, see app/game_data_location.hpp).
inline constexpr const char* kNewGameStudio = "My Studio";

// Every template, by name.
const std::vector<GameTemplate>& game_templates();
// The names alone, as a command line lists them: "empty|menu-only|platformer|...".
std::string template_names(const std::string& separator);
// A name usable as a game folder: letters, digits, spaces, '-' and '_' (not starting with a space).
bool valid_game_name(const std::string& name);
// An id for a new game of that name: the name in small letters and dashes, and a few digits of
// its own, so that two games given the same name are still two games ("my-game-3fa2c1b0").
std::string new_game_id(const std::string& name);

// Creates a playable game folder: the template's files and the shared ones, the folders every game has (scripts/,
// sounds/, scenes/, models/, images/, fonts/), a game.json with the game's title, studio and id, and the
// guide to the game for whoever works on it (AGENTS.md and CLAUDE.md, see assistant_guide.hpp).
// Fails if the folder exists and is not empty.
bool create_game(const std::filesystem::path& game_dir, const std::string& template_name, std::string& error);

}  // namespace redgem
