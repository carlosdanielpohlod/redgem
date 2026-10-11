#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Making a standalone game out of a game folder: the engine and the game's files, for the system
// asked for.
namespace redgem {

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

// The files of a game that go into a build of it, as paths inside its folder, sorted. Every build
// (a folder for a computer, an app for a phone) ships these and nothing else: regular files only,
// never what a link points at, nor hidden files and folders (.git, .env), editors' backups
// (name~) or signing keys.
std::vector<std::filesystem::path> shipped_files(const std::filesystem::path& game_dir);

// Whether a file is a key for signing apps (.keystore, .jks, .p12). A build never copies one:
// whoever has it can pass their app off as the developer's.
bool signing_key_file(const std::filesystem::path& file);

// The name of a game's executable: its folder name in lowercase, with anything that is not a
// letter or a digit turned into '_' ("My Game" -> "my_game"). On Windows the file adds ".exe".
std::string executable_name(const std::filesystem::path& game_dir);

}  // namespace redgem
