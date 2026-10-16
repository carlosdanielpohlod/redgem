#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

// The words of a command line, as every program of the engine reads them: options start with a
// dash, and what an option is given is the word after it.
namespace redgem::command_line {

inline constexpr const char* kHelpOption = "--help";
inline constexpr const char* kShortHelpOption = "-h";
inline constexpr int kLargestPort = 65535;

// Whether a word asks for the program's usage.
bool asks_for_help(const std::string& word);
// Whether a word is an option, and not a file, a folder or what an option is given.
bool is_option(const std::string& word);
// What the option just read was given: the word at `next`, which is then stepped over. Nothing,
// with `next` left where it is, when the line ends there or the word is itself an option.
std::optional<std::string> option_value(const std::vector<std::string>& words, std::size_t& next);

// The number a word says, when it is one from `least` to `most` written with digits only (and
// few enough of them for any number a line takes).
std::optional<int> number_between(const std::string& word, int least, int most);
// What an option says when it is not given a number it takes.
std::string number_refusal(const char* option, int least, int most);
// What a word that is none of a program's says.
std::string unexpected(const std::string& word);

// An option of a line that is given a whole number, from `least` to `most`, and what it sets in
// what the program reads the line into.
template <typename Settings>
struct NumberOption {
  const char* name;
  int least;
  int most;
  void (*set)(Settings&, int);
};

// Reads the option at `next`, one of `options`, and its number into `settings`, stepping over
// both; what is wrong when the word is none of them, or is not given a number it takes.
template <typename Settings, std::size_t kCount>
std::string read_number_option(const NumberOption<Settings> (&options)[kCount], const std::vector<std::string>& words,
                               std::size_t& next, Settings& settings) {
  const std::string& word = words[next];
  const auto* option = std::find_if(std::begin(options), std::end(options),
                                    [&word](const NumberOption<Settings>& known) { return word == known.name; });
  if (option == std::end(options)) return unexpected(word);
  ++next;
  const auto number = number_between(option_value(words, next).value_or(""), option->least, option->most);
  if (!number) return number_refusal(option->name, option->least, option->most);
  option->set(settings, *number);
  return {};
}

}  // namespace redgem::command_line
