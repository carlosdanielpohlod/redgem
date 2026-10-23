#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "storage/durable_file.hpp"
#include "storage/save_store.hpp"

namespace redgem {

// What a game keeps on a player's machine: one JSON file per game, read from disk on first use and
// written back on every change, whole and at once and flushed to the disk: a crash or a power cut
// never leaves half a file. The save it replaces is kept beside it as <name>.bak; a save found
// damaged is moved aside to <name>.damaged (never written over) and the backup loaded instead.
class SaveFile final : public SaveStore {
 public:
  // Kept in memory only, for as long as the store lives: nothing is read or written (a test's).
  SaveFile() = default;
  // `flush` pushes each save to the disk before it takes the old one's place (a test's own, to
  // see that it does).
  explicit SaveFile(std::filesystem::path file, durable::Flush flush = durable::flush_to_disk)
      : file_(std::move(file)), flush_(std::move(flush)) {}

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
  // The save before the last one written, what a damaged save falls back to.
  std::filesystem::path backup_file() const;

 private:
  void load();
  void keep(const nlohmann::json& contents);
  void recover_from_damage();
  bool write();
  bool holds_good_save() const;
  void back_up_previous() const;
  bool in_memory() const { return file_.empty(); }

  std::filesystem::path file_;
  durable::Flush flush_;
  std::map<std::string, nlohmann::json> values_;
  bool loaded_ = false;
  // False while the file holds a damaged save that could not be moved aside: it must not become
  // the backup.
  bool file_is_good_ = true;
};

}  // namespace redgem
