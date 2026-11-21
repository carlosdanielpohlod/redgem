#include "editor/prefab_instances.hpp"

#include <exception>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "core/safe_path.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "scene/prefab_import.hpp"
#include "scene/props.hpp"

namespace redgem::prefab_instances {
namespace {
namespace fs = std::filesystem;
using prefabs::Overrides;
using prefabs::PartOverride;
constexpr const char* kRootPath = "";
constexpr const char* kUnsetText = "unset";
constexpr const char* kSetText = "set";
constexpr const char* kTrueText = "true";
// The placement's own properties that say what it is, not what it changes in its root.
constexpr const char* kPlacementKeys[] = {props::kPrefabFile, props::kPrefabOverrides, props::kPrefabParent,
                                          props::kPrefab};

void require(bool condition, const std::string& reason) {
  if (!condition) throw std::runtime_error(reason);
}
// Runs `work`, turning what it throws into `error`.
bool guarded(const std::function<bool()>& work, std::string& error) {
  error.clear();
  try {
    return work();
  } catch (const std::exception& problem) {
    error = problem.what();
    return false;
  }
}
template <typename T>
const T& held(const std::optional<T>& value, const char* what) {
  if (!value) throw std::runtime_error(what);
  return *value;
}
bool placement_key(const std::string& key) {
  for (const char* name : kPlacementKeys) {
    if (key == name) return true;
  }
  return false;
}
PropertyMap root_props(const NodeDesc& placement) {
  PropertyMap result;
  for (const auto& [key, value] : placement.props) {
    if (!placement_key(key)) result.emplace(key, value);
  }
  return result;
}
void require_placement(const SceneDocument& document, int placement) {
  require(is_placement(document, placement), "the object is not a prefab placement");
}
int by_identity(const SceneDesc& scene, const std::string& identity) {
  for (std::size_t i = 0; i < scene.nodes.size(); ++i) {
    if (scene.nodes[i].stable_id == identity) return static_cast<int>(i);
  }
  return kNoIndex;
}
std::string identity_of(const SceneDocument& document, int node) { return document.node(node).stable_id; }
int found_identity(const SceneDocument& document, const std::string& identity) {
  const int node = by_identity(document.desc(), identity);
  require(node != kNoIndex, "an object went missing while changing the placement");
  return node;
}

std::string vector_text(Vec3 v) {
  std::ostringstream text;
  text << v.x << ',' << v.y << ',' << v.z;
  return text.str();
}
std::string property_text(const PropertyValue& value) {
  if (const auto* flag = std::get_if<bool>(&value)) return *flag ? kTrueText : "false";
  if (const auto* number = std::get_if<double>(&value)) {
    std::ostringstream text;
    text << *number;
    return text.str();
  }
  return std::get<std::string>(value);
}
std::string prop_value(const PartOverride& part, const std::string& key) {
  const auto set = part.props.find(key);
  return set == part.props.end() ? kUnsetText : property_text(set->second);
}
std::string field_value(const PartOverride& part, const std::string& name) {
  const std::string prefix = prefabs::field::kPropPrefix;
  if (name.rfind(prefix, 0) == 0) return prop_value(part, name.substr(prefix.size()));
  if (name == prefabs::field::kPosition) return vector_text(held(part.position, name.c_str()));
  if (name == prefabs::field::kScale) return vector_text(held(part.scale, name.c_str()));
  if (name == prefabs::field::kRotation) return vector_text(quat_to_euler_degrees(held(part.rotation, name.c_str())));
  return name == prefabs::field::kRemoved ? kTrueText : kSetText;
}

void write(SceneDocument& document, int placement, const Overrides& overrides, bool record) {
  if (overrides.empty()) return document.remove_prop(placement, props::kPrefabOverrides, record);
  document.set_prop(placement, props::kPrefabOverrides, prefabs::overrides_text(overrides), record);
}

fs::path prefab_path(const fs::path& game, const std::string& uri) {
  require(safe_asset_name(uri), "unsafe prefab file path");
  require(!through_link(game, fs::u8path(uri)), "prefab file uses a symbolic link");
  const auto found = file_inside(game, fs::u8path(uri));
  return held(found, ("prefab file is missing or outside the game: " + uri).c_str());
}
fs::path placement_file(const SceneDocument& document, int placement, const fs::path& game) {
  return prefab_path(fs::weakly_canonical(game), get_string(document.node(placement).props, props::kPrefabFile).value_or(""));
}

// Where a part path lands in a prefab file: a node of it, and what is left of the path when that
// node is itself a placement (the part is then in its file).
struct Spot {
  int node = kNoIndex;
  std::string rest;
};
Spot locate(const SceneDesc& file, const std::string& path) {
  std::string rest = path;
  while (!rest.empty()) {
    auto [step, after] = prefabs::first_step(rest);
    const int node = by_identity(file, step);
    require(node != kNoIndex, "the prefab file has no part " + path);
    if (after.empty() || file.nodes[node].props.count(props::kPrefabFile) != 0) return {node, after};
    rest = after;
  }
  return {0, {}};
}

// Writes overrides into nodes copied from a prefab file: the file itself (offset 0), or a copy of
// its nodes placed in a scene from `offset` on.
class PartWriter {
 public:
  PartWriter(SceneDocument& document, const SceneDesc& file, int offset)
      : document_(document), file_(file), offset_(offset) {}
  void write_all(const Overrides& overrides) {
    for (const auto& [path, change] : overrides) write_part(path, change);
  }
  Spot spot(const std::string& path) const {
    auto found = locate(file_, path);
    found.node += offset_;
    return found;
  }
  void remove_marked() {
    for (const auto& identity : removed_) document_.remove(found_identity(document_, identity));
  }

