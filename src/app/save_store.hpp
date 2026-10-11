#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "scene/property.hpp"

namespace redgem {

// What a game keeps between runs (Save in Ruby): named numbers, texts and true/false values, in
// one JSON file per game. Read from disk on first use, written back on every change.
class SaveStore {
 public:
  explicit SaveStore(std::filesystem::path file) : file_(std::move(file)) {}

  std::optional<PropertyValue> get(const std::string& key);
  // False when the file could not be written (the value is still kept for this run).
  bool set(const std::string& key, const PropertyValue& value);
  bool remove(const std::string& key);
  bool clear();
  std::vector<std::string> keys();
  const std::filesystem::path& file() const { return file_; }

 private:
  void load();
  bool write() const;

  std::filesystem::path file_;
  std::map<std::string, PropertyValue> values_;
  bool loaded_ = false;
};

// Where a game's save file lives: saves/<game folder's name>-<digits>.json in the user's RedGem
// folder. The digits come from where the folder is, so that two games in folders of the same
// name keep their own saves; they stay the same for as long as the game stays where it is.
std::filesystem::path save_file_for(const std::filesystem::path& game_dir);
// Where a system test's saves go instead, so a test never reads or changes a player's progress:
// test-saves/ in the same folder of the user's own.
std::filesystem::path test_save_file_for(const std::filesystem::path& game_dir);

}  // namespace redgem
