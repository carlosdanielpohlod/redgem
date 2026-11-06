#include "editor/editor_session.hpp"
#include "editor/prefab_files.hpp"

#include <exception>
#include <stdexcept>
#include <utility>
#include "scene/prefab_import.hpp"

namespace redgem {
const SceneDesc& EditorSession::preview_desc() const {
  const auto* resolved = prefab_preview_.resolved();
  return resolved ? *resolved : document_.desc();
}
ObjectId EditorSession::authored_object(ObjectId id) const {
  if (!preview_.is_alive(id)) return kNoObject;
  while (!document_.has_node(id)) {
    id = preview_.at(id).parent;
    if (id == kNoObject) return kNoObject;
  }
  return id;
}
Transform EditorSession::placement_world(ObjectId id) const {
  const auto& node = document_.node(id);
  return node.parent == kNoIndex ? node.local : combine(preview_.world_transform(node.parent), node.local);
}
void EditorSession::sync_preview_assets(const std::vector<int>& changed_meshes) {
  if (!prefab_preview_.sync_assets(document_.desc(), changed_meshes)) refresh_preview();
}
std::vector<Transform> EditorSession::placement_worlds(const std::vector<int>& objects) const {
  std::vector<Transform> worlds;
  worlds.reserve(objects.size());
  for (const auto object : objects) worlds.push_back(placement_world(object));
  return worlds;
}
std::vector<Transform> EditorSession::locals_under(const std::vector<int>& objects, const Transform& parent) const {
  std::vector<Transform> locals;
  locals.reserve(objects.size());
  for (const auto& world : placement_worlds(objects)) locals.push_back(relative_to(parent, world));
  return locals;
}
void EditorSession::move_roots(const std::vector<std::string>& names, const std::string& parent,
                               const std::vector<Transform>& locals) {
  for (std::size_t i = 0; i < names.size(); ++i) {
    const int under = parent.empty() ? kNoIndex : document_.desc().find_node(parent);
    document_.reparent(document_.desc().find_node(names[i]), under, false, &locals[i]);
  }
}
void EditorSession::place_pasted(const std::vector<int>& objects, const std::string& parent,
                                 const Transform& parent_world) {
  const auto names = names_of(objects);
  std::vector<Transform> locals;
  locals.reserve(objects.size());
  for (const auto id : objects) locals.push_back(relative_to(parent_world, document_.node(id).local));
  move_roots(names, parent, locals);
  refresh_and_reselect(names);
}
namespace {
bool invertible(const Transform& parent) { return parent.scale.x != 0 && parent.scale.y != 0 && parent.scale.z != 0; }
Transform reference_local(const Scene& preview, int parent, const Transform& world) {
  if (parent == kNoIndex) return prefabs::checked_placement(world);
  const auto under = preview.world_transform(parent);
  if (!invertible(under)) throw std::runtime_error("cannot place a prefab under a parent with zero scale");
  return prefabs::checked_placement(relative_to(under, world));
}
int add_reference(SceneDocument& document, const std::string& file, const Transform& local, int parent) {
  const int id = document.add_empty(std::filesystem::u8path(file).stem().u8string(), parent);
  document.set_local(id, local, false);
  document.set_prop(id, props::kPrefabFile, file, false);
  return id;
}
struct PrefabPlacement {
  int object = kNoIndex;
  PrefabPreview preview;
  Scene scene;

  std::string prepare(SceneDocument& document, const std::filesystem::path& game, const std::string& file,
                      const Transform& local, int parent) {
    object = add_reference(document, file, local, parent);
    preview.rebuild(document.desc(), game);
    if (!preview.error().empty()) return preview.error();
    scene = Scene::from_desc(*preview.resolved());
    return {};
  }
};
}  // namespace
ObjectId EditorSession::place_prefab(const std::string& file, Vec3 position, std::string& error, ObjectId parent) {
  error.clear();
  if (!can_place_under(parent)) {
    error = "stop the game and choose a valid parent to place a prefab";
    return kNoObject;
  }
  Transform world;
  world.position = position;
  try {
    return place_linked(file, world, parent, error);
  } catch (const std::exception& problem) {
    error = problem.what();
    return kNoObject;
  }
}
ObjectId EditorSession::place_linked(const std::string& file, const Transform& world, ObjectId parent,
                                     std::string& error) {
  const auto local = reference_local(preview_, parent, world);
  PrefabPlacement ready;
  const auto edit = [&] { return ready.prepare(document_, game_dir_, file, local, parent); };
  if (!document_.edit_nodes(edit, error)) return kNoObject;
  prefab_preview_.adopt(std::move(ready.preview));
  preview_ = std::move(ready.scene);
  select(ready.object);
  return ready.object;
}
bool EditorSession::export_prefab(const std::string& name, std::string& error) {
  if (!can_export_prefab()) {
    error = "stop the game and select one hierarchy to export as a prefab";
    return false;
  }
  const int root = selected_roots().front();
  return prefab_files::save(document_, root, placement_world(root), game_dir_, name, error);
}
}  // namespace redgem
