// Part of EditorSession: the objects the menus create, and what they start as.

#include "editor/editor_session.hpp"

#include <utility>

#include "core/log.hpp"
#include "editor/new_object_kinds.hpp"

namespace redgem {

using new_objects::Kind;

ObjectId EditorSession::place_new(int node, Vec3 position) {
  Transform local = document_.node(node).local;
  local.position = position;
  document_.set_local(node, local, false);  // part of the creation's undo step
  refresh_preview();
  selection_.set(node);
  return node;
}

ObjectId EditorSession::place_created(const std::function<int()>& add, Vec3 position) {
  if (!editing()) return kNoObject;
  return place_new(add(), position);
}

// One of the GameObject menu's objects, as new_objects makes it, placed at `position`.
ObjectId EditorSession::create_kind(Kind kind, Vec3 position) {
  return place_created(
      [this, kind] { return new_objects::add_object(document_, kind, new_objects::default_name(kind)); }, position);
}

ObjectId EditorSession::create_empty(Vec3 position) { return create_kind(Kind::Empty, position); }

ObjectId EditorSession::create_primitive(PrimitiveShape shape, Vec3 position) {
  constexpr Kind kKinds[] = {Kind::Cube, Kind::Sphere, Kind::Plane, Kind::Cylinder, Kind::Capsule};
  return create_kind(kKinds[static_cast<int>(shape)], position);
}

ObjectId EditorSession::create_terrain(Vec3 position) {
  // (Wherever the camera looks from, on the ground.)
  return create_kind(Kind::Terrain, {position.x, new_objects::kGroundLevel, position.z});
}

ObjectId EditorSession::create_light(LightType type, Vec3 position) {
  const Kind kind = type == LightType::Directional ? Kind::Sun
                    : type == LightType::Spot      ? Kind::SpotLight
                                                   : Kind::PointLight;
  return create_kind(kind, position);
}

ObjectId EditorSession::create_camera(Vec3 position) { return create_kind(Kind::Camera, position); }

ObjectId EditorSession::create_environment() {
  if (!editing()) return kNoObject;
  const int existing = new_objects::environment_of(document_);
  if (existing == kNoIndex) return create_kind(Kind::Environment, Vec3::zero());
  selection_.set(existing);
  return existing;
}

ObjectId EditorSession::import_model(const std::filesystem::path& path, Vec3 position, std::string& error,
                                     ObjectId parent) {
  if (!editing()) {
    error = "stop the game to edit the scene";
    return kNoObject;
  }
  const int node = document_.import_model(path, error, parent);
  if (node == kNoIndex) return kNoObject;
  log::info("imported " + path.filename().string());
  if (parent == kNoObject) return place_new(node, position);
  refresh_preview();  // the parent's world placement is needed below
  Transform world = preview_.world_transform(node);
  world.position = position;
  return place_new(node, relative_to(preview_.world_transform(parent), world).position);
}

}  // namespace redgem
