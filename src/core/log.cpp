#include "core/log.hpp"

#include <mutex>
#include <utility>

#include "core/platform.hpp"
#include "core/event_journal.hpp"

namespace redgem::log {
namespace {
thread_local Sink captured_sink;

EventJournal& journal() {
  static EventJournal events;
  return events;
}

// Where messages go until someone takes them (set_sink): wherever this system shows a program's
// messages, which the platform layer knows.
void default_sink(Level level, const std::string& message) { platform::write_log(level, message); }

Sink& current_sink() {
  static Sink sink = default_sink;
  return sink;
}

// Messages come from several threads at once (a dedicated server's rooms, web requests): one at a
// time reaches the sink, and the sink is not replaced while one does.
std::mutex& sink_lock() {
  static std::mutex lock;
  return lock;
}

}  // namespace

const char* level_name(Level level) {
  switch (level) {
    case Level::Debug:
      return "debug";
    case Level::Info:
      return "info";
    case Level::Warning:
      return "warning";
    case Level::Error:
      return "error";
  }
  return "unknown";
}

Capture::Capture(Sink sink) : previous_(std::exchange(captured_sink, std::move(sink))) {}
Capture::~Capture() { captured_sink = std::move(previous_); }

void write(Level level, const std::string& message) {
  if (captured_sink) return captured_sink(level, message);
  const std::lock_guard<std::mutex> hold(sink_lock());
  journal().append(level, message);
  current_sink()(level, message);
}
void debug(const std::string& message) { write(Level::Debug, message); }
void info(const std::string& message) { write(Level::Info, message); }
void warning(const std::string& message) { write(Level::Warning, message); }
void error(const std::string& message) { write(Level::Error, message); }

Sink set_sink(Sink sink) {
  const std::lock_guard<std::mutex> hold(sink_lock());
  return std::exchange(current_sink(), std::move(sink));
}

std::uint64_t event_cursor() { return journal().cursor(); }

EventPage events_since(std::uint64_t cursor, std::size_t limit, Level minimum) {
  return journal().since(cursor, limit, minimum);
}

void reset_sink() { set_sink(default_sink); }

}  // namespace redgem::log
