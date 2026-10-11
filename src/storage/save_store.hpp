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

}  // namespace redgem
