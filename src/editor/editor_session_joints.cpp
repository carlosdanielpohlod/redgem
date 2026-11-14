// Part of EditorSession: the Joint component, which holds an object to another one or to the world.

#include "editor/editor_session.hpp"

#include "physics/joint_config.hpp"
#include "scene/props.hpp"

namespace redgem {
namespace {

// Whether a joint can hold to `id`: a node with an identity to be found by.
bool can_be_held_to(const SceneDocument& document, ObjectId id) {
  return document.has_node(id) && !document.node(id).stable_id.empty();
}

}  // namespace

void EditorSession::add_joint(ObjectId id, JointKind kind) {
  if (!editing() || !document_.has_node(id)) return;
  document_.checkpoint();
  for (const auto& [key, value] : inspector_logic::new_joint_props(kind)) document_.set_prop(id, key, value, false);
  document_.drop_checkpoint_if_unchanged();
  refresh_preview();
}

void EditorSession::remove_joint(ObjectId id) {
  if (!editing() || !document_.has_node(id)) return;
  document_.checkpoint();
  for (const std::string& key : joint_property_keys()) {
    if (document_.node(id).props.count(key) != 0) document_.remove_prop(id, key, false);
  }
  document_.drop_checkpoint_if_unchanged();
  refresh_preview();
}

void EditorSession::set_joint_partner(ObjectId id, ObjectId other) {
  if (!editing() || !document_.has_node(id)) return;
  if (can_be_held_to(document_, other)) {
    document_.set_prop(id, props::kJointTo, PropertyValue(document_.node(other).stable_id));
  } else if (document_.node(id).props.count(props::kJointTo) != 0) {
    document_.remove_prop(id, props::kJointTo);
  }
  refresh_preview();
}

}  // namespace redgem
