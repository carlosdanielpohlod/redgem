#pragma once

#include <string>
#include <utility>
#include <vector>

#include "core/log.hpp"

namespace redgem::test {

inline constexpr float kTolerance = 1e-4f;

inline std::string fixture(const std::string& relative) { return std::string(REDGEM_FIXTURES_DIR) + "/" + relative; }

// Captures log output for the lifetime of the object.
class LogCapture {
 public:
  LogCapture() {
    previous_ = log::set_sink([this](log::Level level, const std::string& message) {
      messages_.emplace_back(level, message);
    });
  }
  ~LogCapture() { log::set_sink(std::move(previous_)); }
  LogCapture(const LogCapture&) = delete;
  LogCapture& operator=(const LogCapture&) = delete;

  bool contains(const std::string& text) const {
    for (const auto& [level, message] : messages_) {
      if (message.find(text) != std::string::npos) return true;
    }
    return false;
  }
  int count(log::Level wanted) const {
    int total = 0;
    for (const auto& [level, message] : messages_) total += level == wanted ? 1 : 0;
    return total;
  }

 private:
  log::Sink previous_;
  std::vector<std::pair<log::Level, std::string>> messages_;
};

}  // namespace redgem::test
