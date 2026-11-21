#include "project/scene_edits.hpp"

#include <algorithm>
#include <functional>
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

using Step = std::function<void()>;

std::string done(const Step& step) {
  step();
  return {};
}

// Runs `make` then `rest` as one undo step, rolled back when either throws. Objects that bring
// assets (a mesh, a light, a camera, a terrain, a copy) are made apart, as the editor makes them.
void one_step(SceneDocument& document, bool brings_assets, const Step& make, const Step& rest) {
  std::string unused;  // (every failure throws)
  if (brings_assets) {
    document.make_then_edit(make, [&rest] { return done(rest); }, unused);
    return;
  }
  document.edit_nodes([&] { return make(), done(rest); }, unused);
}

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
  one_step(edit.document, false, [] {}, [&] { assign_each(edit, ids, words); });
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
  one_step(edit.document, false, [] {}, [&] { edit.document.remove(roots); });
  return result;
}

std::optional<int> parent_of_call(const SceneDocument& document, const Json& args) {
  if (!args.contains("parent")) return std::nullopt;
  return resolve(document, args["parent"].get<std::string>());
}

new_objects::Kind kind_of(const Json& args) {
  const auto kind = new_objects::kind_named(args.value("kind", std::string(kEmptyKind)));
  if (!kind) throw wrong("kind must be one of: " + new_objects::kind_words());
  return *kind;
}

// Whether an add makes more than nodes: a copy, a shape, a light, a camera or a terrain.
bool brings_assets(const Json& args) {
  if (args.contains("from")) return true;
  const new_objects::Kind kind = kind_of(args);
  return kind != new_objects::Kind::Empty && kind != new_objects::Kind::Environment;
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
  return new_objects::add_object(document, kind, name, parent_of_call(document, args).value_or(kNoIndex));
}

// A copy of `from`, named and placed after it is made. A copy of a template is an object of the
// scene, not a template.
void finish_copy(SceneDocument& document, const Json& args, const std::string& name, int& index) {
  document.set_name(index, document.unique_name(name));
  document.remove_prop(index, props::kPrefab);
  const auto parent = parent_of_call(document, args);
  if (parent) index = document.reparent(index, *parent);
}

std::string new_name(const Json& args) {
  if (!args.contains("object")) throw wrong("add and group need object: the new object's name");
  const std::string name = args["object"];
  const std::string problem = name_problem(name);
  if (!problem.empty()) throw wrong(problem);
  return name;
}

bool names_both_sources(const Json& args) { return args.contains("from") && args.contains("kind"); }

// One add, as it goes: its object made (index), then named, placed and set.
struct Adding {
  SceneEdit& edit;
  const Json& args;
  std::string name;
  int source = kNoIndex;  // what from copies
  int index = kNoIndex;
  EditOutcome result{};
};

void make_added(Adding& adding) {
  SceneDocument& document = adding.edit.document;
  const bool copies = adding.source != kNoIndex;
  adding.index = copies ? document.duplicate(adding.source) : added_kind(document, adding.args, adding.name);
}

void finish_added(Adding& adding) {
  SceneDocument& document = adding.edit.document;
  if (adding.source != kNoIndex) finish_copy(document, adding.args, adding.name, adding.index);
  const std::string id = id_of(document, adding.index);
  assign(adding.edit, adding.index, words_of(adding.args));
  adding.result = outcome(adding.edit, {id});
}

int source_of(const SceneDocument& document, const Json& args) {
  return args.contains("from") ? resolve(document, args["from"].get<std::string>()) : kNoIndex;
}

EditOutcome add_one(SceneEdit& edit, const Json& args) {
  if (names_both_sources(args)) throw wrong("add takes from or kind, not both");
  Adding adding{edit, args, new_name(args), source_of(edit.document, args)};
  one_step(edit.document, brings_assets(args), [&] { make_added(adding); }, [&] { finish_added(adding); });
  return adding.result;
}

// Copies of the objects, each with its children (the document names them apart: "Crate.001").
EditOutcome duplicate_all(SceneEdit& edit, const Json& args) {
  SceneDocument& document = edit.document;
  const std::vector<int> roots = roots_of(document, targets(document, args, false));
  std::vector<int> copies;
  EditOutcome result;
  const Step rest = [&] {
    Words ids;
    for (const int copy : copies) ids.push_back(id_of(document, copy));
    assign_each(edit, ids, words_of(args));
    result = outcome(edit, ids);
  };
  one_step(document, true, [&] { copies = document.duplicate(roots); }, rest);
  return result;
}

// Whether `parent` is one of the members or under one: a group cannot hold itself.
bool under_members(const SceneDesc& scene, int parent, const std::vector<int>& members) {
  if (parent == kNoIndex) return false;
  return std::find(members.begin(), members.end(), parent) != members.end() || inside_any(scene, parent, members);
}

// Whether the object is a prefab placement or under one: its world placement comes from the
// prefab's file, which only the editor's preview knows, so moving it to a new parent here could
// move it in the world.
bool under_prefab(const SceneDocument& document, int index) {
  for (int at = index; at != kNoIndex; at = document.node(at).parent) {
    if (document.node(at).props.count(props::kPrefabFile) > 0) return true;
  }
  return false;
}

void require_no_prefab_parent(const SceneDocument& document, int parent, const std::vector<int>& members) {
  const auto inherited = [&document](int member) { return under_prefab(document, document.node(member).parent); };
  if (under_prefab(document, parent) || std::any_of(members.begin(), members.end(), inherited))
    throw wrong("group cannot move objects under a prefab placement; reparent them in the editor");
}

// Where a group goes: under `parent`, else where its first member is.
int group_parent(const SceneDocument& document, const Json& args, const std::vector<int>& members) {
  const int parent = parent_of_call(document, args).value_or(document.node(members.front()).parent);
  if (under_members(document.desc(), parent, members)) throw wrong("a group cannot go under its members");
  require_no_prefab_parent(document, parent, members);
  return parent;
}

// Moves the members (ids) under the group (the first id), and says the group and them.
EditOutcome gather(SceneEdit& edit, const Words& group_and_members) {
  SceneDocument& document = edit.document;
  for (auto member = group_and_members.begin() + 1; member != group_and_members.end(); ++member)
    document.reparent(resolve(document, *member), resolve(document, group_and_members.front()));
  EditOutcome result = outcome(edit, group_and_members);
  result.objects.resize(1);  // the group is what is selected
  return result;
}

// The group, then its members (those under another member go along with it).
EditOutcome group_all(SceneEdit& edit, const Json& args) {
  SceneDocument& document = edit.document;
  const std::string name = new_name(args);
  const std::vector<int> roots = roots_of(document, targets(document, args, true));
  const int parent = group_parent(document, args, roots);
  Words ids{std::string()};  // the group's, once made
  for (const int root : roots) ids.push_back(id_of(document, root));
  EditOutcome result;
  const Step make = [&] { ids.front() = id_of(document, document.add_empty(document.unique_name(name), parent)); };
  one_step(document, false, make, [&] { result = gather(edit, ids); });
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
