#pragma once

#include <filesystem>
#include <string>

namespace redgem {

// Where a game's save file lives, in the user's RedGem folder.
// A game with an id (game.json) keeps it in saves/by-id/<id>.json: the save follows the game
// wherever its folder is moved and whatever it is renamed to.
// A game with none keeps it in saves/<game folder's name>-<digits>.json. The digits come from
// where the folder is, so that two games in folders of the same name keep their own saves; they
// stay the same for as long as the game stays where it is.
// (A save is only ever looked for in the one place: none is taken over from another.)
std::filesystem::path save_file_for(const std::filesystem::path& game_dir);
// A game's own file in `folder`, named as its save is named in the saves folder (by-id/<id>.json,
// or <folder name>-<digits>.json): what else the engine keeps per game follows the same rules.
std::filesystem::path game_file_in(const std::filesystem::path& folder, const std::filesystem::path& game_dir);
// What a game's settings are kept by in the user's settings (its display settings): its id, or,
// for a game with none, the name its save file is given after "folder:" ("folder:<name>-<digits>").
std::string game_settings_key(const std::filesystem::path& game_dir);
// The same, for a game whose settings were read already.
struct GameSettings;
std::string game_settings_key(const std::filesystem::path& game_dir, const GameSettings& settings);
// The folder every game's save is under.
std::filesystem::path saves_folder();

}  // namespace redgem
