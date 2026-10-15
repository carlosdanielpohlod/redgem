#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace redgem {

// What a game keeps between runs (Save in Ruby): named values (numbers, texts, true/false, and
// arrays and hashes of them), in one JSON file per game. Read from disk on first use, written back
// on every change, whole and at once: a crash never leaves half a file. Whole numbers come back
// as whole numbers, however they were written (a count of 3 is 3, not 3.0).
class SaveStore {
 public:
  // Kept in memory only, for as long as the store lives: nothing is read or written (a test's).
  SaveStore() = default;
  explicit SaveStore(std::filesystem::path file) : file_(std::move(file)) {}

  std::optional<nlohmann::json> get(const std::string& key);
  // False when the file could not be written (the value is still kept for this run). A null
  // value is none: the key is removed.
  bool set(const std::string& key, const nlohmann::json& value);
  bool remove(const std::string& key);
  bool clear();
  std::vector<std::string> keys();
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
