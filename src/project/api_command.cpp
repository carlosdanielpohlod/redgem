#include "project/api_command.hpp"

#include <nlohmann/json.hpp>

#include "core/command_line.hpp"
#include "core/translations.hpp"
#include "project/api_reference.hpp"
#include "project/closest_words.hpp"

namespace redgem::tools {
namespace {

constexpr const char* kSearchOption = "--search";
constexpr const char* kJsonOption = "--json";
constexpr const char* kMarkdownOption = "--markdown";
constexpr std::size_t kMostFound = 20;
constexpr std::size_t kMostSuggestions = 3;
// The keys of the reference's JSON (see docs/scripting/api.json) a search or a class is cut from.
constexpr const char* kClasses = "classes";
constexpr const char* kName = "name";
constexpr const char* kMethods = "methods";
constexpr const char* kClass = "class";

ToolGrammar grammar() { return {{kSearchOption}, {kJsonOption, kMarkdownOption, command_line::kHelpOption}}; }

ToolResult answer(std::string text) { return ToolResult{kExitSuccess, std::move(text), {}}; }

const nlohmann::json& reference_json() {
  static const nlohmann::json json = nlohmann::json::parse(api_reference_json());
  return json;
}

std::vector<std::string> class_names() {
  std::vector<std::string> names;
  for (const ApiClass& owner : api_reference().classes) names.push_back(owner.name);
  return names;
}

ToolResult no_class(const std::string& name) {
  const std::vector<std::string> near = closest_words(name, class_names(), kMostSuggestions);
  const std::string hint =
      near.empty() ? tr("project.api.see_index") : tr("project.api.did_you_mean", {{"names", joined(near)}});
  return failed(tr("project.api.no_class", {{"class", name}}) + " " + hint);
}

nlohmann::json class_json(const std::string& name) {
  for (const nlohmann::json& owner : reference_json().at(kClasses)) {
    if (owner.at(kName) == name) return owner;
  }
  return nlohmann::json();
}

// The methods a search finds, as data: each with the class it is of.
nlohmann::json found_json(const std::string& word) {
  nlohmann::json found = nlohmann::json::array();
  for (const nlohmann::json& owner : reference_json().at(kClasses)) {
    for (nlohmann::json method : owner.at(kMethods)) {
      method[kClass] = owner.at(kName);
      if (lowered(method.dump()).find(lowered(word)) != std::string::npos) found.push_back(method);
    }
  }
  return found;
}

ToolResult one_class(const std::string& name, bool as_json) {
  const ApiClass* owner = find_api_class(api_reference(), name);
  if (owner == nullptr) return no_class(name);
  return answer(as_json ? class_json(owner->name).dump() + "\n" : api_class_text(*owner));
}

ToolResult search(const std::string& word, bool as_json) {
  if (as_json) return answer(found_json(word).dump() + "\n");
  return answer(api_search(api_reference(), word, kMostFound));
}

ToolResult whole(const ToolLine& line) {
  if (line.has(kMarkdownOption)) return answer(api_markdown(api_reference()));
  if (line.has(kJsonOption)) return answer(reference_json().dump() + "\n");
  return answer(api_index(api_reference()));
}

ToolResult usage_failure(const std::string& problem) {
  return failed(problem + "\n\n" + tr("project.api.usage"), kExitUsage);
}

}  // namespace

ToolResult run_api_command(const std::vector<std::string>& args) {
  std::string error;
  const auto line = read_tool_line(args, 0, grammar(), error);
  if (!line) return usage_failure(error);
  if (line->has(command_line::kHelpOption)) return answer(tr("project.api.usage"));
  if (line->words.size() > 1) return usage_failure(tr("project.api.one_class"));
  if (const auto word = line->value(kSearchOption)) return search(*word, line->has(kJsonOption));
  if (!line->words.empty()) return one_class(line->words.front(), line->has(kJsonOption));
  return whole(*line);
}

}  // namespace redgem::tools
