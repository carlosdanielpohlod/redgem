#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "server/key_locks.hpp"
#include "server/sqlite_statement.hpp"
#include "storage/save_store.hpp"

namespace redgem::server {
class DatabaseSlot;

// What a dedicated server's game saves (Save), in a SQLite database in its data folder: one row
// per key, each change written on its own as it is made, in a transaction of its own (a crash
// keeps every change made before it, and never half of one). Nothing is held in memory but what
// a script asks for: a save grows with its players, not the server's memory. A server's rooms use
// it from their own threads: one call at a time, each whole. Writing a key (set, remove, update)
// also holds that key's own lock (see KeyLocks): update reads the key, makes its new value and
// writes it with no other room's write of that key in between, while rooms changing other keys go
// on (the change itself runs under the key's lock only, not the database's).
class SaveDatabase final : public SaveStore {
 public:
  // Opens the database at `file`, made (with its folder) if there is none yet; with an empty path,
  // one in memory, for this run only. nullptr with `error` when it cannot be opened.
  static std::unique_ptr<SaveDatabase> open(const std::filesystem::path& file, std::string& error);
  ~SaveDatabase() override;

  std::optional<nlohmann::json> get(const std::string& key) override;
  bool set(const std::string& key, const nlohmann::json& value) override;
  bool remove(const std::string& key) override;
  bool clear() override;
  std::vector<std::string> keys() override;
  bool update(const std::string& key, const Change& change) override;
  std::unique_ptr<SaveStore> open_slot(const std::string& name) override;
  std::vector<std::string> slot_names() override;
  std::string where() const override { return file_.string(); }

 private:
  friend class DatabaseSlot;
  SaveDatabase(sqlite3* database, std::filesystem::path file);
  // Why a statement could not be prepared; empty when every one was. A database whose table is
  // not the server's (made by something else) cannot be read or written, and must not open as if
  // it could.
  std::string unprepared() const;

  sqlite3* database_;
  std::filesystem::path file_;
  // Prepared once, when the database opens (and finished before it closes: see the destructor).
  SqliteStatement read_;
  SqliteStatement write_;
  SqliteStatement remove_;
  SqliteStatement keys_;
  // One statement at a time, whichever room's thread it comes from: never held while a change
  // runs, nor while waiting for a key. (Recursive: a slot's calls nest in the database's.)
  std::recursive_mutex lock_;
  // The keys being written, of the save and of its slots. Taken before lock_, never after.
  KeyLocks key_locks_;
};

}  // namespace redgem::server
