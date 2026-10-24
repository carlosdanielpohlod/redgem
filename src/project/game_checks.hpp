#pragma once

// What `redgem check` looks at in a game's folder, without playing it: its scripts and its scenes.

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace redgem::tools {

// What the checks found, a line each, and how much they looked at.
struct CheckReport {
  std::vector<std::string> errors;
  std::vector<std::string> warnings;
  std::size_t scripts = 0;
  std::size_t scenes = 0;
};

// Every script of the game's scripts folder (and the folders in it) compiles.
void check_scripts(const std::filesystem::path& game_dir, CheckReport& report);

// Every scene of the game reads; every script its objects name is there and defines the class it
// is named for; the sky picture it names is in the images folder.
void check_scenes(const std::filesystem::path& game_dir, CheckReport& report);

// Whether a script defines a class of that name (a `class Name` line).
bool defines_class(const std::string& code, const std::string& class_name);

}  // namespace redgem::tools
