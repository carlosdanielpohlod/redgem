#include "core/command_line.hpp"

#include "core/translations.hpp"

namespace redgem::command_line {

namespace {

constexpr char kOptionMark = '-';       // what every option starts with
constexpr std::size_t kMostDigits = 5;  // of any number a line takes
constexpr const char* kDigits = "0123456789";

bool is_number(const std::string& word) {
  return !word.empty() && word.size() <= kMostDigits && word.find_first_not_of(kDigits) == std::string::npos;
}

bool within(int number, int least, int most) { return number >= least && number <= most; }

}  // namespace

bool asks_for_help(const std::string& word) { return word == kHelpOption || word == kShortHelpOption; }

bool is_option(const std::string& word) { return !word.empty() && word.front() == kOptionMark; }

std::optional<std::string> option_value(const std::vector<std::string>& words, std::size_t& next) {
  if (next >= words.size()) return std::nullopt;
  if (is_option(words[next])) return std::nullopt;
  return words[next++];
}

std::optional<int> number_between(const std::string& word, int least, int most) {
  if (!is_number(word)) return std::nullopt;
  const int number = std::stoi(word);
  if (!within(number, least, most)) return std::nullopt;
  return number;
}

std::string number_refusal(const char* option, int least, int most) {
  return tr("core.cli.needs_number",
            {{"option", option}, {"least", std::to_string(least)}, {"most", std::to_string(most)}});
}

std::string unexpected(const std::string& word) { return tr("core.cli.unexpected_argument", {{"argument", word}}); }

}  // namespace redgem::command_line
