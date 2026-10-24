#include "project/script_messages.hpp"

#include <algorithm>
#include <utility>

#include "core/message_lines.hpp"
#include "project/error_hints.hpp"

namespace redgem::tools {

MessageCollector::MessageCollector(std::filesystem::path game_dir) : game_dir_(std::move(game_dir)) {
  previous_ = log::set_sink([this](log::Level level, const std::string& message) { keep(level, message); });
}

MessageCollector::~MessageCollector() { log::set_sink(std::move(previous_)); }

std::string MessageCollector::with_hint(const std::string& text) const {
  const std::string hint = hint_for(text, game_dir_);
  return hint.empty() ? text : text + " -> " + hint;
}

void MessageCollector::keep(log::Level level, const std::string& message) {
  if (level < log::Level::Warning) return;
  (level == log::Level::Error ? errors_ : warnings_) += 1;
  const std::string text = with_hint(compact_message(message, game_dir_));
  const auto same = std::find_if(kept_.begin(), kept_.end(),
                                 [&](const Kept& kept) { return kept.level == level && kept.text == text; });
  if (same != kept_.end()) {
    ++same->times;
    return;
  }
  kept_.push_back({level, text, 1});
}

std::string MessageCollector::lines() const {
  std::string text;
  for (const Kept& kept : kept_) {
    const std::string times = kept.times > 1 ? " (x" + std::to_string(kept.times) + ")" : "";
    text += std::string(log::level_name(kept.level)) + ": " + kept.text + times + "\n";
  }
  return text;
}

}  // namespace redgem::tools
