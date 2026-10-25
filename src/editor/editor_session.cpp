#include "editor/editor_session.hpp"

#include <algorithm>
#include <utility>

#include "editor/editor_logic.hpp"
#include "scene/props.hpp"
#include "script/script_naming.hpp"

namespace redgem {
namespace {

constexpr float kTerrainRayReach = 2000.0f;  // meters a ray is followed looking for the ground

}  // namespace

EditorSession::EditorSession(std::filesystem::path game_dir, SceneDocument document)
    : game_dir_(std::move(game_dir)), document_(std::move(document)) {}

const Scene& EditorSession::scene() const { return runtime_ ? runtime_->scene() : preview_; }

void EditorSession::refresh_preview() {
  preview_ = Scene::from_desc(document_.desc());
  selection_.keep_below(document_.node_count());
}

void EditorSession::refresh_node(ObjectId node) {
  if (!document_.has_node(node)) return refresh_preview();
  preview_.refresh_from_desc(document_.desc(), node);
}

std::vector<int> EditorSession::selected_roots() const {
  return top_level_of(selection_.ids(), document_.desc().nodes);
}

void EditorSession::select(ObjectId id) { selection_.set(document_.has_node(id) ? id : kNoObject); }

std::vector<ObjectId> EditorSession::known(const std::vector<ObjectId>& ids) const {
  std::vector<ObjectId> objects;
  for (const ObjectId id : ids) {
    if (document_.has_node(id)) objects.push_back(id);
  }
  return objects;
}

void EditorSession::select_all(const std::vector<ObjectId>& ids) { selection_.set_all(known(ids)); }

void EditorSession::add_to_selection(const std::vector<ObjectId>& ids) { selection_.add_all(known(ids)); }

void EditorSession::click_select(ObjectId id, ClickMode mode, const std::vector<ObjectId>& order) {
  if (!document_.has_node(id)) return select(kNoObject);
  selection_.click(id, mode, order);
}

void EditorSession::select_by_name(const std::string& name) { select(document_.desc().find_node(name)); }

std::vector<std::string> EditorSession::selected_names() const {
  std::vector<std::string> names;
  names.reserve(selection_.size());
  for (const ObjectId id : selection_.ids()) names.push_back(document_.node(id).name);
  return names;
}

void EditorSession::select_by_names(const std::vector<std::string>& names) {
  std::vector<ObjectId> ids;
  ids.reserve(names.size());
  for (const std::string& name : names) ids.push_back(document_.desc().find_node(name));
  select_all(ids);
}

void EditorSession::refresh_and_reselect(const std::vector<std::string>& names) {
  refresh_preview();
  select_by_names(names);  // indices may have shifted; names are unique
}

void EditorSession::duplicate_selection() {
  if (!can_edit_selection()) return;
  const std::vector<int> copies = document_.duplicate(selected_roots());
  refresh_preview();
  select_all(copies);
}

void EditorSession::delete_selection() {
  if (!can_edit_selection()) return;
  document_.remove(selected_roots());
  selection_.clear();
  refresh_preview();
}

std::optional<std::vector<std::uint8_t>> EditorSession::copy_selection(std::string& error) const {
  if (selection_.empty()) return std::nullopt;
  return document_.copy_nodes(selected_roots(), error);
}

std::optional<std::vector<std::uint8_t>> EditorSession::cut_selection(std::string& error) {
  if (!can_edit_selection()) return std::nullopt;
  std::optional<std::vector<std::uint8_t>> copied = copy_selection(error);
  if (copied) delete_selection();  // (nothing is lost when it could not be copied)
  return copied;
}

bool EditorSession::paste(const std::vector<std::uint8_t>& copied, ObjectId parent, std::string& error) {
  if (!editing()) return false;
  const std::vector<int> pasted = document_.paste_nodes(copied, parent, error);
  if (pasted.empty()) return false;
  refresh_preview();
  select_all(pasted);
  return true;
}

ObjectId EditorSession::paste_parent() const {
  const ObjectId selected = selection();
  return selected == kNoObject ? kNoObject : document_.node(selected).parent;
}

void EditorSession::place_in_world(ObjectId id, const Transform& world) {
  if (!can_edit(id)) return;
  const int parent = document_.node(id).parent;
  const Transform local = parent == kNoIndex ? world : relative_to(preview_.world_transform(parent), world);
  document_.set_local(id, local, false);
  refresh_node(id);
}

bool EditorSession::is_terrain(ObjectId id) const { return document_.terrain_grid(id).has_value(); }

std::optional<Vec3> EditorSession::terrain_hit(ObjectId terrain, Vec3 origin, Vec3 direction) const {
  const auto grid = document_.terrain_grid(terrain);
  if (!grid) return std::nullopt;
  const Transform world = preview_.world_transform(terrain);
  const Vec3 local_origin = editor_logic::to_local(world, origin);
  const Vec3 local_direction = normalize(editor_logic::to_local(world, origin + direction) - local_origin);
  const MeshDesc& mesh = document_.desc().meshes[static_cast<std::size_t>(document_.node(terrain).mesh)];
  const PrimitiveDesc& ground = mesh.primitives.front();
  const auto hit = terrain_raycast(ground, *grid, local_origin, local_direction, kTerrainRayReach);
  if (!hit) return std::nullopt;
  return editor_logic::to_world(world, *hit);
}

bool EditorSession::sculpt_terrain(ObjectId terrain, Vec3 world_point, float radius, float amount, SculptMode mode) {
  if (!can_sculpt(terrain)) return false;
  // A scaled terrain: the brush is in meters of the world, the grid in its own.
  const TerrainBrush brush =
      editor_logic::local_brush(preview_.world_transform(terrain), world_point, radius, amount, mode);
  if (!document_.sculpt_terrain(terrain, brush)) return false;
  // Its vertices reach the renderer as a changed mesh; of its object, nothing else changed.
  refresh_node(terrain);
  return true;
}

void EditorSession::reparent(ObjectId child, ObjectId parent) {
  if (!can_edit(child)) return;
  // (Indices of other objects may shift; names are unique, and the moved one keeps its own.)
  const std::vector<std::string> selected = selected_names();
  document_.reparent(child, parent);
  refresh_and_reselect(selected);
}

void EditorSession::attach_script(ObjectId id, const std::string& file) {
  if (!can_edit(id)) return;
  std::vector<std::string> scripts =
      split_script_list(get_string(document_.node(id).props, props::kScript).value_or(""));
  if (std::find(scripts.begin(), scripts.end(), file) != scripts.end()) return;
  scripts.push_back(file);
  document_.set_prop(id, props::kScript, join_script_list(scripts));
  refresh_preview();
}

void EditorSession::undo() {
  if (!editing()) return;
  const std::vector<std::string> selected = selected_names();
  if (document_.undo()) refresh_and_reselect(selected);
}

void EditorSession::redo() {
  if (!editing()) return;
  const std::vector<std::string> selected = selected_names();
  if (document_.redo()) refresh_and_reselect(selected);
}

}  // namespace redgem