 private:
  void write_part(const std::string& path, const PartOverride& change) {
    const auto where = spot(path);
    if (!where.rest.empty()) return override_nested(where.node, where.rest, change);
    change_node(where.node, change);
    if (change.removed) removed_.push_back(identity_of(document_, where.node));
  }
  void change_node(int index, const PartOverride& change) {
    NodeDesc changed = document_.node(index);
    prefabs::apply_override(changed, change);
    document_.set_local(index, changed.local, false);
    for (const auto& key : change.unset) document_.remove_prop(index, key, false);
    for (const auto& [key, value] : change.props) document_.set_prop(index, key, value, false);
    if (changed.light) document_.set_light(index, *changed.light, false);
    if (changed.camera) document_.set_camera(index, *changed.camera, false);
  }
  void override_nested(int index, const std::string& rest, const PartOverride& change) {
    auto overrides = prefabs::read_overrides(document_.node(index).props);
    prefabs::merge_override(overrides[rest], change);
    write(document_, index, overrides, false);
  }
  SceneDocument& document_;
  const SceneDesc& file_;
  int offset_;
  std::vector<std::string> removed_;
};

// The part an added child hangs from, as a spot in the file.
Spot hang_spot(const PartWriter& writer, const NodeDesc& child) {
  const auto part = get_string(child.props, props::kPrefabParent);
  return part ? writer.spot(*part) : writer.spot(kRootPath);
}
void hang_as(SceneDocument& document, int node, const std::string& rest) {
  if (rest.empty()) return document.remove_prop(node, props::kPrefabParent, false);
  document.set_prop(node, props::kPrefabParent, rest, false);
}
void set_props(SceneDocument& document, int node, const PropertyMap& values) {
  for (const auto& [key, value] : values) document.set_prop(node, key, value, false);
}

// --- apply ---
void copy_child(SceneDocument& scene, int child, SceneDocument& file, const PartWriter& writer) {
  std::string error;
  const auto bytes = scene.copy_nodes({child}, error);
  const auto where = hang_spot(writer, scene.node(child));
  const auto pasted = file.paste_nodes(held(bytes, error.c_str()), where.node, error);
  require(!pasted.empty(), error);
  file.set_local(pasted.front(), scene.node(child).local, false);
  hang_as(file, pasted.front(), where.rest);
}
void write_file(SceneDocument& scene, int placement, SceneDocument& file) {
  const SceneDesc layout = file.desc();
  PartWriter writer(file, layout, 0);
  writer.write_all(overrides_of(scene, placement));
  set_props(file, 0, root_props(scene.node(placement)));
  for (const int child : scene.node(placement).children) copy_child(scene, child, file, writer);
  writer.remove_marked();
  std::string error;
  require(file.save(error), error);
}
std::string clear_overrides(SceneDocument& scene, int placement) {
  for (const auto& entry : root_props(scene.node(placement))) scene.remove_prop(placement, entry.first, false);
  scene.remove_prop(placement, props::kPrefabOverrides, false);
  return {};
}
std::string clear_applied(SceneDocument& scene, int placement) {
  const auto children = scene.node(placement).children;
  clear_overrides(scene, placement);
  if (!children.empty()) scene.remove(children);
  return {};
}
bool apply_to_file(SceneDocument& scene, int placement, const fs::path& game, std::string& error) {
  auto opened = SceneDocument::open(placement_file(scene, placement, game));
  if (const auto* problem = std::get_if<std::string>(&opened)) throw std::runtime_error(*problem);
  write_file(scene, placement, std::get<SceneDocument>(opened));
  return scene.edit_nodes([&] { return clear_applied(scene, placement); }, error);
}

// --- unpack ---
struct Unpacking {
  NodeDesc placement;
  SceneDesc file;
  int first = kNoIndex;  // the copied file's first node in the scene
};
void move_children(SceneDocument& scene, const Unpacking& unpacking, const PartWriter& writer) {
  std::vector<std::string> children;
  children.reserve(unpacking.placement.children.size());
  for (const int child : unpacking.placement.children) children.push_back(identity_of(scene, child));
  for (const auto& identity : children) {
    const int child = found_identity(scene, identity);
    const auto where = hang_spot(writer, scene.node(child));
    const Transform local = scene.node(child).local;
    hang_as(scene, scene.reparent(child, where.node, false, &local), where.rest);
  }
}
void place_root(SceneDocument& scene, const Unpacking& unpacking) {
  const int root = unpacking.first;
  scene.set_local(root, prefabs::checked_placement(combine(unpacking.placement.local, unpacking.file.nodes[0].local)),
                  false);
  scene.remove_prop(root, props::kPrefab, false);
  set_props(scene, root, root_props(unpacking.placement));
  for (const char* key : {props::kPrefab, props::kPrefabParent}) {
    const auto value = unpacking.placement.props.find(key);
    if (value != unpacking.placement.props.end()) scene.set_prop(root, key, value->second, false);
  }
}
std::string unpack_nodes(SceneDocument& scene, const Unpacking& unpacking) {
  PartWriter writer(scene, unpacking.file, unpacking.first);
  const std::string root = identity_of(scene, unpacking.first);
  writer.write_all(prefabs::read_overrides(unpacking.placement.props));
  place_root(scene, unpacking);
  move_children(scene, unpacking, writer);
  writer.remove_marked();
  scene.remove(found_identity(scene, unpacking.placement.stable_id));
  scene.set_name(found_identity(scene, root), unpacking.placement.name);
  return {};
}
std::string unpack_or_reason(SceneDocument& scene, const Unpacking& unpacking) {
  try {
    return unpack_nodes(scene, unpacking);
  } catch (const std::exception& problem) {
    return problem.what();
  }
}
Unpacking paste_file(SceneDocument& scene, int placement, const fs::path& game) {
  const auto path = placement_file(scene, placement, game);
  auto loaded = load_gltf_scene(path.u8string());
  require(loaded.ok, loaded.error);
  const auto bytes = read_file_up_to(path, prefabs::Limits::kDefaultInputBytes);
  Unpacking unpacking{scene.node(placement), std::move(loaded.scene), scene.node_count()};
  std::string error;
  const auto pasted = scene.paste_nodes(held(bytes, "cannot read the prefab file"), scene.node(placement).parent, error);
  require(pasted.size() == 1 && pasted.front() == unpacking.first, error.empty() ? "cannot copy the prefab" : error);
  return unpacking;
}
int unpack_placement(SceneDocument& scene, int placement, const fs::path& game, std::string& error) {
  prefabs::read_overrides(scene.node(placement).props);  // refuses malformed overrides before any change
  const auto unpacking = paste_file(scene, placement, game);
  const std::string root = identity_of(scene, unpacking.first);
  if (!scene.edit_nodes([&] { return unpack_or_reason(scene, unpacking); }, error)) {
    scene.undo();  // takes the copy back out
    return kNoIndex;
  }
  scene.join_last_steps();
  return found_identity(scene, root);
}
// --- revert ---
bool revert_root_prop(SceneDocument& document, int placement, const std::string& name) {
  const std::string prefix = prefabs::field::kPropPrefix;
  require(name.rfind(prefix, 0) == 0, "the placement's own fields are its properties: props.<name>");
  const auto key = name.substr(prefix.size());
  require(root_props(document.node(placement)).count(key) != 0, "the placement does not override " + key);
  document.remove_prop(placement, key);
  return true;
}
bool revert_part(SceneDocument& document, int placement, const std::string& path, const std::string& name) {
  auto overrides = overrides_of(document, placement);
  require(prefabs::revert_field(overrides, path, name), "nothing to revert at " + path + " " + name);
  write(document, placement, overrides, true);
  return true;
}
bool revert_any(SceneDocument& document, int placement, const std::string& path, const std::string& name,
                std::string& error) {
  if (!path.empty()) return revert_part(document, placement, path, name);
  if (!name.empty()) return revert_root_prop(document, placement, name);
  return document.edit_nodes([&] { return clear_overrides(document, placement); }, error);
}
}  // namespace

bool is_placement(const SceneDocument& document, int node) {
  return document.has_node(node) && get_string(document.node(node).props, props::kPrefabFile).has_value();
}

Overrides overrides_of(const SceneDocument& document, int placement) {
  if (!is_placement(document, placement)) return {};
  return prefabs::read_overrides(document.node(placement).props);
}

std::vector<Field> fields(const SceneDocument& document, int placement) {
  std::vector<Field> result;
  if (!is_placement(document, placement)) return result;
  for (const auto& [key, value] : root_props(document.node(placement)))
    result.push_back({kRootPath, prefabs::field::kPropPrefix + key, property_text(value)});
  for (const auto& [path, part] : overrides_of(document, placement)) {
    for (const auto& name : prefabs::overridden_fields(part)) result.push_back({path, name, field_value(part, name)});
  }
  return result;
}

bool set_override(SceneDocument& document, int placement, const std::string& path, const PartOverride& change,
                  std::string& error) {
  return guarded(
      [&] {
        require_placement(document, placement);
        require(!path.empty(), "name an inherited part; the placement's own fields are edited on it");
        auto overrides = overrides_of(document, placement);
        prefabs::merge_override(overrides[path], change);
        if (overrides[path].empty()) overrides.erase(path);
        write(document, placement, overrides, true);
        return true;
      },
      error);
}

bool revert(SceneDocument& document, int placement, const std::string& path, const std::string& name,
            std::string& error) {
  return guarded(
      [&] {
        require_placement(document, placement);
        return revert_any(document, placement, path, name, error);
      },
      error);
}

bool apply(SceneDocument& document, int placement, const fs::path& game_dir, std::string& error) {
  return guarded(
      [&] {
        require_placement(document, placement);
        return apply_to_file(document, placement, game_dir, error);
      },
      error);
}

int unpack(SceneDocument& document, int placement, const fs::path& game_dir, std::string& error) {
  int root = kNoIndex;
  guarded(
      [&] {
        require_placement(document, placement);
        root = unpack_placement(document, placement, game_dir, error);
        return root != kNoIndex;
      },
      error);
  return root;
}

}  // namespace redgem::prefab_instances
