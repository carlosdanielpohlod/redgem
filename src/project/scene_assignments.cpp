#include "project/scene_assignments.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <optional>
#include <sstream>

#include "core/config.hpp"
#include "core/translations.hpp"
#include "project/closest_words.hpp"
#include "project/scene_lookup.hpp"
#include "project/tool_line.hpp"
#include "scene/props.hpp"
#include "script/script_naming.hpp"

namespace redgem::tools {
namespace {

constexpr char kEquals = '=';
constexpr char kAdd = '+';
constexpr char kTake = '-';
constexpr char kComma = ',';
constexpr char kQuote = '"';
constexpr std::size_t kQuotes = 2;  // one on each side
constexpr const char* kPropPrefix = "prop.";
constexpr const char* kTrue = "true";
constexpr const char* kFalse = "false";
constexpr std::size_t kVectorSides = 3;
// Every key a word may set (the props are "prop.<key>").
constexpr const char* kKeys = "position (pos), rotation (rot), scale, name, parent, script, prop.<key>";

// One word, read: `key` `op` `value`, op being '=', '+' (for +=) or '-' (for -=).
struct Assignment {
  std::string key;
  char op = kEquals;
  std::string value;
};

// Whether the letter before '=' turns it into += or -=.
bool is_operator_letter(char letter) { return letter == kAdd || letter == kTake; }

std::optional<Assignment> read_word(const std::string& word) {
  const std::size_t equals = word.find(kEquals);
  if (equals == std::string::npos || equals == 0) return std::nullopt;
  Assignment assignment{word.substr(0, equals), kEquals, word.substr(equals + 1)};
  if (is_operator_letter(assignment.key.back())) {
    assignment.op = assignment.key.back();
    assignment.key.pop_back();
  }
  return assignment;
}

std::vector<std::string> split(const std::string& text, char separator) {
  std::vector<std::string> parts;
  std::stringstream stream(text);
  for (std::string part; std::getline(stream, part, separator);) parts.push_back(part);
  return parts;
}

std::optional<Vec3> vector_from(const std::string& text) {
  const std::vector<std::string> parts = split(text, kComma);
  if (parts.size() != kVectorSides) return std::nullopt;
  std::array<float, kVectorSides> sides{};
  for (std::size_t side = 0; side < kVectorSides; ++side) {
    const auto number = number_from(parts[side]);
    if (!number) return std::nullopt;
    sides[side] = static_cast<float>(*number);
  }
  return Vec3{sides[kXOffset], sides[kYOffset], sides[kZOffset]};
}

// A scale may be one number, the same on every side.
std::optional<Vec3> scale_from(const std::string& text) {
  if (const auto number = number_from(text)) return Vec3::one() * static_cast<float>(*number);
  return vector_from(text);
}

std::string needs_vector(const Assignment& assignment) {
  return tr("project.scene.needs_vector", {{"key", assignment.key}, {"value", assignment.value}});
}

std::string only_equals(const Assignment& assignment) {
  return tr("project.scene.only_equals", {{"key", assignment.key}});
}

// Changes where the object is, keeping the rest of its placement.
template <typename Change>
std::string place(SceneEdit& edit, int index, Change change) {
  Transform local = edit.document.node(index).local;
  change(local);
  edit.document.set_local(index, local);
  return {};
}

std::string set_position(SceneEdit& edit, int& index, const Assignment& assignment) {
  const auto position = vector_from(assignment.value);
  if (!position) return needs_vector(assignment);
  return place(edit, index, [&position](Transform& local) { local.position = *position; });
}

std::string set_rotation(SceneEdit& edit, int& index, const Assignment& assignment) {
  const auto degrees = vector_from(assignment.value);
  if (!degrees) return needs_vector(assignment);
  return place(edit, index, [&degrees](Transform& local) { local.rotation = quat_from_euler_degrees(*degrees); });
}

std::string set_scale(SceneEdit& edit, int& index, const Assignment& assignment) {
  const auto scale = scale_from(assignment.value);
  if (!scale) return needs_vector(assignment);
  return place(edit, index, [&scale](Transform& local) { local.scale = *scale; });
}

std::string set_name(SceneEdit& edit, int& index, const Assignment& assignment) {
  if (assignment.value.empty()) return tr("project.scene.needs_name");
  edit.document.set_name(index, assignment.value);
  return {};
}

// The parent a word names: the top of the scene for nothing.
FoundObject parent_named(const SceneDesc& scene, const std::string& reference) {
  if (reference.empty()) return {kNoIndex, {}};
  return find_object(scene, reference);
}

std::string set_parent(SceneEdit& edit, int& index, const Assignment& assignment) {
  const FoundObject parent = parent_named(edit.document.desc(), assignment.value);
  if (!parent.error.empty()) return parent.error;
  const int moved = edit.document.reparent(index, parent.index);
  if (edit.document.node(moved).parent != parent.index) return tr("project.scene.cannot_parent");
  index = moved;
  return {};
}

std::vector<std::string> scripts_of(const SceneDocument& document, int index) {
  return split_script_list(get_string(document.node(index).props, props::kScript).value_or(""));
}

// A script that is not in the game's scripts folder (yet) is attached all the same, and said.
void note_missing(SceneEdit& edit, const std::string& script) {
  if (std::filesystem::exists(edit.scripts_dir / script)) return;
  edit.notes.push_back(
      tr("project.scene.script_missing", {{"script", script}, {"class", class_name_for_script(script)}}));
}

void write_scripts(SceneEdit& edit, int index, const std::vector<std::string>& scripts) {
  if (scripts.empty()) {
    edit.document.remove_prop(index, props::kScript);
    return;
  }
  edit.document.set_prop(index, props::kScript, join_script_list(scripts));
}

std::vector<std::string> script_files(const std::string& names) {
  std::vector<std::string> files;
  for (const std::string& name : split_script_list(names)) files.push_back(script_file_for(name));
  return files;
}

// `script-=Door`: the script goes, and has to be there to go.
std::string take_script(SceneEdit& edit, int index, std::vector<std::string> scripts, const std::string& script) {
  const auto found = std::find(scripts.begin(), scripts.end(), script);
  if (found == scripts.end()) {
    return tr("project.scene.script_not_attached", {{"script", script}, {"scripts", joined(scripts)}});
  }
  scripts.erase(found);
  write_scripts(edit, index, scripts);
  return {};
}

std::vector<std::string> with_added(std::vector<std::string> scripts, const std::vector<std::string>& added) {
  for (const std::string& script : added) {
    if (std::find(scripts.begin(), scripts.end(), script) == scripts.end()) scripts.push_back(script);
  }
  return scripts;
}

std::string set_scripts(SceneEdit& edit, int& index, const Assignment& assignment) {
  const std::vector<std::string> named = script_files(assignment.value);
  const std::vector<std::string> attached = scripts_of(edit.document, index);
  if (assignment.op == kTake) return take_script(edit, index, attached, named.empty() ? "" : named.front());
  for (const std::string& script : named) note_missing(edit, script);
  write_scripts(edit, index, assignment.op == kAdd ? with_added(attached, named) : named);
  return {};
}

std::string set_property(SceneEdit& edit, int index, const std::string& key, const Assignment& assignment) {
  if (assignment.op != kEquals) return only_equals(assignment);
  if (assignment.value.empty()) {
    edit.document.remove_prop(index, key);
  } else {
    edit.document.set_prop(index, key, typed_value(assignment.value));
  }
  return {};
}

using Setter = std::string (*)(SceneEdit&, int&, const Assignment&);
struct Key {
  const char* name;
  Setter set;
  bool adds_and_takes;  // whether it takes += and -=
};
constexpr Key kSetters[] = {
    {"position", set_position, false}, {"pos", set_position, false},        {"rotation", set_rotation, false},
    {"rot", set_rotation, false},      {"scale", set_scale, false},         {"name", set_name, false},
    {"parent", set_parent, false},     {props::kScript, set_scripts, true},
};

const Key* key_named(const std::string& name) {
  const auto* found =
      std::find_if(std::begin(kSetters), std::end(kSetters), [&name](const Key& key) { return name == key.name; });
  return found == std::end(kSetters) ? nullptr : found;
}

bool is_property(const std::string& key) {
  return key.rfind(kPropPrefix, 0) == 0 && key.size() > std::string(kPropPrefix).size();
}

std::string unknown_key(const std::string& key) {
  std::vector<std::string> names;
  for (const Key& known : kSetters) names.emplace_back(known.name);
  const std::vector<std::string> near = closest_words(key, names, 1);
  const std::string hint = near.empty() ? "" : tr("project.scene.did_you_mean", {{"name", near.front()}});
  return tr("project.scene.unknown_key", {{"key", key}, {"keys", kKeys}}) + hint;
}

bool in_quotes(const std::string& text) {
  return text.size() >= kQuotes && text.front() == kQuote && text.back() == kQuote;
}

bool ends_with(const std::string& text, const std::string& end) {
  return text.size() > end.size() && text.compare(text.size() - end.size(), end.size(), end) == 0;
}

bool takes(const Key& key, const Assignment& assignment) { return assignment.op == kEquals || key.adds_and_takes; }

}  // namespace

std::string apply_assignment(SceneEdit& edit, int& index, const std::string& word) {
  const auto assignment = read_word(word);
  if (!assignment) return tr("project.scene.not_an_assignment", {{"word", word}, {"keys", kKeys}});
  if (is_property(assignment->key)) {
    return set_property(edit, index, assignment->key.substr(std::string(kPropPrefix).size()), *assignment);
  }
  const Key* key = key_named(assignment->key);
  if (key == nullptr) return unknown_key(assignment->key);
  if (!takes(*key, *assignment)) return only_equals(*assignment);
  return key->set(edit, index, *assignment);
}

PropertyValue typed_value(const std::string& text) {
  if (text == kTrue) return true;
  if (text == kFalse) return false;
  if (const auto number = number_from(text)) return *number;
  return in_quotes(text) ? text.substr(1, text.size() - kQuotes) : text;
}

std::string script_file_for(const std::string& name) {
  return ends_with(name, config::kRubyFileExtension) ? name : script_for_class_name(name);
}

}  // namespace redgem::tools
