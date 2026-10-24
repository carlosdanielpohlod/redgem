#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "app/game_identity.hpp"
#include "project/game_build.hpp"

// What a build gives a game besides its player and its files, so each system shows it as the
// game it is: its name, its icon and its version. (See game_build.hpp for the build itself.)
namespace redgem {

// What a build knows of the game it ships, from its game.json.
struct BuildIdentity {
  std::string title;
  std::string id;  // the game's id; empty for none
  GameIdentity identity;
  std::filesystem::path icon;  // the icon's file; empty for a game with none
};
// What game.json says, checked: nullopt, with `error` saying why, for a version, a picture or a
// network flag that cannot be used, or an icon that is not a file inside the game. (An id that
// cannot be used is no id, as when the game plays: see GameManifest.)
std::optional<BuildIdentity> build_identity(const std::filesystem::path& game_dir, std::string& error);

// The version systems are given for a game that has none.
inline constexpr const char* kDefaultGameVersion = "1.0";

// Linux: a menu entry (<executable>.desktop) beside the executable, and the icon as
// <executable>.png. The entry names the program and its icon the way a package installs them
// (on the PATH, in the icon theme): AppImage, Flatpak and distribution packages take it as it is.
std::string desktop_entry(const std::string& title, const std::string& executable, bool has_icon);
std::string desktop_entry_file(const std::string& executable);
std::string desktop_icon_file(const std::string& executable);

// macOS: <name>.app, a folder the Mac shows as one program, with the game inside it.
//   <name>.app/Contents/Info.plist            what the app is (name, id, version, icon)
//   <name>.app/Contents/MacOS/<executable>    the player
//   <name>.app/Contents/Resources/game/       the game's files (see app/bundled_game.hpp)
//   <name>.app/Contents/Resources/<executable>.icns, licenses/
struct MacApp {
  std::string title;
  std::string executable;
  std::string identifier;  // the bundle's id (com.mystudio.mygame)
  std::string version;
  bool icon = false;
};
std::string mac_info_plist(const MacApp& app);
// The bundle id of a game: its id with '_' made '-' (what a bundle id may hold) when it has one,
// else "com.redgem." and its executable's name.
std::string mac_bundle_id(const std::string& game_id, const std::string& executable);
// The app's folder: the game's title, without what a file name cannot hold, then ".app"; the
// executable's name when nothing of the title is left.
std::string mac_app_folder(const std::string& title, const std::string& executable);
bool build_mac_app(const std::filesystem::path& game_dir, const std::filesystem::path& out_dir,
                   const std::filesystem::path& player, const BuildIdentity& identity, std::string& error);
// Signs a built app with no identity (ad hoc), as every program on an Apple Silicon Mac must be,
// with what it holds sealed into the signature. Only on a Mac (codesign is Apple's).
bool sign_mac_app(const std::filesystem::path& app, std::string& error);

// The steps every build for a computer takes (game_build.cpp): the player copied as the game's
// executable, allowed to run; a fresh, empty folder (an earlier build's is emptied first); a fresh
// copy of the files of the game that ship (see shipped_files) in such a folder.
bool copy_player(const std::filesystem::path& player, const std::filesystem::path& binary, std::string& error);
bool fresh_bundle(const std::filesystem::path& bundle, std::string& error);
bool copy_game(const std::filesystem::path& game_dir, const std::filesystem::path& bundle, std::string& error);

// Windows: the game's icon inside its executable (see icons/exe_icon.hpp).
bool give_exe_icon(const std::filesystem::path& exe, const std::filesystem::path& icon, std::string& error);

// What a build for a computer writes beside its executable for `target`: Linux's entry and icon.
std::vector<std::string> shipping_outputs(const std::filesystem::path& game_dir, BuildTarget target);
bool write_shipping_files(const std::filesystem::path& game_dir, const std::filesystem::path& out_dir,
                          BuildTarget target, const BuildIdentity& identity, std::string& error);

}  // namespace redgem
