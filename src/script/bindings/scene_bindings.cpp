// Scene natives: finding the scene's objects and adding copies of them.

#include <mruby.h>
#include <mruby/array.h>

#include <optional>

#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/script_host.hpp"

namespace redgem {
namespace {

using namespace binding;

mrb_value scene_find(mrb_state* mrb, mrb_value) { return object_or_nil(mrb, scene(mrb).find(name_arg(mrb))); }

// The objects as Ruby sees them, without those on their way out.
mrb_value ids_to_array(mrb_state* mrb, const std::vector<ObjectId>& ids) {
  mrb_value out = mrb_ary_new(mrb);
  for (const ObjectId id : ids) {
    if (!scene(mrb).at(id).pending_destroy) mrb_ary_push(mrb, out, object_or_nil(mrb, id));
  }
  return out;
}

mrb_value scene_find_all(mrb_state* mrb, mrb_value) {
  return ids_to_array(mrb, scene(mrb).find_all_by_tag(name_arg(mrb)));
}

mrb_value scene_objects(mrb_state* mrb, mrb_value) { return ids_to_array(mrb, scene(mrb).alive_ids()); }

// What Scene.spawn copies: a GameObject, or the object of that name.
ObjectId spawn_source(mrb_state* mrb, mrb_value source) {
  if (is_game_object(mrb, source)) return alive_id(mrb, source);
  const std::string name = to_name(mrb, source);
  const ObjectId id = scene(mrb).find(name);
  if (id == kNoObject) mrb_raisef(mrb, E_ARGUMENT_ERROR, "no object named %s to spawn", name.c_str());
  return id;
}

// Where a copy of `id` is put: where `id` is, moved and turned as asked. nullopt when nothing is asked.
std::optional<Transform> spawn_placement(mrb_state* mrb, ObjectId id, mrb_value position, mrb_value rotation) {
  const bool as_the_original = mrb_nil_p(position) && mrb_nil_p(rotation);
  if (as_the_original) return std::nullopt;
  Transform world = scene(mrb).world_transform(id);
  if (!mrb_nil_p(position)) world.position = to_vec3(mrb, position);
  if (!mrb_nil_p(rotation)) world.rotation = quat_from_euler_degrees(to_vec3(mrb, rotation));
  return world;
}

// Making the object, its bodies and its behaviours is the engine's time, not the script's.
ObjectId spawned(mrb_state* mrb, ObjectId source, const std::optional<Transform>& placement, bool physics) {
  const BudgetPause pause(script_host(mrb).vm().budget_keeper());
  return context(mrb).spawn(source, placement, physics);
}

mrb_value scene_spawn(mrb_state* mrb, mrb_value) {
  mrb_value source;
  mrb_value position = mrb_nil_value();
  mrb_value rotation = mrb_nil_value();
  mrb_bool physics = true;
  mrb_get_args(mrb, "o|oob", &source, &position, &rotation, &physics);
  ScriptContext& ctx = context(mrb);
  if (!ctx.spawn) mrb_raise(mrb, E_RUNTIME_ERROR, "Scene.spawn only works while the game runs");
  const ObjectId id = spawn_source(mrb, source);
  return object_or_nil(mrb, spawned(mrb, id, spawn_placement(mrb, id, position, rotation), physics));
}

}  // namespace

void install_scene_bindings(mrb_state* mrb) {
  RClass* scene_module = mrb_define_module(mrb, "Scene");
  mrb_define_module_function(mrb, scene_module, "find", scene_find, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, scene_module, "find_all", scene_find_all, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, scene_module, "objects", scene_objects, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, scene_module, "__spawn", scene_spawn, MRB_ARGS_ARG(1, 3));
}

}  // namespace redgem
