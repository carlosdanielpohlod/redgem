#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace redgem {

// Files dragged from the desktop onto the editor become the game's own: a model goes to its
// models/ folder, a sound to sounds/. Anything else is not for a game.
enum class DroppedKind { Model, Sound };

std::optional<DroppedKind> kind_of_dropped(const std::filesystem::path& file);

// What became of the files of one drop.
struct DropReport {
  std::vector<std::string> models;   // file names now in models/
  std::vector<std::string> sounds;   // file names now in sounds/
  std::vector<std::string> refused;  // one line each: the file and why it was left out
};

// Copies each file into the game folder it belongs in. A file the game already has under that
// name is left as it is, and said so.
DropReport add_dropped_files(const std::filesystem::path& game_dir, const std::vector<std::filesystem::path>& files);

}  // namespace redgem
