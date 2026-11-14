#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include <thread>
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
// kNestedKeyWait for another, then is refused with SaveKeyBusy: a cycle of rooms waiting for each
// other can only form among such nested waits, so it always breaks. (The wait is bounded whether or
// not there is a cycle: a nested write of a key another room holds for longer is refused too.)
class KeyLocks {
  using Name = std::pair<std::string, std::string>;  // the slot (empty for the save itself), the key
  // (Kept by hand under table_lock_, not in a timed mutex of its own: ThreadSanitizer does not
  // follow every way a timed mutex is locked, and would see unlocks of mutexes never locked.)
  struct Entry {
    std::thread::id owner;  // the thread holding it, while depth > 0
    int depth = 0;          // how many times its owner holds it
    int users = 0;          // threads holding or waiting for it
  };
  using Entries = std::map<Name, Entry>;

 public:
  // A key held until it is destroyed (on the thread that took it).
  class Hold {
   public:
    Hold(KeyLocks* owner, Entries::iterator entry) : owner_(owner), entry_(entry) {}
    Hold(Hold&& other) noexcept : owner_(std::exchange(other.owner_, nullptr)), entry_(other.entry_) {}
    Hold(const Hold&) = delete;
    Hold& operator=(const Hold&) = delete;
    Hold& operator=(Hold&&) = delete;
    ~Hold();

   private:
    KeyLocks* owner_ = nullptr;
    Entries::iterator entry_;
  };

  KeyLocks() = default;
  KeyLocks(const KeyLocks&) = delete;
  KeyLocks& operator=(const KeyLocks&) = delete;

  // Throws SaveKeyBusy when a nested wait ran out.
  Hold hold(const std::string& slot, const std::string& key);
  // How many keys have a lock now (for the tests: none once nobody holds one).
  std::size_t size();

 private:
  // Takes the entry for this thread, waiting as KeyLocks says; false when a nested wait ran out.
  bool acquired(std::unique_lock<std::mutex>& table, Entry& entry);
  // Gives the entry back, and forgets it once nobody holds or waits for it.
  void release(Entries::iterator entry);
  void leave(Entries::iterator entry);

  std::mutex table_lock_;             // guards entries_ and everything in them; let go of while waiting
  std::condition_variable released_;  // some key was given back
  Entries entries_;
};

}  // namespace redgem::server
