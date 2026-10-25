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
#include "project/scene_text.hpp"
#include "editor/node_tree.hpp"
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
constexpr const char* kKeys =
    "position (pos), rotation (rot), scale, name, parent, script (scripts), prop.<key> (or <key> alone)";
// What a line calls the scripts, the mesh and the light (see scene_text.hpp).
constexpr const char* kScriptsKey = "scripts";
constexpr const char* kMeshKey = "mesh";
constexpr const char* kLightKey = "light";
constexpr char kPathSeparator = '/';
// A key of up to this many letters is mistyped by one change at most; longer ones by two.
constexpr std::size_t kShortKey = 5;
constexpr std::size_t kTypoChanges = 2;

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
  std::string problem = name_problem(assignment.value);
  if (!problem.empty()) return problem;
  // (A name another object has is made its own, as the editor makes it: "Wall.001".)
  if (edit.document.node(index).name != assignment.value) {
    edit.document.set_name(index, edit.document.unique_name(assignment.value));
  }
  return {};
}

// The parent a word names: the top of the scene for nothing.
FoundObject parent_named(const SceneDesc& scene, const std::string& reference) {
  if (reference.empty()) return {kNoIndex, {}};
  return find_object(scene, reference);
}

// Whether `parent` is the object or one of its children: no object goes under itself.
bool is_within(const SceneDesc& scene, int index, int parent) {
  const std::vector<int> family = node_tree::subtree(scene.nodes, index);
  return std::find(family.begin(), family.end(), parent) != family.end();
}

std::string set_parent(SceneEdit& edit, int& index, const Assignment& assignment) {
  const FoundObject parent = parent_named(edit.document.desc(), assignment.value);
  if (!parent.error.empty()) return parent.error;
  if (is_within(edit.document.desc(), index, parent.index)) return tr("project.scene.cannot_parent");
  // (Moving renumbers the objects, the parent among them: the object is where reparent says.)
  index = edit.document.reparent(index, parent.index);
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

// `script-=Door,Sound`: the scripts go, and each has to be there to go.
std::string take_scripts(SceneEdit& edit, int index, std::vector<std::string> scripts,
                         const std::vector<std::string>& taken) {
  for (const std::string& script : taken) {
    const auto found = std::find(scripts.begin(), scripts.end(), script);
    if (found == scripts.end()) {
      return tr("project.scene.script_not_attached", {{"script", script}, {"scripts", joined(scripts)}});
    }
    scripts.erase(found);
  }
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
  if (assignment.op == kTake) return take_scripts(edit, index, attached, named);
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

// mesh= and light= say what an object is: a line's words are taken back as they are, and not changed.
std::string shown_as(const std::string& key, const std::string& shown, const Assignment& assignment) {
  if (assignment.value == shown) return {};
  return tr("project.scene.read_only", {{"key", key}, {"value", shown.empty() ? "-" : shown}});
}

std::string check_mesh(SceneEdit& edit, int& index, const Assignment& assignment) {
  const SceneDesc& scene = edit.document.desc();
  return shown_as(assignment.key, shown_mesh(scene, scene.nodes[index]), assignment);
}

std::string check_light(SceneEdit& edit, int& index, const Assignment& assignment) {
  const NodeDesc& node = edit.document.node(index);
  return shown_as(assignment.key, node.light ? light_name(node.light->type) : "", assignment);
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
    {"parent", set_parent, false},     {props::kScript, set_scripts, true}, {kScriptsKey, set_scripts, true},
    {kMeshKey, check_mesh, false},     {kLightKey, check_light, false},
};

const Key* key_named(const std::string& name) {
  const auto* found =
      std::find_if(std::begin(kSetters), std::end(kSetters), [&name](const Key& key) { return name == key.name; });
  return found == std::end(kSetters) ? nullptr : found;
}

bool is_property(const std::string& key) {
  return key.rfind(kPropPrefix, 0) == 0 && key.size() > std::string(kPropPrefix).size();
}

std::vector<std::string> key_names() {
  std::vector<std::string> names;
  for (const Key& known : kSetters) names.emplace_back(known.name);
  return names;
}

// How many letters a word may be off a key and still be taken for a mistyped key, not a property.
std::size_t typo_changes(const std::string& key) { return key.size() > kShortKey ? kTypoChanges : 1; }

// Whether a word that is no key is so near one that it is a typo of it (`positon`), rather than
// a custom property (`speed`, `tag`).
bool mistyped_key(const std::string& word) {
  const std::vector<std::string> names = key_names();
  return std::any_of(names.begin(), names.end(),
                     [&word](const std::string& key) { return edit_distance(word, key) <= typo_changes(key); });
}

std::string unknown_key(const std::string& key) {
  const std::vector<std::string> names = key_names();
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

// Whether a name would be read as an id ("#3") or a path ("Level/Wall") when an object is named.
bool reads_as_reference(const std::string& name) {
  return name.front() == kIdMark || name.find(kPathSeparator) != std::string::npos;
}

// The word `camera` alone, as a line says an object has one: taken back only by one that has.
std::string camera_word(const SceneEdit& edit, int index) {
  if (edit.document.node(index).camera) return {};
  return tr("project.scene.read_only", {{"key", kCameraWord}, {"value", "-"}});
}

// key=value whose key is no key of set's: a custom property (as a line lists them), unless the
// object has no such property and the key is a typo of one of set's (then prop.<key>= says it is one).
std::string property_or_typo(SceneEdit& edit, int index, const Assignment& assignment) {
  const bool has_it = edit.document.node(index).props.count(assignment.key) > 0;
  if (!has_it && mistyped_key(assignment.key)) return unknown_key(assignment.key);
  return set_property(edit, index, assignment.key, assignment);
}

std::string apply_assignment(SceneEdit& edit, int& index, const std::string& word) {
  if (word == kCameraWord) return camera_word(edit, index);
  const auto assignment = read_word(word);
  if (!assignment) return tr("project.scene.not_an_assignment", {{"word", word}, {"keys", kKeys}});
  if (is_property(assignment->key)) {
    return set_property(edit, index, assignment->key.substr(std::string(kPropPrefix).size()), *assignment);
  }
  const Key* key = key_named(assignment->key);
  if (key == nullptr) return property_or_typo(edit, index, *assignment);
  if (!takes(*key, *assignment)) return only_equals(*assignment);
  return key->set(edit, index, *assignment);
}

std::string name_problem(const std::string& name) {
  if (name.empty()) return tr("project.scene.needs_name");
  if (reads_as_reference(name)) return tr("project.scene.bad_name", {{"name", name}});
  return {};
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
