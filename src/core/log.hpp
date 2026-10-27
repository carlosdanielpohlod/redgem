#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace redgem {
struct EventPage;
}

namespace redgem::log {

enum class Level { Debug, Info, Warning, Error };

using Sink = std::function<void(Level, const std::string&)>;

void write(Level level, const std::string& message);
void debug(const std::string& message);
void info(const std::string& message);
void warning(const std::string& message);
void error(const std::string& message);

// Replaces the output sink (tests capture messages). Returns the previous sink. Any thread may
// write; the sink is called by one at a time, so it must not write to the log itself.
Sink set_sink(Sink sink);
void reset_sink();

const char* level_name(Level level);

inline constexpr std::size_t kDefaultEventLimit = 128;

// Every message reaches the process history, independently of the installed sink.
// Include core/event_journal.hpp for the page's fields. Cursors last for this process only.
std::uint64_t event_cursor();
EventPage events_since(std::uint64_t cursor, std::size_t limit = kDefaultEventLimit, Level minimum = Level::Debug);

}  // namespace redgem::log
