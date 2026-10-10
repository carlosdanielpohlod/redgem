#include "editor/console_log.hpp"

#include <algorithm>
#include <utility>

namespace redgem {
namespace {

constexpr std::size_t kMaxEntries = 2000;

}  // namespace

ConsoleLog::ConsoleLog() {
  previous_ = log::set_sink([this](log::Level level, const std::string& message) {
    entries_.push_back({level, message});
    if (entries_.size() > kMaxEntries) entries_.pop_front();
    if (previous_) previous_(level, message);
  });
}

ConsoleLog::~ConsoleLog() { log::set_sink(std::move(previous_)); }

std::size_t ConsoleLog::error_count() const {
  return static_cast<std::size_t>(std::count_if(entries_.begin(), entries_.end(), [](const ConsoleEntry& entry) {
    return entry.level == log::Level::Error;
  }));
}

}  // namespace redgem
