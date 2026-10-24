#include "project/error_hints.hpp"

#include <algorithm>
#include <regex>
#include <vector>

#include "core/config.hpp"
#include "core/translations.hpp"
#include "project/api_reference.hpp"
#include "project/closest_words.hpp"
#include "project/scene_command.hpp"
#include "script/script_naming.hpp"

namespace redgem::tools {
namespace {

namespace fs = std::filesystem;

constexpr std::size_t kMostSuggestions = 3;
// What mruby and the engine say, and what of it a hint needs (the first group, then the second).
const char* const kUnknownMethod = R"(undefined method '([^']+)' for (?:module |class )?([A-Z][A-Za-z0-9_:]*))";
const char* const kUnknownConstant = R"(uninitialized constant ([A-Z][A-Za-z0-9_]*))";
const char* const kUnknownScene = R"(no scene named ([^ ]+))";
const char* const kUnreadableScript = R"(loading scripts/([^ :]+\.rb): cannot read)";
constexpr std::size_t kFirst = 1;
constexpr std::size_t kSecond = 2;

using Groups = std::smatch;
using Hinter = std::string (*)(const Groups&, const fs::path&);

std::vector<std::string> method_names(const ApiClass& owner) {
  std::vector<std::string> names;
  names.reserve(owner.methods.size());
  for (const ApiMethod& method : owner.methods) names.push_back(method.name);
  return names;
}

// The game's scripts, as their file names ("coin.rb").
std::vector<std::string> script_files(const fs::path& game_dir) {
  std::vector<std::string> files;
  std::error_code error;
  for (fs::recursive_directory_iterator entry(game_dir / config::kScriptsDirName, error), end; !error && entry != end;
       entry.increment(error)) {
    if (entry->path().extension() == config::kRubyFileExtension) files.push_back(entry->path().filename().string());
  }
  std::sort(files.begin(), files.end());
  return files;
}

// The classes the game's scripts are named for, and the engine's.
std::vector<std::string> class_names(const fs::path& game_dir) {
  std::vector<std::string> names;
  for (const std::string& file : script_files(game_dir)) names.push_back(class_name_for_script(file));
  for (const ApiClass& owner : api_reference().classes) names.push_back(owner.name);
  return names;
}

std::string method_hint(const Groups& groups, const fs::path&) {
  const ApiClass* owner = find_api_class(api_reference(), groups[kSecond]);
  if (owner == nullptr) return {};
  const std::vector<std::string> near = closest_words(groups[kFirst], method_names(*owner), kMostSuggestions);
  if (near.empty()) return tr("project.hint.see_class", {{"class", owner->name}});
  return tr("project.hint.method", {{"names", joined(near)}, {"class", owner->name}});
}

std::string constant_hint(const Groups& groups, const fs::path& game_dir) {
  const std::string name = groups[kFirst];
  const std::vector<std::string> near = closest_words(name, class_names(game_dir), kMostSuggestions);
  const std::string file = std::string(config::kScriptsDirName) + "/" + script_for_class_name(name);
  const std::string looked = tr("project.hint.constant", {{"class", name}, {"file", file}});
  return near.empty() ? looked : looked + " " + tr("project.hint.did_you_mean", {{"names", joined(near)}});
}

std::string scene_hint(const Groups& groups, const fs::path& game_dir) {
  const std::vector<std::string> scenes = scene_names(game_dir);
  const std::vector<std::string> near = closest_words(groups[kFirst], scenes, kMostSuggestions);
  if (!near.empty()) return tr("project.hint.did_you_mean", {{"names", joined(near)}});
  return tr("project.hint.scenes", {{"scenes", joined(scenes)}});
}

std::string script_hint(const Groups& groups, const fs::path& game_dir) {
  const std::vector<std::string> files = script_files(game_dir);
  const std::vector<std::string> near = closest_words(groups[kFirst], files, kMostSuggestions);
  if (!near.empty()) return tr("project.hint.did_you_mean", {{"names", joined(near)}});
  return tr("project.hint.scripts", {{"scripts", joined(files)}});
}

struct Pattern {
  const char* expression;
  Hinter hint;
};
const Pattern kPatterns[] = {
    {kUnknownMethod, method_hint},
    {kUnknownConstant, constant_hint},
    {kUnknownScene, scene_hint},
    {kUnreadableScript, script_hint},
};

}  // namespace

std::string hint_for(const std::string& message, const fs::path& game_dir) {
  for (const Pattern& pattern : kPatterns) {
    Groups groups;
    if (std::regex_search(message, groups, std::regex(pattern.expression))) return pattern.hint(groups, game_dir);
  }
  return {};
}

}  // namespace redgem::tools
