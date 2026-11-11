#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "render/display_settings.hpp"

namespace redgem {

// Where a player's display settings are kept between runs: the game's own settings file (see
// app/game_data_location.hpp).
struct DisplayKeeping {
  std::filesystem::path file;
};

// How a player shows a game, and where that is kept.
struct PlayerDisplay {
  DisplaySettings display;
  DisplayKeeping kept_in;
};

// What a game's player chose, kept in `settings_file`, over what its game.json says: game.json and
// the file read once each.
PlayerDisplay player_display(const std::filesystem::path& game_dir, const std::filesystem::path& settings_file);
// Keeps what the player chose for the game's next runs.
bool keep_display(const DisplayKeeping& kept_in, const DisplaySettings& display, std::string& error);

// Only a game its player plays keeps what the player chooses: the editor's Play, a server's
// rooms, a developer's run and every test show a game as its game.json says and keep nothing
// (their engines are given no DisplayKeeping). Whether a run keeps it.
inline bool keeps_display(const std::optional<DisplayKeeping>& kept_in) { return kept_in.has_value(); }

}  // namespace redgem
