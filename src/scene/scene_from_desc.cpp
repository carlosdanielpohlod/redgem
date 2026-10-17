// A scene's objects, made out of what a file describes.

#include "scene/props.hpp"
#include "scene/scene.hpp"

namespace redgem {
namespace {

// Where a node is in the tree.
GameObject placed_object(const NodeDesc& node, ObjectId id) {
  GameObject object;
  object.id = id;
  object.name = node.name;
  object.parent = node.parent;
  object.children = node.children;
  object.local = node.local;
  return object;
}

// What a node shows: its mesh and skeleton, its camera, its light. (The skeleton's bones are
// found once every object is in the scene: see Scene::joints_of.)
void copy_parts(const NodeDesc& node, GameObject& object) {
  object.mesh = node.mesh;
  object.skin = node.skin;
  object.camera = node.camera;
  object.light = node.light;
}

// What a node's custom properties say about the object itself (its skeleton is known by then).
void apply_props(const PropertyMap& props, GameObject& object) {
  object.props = props;
  object.tag = get_string(props, props::kTag).value_or("");
  object.visible = get_bool(props, props::kVisible).value_or(true);
  object.prefab = get_bool(props, props::kPrefab).value_or(false);
  object.cast_shadow = get_bool(props, props::kCastShadow).value_or(true);
  object.sways = get_bool(props, props::kSway).value_or(object.skin == kNoIndex);
  if (const auto tint = get_string(props, props::kTint)) {
    object.tint = parse_hex_color(*tint).value_or(Vec3::one());
  }
  if (const auto from = get_number(props, props::kVisibleFrom)) object.visible_from = static_cast<float>(*from);
  if (const auto to = get_number(props, props::kVisibleTo)) object.visible_to = static_cast<float>(*to);
}

GameObject object_of(const NodeDesc& node, ObjectId id) {
  GameObject object = placed_object(node, id);
  copy_parts(node, object);
  apply_props(node.props, object);
  return object;
}

}  // namespace

Scene Scene::from_desc(const SceneDesc& desc) {
  Scene scene;
  scene.objects_.reserve(desc.nodes.size());
  for (std::size_t i = 0; i < desc.nodes.size(); ++i) {
    GameObject object = object_of(desc.nodes[i], static_cast<ObjectId>(i));
    object.serial = new_object_serial();
    // Parents come first, so a prefab's descendants inherit the flag.
    if (scene.is_prefab(object.parent)) object.prefab = true;
    scene.objects_.push_back(std::move(object));
  }
  scene.loaded_count_ = scene.objects_.size();
  for (std::size_t i = 0; i < desc.nodes.size(); ++i) scene.objects_[i].joints = scene.joints_of(desc.nodes[i], desc);
  return scene;
}

std::vector<ObjectHandle> Scene::joints_of(const NodeDesc& node, const SceneDesc& desc) const {
  std::vector<ObjectHandle> bones;
  if (!is_index_of(node.skin, desc.skins.size())) return bones;
  for (const int joint : desc.skins[static_cast<std::size_t>(node.skin)].joints) bones.push_back(handle(joint));
  return bones;
}

void Scene::refresh_from_desc(const SceneDesc& desc, ObjectId id) {
  const NodeDesc& node = desc.nodes.at(static_cast<std::size_t>(id));
  GameObject object = object_of(node, id);
  object.serial = at(id).serial;
  object.joints = joints_of(node, desc);
  if (is_prefab(object.parent)) object.prefab = true;
  objects_[id] = std::move(object);
}

}  // namespace redgem
