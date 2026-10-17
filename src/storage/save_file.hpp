#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "storage/save_store.hpp"

namespace redgem {

// What a game keeps on a player's machine: one JSON file per game, read from disk on first use and
// written back on every change, whole and at once: a crash never leaves half a file.
class SaveFile final : public SaveStore {
 public:
  // Kept in memory only, for as long as the store lives: nothing is read or written (a test's).
  SaveFile() = default;
  explicit SaveFile(std::filesystem::path file) : file_(std::move(file)) {}

  std::optional<nlohmann::json> get(const std::string& key) override;
  // False when the file could not be written (the value is still kept for this run).
  bool set(const std::string& key, const nlohmann::json& value) override;
  bool remove(const std::string& key) override;
  bool clear() override;
  std::vector<std::string> keys() override;
  // (A player's machine runs one game: nothing else reads or writes in between.)
  bool update(const std::string& key, const Change& change) override { return set(key, change(get(key))); }
  std::string where() const override { return file_.string(); }
  const std::filesystem::path& file() const { return file_; }

 private:
  void load();
  bool write() const;
  bool in_memory() const { return file_.empty(); }

  std::filesystem::path file_;
  std::map<std::string, nlohmann::json> values_;
  bool loaded_ = false;
};

}  // namespace redgem
