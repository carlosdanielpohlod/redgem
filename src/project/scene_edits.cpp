#include "project/scene_edits.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>

#include "core/translations.hpp"
#include "editor/new_object_kinds.hpp"
#include "editor/node_tree.hpp"
#include "project/scene_lookup.hpp"
#include "project/scene_text.hpp"
#include "scene/props.hpp"

namespace redgem::tools {
namespace {

using mcp::Json;
using Words = std::vector<std::string>;

constexpr const char* kSet = "set";
constexpr const char* kAdd = "add";
constexpr const char* kDuplicate = "duplicate";
constexpr const char* kGroup = "group";
constexpr const char* kRemove = "remove";
constexpr const char* kEmptyKind = "empty";

std::invalid_argument wrong(const std::string& what) { return std::invalid_argument(what); }

int resolve(const SceneDocument& document, const std::string& reference) {
  const FoundObject found = find_object(document.desc(), reference);
  if (found.index == kNoIndex) throw wrong(found.error);
  return found.index;
}

std::string id_of(const SceneDocument& document, int index) { return object_id(document.desc(), index); }

// Whether `object` names an object of the scene (set, duplicate, remove) rather than a new one.
bool names_target(const Json& args, bool object_is_new) { return !object_is_new && args.contains("object"); }

// The objects `objects` names (and `object` too, when it names one rather than a new object), as
// persistent ids, each once.
Words targets(const SceneDocument& document, const Json& args, bool object_is_new) {
  Words references = args.value("objects", Words{});
  if (names_target(args, object_is_new)) references.insert(references.begin(), args["object"].get<std::string>());
  if (references.empty()) throw wrong("name the objects with object or objects");
  Words ids;
  for (const auto& reference : references) {
    const std::string id = id_of(document, resolve(document, reference));
    if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
  }
  return ids;
}

// Whether `index` is a child of `other`, or a child of its children.
bool holds(const SceneDesc& scene, int other, int index) {
  const auto family = node_tree::subtree(scene.nodes, other);
  return other != index && std::find(family.begin(), family.end(), index) != family.end();
}

// Whether `index` is under one of `others` (or is one of them), so that it goes along with it.
bool inside_any(const SceneDesc& scene, int index, const std::vector<int>& others) {
  return std::any_of(others.begin(), others.end(), [&](int other) { return holds(scene, other, index); });
}

// The objects the ids name, without those under another of them: what a copy or removal takes whole.
std::vector<int> roots_of(const SceneDocument& document, const Words& ids) {
  std::vector<int> all;
  for (const auto& id : ids) all.push_back(resolve(document, id));
  std::vector<int> roots;
  for (const int index : all) {
    if (!inside_any(document.desc(), index, all)) roots.push_back(index);
  }
  return roots;
}

void assign(SceneEdit& edit, int& index, const Words& words) {
  for (const auto& word : words) {
    const std::string problem = apply_assignment(edit, index, word);
    if (!problem.empty()) throw wrong(problem);
  }
}

// Sets the words on each object; the ids keep naming them when a parent= renumbers the scene.
void assign_each(SceneEdit& edit, const Words& ids, const Words& words) {
  for (const auto& id : ids) {
    int index = resolve(edit.document, id);
    assign(edit, index, words);
  }
}

EditOutcome outcome(const SceneEdit& edit, const Words& ids) {
  EditOutcome result;
  for (const auto& id : ids) {
    result.objects.push_back(resolve(edit.document, id));
    result.text += object_line(edit.document.desc(), result.objects.back()) + "\n";
  }
  for (const auto& note : edit.notes) result.text += note + "\n";
  return result;
}

Words words_of(const Json& args) { return args.value("assignments", Words{}); }

EditOutcome set_all(SceneEdit& edit, const Json& args) {
  const Words ids = targets(edit.document, args, false);
  const Words words = words_of(args);
  if (words.empty()) throw wrong("set needs assignments");
  assign_each(edit, ids, words);
  return outcome(edit, ids);
}

std::string removal_line(const SceneDocument& document, int root) {
  const std::string object = id_of(document, root) + " " + document.node(root).name;
  const std::size_t count = node_tree::subtree(document.desc().nodes, root).size();
  return tr("project.scene.removed", {{"object", object}, {"count", std::to_string(count)}}) + "\n";
}

EditOutcome remove_all(SceneEdit& edit, const Json& args) {
  if (!args.value("confirm", false)) throw wrong("remove needs confirm=true");
  if (!words_of(args).empty()) throw wrong("remove does not take assignments");
  const std::vector<int> roots = roots_of(edit.document, targets(edit.document, args, false));
  EditOutcome result;
  for (const int root : roots) result.text += removal_line(edit.document, root);
  edit.document.remove(roots);
  return result;
}

std::optional<int> parent_of_call(const SceneDocument& document, const Json& args) {
  if (!args.contains("parent")) return std::nullopt;
  return resolve(document, args["parent"].get<std::string>());
}

// A copy of a template object is an object of the scene, not a template.
int copy_of(SceneDocument& document, int source, const std::string& name) {
  const int copy = document.duplicate(source);
  document.set_name(copy, document.unique_name(name));
  document.remove_prop(copy, props::kPrefab);
  return copy;
}

int added_copy(SceneDocument& document, const Json& args, const std::string& name) {
  const int copy = copy_of(document, resolve(document, args["from"].get<std::string>()), name);
  const auto parent = parent_of_call(document, args);
  return parent ? document.reparent(copy, *parent) : copy;
}

new_objects::Kind kind_of(const Json& args) {
  const auto kind = new_objects::kind_named(args.value("kind", std::string(kEmptyKind)));
  if (!kind) throw wrong("kind must be one of: " + new_objects::kind_words());
  return *kind;
}

bool second_environment(new_objects::Kind kind, int existing) {
  return kind == new_objects::Kind::Environment && existing != kNoIndex;
}

// The scene has one environment: a second would fight the first over the sky.
void require_no_environment(const SceneDocument& document, new_objects::Kind kind) {
  const int existing = new_objects::environment_of(document);
  if (!second_environment(kind, existing)) return;
  throw wrong("the scene has an environment already: " + object_line(document.desc(), existing) +
              "; set its properties instead");
}

int added_kind(SceneDocument& document, const Json& args, const std::string& name) {
  const new_objects::Kind kind = kind_of(args);
  require_no_environment(document, kind);
  const int parent = parent_of_call(document, args).value_or(kNoIndex);
  if (kind == new_objects::Kind::Empty) return document.add_empty(name, parent);
  return new_objects::add_object(document, kind, name, parent);
}

std::string new_name(const Json& args) {
  if (!args.contains("object")) throw wrong("add and group need object: the new object's name");
  const std::string name = args["object"];
  const std::string problem = name_problem(name);
  if (!problem.empty()) throw wrong(problem);
  return name;
}

bool names_both_sources(const Json& args) { return args.contains("from") && args.contains("kind"); }

EditOutcome add_one(SceneEdit& edit, const Json& args) {
  const std::string name = new_name(args);
  if (names_both_sources(args)) throw wrong("add takes from or kind, not both");
  int index = args.contains("from") ? added_copy(edit.document, args, name) : added_kind(edit.document, args, name);
  const std::string id = id_of(edit.document, index);
  assign(edit, index, words_of(args));
  return outcome(edit, {id});
}

// Copies of the objects, each with its children (the document names them apart: "Crate.001").
EditOutcome duplicate_all(SceneEdit& edit, const Json& args) {
  SceneDocument& document = edit.document;
  Words ids;
  for (const int copy : document.duplicate(roots_of(document, targets(document, args, false))))
    ids.push_back(id_of(document, copy));
  assign_each(edit, ids, words_of(args));
  return outcome(edit, ids);
}

// Whether `parent` is one of the members or under one: a group cannot hold itself.
bool under_members(const SceneDesc& scene, int parent, const std::vector<int>& members) {
  if (parent == kNoIndex) return false;
  return std::find(members.begin(), members.end(), parent) != members.end() || inside_any(scene, parent, members);
}

// Where a group goes: under `parent`, else where its first member is.
int group_parent(const SceneDocument& document, const Json& args, const std::vector<int>& members) {
  const int parent = parent_of_call(document, args).value_or(document.node(members.front()).parent);
  if (under_members(document.desc(), parent, members)) throw wrong("a group cannot go under its members");
  return parent;
}

// The group, then its members (those under another member go along with it).
EditOutcome group_all(SceneEdit& edit, const Json& args) {
  SceneDocument& document = edit.document;
  const std::string name = new_name(args);
  const std::vector<int> roots = roots_of(document, targets(document, args, true));
  Words members;
  for (const int root : roots) members.push_back(id_of(document, root));
  const std::string group = id_of(document, document.add_empty(name, group_parent(document, args, roots)));
  for (const auto& member : members) document.reparent(resolve(document, member), resolve(document, group));
  members.insert(members.begin(), group);
  EditOutcome result = outcome(edit, members);
  result.objects.resize(1);  // the group is what is selected
  return result;
}

bool names_source(const Json& args) { return args.contains("from") || args.contains("kind"); }
bool places_under(const std::string& action) { return action == kAdd || action == kGroup; }
// from and kind say what add makes; parent where add and group put it; objects what the others are about.
bool misplaced_source(const std::string& action, const Json& args) { return action != kAdd && names_source(args); }
bool misplaced_parent(const std::string& action, const Json& args) {
  return !places_under(action) && args.contains("parent");
}
bool adds_several(const std::string& action, const Json& args) { return action == kAdd && args.contains("objects"); }

void require_add_options(const std::string& action, const Json& args) {
  if (misplaced_source(action, args)) throw wrong("from and kind are only accepted by add");
  if (misplaced_parent(action, args)) throw wrong("parent is only accepted by add and group");
  if (adds_several(action, args)) throw wrong("add makes one object: name it with object");
}

}  // namespace

EditOutcome edit_scene(SceneEdit& edit, const Json& args) {
  const std::string action = args.at("action");
  require_add_options(action, args);
  if (action == kSet) return set_all(edit, args);
  if (action == kAdd) return add_one(edit, args);
  if (action == kDuplicate) return duplicate_all(edit, args);
  if (action == kGroup) return group_all(edit, args);
  if (action == kRemove) return remove_all(edit, args);
  throw wrong("action must be one of: set, add, duplicate, group, remove");
}

}  // namespace redgem::tools
