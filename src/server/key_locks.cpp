#include "server/key_locks.hpp"

#include "storage/save_store.hpp"

namespace redgem::server {

namespace {

constexpr const char* kBusyStart = "Save key '";
constexpr const char* kBusyEnd =
    "' is being changed by another room for too long to be written from inside a Save.update block";

// How many key locks the calling thread holds, of every save.
thread_local int keys_held = 0;

// Takes `lock`: waiting for as long as it takes when the thread holds no other key, else at most
// kNestedKeyWait (see KeyLocks).
bool acquired(std::recursive_timed_mutex& lock) {
  if (keys_held == 0) {
    lock.lock();
  } else if (!lock.try_lock_for(kNestedKeyWait)) {
    return false;
  }
  ++keys_held;
  return true;
}

}  // namespace

KeyLocks::Hold::~Hold() {
  if (owner_ == nullptr) return;
  entry_->second.lock.unlock();
  --keys_held;
  owner_->leave(entry_);
}

KeyLocks::Hold KeyLocks::hold(const std::string& slot, const std::string& key) {
  const Entries::iterator entry = enter({slot, key});
  if (acquired(entry->second.lock)) return Hold(this, entry);
  leave(entry);
  throw SaveKeyBusy(kBusyStart + key + kBusyEnd);
}

std::size_t KeyLocks::size() {
  const std::lock_guard<std::mutex> hold(table_lock_);
  return entries_.size();
}

// (Map entries stay where they are while others come and go: the iterator stays good.)
KeyLocks::Entries::iterator KeyLocks::enter(const Name& name) {
  const std::lock_guard<std::mutex> hold(table_lock_);
  const Entries::iterator entry = entries_.try_emplace(name).first;
  ++entry->second.users;
  return entry;
}

void KeyLocks::leave(Entries::iterator entry) {
  const std::lock_guard<std::mutex> hold(table_lock_);
  if (--entry->second.users == 0) entries_.erase(entry);
}

}  // namespace redgem::server
