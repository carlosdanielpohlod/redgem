// Scene natives: finding the scene's objects and adding copies of them.

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/error.h>

#include <exception>
#include <optional>

#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/script_host.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr mrb_int kSpawnArguments = 6;  // template, position, rotation, physics, name, parent

mrb_value scene_find_by_id(mrb_state* mrb, mrb_value) {
  return object_or_nil(mrb, scene(mrb).find_by_stable_id(name_arg(mrb)));
}

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
ObjectId spawned(mrb_state* mrb, const SpawnRequest& request) {
  const BudgetPause pause(script_host(mrb).vm().budget_keeper());
  return context(mrb).spawn(request);
}

// What a copy is called: its template's name for nil, else a name that is not empty.
std::string spawn_name(mrb_state* mrb, mrb_value name) {
  if (mrb_nil_p(name)) return {};
  std::string text = to_name(mrb, name);
  if (text.empty()) mrb_raise(mrb, E_ARGUMENT_ERROR, "Scene.spawn name: must not be empty");
  return text;
}

// What a copy goes under: nothing for nil, else an object things may be put under.
ObjectId spawn_parent(mrb_state* mrb, mrb_value parent) {
  return mrb_nil_p(parent) ? kNoObject : placeable_id(mrb, parent, "Scene.spawn parent:");
}

// Scene.__spawn(template, position, rotation, physics, name, parent): see ruby/engine/scene.rb.
mrb_value scene_spawn(mrb_state* mrb, mrb_value) {
  mrb_value source, position, rotation, name, parent;
  mrb_bool physics = true;
  mrb_get_args(mrb, "oooboo", &source, &position, &rotation, &physics, &name, &parent);
  if (!context(mrb).spawn) mrb_raise(mrb, E_RUNTIME_ERROR, "Scene.spawn only works while the game runs");
  SpawnRequest request;
  request.source = spawn_source(mrb, source);
  request.world = spawn_placement(mrb, request.source, position, rotation);
  request.physics = physics;
  request.name = spawn_name(mrb, name);
  request.parent = spawn_parent(mrb, parent);
  return object_or_nil(mrb, spawned(mrb, request));
}

ObjectId spawned_prefab(mrb_state* mrb, const std::string& file, const SpawnRequest& request, std::string& failure) {
  try {
    const BudgetPause pause(script_host(mrb).vm().budget_keeper());
    return context(mrb).spawn_prefab(file, request);
  } catch (const std::exception& error) {
    failure = error.what();
    return kNoObject;
  }
}

SpawnRequest prefab_spawn_request(mrb_state* mrb, mrb_value position, mrb_value rotation, mrb_bool physics,
                                  mrb_value name, mrb_value parent) {
  SpawnRequest request;
  Transform world;
  if (!mrb_nil_p(position)) world.position = to_vec3(mrb, position);
  if (!mrb_nil_p(rotation)) world.rotation = quat_from_euler_degrees(to_vec3(mrb, rotation));
  request.world = world;
  request.physics = physics;
  request.name = spawn_name(mrb, name);
  request.parent = spawn_parent(mrb, parent);
  return request;
}

mrb_value prefab_spawn_result(mrb_state* mrb, mrb_value file, const SpawnRequest& request, bool& failed) {
  std::string failure;
  const ObjectId id = spawned_prefab(mrb, to_name(mrb, file), request, failure);
  if (id != kNoObject) return object_or_nil(mrb, id);
  failed = true;
  return mrb_exc_new(mrb, E_ARGUMENT_ERROR, failure.data(), failure.size());
}

mrb_value scene_spawn_prefab(mrb_state* mrb, mrb_value) {
  mrb_value file, position, rotation, name, parent;
  mrb_bool physics = true;
  mrb_get_args(mrb, "oooboo", &file, &position, &rotation, &physics, &name, &parent);
  if (!context(mrb).spawn_prefab) mrb_raise(mrb, E_RUNTIME_ERROR, "Scene.spawn_prefab only works while the game runs");
  bool failed = false;
  const mrb_value result =
      prefab_spawn_result(mrb, file, prefab_spawn_request(mrb, position, rotation, physics, name, parent), failed);
  if (failed) mrb_exc_raise(mrb, result);
  return result;
}

}  // namespace

void install_scene_bindings(mrb_state* mrb) {
  RClass* scene_module = mrb_define_module(mrb, "Scene");
  mrb_define_module_function(mrb, scene_module, "find", scene_find, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, scene_module, "find_by_id", scene_find_by_id, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, scene_module, "find_all", scene_find_all, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, scene_module, "objects", scene_objects, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, scene_module, "__spawn", scene_spawn, MRB_ARGS_REQ(kSpawnArguments));
  mrb_define_module_function(mrb, scene_module, "__spawn_prefab", scene_spawn_prefab, MRB_ARGS_REQ(kSpawnArguments));
}

}  // namespace redgem
