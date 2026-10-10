#pragma once

#include <cstddef>
#include <deque>
#include <string>

#include "core/log.hpp"

namespace redgem {

struct ConsoleEntry {
  log::Level level = log::Level::Info;
  std::string message;
};

// Captures engine and Ruby log output for the editor's Console panel, keeping the previous
// sink (the terminal) working too. Restores the previous sink when destroyed.
class ConsoleLog {
 public:
  ConsoleLog();
  ~ConsoleLog();
  ConsoleLog(const ConsoleLog&) = delete;
  ConsoleLog& operator=(const ConsoleLog&) = delete;

  const std::deque<ConsoleEntry>& entries() const { return entries_; }
  void clear() { entries_.clear(); }
  std::size_t error_count() const;

 private:
  log::Sink previous_;
  std::deque<ConsoleEntry> entries_;
};

}  // namespace redgem
