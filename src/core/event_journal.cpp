#include "core/event_journal.hpp"

#include <algorithm>

#include "core/safe_path.hpp"
#include "core/short_text.hpp"

namespace redgem {
namespace {

std::string short_event(const std::string& text) {
  const std::string_view prefix = utf8_prefix(text, kMaxEventTextBytes);
  return printable_lines(prefix) + (prefix.size() < text.size() ? "..." : "");
}

EventPage page_after(std::uint64_t cursor, std::uint64_t latest) {
  EventPage page;
  page.cursor = std::min(cursor, latest);
  page.cursor_ahead = cursor > latest;
  return page;
}

// A matching event with no room waits for the next page; skipped levels still advance the cursor.
bool include_event(EventPage& page, const JournalEvent& event, std::size_t limit, log::Level minimum) {
  if (event.level >= minimum) {
    if (page.events.size() == limit) return false;
    page.events.push_back(event);
  }
  page.cursor = event.id;
  return true;
}

void fill_page(EventPage& page, const std::deque<JournalEvent>& events, std::size_t limit, log::Level minimum) {
  const std::uint64_t since = page.cursor;
  for (const JournalEvent& event : events) {
    if (event.id <= since) continue;
    if (!include_event(page, event, limit, minimum)) {
      page.more = true;
      break;
    }
  }
}

}  // namespace

void EventJournal::append(log::Level level, const std::string& text) {
  const std::lock_guard<std::mutex> lock(mutex_);
  events_.push_back({++last_id_, level, short_event(text)});
  if (events_.size() > kEventHistoryCapacity) events_.pop_front();
}

std::uint64_t EventJournal::cursor() const {
  const std::lock_guard<std::mutex> lock(mutex_);
  return last_id_;
}

EventPage EventJournal::since(std::uint64_t cursor, std::size_t limit, log::Level minimum) const {
  const std::lock_guard<std::mutex> lock(mutex_);
  EventPage page = page_after(cursor, last_id_);
  if (page.cursor_ahead || events_.empty()) return page;
  limit = std::clamp(limit, std::size_t{1}, kMaxEventPage);
  if (cursor < events_.front().id - 1) page.dropped = events_.front().id - 1 - cursor;
  fill_page(page, events_, limit, minimum);
  return page;
}

}  // namespace redgem
