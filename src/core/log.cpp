#include "core/log.hpp"

#include <mutex>
#include <utility>

#include "core/platform.hpp"

namespace redgem::log {
namespace {

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

void write(Level level, const std::string& message) {
  const std::lock_guard<std::mutex> hold(sink_lock());
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

void reset_sink() { set_sink(default_sink); }

}  // namespace redgem::log
