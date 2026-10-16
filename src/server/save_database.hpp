#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "storage/save_store.hpp"

struct sqlite3;

namespace redgem::server {

// What a dedicated server's game saves (Save), in a SQLite database in its data folder: one row
// per key, each change written on its own as it is made, in a transaction of its own (a crash
// keeps every change made before it, and never half of one). Nothing is held in memory but what
// a script asks for: a save grows with its players, not the server's memory. A server's rooms use
// it from their own threads: one call at a time, each whole (the store's lock is all they share).
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
  std::string where() const override { return file_.string(); }
  // Puts all of `values` in at once, in one transaction: all of them or, failing that, none.
  bool set_all(const std::map<std::string, nlohmann::json>& values);

 private:
  SaveDatabase(sqlite3* database, std::filesystem::path file) : database_(database), file_(std::move(file)) {}
  // Runs SQL that takes no values; true when it worked.
  bool execute(const char* sql);
  bool write(const std::string& key, const nlohmann::json& value);

  sqlite3* database_;
  std::filesystem::path file_;
  std::mutex lock_;  // one call at a time, whichever room's thread it comes from
};

}  // namespace redgem::server
