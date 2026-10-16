// GameObject natives.

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/string.h>

#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/script_host.hpp"

namespace redgem {
namespace {

using namespace binding;

mrb_value property_value(mrb_state* mrb, const PropertyValue& value) {
  if (const auto* flag = std::get_if<bool>(&value)) return mrb_bool_value(*flag);
  if (const auto* number = std::get_if<double>(&value)) return mrb_float_value(mrb, *number);
  return text_value(mrb, std::get<std::string>(value));
}

// Still there, and not about to be destroyed.
bool is_staying(const Scene& objects, ObjectId id) { return id != kNoObject && !objects.at(id).pending_destroy; }

mrb_value game_object_alive(mrb_state* mrb, mrb_value self) {
  return mrb_bool_value(is_staying(scene(mrb), scene(mrb).resolve(wrapped_handle(mrb, self))));
}

// Destroying what is already gone does nothing (request_destroy ignores kNoObject).
mrb_value game_object_destroy(mrb_state* mrb, mrb_value self) {
  scene(mrb).request_destroy(scene(mrb).resolve(wrapped_handle(mrb, self)));
  return mrb_nil_value();
}

mrb_value game_object_props(mrb_state* mrb, mrb_value self) {
  const GameObject& object = alive_object(mrb, self);
  mrb_value hash = mrb_hash_new(mrb);
  for (const auto& [key, value] : object.props) set_entry(mrb, hash, key.c_str(), property_value(mrb, value));
  return hash;
}

mrb_value game_object_visible(mrb_state* mrb, mrb_value self) {
  return mrb_bool_value(alive_object(mrb, self).visible);
}

mrb_value game_object_set_visible(mrb_state* mrb, mrb_value self) {
  const bool visible = bool_arg(mrb);
  alive_object(mrb, self).visible = visible;
  return mrb_bool_value(visible);
}

mrb_value game_object_tint(mrb_state* mrb, mrb_value self) { return vec3_value(mrb, alive_object(mrb, self).tint); }

mrb_value game_object_set_tint(mrb_state* mrb, mrb_value self) {
  const mrb_value color = single_arg(mrb);
  alive_object(mrb, self).tint = to_vec3(mrb, color);
  return color;
}

mrb_value game_object_animation_offset(mrb_state* mrb, mrb_value self) {
  return mrb_float_value(mrb, alive_object(mrb, self).animation_offset);
}

mrb_value game_object_set_animation_offset(mrb_state* mrb, mrb_value self) {
  const mrb_float seconds = float_arg(mrb);
  alive_object(mrb, self).animation_offset = static_cast<float>(seconds);
  return mrb_float_value(mrb, seconds);
}

mrb_value game_object_parent(mrb_state* mrb, mrb_value self) {
  return object_or_nil(mrb, alive_object(mrb, self).parent);
}

mrb_value game_object_children(mrb_state* mrb, mrb_value self) {
  const GameObject& object = alive_object(mrb, self);
  mrb_value children = mrb_ary_new(mrb);
  for (const ObjectId child : object.children) {
    if (scene(mrb).is_alive(child)) mrb_ary_push(mrb, children, object_or_nil(mrb, child));
  }
  return children;
}

// What the physics world takes the object for.
BodyType body_type_of(mrb_state* mrb, mrb_value self) { return context(mrb).physics->body_type(alive_id(mrb, self)); }

mrb_value game_object_has_character(mrb_state* mrb, mrb_value self) {
  return mrb_bool_value(body_type_of(mrb, self) == BodyType::Character);
}

mrb_value game_object_has_vehicle(mrb_state* mrb, mrb_value self) {
  return mrb_bool_value(body_type_of(mrb, self) == BodyType::Vehicle);
}

// What a Rigidbody can be asked about: a body that forces or scripts move.
bool is_rigid_body(BodyType type) {
  return type == BodyType::Dynamic || type == BodyType::Kinematic || type == BodyType::Vehicle;
}

mrb_value game_object_has_rigidbody(mrb_state* mrb, mrb_value self) {
  return mrb_bool_value(is_rigid_body(body_type_of(mrb, self)));
}

mrb_value game_object_add_component(mrb_state* mrb, mrb_value self) {
  const mrb_value klass = single_arg(mrb);
  return script_host(mrb).add_behaviour(alive_id(mrb, self), klass);
}

// Loads the game script a missing constant should come from (see ruby/engine/kernel.rb).
mrb_value kernel_require_class(mrb_state* mrb, mrb_value) {
  return mrb_bool_value(script_host(mrb).require_class(name_arg(mrb)));
}

void install_identity_bindings(RClass* game_object, mrb_state* mrb) {
  mrb_define_method(mrb, game_object, "alive?", game_object_alive, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "destroy", game_object_destroy, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "props", game_object_props, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "parent", game_object_parent, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "children", game_object_children, MRB_ARGS_NONE());
}

void install_look_bindings(RClass* game_object, mrb_state* mrb) {
  mrb_define_method(mrb, game_object, "visible", game_object_visible, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "visible=", game_object_set_visible, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, game_object, "tint", game_object_tint, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "tint=", game_object_set_tint, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, game_object, "animation_offset", game_object_animation_offset, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "animation_offset=", game_object_set_animation_offset, MRB_ARGS_REQ(1));
}

void install_component_bindings(RClass* game_object, mrb_state* mrb) {
  mrb_define_method(mrb, game_object, "__add_component", game_object_add_component, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, mrb->kernel_module, "__require_class", kernel_require_class, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, game_object, "__has_character?", game_object_has_character, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "__has_rigidbody?", game_object_has_rigidbody, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "__has_vehicle?", game_object_has_vehicle, MRB_ARGS_NONE());
}

}  // namespace

void install_game_object_bindings(mrb_state* mrb) {
  RClass* game_object = mrb_define_class(mrb, kGameObjectClass, mrb->object_class);
  install_identity_bindings(game_object, mrb);
  install_look_bindings(game_object, mrb);
  install_component_bindings(game_object, mrb);
}

}  // namespace redgem
