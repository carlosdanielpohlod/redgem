#include "project/tool_line.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "core/command_line.hpp"
#include "core/translations.hpp"

namespace redgem::tools {
namespace {

bool listed(const std::vector<std::string>& names, const std::string& name) {
  return std::find(names.begin(), names.end(), name) != names.end();
}

// Reads the option at `next` (stepping over it and its value). False, with `error`, when it is
// none the grammar knows or is not given its value.
bool read_option(const std::vector<std::string>& args, std::size_t& next, const ToolGrammar& grammar, ToolLine& line,
                 std::string& error) {
  const std::string& option = args[next++];
  if (listed(grammar.flags, option)) {
    line.flags.insert(option);
    return true;
  }
  if (!listed(grammar.valued, option)) {
    error = tr("project.tools.unknown_option", {{"option", option}});
    return false;
  }
  const auto value = command_line::option_value(args, next);
  if (!value) error = tr("project.tools.option_needs_value", {{"option", option}});
  if (value) line.values[option] = *value;
  return value.has_value();
}

// A negative number is a value, not an option.
bool is_option_word(const std::string& word) { return command_line::is_option(word) && !number_from(word); }

}  // namespace

ToolResult failed(const std::string& message, int code) {
  ToolResult result;
  result.exit_code = code;
  result.err = message;
  return result;
}

std::optional<std::string> ToolLine::value(const std::string& option) const {
  const auto found = values.find(option);
  if (found == values.end()) return std::nullopt;
  return found->second;
}

std::optional<ToolLine> read_tool_line(const std::vector<std::string>& args, std::size_t first,
                                       const ToolGrammar& grammar, std::string& error) {
  ToolLine line;
  std::size_t next = first;
  while (next < args.size()) {
    if (!is_option_word(args[next])) {
      line.words.push_back(args[next++]);
    } else if (!read_option(args, next, grammar, line, error)) {
      return std::nullopt;
    }
  }
  return line;
}

std::optional<double> number_from(const std::string& text) {
  if (text.empty()) return std::nullopt;
  char* end = nullptr;
  const double number = std::strtod(text.c_str(), &end);
  const bool whole_text = end == text.c_str() + text.size();
  if (!whole_text || !std::isfinite(number)) return std::nullopt;
  return number;
}

}  // namespace redgem::tools
