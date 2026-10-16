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

// What a node shows: its mesh and skeleton, its camera, its light.
void copy_parts(const NodeDesc& node, const SceneDesc& desc, GameObject& object) {
  object.mesh = node.mesh;
  object.skin = node.skin;
  if (is_index_of(node.skin, desc.skins.size())) {
    object.joints = desc.skins[static_cast<std::size_t>(node.skin)].joints;
  }
  object.camera = node.camera;
  object.light = node.light;
}

// What a node's custom properties say about the object itself.
void apply_props(const PropertyMap& props, GameObject& object) {
  object.props = props;
  object.tag = get_string(props, props::kTag).value_or("");
  object.visible = get_bool(props, props::kVisible).value_or(true);
  object.prefab = get_bool(props, props::kPrefab).value_or(false);
  if (const auto tint = get_string(props, props::kTint)) {
    object.tint = parse_hex_color(*tint).value_or(Vec3::one());
  }
  if (const auto from = get_number(props, props::kVisibleFrom)) object.visible_from = static_cast<float>(*from);
  if (const auto to = get_number(props, props::kVisibleTo)) object.visible_to = static_cast<float>(*to);
}

GameObject object_of(const NodeDesc& node, const SceneDesc& desc, ObjectId id) {
  GameObject object = placed_object(node, id);
  copy_parts(node, desc, object);
  apply_props(node.props, object);
  return object;
}

}  // namespace

Scene Scene::from_desc(const SceneDesc& desc) {
  Scene scene;
  scene.objects_.reserve(desc.nodes.size());
  for (std::size_t i = 0; i < desc.nodes.size(); ++i) {
    GameObject object = object_of(desc.nodes[i], desc, static_cast<ObjectId>(i));
    object.serial = new_object_serial();
    // Parents come first, so a prefab's descendants inherit the flag.
    if (scene.is_prefab(object.parent)) object.prefab = true;
    scene.objects_.push_back(std::move(object));
  }
  scene.loaded_count_ = scene.objects_.size();
  return scene;
}

}  // namespace redgem
