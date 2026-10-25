#pragma once

// What `redgem check` looks at in a game's folder, without playing it: its scripts and its scenes.

#include <cstddef>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace redgem::tools {

// What the checks found, a line each, and how much they looked at.
struct CheckReport {
  std::vector<std::string> errors;
  std::vector<std::string> warnings;
  std::size_t scripts = 0;
  std::size_t scenes = 0;
  std::set<std::string> broken_scripts;  // those that do not compile (their class is not looked for)
};

// The scripts of a scripts folder (and the folders in it), as paths inside it ("enemies/slime.rb"),
// sorted.
std::vector<std::string> script_files(const std::filesystem::path& scripts_dir);

// Every script of the game's scripts folder (and the folders in it) compiles.
void check_scripts(const std::filesystem::path& game_dir, CheckReport& report);

// Every scene of the game reads; every script its objects name is there and defines the class it
// is named for (as the engine finds out: it runs them, as playing does, in its sandbox); the sky
// picture it names is in the images folder.
void check_scenes(const std::filesystem::path& game_dir, CheckReport& report);

}  // namespace redgem::tools
