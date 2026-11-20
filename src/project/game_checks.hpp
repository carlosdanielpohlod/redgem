#pragma once

// What `redgem check` looks at in a game's folder, without playing it: its game.json, its scripts,
// its scenes, its dialogues and its quests.

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

// What game.json's "physics" section says that the game cannot use: a body limit out of bounds
// (an error), and collision layers it leaves out (warnings).
void check_game_file(const std::filesystem::path& game_dir, CheckReport& report);

// The scripts of a scripts folder (and the folders in it), as paths inside it ("enemies/slime.rb"),
// sorted.
std::vector<std::string> script_files(const std::filesystem::path& scripts_dir);

// Every script of the game's scripts folder (and the folders in it) compiles.
void check_scripts(const std::filesystem::path& game_dir, CheckReport& report);

// Every scene of the game reads; every script its objects name is there and defines the class it
// is named for (as the engine finds out: it runs them, as playing does, in its sandbox); the sky
// picture it names is in the images folder.
void check_scenes(const std::filesystem::path& game_dir, CheckReport& report);

// Every dialogue of the game (dialogues/*.json, and the folders in it) reads and is well made
// (every node it names is there, every node leads somewhere, every condition and effect is one it
// can have); every I18n key it shows has a text in the game's locales (a warning for each language
// that lacks one); every quest it names is in quests/, when the game has that folder, with the
// stage it names.
void check_dialogues(const std::filesystem::path& game_dir, CheckReport& report);

// Every quest of the game (quests/*.json, and the folders in it) reads and is well made (every stage
// it names is there, every objective can be done, every key is one it can have); every I18n key it
// shows has a text; every dialogue it names is in dialogues/, with the node it names; every item it
// names is in items/, when the game has that folder.
void check_quests(const std::filesystem::path& game_dir, CheckReport& report);

// Every check above, in that order: what `redgem check` finds (its game's tests aside).
CheckReport check_game(const std::filesystem::path& game_dir);

}  // namespace redgem::tools
