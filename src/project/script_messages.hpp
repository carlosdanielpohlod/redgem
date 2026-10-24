#pragma once

// What a game run by a tool (`redgem run`, `shot`) had to say, kept short: its warnings and
// errors, one line each, starting with the place of the game's own files they point at
// ("scripts/coin.rb:12: ..."), each said once with how many times it came, and with what to do
// about it when the error is a common one.

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "core/log.hpp"

namespace redgem::tools {

// Keeps the warnings and errors logged while it lives (and lets the rest go): the other messages
// of a run are the engine talking to itself.
class MessageCollector {
 public:
  explicit MessageCollector(std::filesystem::path game_dir);
  ~MessageCollector();
  MessageCollector(const MessageCollector&) = delete;
  MessageCollector& operator=(const MessageCollector&) = delete;

  // Every message, a line each ("error: ..." / "warning: ..."), the same one once with "(xN)".
  std::string lines() const;
  std::size_t errors() const { return errors_; }
  std::size_t warnings() const { return warnings_; }

 private:
  struct Kept {
    log::Level level = log::Level::Error;
    std::string text;
    std::size_t times = 1;
  };
  void keep(log::Level level, const std::string& message);
  // The message with what to do about it, when there is something to say (see error_hints.hpp).
  std::string with_hint(const std::string& text) const;

  std::filesystem::path game_dir_;
  log::Sink previous_;
  std::vector<Kept> kept_;
  std::size_t errors_ = 0;
  std::size_t warnings_ = 0;
};

}  // namespace redgem::tools
