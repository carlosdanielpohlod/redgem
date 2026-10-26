// The Physics module's natives: rays, casts and overlaps (ruby/engine/physics.rb builds the
// methods games call on them).

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/hash.h>

#include <optional>
#include <vector>

#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/bindings/physics_args.hpp"

namespace redgem {
namespace {

using namespace binding;

const PhysicsWorld& physics(mrb_state* mrb) { return *context(mrb).physics; }

// What a query answers with: { object:, point:, normal:, distance: }.
mrb_value hit_value(mrb_state* mrb, const RaycastHit& hit) {
  mrb_value result = mrb_hash_new(mrb);
  set_entry(mrb, result, "object", object_or_nil(mrb, hit.object));
  set_entry(mrb, result, "point", vec3_value(mrb, hit.point));
  set_entry(mrb, result, "normal", vec3_value(mrb, hit.normal));
  set_entry(mrb, result, "distance", mrb_float_value(mrb, hit.distance));
  return result;
}

mrb_value hits_value(mrb_state* mrb, const std::vector<RaycastHit>& hits) {
  mrb_value list = mrb_ary_new_capa(mrb, static_cast<mrb_int>(hits.size()));
  for (const RaycastHit& hit : hits) mrb_ary_push(mrb, list, hit_value(mrb, hit));
  return list;
}

mrb_value hit_or_nil(mrb_state* mrb, const std::optional<RaycastHit>& hit) {
  return hit ? hit_value(mrb, *hit) : mrb_nil_value();
}

QueryFilter filter_of(mrb_state* mrb, mrb_value ignore, mrb_value layers) {
  return QueryFilter{layer_mask_arg(mrb, layers), objects_arg(mrb, ignore)};
}

// What a ray of a script is: where from, which way, how far, and what it looks at.
struct RayArgs {
  Vec3 origin;
  Vec3 direction;
  float max_distance = 0.0f;
  QueryFilter filter;
};

// (origin, direction, max_distance, ignore, layers)
RayArgs ray_args(mrb_state* mrb) {
  mrb_value origin, direction, ignore, layers;
  mrb_float max_distance = 0.0;
  mrb_get_args(mrb, "oofoo", &origin, &direction, &max_distance, &ignore, &layers);
  const auto distance = static_cast<float>(at_least(mrb, max_distance, 0.0, "Physics.raycast's distance"));
  return {to_vec3(mrb, origin), to_vec3(mrb, direction), distance, filter_of(mrb, ignore, layers)};
}

// __raycast: the first hit, or nil.
mrb_value physics_raycast(mrb_state* mrb, mrb_value) {
  const RayArgs ray = ray_args(mrb);
  return hit_or_nil(mrb, physics(mrb).raycast(ray.origin, ray.direction, ray.max_distance, ray.filter));
}

// __raycast_all: every hit.
mrb_value physics_raycast_all(mrb_state* mrb, mrb_value) {
  const RayArgs ray = ray_args(mrb);
  return hits_value(mrb, physics(mrb).raycast_all(ray.origin, ray.direction, ray.max_distance, ray.filter));
}

// __cast(kind, size, rotation, from, to, ignore, layers): the first hit, or nil.
mrb_value physics_cast(mrb_state* mrb, mrb_value) {
  mrb_value kind, size, rotation, from, to, ignore, layers;
  mrb_get_args(mrb, "ooooooo", &kind, &size, &rotation, &from, &to, &ignore, &layers);
  const QueryShape shape = query_shape_arg(mrb, kind, size, rotation);
  const QueryFilter filter = filter_of(mrb, ignore, layers);
  return hit_or_nil(mrb, physics(mrb).shape_cast(shape, to_vec3(mrb, from), to_vec3(mrb, to), filter));
}

// __overlap(kind, size, rotation, center, ignore, layers): every hit.
mrb_value physics_overlap(mrb_state* mrb, mrb_value) {
  mrb_value kind, size, rotation, center, ignore, layers;
  mrb_get_args(mrb, "oooooo", &kind, &size, &rotation, &center, &ignore, &layers);
  const QueryShape shape = query_shape_arg(mrb, kind, size, rotation);
  const QueryFilter filter = filter_of(mrb, ignore, layers);
  return hits_value(mrb, physics(mrb).overlap(shape, to_vec3(mrb, center), filter));
}

mrb_value physics_layers(mrb_state* mrb, mrb_value) {
  const std::size_t count = physics(mrb).layers().count();
  mrb_value names = mrb_ary_new_capa(mrb, static_cast<mrb_int>(count));
  for (std::size_t layer = 0; layer < count; ++layer) mrb_ary_push(mrb, names, layer_value(mrb, layer));
  return names;
}

}  // namespace

void install_physics_query_bindings(mrb_state* mrb) {
  RClass* physics_module = mrb_define_module(mrb, "Physics");
  mrb_define_module_function(mrb, physics_module, "__raycast", physics_raycast, MRB_ARGS_REQ(5));
  mrb_define_module_function(mrb, physics_module, "__raycast_all", physics_raycast_all, MRB_ARGS_REQ(5));
  mrb_define_module_function(mrb, physics_module, "__cast", physics_cast, MRB_ARGS_REQ(7));
  mrb_define_module_function(mrb, physics_module, "__overlap", physics_overlap, MRB_ARGS_REQ(6));
  mrb_define_module_function(mrb, physics_module, "layers", physics_layers, MRB_ARGS_NONE());
}

}  // namespace redgem
