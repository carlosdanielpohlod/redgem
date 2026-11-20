// Head look-at natives: a character's bone turned toward a point (see Animator#look_at).

#include <mruby.h>

#include "scene/animation/head_aim.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/script_context.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr mrb_float kNoTurn = 0.0;
constexpr mrb_float kHalfTurnDegrees = 180.0;

HeadAims& head_aims(mrb_state* mrb) {
  return required(mrb, context(mrb).head_aims, "Animator#look_at is only available while a game runs");
}

// The bone given, which must be the character or under it.
ObjectId bone_arg(mrb_state* mrb, ObjectId character, mrb_value bone) {
  const ObjectId id = alive_id(mrb, bone);
  for (ObjectId part = id; part != kNoObject; part = scene(mrb).at(part).parent) {
    if (part == character) return id;
  }
  mrb_raise(mrb, E_ARGUMENT_ERROR, "the bone to turn must be part of the character");
}

// GameObject#__look_at(bone, point, max_degrees)
mrb_value game_object_look_at(mrb_state* mrb, mrb_value self) {
  mrb_value bone;
  mrb_value point;
  mrb_float most = 0.0;
  mrb_get_args(mrb, "oof", &bone, &point, &most);
  most = from_to(mrb, most, kNoTurn, kHalfTurnDegrees, "max_angle");
  const ObjectId character = alive_id(mrb, self);
  const ObjectId id = bone_arg(mrb, character, bone);
  head_aims(mrb).aim(scene(mrb), character, id, to_vec3(mrb, point), static_cast<float>(most));
  return mrb_nil_value();
}

// GameObject#__look_away(bone)
mrb_value game_object_look_away(mrb_state* mrb, mrb_value self) {
  const ObjectId id = bone_arg(mrb, alive_id(mrb, self), single_arg(mrb));
  head_aims(mrb).release(scene(mrb), id);
  return mrb_nil_value();
}

// GameObject#__looking(bone)
mrb_value game_object_looking(mrb_state* mrb, mrb_value self) {
  const ObjectId id = bone_arg(mrb, alive_id(mrb, self), single_arg(mrb));
  return mrb_bool_value(head_aims(mrb).aiming(scene(mrb), id));
}

}  // namespace

void install_head_aim_bindings(mrb_state* mrb) {
  RClass* game_object = mrb_class_get(mrb, kGameObjectClass);
  mrb_define_method(mrb, game_object, "__look_at", game_object_look_at, MRB_ARGS_REQ(3));
  mrb_define_method(mrb, game_object, "__look_away", game_object_look_away, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, game_object, "__looking", game_object_looking, MRB_ARGS_REQ(1));
}

}  // namespace redgem
