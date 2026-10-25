#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "storage/durable_file.hpp"
#include "storage/save_store.hpp"

namespace redgem {

// How many damaged saves of one game are kept apart (<name>.damaged, then .damaged.2 and on). With
// all of those names taken, a damaged save is not moved, and nothing is written over it.
inline constexpr int kMostDamagedSavesKept = 100;

// Removes a save and its backup: a game finds nothing saved the next time it runs (a test's fresh
// start). What was set aside as damaged stays.
void remove_save(const std::filesystem::path& file);
// Where a save's backup is kept (the save before the last write), beside it.
std::filesystem::path save_backup_of(const std::filesystem::path& file);

// Moves a damaged file aside, to <name>.damaged (then .damaged.2 and on, as saves are): the name it
// was moved to, or nullopt when it could not be moved (it is left where it is).
std::optional<std::filesystem::path> move_damaged_aside(const std::filesystem::path& file);

struct SaveReading;  // what reading a save file found (see save_file.cpp)

// What a game keeps on a player's machine: one JSON file per game, read from disk on first use and
// written back on every change, whole and at once and flushed to the disk: a crash or a power cut
// never leaves half a file. The save it replaces is kept beside it as <name>.bak, written the same
// way. A save found damaged is moved aside to <name>.damaged (never written over), and the backup
// takes its place, on the disk too; a damaged backup is moved aside as well. A save that cannot be
// read, or a damaged one that cannot be moved aside, is never written over: the game runs without
// saving, and the log says why.
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
  void take(const SaveReading& reading);
  void recover(const std::string& trouble, bool damaged);
  void restore_from(const SaveReading& backup, const std::string& trouble, bool damaged);
  std::optional<std::filesystem::path> set_aside(const std::filesystem::path& damaged);
  void refuse_writes(const std::string& why);
  bool write();
  void back_up_previous() const;
  bool in_memory() const { return file_.empty(); }

  std::filesystem::path file_;
  durable::Flush flush_;
  std::map<std::string, nlohmann::json> values_;
  bool loaded_ = false;
  std::string on_disk_;  // what the file holds, as written: what the next write keeps as the backup
  // A save that cannot be read or moved aside is in the way: it is not written over.
  bool writes_refused_ = false;
};

}  // namespace redgem
