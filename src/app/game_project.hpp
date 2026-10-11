#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace redgem {

// game.json: what a game says about itself.
struct GameSettings {
  std::string title;
  int window_width = 0;
  int window_height = 0;
};

// Defaults (folder name as title, engine window size) for anything missing.
GameSettings read_game_settings(const std::filesystem::path& game_dir);
bool write_game_settings(const std::filesystem::path& game_dir, const GameSettings& settings, std::string& error);

// Starting points for new games: "empty", "platformer".
const std::vector<std::string>& game_templates();
// One line about what a template starts with.
std::string template_description(const std::string& template_name);
// A name usable as a game folder: letters, digits, spaces, '-' and '_' (not starting with a space).
bool valid_game_name(const std::string& name);

// Creates a playable game folder (scene.glb, scripts/, sounds/, scenes/, models/, game.json).
// Fails if the folder exists and is not empty.
bool create_game(const std::filesystem::path& game_dir, const std::string& template_name, std::string& error);

// A folder path as the rest of the code expects it: no "." or "..", no trailing separator
// ("games/foo/" -> "games/foo", whose filename is the game's name).
std::filesystem::path normalized_folder(const std::filesystem::path& folder);

// The systems a game can be built for.
enum class BuildTarget { Linux, Windows, Android };
// "linux" / "windows" / "android" (and back); nullopt for anything else.
std::optional<BuildTarget> build_target_from_name(const std::string& name);
const char* build_target_name(BuildTarget target);
// The system this program runs on.
BuildTarget host_build_target();
// The engine a build for `target` ships: this program (`self`) for the system it runs on;
// otherwise the runtime built for that system, in runtimes/<target>/ next to this program
// (tools/windows/build_runtime.sh makes the Windows one). nullopt when that runtime is missing.
// (Android is packed differently, see android_package.hpp: this has no single runtime for it.)
std::optional<std::filesystem::path> runtime_for(BuildTarget target, const std::filesystem::path& self);

// Makes a standalone game in `out_dir`: an executable named after the game (a copy of
// `engine_binary`, with .exe for Windows) next to a folder with the game's files. Started with
// no arguments, that executable plays the game beside it. Refuses an `out_dir` that is the game
// folder or inside it (the copy would contain itself).
bool build_game(const std::filesystem::path& game_dir, const std::filesystem::path& out_dir,
                const std::filesystem::path& engine_binary, BuildTarget target, std::string& error);
// Everything a build needs, for the editor and the command line: finds the runtime for `target`
// and builds. Returns the game's executable (for Android, its .apk), or nullopt with `error` set.
std::optional<std::filesystem::path> build_game_for(const std::filesystem::path& game_dir,
                                                    const std::filesystem::path& out_dir, BuildTarget target,
                                                    std::string& error);

// Whether `folder` is `outer` itself or somewhere inside it, by their real locations (a build
// must not go into the game it copies).
bool folder_inside(const std::filesystem::path& folder, const std::filesystem::path& outer);

// Whether a file is a key for signing apps (.keystore, .jks, .p12). A build never copies one:
// whoever has it can pass their app off as the developer's.
bool signing_key_file(const std::filesystem::path& file);

// The name of a game's executable: its folder name in lowercase, with anything that is not a
// letter or a digit turned into '_' ("My Game" -> "my_game"). On Windows the file adds ".exe".
std::string executable_name(const std::filesystem::path& game_dir);
// The folder a built game's files are in, next to its executable.
inline constexpr const char* kBundledGameFolder = "game";
// The game a built executable ships with: the game folder next to it, when there is one.
std::optional<std::filesystem::path> bundled_game(const std::filesystem::path& executable);

// Games found in a folder (subfolders with a scene.glb), sorted by name.
std::vector<std::filesystem::path> find_games(const std::filesystem::path& folder);

// Where the project hub creates new games: ~/RedGemGames.
std::filesystem::path default_games_folder();
// Games opened recently, most recent first, in a small JSON file (~/.config/redgem/recent.json).
std::filesystem::path recent_games_file();
// Only games that still exist are returned.
std::vector<std::filesystem::path> read_recent_games(const std::filesystem::path& file);
void remember_game(const std::filesystem::path& file, const std::filesystem::path& game_dir);

}  // namespace redgem
