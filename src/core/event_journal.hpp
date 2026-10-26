#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "core/log.hpp"

namespace redgem {

inline constexpr std::size_t kEventHistoryCapacity = 512;
inline constexpr std::size_t kMaxEventPage = log::kDefaultEventLimit;
inline constexpr std::size_t kMaxEventTextBytes = 2048;

struct JournalEvent {
  std::uint64_t id = 0;
  log::Level level = log::Level::Info;
  std::string text;
};

struct EventPage {
  std::vector<JournalEvent> events;
  std::uint64_t cursor = 0;   // pass this as `since` next time, even if nothing matched the filter
  std::uint64_t dropped = 0;  // events overwritten since the requested cursor
  bool more = false;
  bool cursor_ahead = false;  // a cursor of another process, or one that was never issued
};

// A bounded, thread-safe history. IDs increase even when the oldest events are overwritten.
class EventJournal {
 public:
  void append(log::Level level, const std::string& text);
  std::uint64_t cursor() const;
  EventPage since(std::uint64_t cursor, std::size_t limit = kMaxEventPage,
                  log::Level minimum = log::Level::Debug) const;

 private:
  mutable std::mutex mutex_;
  std::deque<JournalEvent> events_;
  std::uint64_t last_id_ = 0;
};

}  // namespace redgem
