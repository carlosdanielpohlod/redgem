#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Making a standalone game out of a game folder: the player (the program that plays a game and
// nothing else, redgem-player) and the game's files, for the system asked for. A build never
// copies the program that makes it: the editor and the other tools stay with whoever makes games.
namespace redgem {

// The systems a game can be built for. (A game for the Mac is built on a Mac, with the player that
// comes with the editor there.)
enum class BuildTarget { Linux, Windows, Android, MacOS };
// "linux" / "windows" / "android" / "macos" (and back); nullopt for anything else.
std::optional<BuildTarget> build_target_from_name(const std::string& name);
const char* build_target_name(BuildTarget target);
// The system this program runs on.
BuildTarget host_build_target();
// Where the player a build for `target` ships is looked for, from the program that builds
// (`self`): beside it for the system it runs on (redgem-player), otherwise in runtimes/<target>/
// beside it (runtimes/windows/redgem-player.exe). The same in a build folder and in an
// installation, which keep their programs together. (Android is packed differently, see
// android_package.hpp: its players are libraries, one per kind of processor, in
// runtimes/android/<abi>/.)
std::filesystem::path runtime_location(BuildTarget target, const std::filesystem::path& self);
// That player, when it is there; nullopt when it is missing (and always for Android).
std::optional<std::filesystem::path> runtime_for(BuildTarget target, const std::filesystem::path& self);
// What to tell someone whose build found no player at `looked_for`: which file is missing, and
// how it is made from the engine's sources.
std::string missing_runtime_message(BuildTarget target, const std::filesystem::path& looked_for);

// Makes a standalone game in `out_dir`: an executable named after the game (a copy of `player`,
// with .exe for Windows) next to a folder with the game's files, and what the system shows the
// game by (see game_shipping.hpp): on Windows the game's icon inside the executable, on Linux a
// menu entry and the icon beside it. For the Mac it is an app instead, <title>.app, with all of
// that inside it. Started, that executable plays
// the game beside it. Refuses an `out_dir` that is the game folder or inside it (the copy would
// contain itself), and one whose game folder holds the game (that folder is emptied on each
// build). What it writes in `out_dir` (the executable, the game folder, the licenses) replaces
// only what an earlier build wrote there, as it recorded (build_record.hpp): a file or folder of
// one of those names that no build made is left alone, and the build refused.
bool build_game(const std::filesystem::path& game_dir, const std::filesystem::path& out_dir,
                const std::filesystem::path& player, BuildTarget target, std::string& error);
// What a build for Android makes: an APK, to install on a phone, or an App Bundle (.aab), which
// Google Play takes and makes the phones' APKs from.
enum class AndroidFormat { Apk, Bundle };
// Everything a build needs, for the editor and the command line: finds the player for `target`
// and builds. Returns the game's executable (for the Mac, its .app; for Android, its .apk or
// .aab), or nullopt with `error` set. An App Bundle is for Android only.
std::optional<std::filesystem::path> build_game_for(const std::filesystem::path& game_dir,
                                                    const std::filesystem::path& out_dir, BuildTarget target,
                                                    std::string& error, AndroidFormat format = AndroidFormat::Apk);

// What every build checks before it starts, each false with `error` saying why not: that what is
// built is a game, and that the build does not go into the game it copies. (An APK replaces no
// folder of the output's, so a game beside it is fine; see build_game for what else a build for
// a computer checks.)
bool is_game_to_build(const std::filesystem::path& game_dir, std::string& error);
bool builds_outside_game(const std::filesystem::path& game_dir, const std::filesystem::path& out_dir,
                         std::string& error);

// Whether `folder` is `outer` itself or somewhere inside it, by their real locations (a build
// must not go into the game it copies).
bool folder_inside(const std::filesystem::path& folder, const std::filesystem::path& outer);

// The files of a game that go into a build of it, as paths inside its folder, sorted. Every build
// (a folder for a computer, an app for a phone) ships these and nothing else: regular files only,
// never what a link points at, nor hidden files and folders (.git, .env), editors' backups
// (name~), signing keys, or the keystore the game's game.json signs its app with.
std::vector<std::filesystem::path> shipped_files(const std::filesystem::path& game_dir);

// Whether a file is a key for signing apps (.keystore, .jks, .p12, .pfx, .pem, .key, in any
// case). A build never copies one: whoever has it can pass their app off as the developer's.
bool signing_key_file(const std::filesystem::path& file);

// The name of a game's executable: its folder name in lowercase, with anything that is not a
// letter or a digit turned into '_' ("My Game" -> "my_game"). On Windows the file adds ".exe".
std::string executable_name(const std::filesystem::path& game_dir);

}  // namespace redgem
