#pragma once

#include <chrono>
#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include <utility>

namespace redgem::server {

// How long a thread that already holds a key's lock waits for another key's before it gives up.
// Only such a nested wait is bounded: two rooms each holding one key and waiting for the other's
// would otherwise wait for ever.
inline constexpr std::chrono::milliseconds kNestedKeyWait{250};

// One lock per key of a save, so that changes to different keys (each room's own player, say) run
// side by side, and changes to the same key one after another. A key's lock exists while some
// thread holds or waits for it, and is gone after. The same thread may take a key it holds again.
// A thread holding no key waits for as long as it takes; one already holding a key waits at most
// kNestedKeyWait for another, then is refused: a cycle of rooms waiting for each other can only
// form among such nested waits, so it always breaks.
class KeyLocks {
  using Name = std::pair<std::string, std::string>;  // the slot (empty for the save itself), the key
  struct Entry {
    std::recursive_timed_mutex lock;
    int users = 0;  // threads holding or waiting for it
  };
  using Entries = std::map<Name, Entry>;

 public:
  // A key held until it is destroyed; false when the lock was refused (a nested wait ran out).
  class Hold {
   public:
    Hold() = default;
    Hold(KeyLocks* owner, Entries::iterator entry) : owner_(owner), entry_(entry) {}
    Hold(Hold&& other) noexcept : owner_(std::exchange(other.owner_, nullptr)), entry_(other.entry_) {}
    Hold(const Hold&) = delete;
    Hold& operator=(const Hold&) = delete;
    Hold& operator=(Hold&&) = delete;
    ~Hold();
    explicit operator bool() const { return owner_ != nullptr; }

   private:
    KeyLocks* owner_ = nullptr;
    Entries::iterator entry_;
  };

  KeyLocks() = default;
  KeyLocks(const KeyLocks&) = delete;
  KeyLocks& operator=(const KeyLocks&) = delete;

  Hold hold(const std::string& slot, const std::string& key);
  // How many keys have a lock now (for the tests: none once nobody holds one).
  std::size_t size();

 private:
  Entries::iterator enter(const Name& name);
  void leave(Entries::iterator entry);

  std::mutex table_lock_;  // guards entries_ and the users counts, never held while waiting for a key
  Entries entries_;
};

}  // namespace redgem::server
