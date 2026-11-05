#include "editor/editor_session.hpp"

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
}  // namespace redgem
