#include "server/save_database.hpp"

#include <sqlite3.h>

#include <fstream>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "storage/saved_values.hpp"

namespace redgem::server {

namespace {

constexpr const char* kInMemory = ":memory:";
// Written ahead of the database: a write never waits for the readers, nor a reader for a write.
constexpr const char* kSetUp =
    "PRAGMA journal_mode = WAL;"
    "CREATE TABLE IF NOT EXISTS saves (key TEXT PRIMARY KEY NOT NULL, value TEXT NOT NULL) WITHOUT ROWID;";
constexpr const char* kRead = "SELECT value FROM saves WHERE key = ?1";
constexpr const char* kWrite = "INSERT OR REPLACE INTO saves (key, value) VALUES (?1, ?2)";
constexpr const char* kRemove = "DELETE FROM saves WHERE key = ?1";
constexpr const char* kClear = "DELETE FROM saves";
// (Only rows that hold a value: one written by something else than the server is none.)
constexpr const char* kKeys = "SELECT key FROM saves WHERE json_valid(value) ORDER BY key";
// Where the statements above take their values (?1, ?2), and give them back.
constexpr int kKeyPlace = 1;
constexpr int kValuePlace = 2;
constexpr int kFirstColumn = 0;

bool executed(sqlite3* database, const char* sql) {
  return sqlite3_exec(database, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

// sqlite3 opens a missing file, not a missing folder.
void make_folder_for(const std::filesystem::path& file) {
  std::error_code ignored;  // (opening the database says what went wrong)
  if (!file.empty()) std::filesystem::create_directories(file.parent_path(), ignored);
}

// The files sqlite keeps beside a database in WAL mode: the log and its index.
constexpr const char* kSideFileSuffixes[] = {"-wal", "-shm"};

// The database, made empty and its owner's alone from its first moment (sqlite takes an empty
// file as a new database). On Windows a file takes its folder's ACL instead: see
// KNOWN_LIMITATIONS.md.
void make_owners_only(const std::filesystem::path& file) {
#ifdef _WIN32
  std::error_code ignored;
  if (!std::filesystem::exists(file, ignored)) std::ofstream(file, std::ios::binary);  // (never over a database)
#else
  const int made = ::open(file.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
  if (made >= 0) ::close(made);
#endif
}

// What `file` is let be read by: its owner only, whatever it was let before.
void restrict_to_owner(const std::filesystem::path& file) {
  std::error_code ignored;  // (opening the database says what went wrong)
  if (!std::filesystem::exists(file, ignored)) return;
  std::filesystem::permissions(file, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                               std::filesystem::perm_options::replace, ignored);
}

// Saves are players' data: the database is its owner's alone, and so are the files beside it
// that an earlier run left (sqlite makes new ones with the database's permissions).
void keep_to_owner(const std::filesystem::path& file) {
  if (file.empty()) return;
  make_owners_only(file);
  restrict_to_owner(file);
  for (const char* suffix : kSideFileSuffixes) restrict_to_owner(file.string() + suffix);
}

// Whether sqlite opened the database and set it up as the server keeps it.
bool set_up(int opened, sqlite3* database) { return opened == SQLITE_OK && executed(database, kSetUp); }

}  // namespace

std::unique_ptr<SaveDatabase> SaveDatabase::open(const std::filesystem::path& file, std::string& error) {
  make_folder_for(file);
  keep_to_owner(file);
  sqlite3* database = nullptr;
  const int opened = sqlite3_open(file.empty() ? kInMemory : file.u8string().c_str(), &database);
  if (!set_up(opened, database)) {
    error = sqlite3_errmsg(database);
    sqlite3_close(database);
    return nullptr;
  }
  std::unique_ptr<SaveDatabase> saves(new SaveDatabase(database, file));
  error = saves->unprepared();
  if (error.empty()) return saves;
  return nullptr;  // (the database closes with `saves`)
}

std::string SaveDatabase::unprepared() const {
  for (const SqliteStatement* statement : {&read_, &write_, &remove_, &keys_}) {
    if (!statement->prepared()) return statement->error();
  }
  return {};
}

SaveDatabase::SaveDatabase(sqlite3* database, std::filesystem::path file)
    : database_(database),
      file_(std::move(file)),
      read_(database, kRead),
      write_(database, kWrite),
      remove_(database, kRemove),
      keys_(database, kKeys) {}

// (Closed once its statements are finished too, which happens right after: they are members.)
SaveDatabase::~SaveDatabase() { sqlite3_close_v2(database_); }

std::optional<nlohmann::json> SaveDatabase::get(const std::string& key) {
  const std::lock_guard<std::recursive_mutex> hold(lock_);
  SqliteStatement::Use read = read_.use();
  if (!read.bind(kKeyPlace, key).next_row()) return std::nullopt;
  // (A row that is not a value, written by something else than the server, counts as none.)
  return saved::entry(nlohmann::json::parse(read.text(kFirstColumn), nullptr, false));
}

bool SaveDatabase::set(const std::string& key, const nlohmann::json& value) {
  if (value.is_null()) return remove(key);
  const std::string text = saved::written(saved::kept(value).value_or(nlohmann::json()));
  const std::lock_guard<std::recursive_mutex> hold(lock_);
  return write_.use().bind(kKeyPlace, key).bind(kValuePlace, text).run();
}

bool SaveDatabase::remove(const std::string& key) {
  const std::lock_guard<std::recursive_mutex> hold(lock_);
  return remove_.use().bind(kKeyPlace, key).run();
}

bool SaveDatabase::clear() {
  const std::lock_guard<std::recursive_mutex> hold(lock_);
  return executed(database_, kClear);
}

std::vector<std::string> SaveDatabase::keys() {
  const std::lock_guard<std::recursive_mutex> hold(lock_);
  std::vector<std::string> names;
  SqliteStatement::Use listing = keys_.use();
  while (listing.next_row()) names.push_back(listing.text(kFirstColumn));
  return names;
}

bool SaveDatabase::update(const std::string& key, const Change& change) {
  // (The change runs under the lock: no other room reads or writes meanwhile.)
  const std::lock_guard<std::recursive_mutex> hold(lock_);
  return set(key, change(get(key)));
}

}  // namespace redgem::server
