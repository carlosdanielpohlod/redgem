#include <doctest/doctest.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "core/event_journal.hpp"
#include "core/short_text.hpp"

using namespace redgem;
using namespace std::string_literals;

TEST_CASE("an event cursor returns only later messages, in order") {
  EventJournal journal;
  CHECK(journal.cursor() == 0);
  journal.append(log::Level::Info, "first");
  const auto first = journal.cursor();
  journal.append(log::Level::Warning, "second");
  const EventPage page = journal.since(first);
  REQUIRE(page.events.size() == 1);
  CHECK(page.events.front().text == "second");
  CHECK(page.events.front().level == log::Level::Warning);
  CHECK(page.events.front().id == first + 1);
  CHECK(page.cursor == journal.cursor());
  CHECK_FALSE(page.more);
  CHECK(journal.since(page.cursor).events.empty());
}

TEST_CASE("pages stop at the limit without losing the next event") {
  constexpr int kMessages = 5;
  constexpr std::size_t kPageSize = 2;
  EventJournal journal;
  for (int i = 0; i < kMessages; ++i) journal.append(log::Level::Info, std::to_string(i));
  const EventPage first = journal.since(0, kPageSize);
  REQUIRE(first.events.size() == kPageSize);
  CHECK(first.more);
  const EventPage second = journal.since(first.cursor, kPageSize);
  REQUIRE(second.events.size() == kPageSize);
  CHECK(second.events.front().id == first.cursor + 1);
  CHECK(second.more);
  const EventPage last = journal.since(second.cursor, kPageSize);
  REQUIRE(last.events.size() == 1);
  CHECK(last.events.front().text == "4");
  CHECK_FALSE(last.more);
}

TEST_CASE("a level filter advances over skipped events without skipping a waiting match") {
  EventJournal journal;
  journal.append(log::Level::Debug, "skip");
  journal.append(log::Level::Error, "first error");
  journal.append(log::Level::Info, "skip too");
  journal.append(log::Level::Error, "next error");
  const EventPage first = journal.since(0, 1, log::Level::Error);
  REQUIRE(first.events.size() == 1);
  CHECK(first.events.front().text == "first error");
  CHECK(first.more);
  const EventPage next = journal.since(first.cursor, 1, log::Level::Error);
  REQUIRE(next.events.size() == 1);
  CHECK(next.events.front().text == "next error");
  CHECK_FALSE(next.more);
  journal.append(log::Level::Info, "only a skipped message");
  const EventPage empty = journal.since(next.cursor, 1, log::Level::Error);
  CHECK(empty.events.empty());
  CHECK(empty.cursor == journal.cursor());
  CHECK_FALSE(empty.more);
}

TEST_CASE("the history reports overwritten events and keeps increasing ids") {
  constexpr std::size_t kOverwritten = 3;
  EventJournal journal;
  for (std::size_t i = 0; i < kEventHistoryCapacity + kOverwritten; ++i) journal.append(log::Level::Info, "event");
  const EventPage page = journal.since(0);
  CHECK(page.dropped == kOverwritten);
  REQUIRE_FALSE(page.events.empty());
  CHECK(page.events.front().id == kOverwritten + 1);
  CHECK(page.events.size() == kMaxEventPage);
  CHECK(page.more);
  const EventPage caught_up = journal.since(journal.cursor());
  CHECK(caught_up.events.empty());
  CHECK(caught_up.dropped == 0);
}

TEST_CASE("a future cursor is reported instead of silently returning nothing forever") {
  EventJournal journal;
  journal.append(log::Level::Info, "one");
  const EventPage page = journal.since(journal.cursor() + 1);
  CHECK(page.cursor_ahead);
  CHECK(page.events.empty());
  CHECK(page.cursor == journal.cursor());
}

