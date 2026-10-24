#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "input/input_map.hpp"
#include "input/player_bindings.hpp"

// A game's actions and axes as its game.json declares them, and the bindings its players chose,
// kept between runs in the user's RedGem folder.
namespace redgem {

// What the "input" section of a game's game.json declares (nothing when it has none). What it
// gets wrong is left out and said in `problems`.
InputMap read_game_input(const std::filesystem::path& game_dir, std::vector<std::string>& problems);

// Where the players of a game keep their bindings: in bindings/ of the user's RedGem folder,
// named as the game's save is (by-id/<game id>.json, or after the game's folder for a game with
// no id), so that they follow the game as its save does.
std::filesystem::path bindings_file_for(const std::filesystem::path& game_dir);

// Gives `bindings` the players' changes a file keeps. A missing or broken file keeps none: every
// player starts with the game's bindings.
void read_player_bindings(const std::filesystem::path& file, PlayerBindings& bindings);
// Writes every player's changes, whole: the file holds what it held when the writing fails.
bool write_player_bindings(const std::filesystem::path& file, const PlayerBindings& bindings);
// The same in two steps: what the file would hold, and writing it (whoever keeps the file can
// leave a text it already wrote alone).
std::string player_bindings_text(const PlayerBindings& bindings);
bool write_player_bindings_text(const std::filesystem::path& file, const std::string& text);

}  // namespace redgem
