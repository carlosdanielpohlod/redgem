#pragma once

#include <filesystem>
#include <string>

#include "render/display_settings.hpp"

namespace redgem {

// The user's settings file a run keeps its players' choices in: `given`, or the user's own
// (user_settings_file) when it is empty.
std::filesystem::path player_settings_file(const std::filesystem::path& given);

// How a game is shown: what its game.json says, with what its player chose saved over it in the
// settings file.
DisplaySettings game_display(const std::filesystem::path& game_dir, const std::filesystem::path& settings_file);
// Keeps what the player chose for the game in the settings file, for its next runs.
bool save_game_display(const std::filesystem::path& game_dir, const std::filesystem::path& settings_file,
                       const DisplaySettings& display, std::string& error);

}  // namespace redgem
