#include "core/command_line.hpp"

namespace redgem::command_line {

namespace {

constexpr char kOptionMark = '-';  // what every option starts with

}  // namespace

bool asks_for_help(const std::string& word) { return word == kHelpOption || word == kShortHelpOption; }

bool is_option(const std::string& word) { return !word.empty() && word.front() == kOptionMark; }

std::optional<std::string> option_value(const std::vector<std::string>& words, std::size_t& next) {
  if (next >= words.size()) return std::nullopt;
  if (is_option(words[next])) return std::nullopt;
  return words[next++];
}

}  // namespace redgem::command_line
