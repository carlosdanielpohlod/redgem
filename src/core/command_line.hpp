#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

// The words of a command line, as every program of the engine reads them: options start with a
// dash, and what an option is given is the word after it.
namespace redgem::command_line {

inline constexpr const char* kHelpOption = "--help";
inline constexpr const char* kShortHelpOption = "-h";

// Whether a word asks for the program's usage.
bool asks_for_help(const std::string& word);
// Whether a word is an option, and not a file, a folder or what an option is given.
bool is_option(const std::string& word);
// What the option just read was given: the word at `next`, which is then stepped over. Nothing,
// with `next` left where it is, when the line ends there or the word is itself an option.
std::optional<std::string> option_value(const std::vector<std::string>& words, std::size_t& next);

}  // namespace redgem::command_line
