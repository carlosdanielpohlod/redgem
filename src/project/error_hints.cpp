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
// (mruby says "for class X" or "for module X" of a call on the class itself, "for X" of one on an
// instance: the third group.)
const char* const kUnknownMethod = R"(undefined method '([^']+)' for (?:module |class )?([A-Z][A-Za-z0-9_:]*))";
const char* const kClassReceiver = R"(' for (module|class) )";
const char* const kUnknownConstant = R"(uninitialized constant ([A-Z][A-Za-z0-9_]*(?:::[A-Z][A-Za-z0-9_]*)*))";
const char* const kUnknownScene = R"(no scene named ([^ ]+))";
const char* const kUnreadableScript = R"(loading scripts/([^ :]+\.rb): cannot read)";
constexpr std::size_t kFirst = 1;
constexpr std::size_t kSecond = 2;

using Groups = std::smatch;
using Hinter = std::string (*)(const Groups&, const fs::path&);

// The methods of a class that are called on it (`on_class`) or on its instances.
std::vector<std::string> method_names(const ApiClass& owner, bool on_class) {
  std::vector<std::string> names;
  for (const ApiMethod& method : owner.methods) {
    if (method.on_class == on_class) names.push_back(method.name);
  }
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

bool called_on_class(const std::string& message) {
  static const std::regex receiver(kClassReceiver);
  return std::regex_search(message, receiver);
}

std::string method_hint(const Groups& groups, const fs::path&) {
  const ApiClass* owner = find_api_class(api_reference(), groups[kSecond]);
  if (owner == nullptr) return {};
  const std::vector<std::string> names = method_names(*owner, called_on_class(groups.str()));
  const std::vector<std::string> near = closest_words(groups[kFirst], names, kMostSuggestions);
  if (near.empty()) return tr("project.hint.see_class", {{"class", owner->name}});
  return tr("project.hint.method", {{"names", joined(near)}, {"class", owner->name}});
}

// A class inside another that is not there: the engine's classes near it, or where they are listed.
std::string nested_hint(const std::string& name) {
  std::vector<std::string> names;
  for (const ApiClass& owner : api_reference().classes) names.push_back(owner.name);
  const std::vector<std::string> near = closest_words(name, names, kMostSuggestions);
  if (near.empty()) return tr("project.hint.see_index");
  return tr("project.hint.did_you_mean", {{"names", joined(near)}});
}

// Whether a name is inside another class or module ("GameMenu::Screen"): no script is looked for.
bool is_nested(const std::string& name) { return name.find("::") != std::string::npos; }

std::string constant_hint(const Groups& groups, const fs::path& game_dir) {
  const std::string name = groups[kFirst];
  if (is_nested(name)) return nested_hint(name);
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
  std::regex expression;
  Hinter hint = nullptr;
};

// (Made once: a game may say the same warning every frame.)
const std::vector<Pattern>& patterns() {
  static const std::vector<Pattern> all = {
      {std::regex(kUnknownMethod), method_hint},
      {std::regex(kUnknownConstant), constant_hint},
      {std::regex(kUnknownScene), scene_hint},
      {std::regex(kUnreadableScript), script_hint},
  };
  return all;
}

}  // namespace

std::string hint_for(const std::string& message, const fs::path& game_dir) {
  for (const Pattern& pattern : patterns()) {
    Groups groups;
    if (std::regex_search(message, groups, pattern.expression)) return pattern.hint(groups, game_dir);
  }
  return {};
}

}  // namespace redgem::tools
