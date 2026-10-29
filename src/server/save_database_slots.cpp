#include "server/save_database.hpp"
#include <stdexcept>
#include "storage/save_slots.hpp"
#include "storage/saved_values.hpp"
namespace redgem::server {
namespace {
constexpr int kSlot = 1;
constexpr int kKey = 2;
constexpr int kValue = 3;
constexpr int kColumn = 0;
constexpr const char* kRead = "SELECT value FROM save_slots WHERE slot=?1 AND key=?2";
constexpr const char* kWrite = "INSERT OR REPLACE INTO save_slots(slot,key,value) VALUES(?1,?2,?3)";
constexpr const char* kRemove = "DELETE FROM save_slots WHERE slot=?1 AND key=?2";
constexpr const char* kClear = "DELETE FROM save_slots WHERE slot=?1";
constexpr const char* kKeys = "SELECT key FROM save_slots WHERE slot=?1 AND json_valid(value) ORDER BY key";
constexpr const char* kRemember = "INSERT OR IGNORE INTO save_slot_names(name) VALUES(?1)";
constexpr const char* kNames = "SELECT name FROM save_slot_names ORDER BY name LIMIT ?1";
}  // namespace
namespace {
struct SlotStatements {
  SqliteStatement read, write, remove, clear, keys;
  explicit SlotStatements(sqlite3* database)
      : read(database, kRead),
        write(database, kWrite),
        remove(database, kRemove),
        clear(database, kClear),
        keys(database, kKeys) {
    for (const auto* statement : {&read, &write, &remove, &clear, &keys})
      if (!statement->prepared()) throw std::runtime_error(statement->error());
  }
};
}  // namespace
// Each room owns this view; only the connection's recursive lock is shared. The server's root
// store outlives every engine/slot view, just as required for EngineOptions::save.
class DatabaseSlot final : public SaveStore {
 public:
  DatabaseSlot(SaveDatabase& owner, std::string name)
      : owner_(owner), name_(std::move(name)), statements_(std::make_unique<SlotStatements>(owner.database_)) {}
  ~DatabaseSlot() override {
    const std::lock_guard<std::recursive_mutex> hold(owner_.lock_);
    statements_.reset();  // finalizing statements also touches the shared SQLite connection
  }
  std::optional<nlohmann::json> get(const std::string& key) override {
    const std::lock_guard<std::recursive_mutex> hold(owner_.lock_);
    auto use = statements_->read.use();
    if (!use.bind(kSlot, name_).bind(kKey, key).next_row()) return std::nullopt;
    return saved::entry(nlohmann::json::parse(use.text(kColumn), nullptr, false));
  }
  bool set(const std::string& key, const nlohmann::json& value) override {
    if (value.is_null()) return remove(key);
    const auto text = saved::written(saved::kept(value).value_or(nlohmann::json()));
    const std::lock_guard<std::recursive_mutex> hold(owner_.lock_);
    return statements_->write.use().bind(kSlot, name_).bind(kKey, key).bind(kValue, text).run();
  }
  bool remove(const std::string& key) override {
    const std::lock_guard<std::recursive_mutex> hold(owner_.lock_);
    return statements_->remove.use().bind(kSlot, name_).bind(kKey, key).run();
  }
  bool clear() override {
    const std::lock_guard<std::recursive_mutex> hold(owner_.lock_);
    return statements_->clear.use().bind(kSlot, name_).run();
  }
  std::vector<std::string> keys() override {
    const std::lock_guard<std::recursive_mutex> hold(owner_.lock_);
    std::vector<std::string> keys;
    auto use = statements_->keys.use();
    use.bind(kSlot, name_);
    while (use.next_row()) keys.push_back(use.text(kColumn));
    return keys;
  }
  bool update(const std::string& key, const Change& change) override {
    const std::lock_guard<std::recursive_mutex> hold(owner_.lock_);
    return set(key, change(get(key)));
  }
  std::string where() const override { return owner_.where() + " (slot " + name_ + ")"; }

 private:
  SaveDatabase& owner_;
  std::string name_;
  std::unique_ptr<SlotStatements> statements_;
};
std::unique_ptr<SaveStore> SaveDatabase::open_slot(const std::string& name) {
  require_named_save_slot(name);
  const std::lock_guard<std::recursive_mutex> hold(lock_);
  SqliteStatement remember(database_, kRemember);
  if (!remember.prepared()) throw std::runtime_error(remember.error());
  if (!remember.use().bind(kSlot, name).run()) throw std::runtime_error("cannot create save slot");
  return std::make_unique<DatabaseSlot>(*this, name);
}
std::vector<std::string> SaveDatabase::slot_names() {
  const std::lock_guard<std::recursive_mutex> hold(lock_);
  SqliteStatement listing(database_, kNames);
  if (!listing.prepared()) throw std::runtime_error(listing.error());
  const auto limit = std::to_string(kMaxListedSaveSlots + 1);
  auto use = listing.use();
  use.bind(kSlot, limit);
  std::vector<std::string> names;
  while (use.next_row()) names.push_back(use.text(kColumn));
  if (names.size() > kMaxListedSaveSlots) throw std::runtime_error("too many save slots to list");
  return names;
}
}  // namespace redgem::server
