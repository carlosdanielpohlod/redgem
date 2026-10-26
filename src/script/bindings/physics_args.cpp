#include "script/bindings/physics_args.hpp"

#include <mruby/array.h>

#include <optional>
#include <string>

#include "script/bindings/binding_helpers.hpp"

namespace redgem::binding {
namespace {

// What the shapes of a query are called in Ruby.
struct ShapeName {
  QueryShapeKind kind;
  const char* name;
};
constexpr ShapeName kShapeNames[] = {
    {QueryShapeKind::Sphere, "sphere"},
    {QueryShapeKind::Box, "box"},
    {QueryShapeKind::Capsule, "capsule"},
};

bool is_list(mrb_value value) { return mrb_array_p(value); }

const CollisionLayers& game_layers(mrb_state* mrb) { return context(mrb).physics->layers(); }

std::optional<QueryShapeKind> shape_named(const std::string& name) {
  for (const ShapeName& known : kShapeNames) {
    if (name == known.name) return known.kind;
  }
  return std::nullopt;
}

// A length a shape is made of: more than zero.
float shape_length(mrb_state* mrb, float value, const char* what) {
  return static_cast<float>(more_than(mrb, value, 0.0, what));
}

void read_box(mrb_state* mrb, Vec3 size, QueryShape& shape) {
  shape.half_extents = Vec3{shape_length(mrb, size.x, "a box's width"), shape_length(mrb, size.y, "a box's height"),
                            shape_length(mrb, size.z, "a box's depth")} *
                       kHalf;
}

// A capsule is at least as tall as its two round ends.
void read_capsule(mrb_state* mrb, Vec3 size, QueryShape& shape) {
  shape.radius = shape_length(mrb, size.x, "a capsule's radius");
  shape.height = static_cast<float>(at_least(mrb, size.y, shape.radius * kTwo, "a capsule's height"));
}

void read_size(mrb_state* mrb, Vec3 size, QueryShape& shape) {
  switch (shape.kind) {
    case QueryShapeKind::Box:
      read_box(mrb, size, shape);
      return;
    case QueryShapeKind::Capsule:
      read_capsule(mrb, size, shape);
      return;
    case QueryShapeKind::Sphere:
      shape.radius = shape_length(mrb, size.x, "a sphere's radius");
      return;
  }
}

}  // namespace

std::size_t layer_arg(mrb_state* mrb, mrb_value name) {
  const std::string asked = to_name(mrb, name);
  const std::optional<std::size_t> layer = game_layers(mrb).find(asked);
  if (!layer) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "no physics layer '%s' (game.json's physics layers: %s)", asked.c_str(),
               game_layers(mrb).names_listed().c_str());
  }
  return *layer;
}

LayerMask layer_mask_arg(mrb_state* mrb, mrb_value names) {
  if (mrb_nil_p(names)) return kAllLayers;
  if (!is_list(names)) mrb_raise(mrb, E_TYPE_ERROR, "layers must be a list of layer names");
  LayerMask mask = 0;
  for (mrb_int i = 0; i < RARRAY_LEN(names); ++i) mask |= layer_bit(layer_arg(mrb, mrb_ary_entry(names, i)));
  return mask;
}

mrb_value layer_value(mrb_state* mrb, std::size_t layer) {
  return mrb_symbol_value(mrb_intern_cstr(mrb, game_layers(mrb).name(layer).c_str()));
}

std::vector<ObjectId> objects_arg(mrb_state* mrb, mrb_value objects) {
  if (!is_list(objects)) mrb_raise(mrb, E_TYPE_ERROR, "expected a list of game objects");
  std::vector<ObjectId> found;
  for (mrb_int i = 0; i < RARRAY_LEN(objects); ++i) {
    const ObjectId object = scene(mrb).resolve(wrapped_handle(mrb, mrb_ary_entry(objects, i)));
    if (object != kNoObject) found.push_back(object);
  }
  return found;
}

QueryShape query_shape_arg(mrb_state* mrb, mrb_value kind, mrb_value size, mrb_value rotation) {
  const std::string name = to_name(mrb, kind);
  QueryShape shape;
  shape.kind = known(mrb, shape_named(name), "unknown query shape '%s' (sphere, box, capsule)", name);
  read_size(mrb, to_vec3(mrb, size), shape);
  if (!mrb_nil_p(rotation)) shape.rotation = quat_from_euler_degrees(to_vec3(mrb, rotation));
  return shape;
}

}  // namespace redgem::binding