TEST_CASE("event text is bounded, keeps whole UTF-8 characters and removes terminal controls") {
  EventJournal journal;
  const std::string prefix(kMaxEventTextBytes - 1, 'x');
  journal.append(log::Level::Info, prefix + "水");
  const EventPage page = journal.since(0);
  REQUIRE(page.events.size() == 1);
  CHECK(page.events.front().text == prefix + "...");
  journal.append(log::Level::Error, "line\n\x1b[31mred\x00tail"s);
  const auto sanitized = journal.since(page.cursor);
  REQUIRE(sanitized.events.size() == 1);
  CHECK(sanitized.events.front().text.find('\x1b') == std::string::npos);
  CHECK(sanitized.events.front().text.find('\0') == std::string::npos);
  CHECK(sanitized.events.front().text.find('\n') != std::string::npos);
}

TEST_CASE("UTF-8 prefixes stop before cut two, three and four byte characters") {
  const std::string text = "Aé水😀Z";
  CHECK(utf8_prefix(text, 0).empty());
  CHECK(utf8_prefix(text, 2) == "A");
  CHECK(utf8_prefix(text, 3) == "Aé");
  CHECK(utf8_prefix(text, 5) == "Aé");
  CHECK(utf8_prefix(text, 6) == "Aé水");
  CHECK(utf8_prefix(text, 9) == "Aé水");
  CHECK(utf8_prefix(text, 10) == "Aé水😀");
  CHECK(utf8_prefix(text, text.size() + 1) == text);
}

TEST_CASE("threads append complete events under one increasing cursor") {
  constexpr int kThreads = 4;
  constexpr int kPerThread = 50;
  EventJournal journal;
  std::vector<std::thread> writers;
  for (int i = 0; i < kThreads; ++i) {
    writers.emplace_back([&journal] {
      for (int message = 0; message < kPerThread; ++message) journal.append(log::Level::Info, "whole message");
    });
  }
  for (auto& writer : writers) writer.join();
  std::uint64_t cursor = 0;
  while (cursor < journal.cursor()) {
    const EventPage page = journal.since(cursor);
    for (const JournalEvent& event : page.events) {
      CHECK(event.id == ++cursor);
      CHECK(event.text == "whole message");
    }
  }
  CHECK(cursor == kThreads * kPerThread);
}

TEST_CASE("the process journal receives messages even when a custom sink captures them") {
  const auto before = log::event_cursor();
  std::vector<std::string> captured;
  auto previous = log::set_sink([&captured](log::Level, const std::string& text) { captured.push_back(text); });
  log::warning("journal integration marker");
  log::set_sink(std::move(previous));
  REQUIRE(captured.size() == 1);
  const EventPage page = log::events_since(before);
  CHECK(std::any_of(page.events.begin(), page.events.end(), [](const JournalEvent& event) {
    return event.level == log::Level::Warning && event.text == "journal integration marker";
  }));
}

TEST_CASE("readers and writers account for every event while the ring wraps") {
  constexpr int kMessages = 2000;
  constexpr std::size_t kPageSize = 17;
  EventJournal journal;
  std::atomic<bool> done{false};
  std::thread writer([&] {
    for (int i = 0; i < kMessages; ++i) journal.append(log::Level::Info, "concurrent event");
    done.store(true);
  });
  std::uint64_t cursor = 0;
  std::uint64_t counted = 0;
  while (!done.load() || cursor < journal.cursor()) {
    const EventPage page = journal.since(cursor, kPageSize);
    CHECK_FALSE(page.cursor_ahead);
    counted += page.dropped + page.events.size();
    CHECK(page.cursor >= cursor);
    cursor = page.cursor;
    std::this_thread::yield();
  }
  writer.join();
  CHECK(cursor == kMessages);
  CHECK(counted == kMessages);
}

TEST_CASE("page size is bounded and a zero limit still makes progress") {
  EventJournal journal;
  for (std::size_t i = 0; i < kEventHistoryCapacity; ++i) journal.append(log::Level::Debug, "message");
  const EventPage small = journal.since(0, 0);
  CHECK(small.events.size() == 1);
  CHECK(small.cursor > 0);
  const EventPage large = journal.since(0, kEventHistoryCapacity);
  CHECK(large.events.size() == kMaxEventPage);
  CHECK(large.more);
}
