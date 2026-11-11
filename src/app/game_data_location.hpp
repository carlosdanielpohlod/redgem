#pragma once

#include <filesystem>

// Where a game keeps what lasts between its runs, all of it in one folder of the game's own:
//   save.json       what Save keeps (save.json.bak and save.json.slots/ beside it)
//   settings.json   what its player chose (Settings.save), and the card and language to use
//   bindings.json   what its players bound (Input.bind)
//   crashes/        the reports of a built game's crashes
//
// A player's folder is named after the game's studio and title (game.json's "studio" and
// "title"), where the system keeps what programs remember (see platform::programs_data_dir):
//   Linux    $XDG_DATA_HOME/<studio>/<title>, else ~/.local/share/<studio>/<title>
//   Windows  %APPDATA%\<studio>\<title>
//   macOS    ~/Library/Application Support/<studio>/<title>
//   Android  the app's own files folder (the app is the game: set_players_data_folder)
// Only the game its player plays uses it. The editor's Play and `redgem play` play in a
// developer's folder (developer_data_folder), `redgem run` and `shot` in a scratch folder that is
// removed when they end, and `redgem-test` in one of the tests'
// (testing/test_saves.hpp): a developer's runs never touch what a player of the game kept.
//
// The folder is the same for as long as the studio and the title are: renaming either starts the
// game in a new folder, and what the old one holds stays there.
namespace redgem {

struct GameSettings;

// The folders a game with no usable studio, or no usable title, is kept under.
inline constexpr const char* kUnnamedStudioFolder = "RedGem Games";
inline constexpr const char* kUnnamedGameFolder = "Game";

// The files a game keeps in its folder.
struct GameDataFiles {
  std::filesystem::path save;
  std::filesystem::path settings;
  std::filesystem::path bindings;
  std::filesystem::path crashes;  // a folder of reports
};
GameDataFiles game_data_files(const std::filesystem::path& folder);

// <studio>/<title>, each made a safe folder name (see core/folder_names.hpp): kUnnamedStudioFolder
// for a game that names no studio, kUnnamedGameFolder for a title with nothing usable in it.
std::filesystem::path game_data_subfolder(const GameSettings& settings);
// The same for the game in `game_dir`, its game.json read. A game whose game.json gives no title
// goes by the name of its folder, however the path to it is written: for a built game, the folder
// around its "game" folder (where the player put it, so that a title is what keeps it in place).
std::filesystem::path game_data_subfolder(const std::filesystem::path& game_dir);

// The folder of the game's players, on this system (see above).
std::filesystem::path players_data_folder(const std::filesystem::path& game_dir);
// Says where that folder is, for systems that hand each app a place of its own (an Android app):
// every game played is then kept there. An empty path goes back to the usual one.
void set_players_data_folder(const std::filesystem::path& folder);

// The folder a game keeps its data in when its developer plays it (the editor's Play, `redgem
// play`): game-data/<studio>/<title> in the user's RedGem folder, never the players'.
std::filesystem::path developer_data_folder(const std::filesystem::path& game_dir);

}  // namespace redgem
