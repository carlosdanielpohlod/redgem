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
  if (selection_ >= document_.node_count()) selection_ = kNoObject;
}

void EditorSession::refresh_node(ObjectId node) {
  if (!document_.has_node(node)) return refresh_preview();
  preview_.refresh_from_desc(document_.desc(), node);
}

void EditorSession::select(ObjectId id) { selection_ = document_.has_node(id) ? id : kNoObject; }

void EditorSession::select_by_name(const std::string& name) { select(document_.desc().find_node(name)); }

std::string EditorSession::selected_name() const {
  return selection_ != kNoObject ? document_.node(selection_).name : "";
}

void EditorSession::refresh_and_reselect(const std::string& name) {
  refresh_preview();
  select_by_name(name);  // indices may have shifted; names are unique
}

void EditorSession::duplicate_selection() {
  if (!can_edit(selection_)) return;
  selection_ = document_.duplicate(selection_);
  refresh_preview();
}

void EditorSession::delete_selection() {
  if (!can_edit(selection_)) return;
  document_.remove(selection_);
  selection_ = kNoObject;
  refresh_preview();
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
  const bool child_selected = selection_ == child;
  const std::string selected = selected_name();
  const int moved = document_.reparent(child, parent);
  refresh_preview();
  if (child_selected) {
    selection_ = moved;
  } else {
    select_by_name(selected);  // indices of other objects may shift
  }
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
  const std::string selected = selected_name();
  if (document_.undo()) refresh_and_reselect(selected);
}

void EditorSession::redo() {
  if (!editing()) return;
  const std::string selected = selected_name();
  if (document_.redo()) refresh_and_reselect(selected);
}

}  // namespace redgem
