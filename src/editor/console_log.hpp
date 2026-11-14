#pragma once

#include <cstddef>
#include <deque>
#include <optional>
#include <string_view>
#include <vector>
#include <string>

#include "core/log.hpp"

namespace redgem {

struct ConsoleScriptLine {
  std::string file;
  std::size_t line = 0;  // one-based
};

// The source location at the start of a Ruby backtrace row.
std::optional<ConsoleScriptLine> console_script_line(std::string_view row);

struct ConsoleRow {
  std::size_t offset = 0;
  std::size_t length = 0;
  std::optional<ConsoleScriptLine> source;
  mutable std::optional<bool> source_exists{};  // checked when first shown, outside the log sink
};

struct ConsoleEntry {
  log::Level level = log::Level::Info;
  std::string message;
  std::vector<ConsoleRow> rows;
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
