#include "testing/opened_classes.hpp"

#include <algorithm>
#include <regex>
#include <sstream>

namespace redgem::testing {
namespace {

// A line that opens a class or a module, unindented, by a constant's name (not `class << self`).
const std::regex kOpening(R"(^(?:class|module)\s+([A-Z]\w*))");
constexpr std::size_t kNameGroup = 1;

bool listed(const std::vector<std::string>& names, const std::string& name) {
  return std::find(names.begin(), names.end(), name) != names.end();
}

}  // namespace

std::vector<std::string> opened_classes(const std::string& source) {
  std::vector<std::string> names;
  std::istringstream lines(source);
  std::smatch opening;
  for (std::string line; std::getline(lines, line);) {
    if (!std::regex_search(line, opening, kOpening)) continue;
    const std::string name = opening[kNameGroup].str();
    if (!listed(names, name)) names.push_back(name);
  }
  return names;
}

}  // namespace redgem::testing
