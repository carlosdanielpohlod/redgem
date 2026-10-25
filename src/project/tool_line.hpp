#pragma once

// The developer tools of the command line that answer in text and end: `redgem scene`, `run`,
// `shot` and `check`. Each reads the words after its name the same way, and answers with what
// it says and how the program ends; nothing here prints, so every answer can be tested.

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "core/exit_codes.hpp"

namespace redgem::tools {

// What a tool says, and how the program ends.
struct ToolResult {
  int exit_code = kExitSuccess;
  std::string out;  // the answer
  std::string err;  // what went wrong, or what to look at
};

// A result that failed with `message` (and `code`).
ToolResult failed(const std::string& message, int code = kExitFailure);

// What a tool accepts after its name: the options that are given a value (`--scene main`), and
// the ones that are a word alone (`--headless`).
struct ToolGrammar {
  std::vector<std::string> valued;
  std::vector<std::string> flags;
};

// The words after a tool's name, read.
struct ToolLine {
  std::vector<std::string> words;             // what is no option, in order
  std::map<std::string, std::string> values;  // the options given a value
  std::set<std::string> flags;                // the options that are a word alone

  bool has(const std::string& flag) const { return flags.count(flag) > 0; }
  std::optional<std::string> value(const std::string& option) const;
};

// Reads `args` from `first` on. Nothing, with `error` saying why, for an option the grammar does
// not know or one that is not given its value.
std::optional<ToolLine> read_tool_line(const std::vector<std::string>& args, std::size_t first,
                                       const ToolGrammar& grammar, std::string& error);

// A number written whole: "12", "-3.5", "1e3". Nothing for anything else.
std::optional<double> number_from(const std::string& text);

}  // namespace redgem::tools
