#include "editor/console_log.hpp"

#include <algorithm>
#include <charconv>
#include <utility>

namespace redgem {
namespace {

constexpr std::size_t kMaxEntries = 2000;
constexpr std::string_view kBacktracePrefix = "    from ";
constexpr std::string_view kRubyLocation = ".rb:";
constexpr std::size_t kMaxLocationBytes = 4096;

std::size_t positive_line(std::string_view digits) {
  std::size_t line = 0;
  const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), line);
  return parsed.ec == std::errc{} && parsed.ptr == digits.data() + digits.size() ? line : 0;
}

}  // namespace

std::optional<ConsoleScriptLine> console_script_line(std::string_view row) {
  if (row.substr(0, kBacktracePrefix.size()) != kBacktracePrefix) return std::nullopt;
  row.remove_prefix(kBacktracePrefix.size());
  const auto extension = row.substr(0, kMaxLocationBytes).find(kRubyLocation);
  if (extension == std::string_view::npos) return std::nullopt;
  const auto number = row.substr(extension + kRubyLocation.size(), kMaxLocationBytes);
  const auto line = positive_line(number.substr(0, number.find(':')));
  if (line == 0) return std::nullopt;
  return ConsoleScriptLine{std::string(row.substr(0, extension + kRubyLocation.size() - 1)), line};
}

ConsoleLog::ConsoleLog() {
  previous_ = log::set_sink([this](log::Level level, const std::string& message) {
    entries_.push_back({level, message});
    if (entries_.size() > kMaxEntries) entries_.pop_front();
    if (previous_) previous_(level, message);
  });
}

ConsoleLog::~ConsoleLog() { log::set_sink(std::move(previous_)); }

std::size_t ConsoleLog::error_count() const {
  return static_cast<std::size_t>(std::count_if(
      entries_.begin(), entries_.end(), [](const ConsoleEntry& entry) { return entry.level == log::Level::Error; }));
}

}  // namespace redgem
