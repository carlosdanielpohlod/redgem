#include "server/key_locks.hpp"

#include "storage/save_store.hpp"

namespace redgem::server {

namespace {

constexpr const char* kBusyStart = "Save key '";
constexpr const char* kBusyEnd =
    "' is being changed by another room for too long to be written from inside a Save.update block";

// How many key locks the calling thread holds, of every save.
thread_local int keys_held = 0;

}  // namespace

KeyLocks::Hold::~Hold() {
  if (owner_ != nullptr) owner_->release(entry_);
}

KeyLocks::Hold KeyLocks::hold(const std::string& slot, const std::string& key) {
  std::unique_lock<std::mutex> table(table_lock_);
  const Entries::iterator entry = entries_.try_emplace({slot, key}).first;
  ++entry->second.users;
  if (acquired(table, entry->second)) return Hold(this, entry);
  table.unlock();
  leave(entry);
  throw SaveKeyBusy(kBusyStart + key + kBusyEnd);
}

// Waiting for as long as it takes when the thread holds no other key, else at most kNestedKeyWait.
bool KeyLocks::acquired(std::unique_lock<std::mutex>& table, Entry& entry) {
  const std::thread::id me = std::this_thread::get_id();
  const auto free = [&entry, me] { return entry.depth == 0 || entry.owner == me; };
  if (keys_held == 0) {
    released_.wait(table, free);
  } else if (!released_.wait_for(table, kNestedKeyWait, free)) {
    return false;
  }
  entry.owner = me;
  ++entry.depth;
  ++keys_held;
  return true;
}

void KeyLocks::release(Entries::iterator entry) {
  {
    const std::lock_guard<std::mutex> table(table_lock_);
    --entry->second.depth;
    --keys_held;
  }
  released_.notify_all();
  leave(entry);
}

std::size_t KeyLocks::size() {
  const std::lock_guard<std::mutex> hold(table_lock_);
  return entries_.size();
}

// (Map entries stay where they are while others come and go: the iterator stays good.)
void KeyLocks::leave(Entries::iterator entry) {
  const std::lock_guard<std::mutex> hold(table_lock_);
  if (--entry->second.users == 0) entries_.erase(entry);
}

}  // namespace redgem::server
