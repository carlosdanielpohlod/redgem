#include "project/error_hints.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <vector>

#include "core/config.hpp"
#include "core/translations.hpp"
#include "project/api_reference.hpp"
#include "project/closest_words.hpp"
#include "project/game_checks.hpp"
#include "project/scene_command.hpp"
#include "script/script_naming.hpp"

namespace redgem::tools {
namespace {

namespace fs = std::filesystem;

constexpr std::size_t kMostSuggestions = 3;
// What mruby and the engine say, and what of it a hint needs (the first group, then the second).
// Of a call on an instance mruby names the class ("for Transform"); of one on a class or a module
// itself, only that it is one ("for Module", "for Class"): the place's line of code says which.
const char* const kUnknownMethod = R"(undefined method '([^']+)' for ([A-Z][A-Za-z0-9_:]*))";
const char* const kPlace = R"(^([^ :]+\.rb):([0-9]+):)";
const char* const kClassName = R"(([A-Z][A-Za-z0-9_]*(?:::[A-Z][A-Za-z0-9_]*)*)\.)";
const char* const kModuleReceiver = "Module";
const char* const kClassReceiver = "Class";
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

// The game's scripts, as paths in its scripts folder ("enemies/slime.rb").
std::vector<std::string> scripts_of(const fs::path& game_dir) {
  return script_files(game_dir / config::kScriptsDirName);
}

// The classes the game's scripts are named for, and the engine's.
std::vector<std::string> class_names(const fs::path& game_dir) {
  std::vector<std::string> names;
  for (const std::string& file : scripts_of(game_dir)) names.push_back(class_name_for_script(file));
  for (const ApiClass& owner : api_reference().classes) names.push_back(owner.name);
  return names;
}

bool is_class_receiver(const std::string& receiver) {
  return receiver == kModuleReceiver || receiver == kClassReceiver;
}

// The names near `method` of the methods of `owner` called on it or on its instances.
std::string methods_hint(const ApiClass& owner, const std::string& method, bool on_class) {
  const std::vector<std::string> near = closest_words(method, method_names(owner, on_class), kMostSuggestions);
  if (near.empty()) return tr("project.hint.see_class", {{"class", owner.name}});
  return tr("project.hint.method", {{"names", joined(near)}, {"class", owner.name}});
}

// Line `number` of a file; empty when there is none.
std::string line_of(const fs::path& file, std::size_t number) {
  std::ifstream in(file);
  std::string line;
  std::size_t read = 0;
  while (read < number && std::getline(in, line)) ++read;
  return read == number ? line : std::string();
}

// The text of a regular expression that finds `text` as it is.
std::string escaped(const std::string& text) {
  std::string out;
  for (const char letter : text) {
    if (std::isalnum(static_cast<unsigned char>(letter)) == 0) out += '\\';
    out += letter;
  }
  return out;
}

// The class a failed call was made on, read from the line of code the message points at
// ("Input.presed?(:space)" -> "Input"); empty when the line does not say.
std::string receiver_in_code(const std::string& message, const std::string& method, const fs::path& game_dir) {
  static const std::regex place(kPlace);
  Groups found;
  if (!std::regex_search(message, found, place)) return {};
  const std::string code = line_of(game_dir / found[kFirst].str(), std::stoul(found[kSecond].str()));
  Groups call;
  if (!std::regex_search(code, call, std::regex(kClassName + escaped(method)))) return {};
  return call[kFirst];
}

// The calls on classes near `method`, whatever the class: "Input.pressed?".
std::string any_class_hint(const std::string& method) {
  std::vector<std::string> near;
  for (const ApiClass& owner : api_reference().classes) {
    for (const std::string& name : closest_words(method, method_names(owner, true), kMostSuggestions)) {
      near.push_back(owner.name + "." + name);
    }
  }
  if (near.size() > kMostSuggestions) near.resize(kMostSuggestions);
  return near.empty() ? std::string() : tr("project.hint.did_you_mean", {{"names", joined(near)}});
}

// A method called on a class or a module itself: that class's, when the code says which.
std::string class_call_hint(const Groups& groups, const fs::path& game_dir) {
  const std::string method = groups[kFirst];
  const ApiClass* owner =
      find_api_class(api_reference(), receiver_in_code(groups.prefix().str() + groups.str(), method, game_dir));
  return owner != nullptr ? methods_hint(*owner, method, true) : any_class_hint(method);
}

std::string method_hint(const Groups& groups, const fs::path& game_dir) {
  if (is_class_receiver(groups[kSecond])) return class_call_hint(groups, game_dir);
  const ApiClass* owner = find_api_class(api_reference(), groups[kSecond]);
  if (owner == nullptr) return {};
  return methods_hint(*owner, groups[kFirst], false);
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
  const std::vector<std::string> files = scripts_of(game_dir);
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
