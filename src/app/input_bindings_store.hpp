#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "input/input_map.hpp"
#include "input/player_bindings.hpp"

// A game's actions and axes as its game.json declares them, and the bindings its players chose,
// kept between runs in the game's own folder (bindings.json, see app/game_data_location.hpp).
namespace redgem {

// What the "input" section of a game's game.json declares (nothing when it has none). What it
// gets wrong is left out and said in `problems`.
InputMap read_game_input(const std::filesystem::path& game_dir, std::vector<std::string>& problems);

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
