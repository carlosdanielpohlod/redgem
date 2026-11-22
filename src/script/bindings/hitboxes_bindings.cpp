// Hitboxes natives (ruby/engine/hitboxes.rb builds them from a character's bones).

#include <mruby.h>

#include <string>

#include "scene/props.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr float kLargestHitbox = 100.0f;  // meters a side: a larger one is a mistake
constexpr std::size_t kMakeArgs = 7;

// What Hitboxes.__make is called with, as mruby hands it over.
struct MakeArgs {
  mrb_value bone = mrb_nil_value();
  const char* name = nullptr;
  const char* shape = nullptr;
  mrb_value size = mrb_nil_value();
  mrb_value offset = mrb_nil_value();
  const char* layer = nullptr;
  const char* region = nullptr;
};

MakeArgs make_args(mrb_state* mrb) {
  MakeArgs args;
  mrb_get_args(mrb, "ozzoozz", &args.bone, &args.name, &args.shape, &args.size, &args.offset, &args.layer,
               &args.region);
  return args;
}

bool known_shape(const std::string& shape) {
  return shape == props::kColliderBox || shape == props::kColliderCapsule || shape == props::kColliderSphere;
}

// Meters a side, each above nothing and at most kLargestHitbox.
Vec3 size_of(mrb_state* mrb, mrb_value value) {
  const Vec3 size = to_vec3(mrb, value);
  for (const float side : {size.x, size.y, size.z}) {
    more_than(mrb, side, 0.0, "a hit box's size");
    from_to(mrb, side, 0.0, kLargestHitbox, "a hit box's size");
  }
  return size;
}

// A layer the game's physics has; none (empty) is the default one.
void check_layer(mrb_state* mrb, const char* layer) {
  const PhysicsWorld* physics = context(mrb).physics;
  if (*layer == '\0' || physics == nullptr || physics->layers().find(layer)) return;
  mrb_raisef(mrb, E_ARGUMENT_ERROR, "Hitboxes.build: no collision layer named %s in game.json (layers: %s)", layer,
             physics->layers().names_listed().c_str());
}

// A kinematic volume of its shape, `size` meters a side (its own scale: the collider is a meter
// a side at scale 1), on its layer (none: the default), standing for its region.
NodeDesc hitbox_node(mrb_state* mrb, const MakeArgs& args, Vec3 size) {
  if (!known_shape(args.shape)) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "a hit box is a box, a capsule or a sphere, not %s", args.shape);
  }
  check_layer(mrb, args.layer);
  NodeDesc node;
  node.name = args.name;
  node.local.scale = size;
  node.props = {{props::kCollider, std::string(args.shape)},
                {props::kBody, std::string(props::kBodyKinematic)},
                {props::kHitbox, true},
                {props::kRegion, std::string(args.region)}};
  if (*args.layer != '\0') node.props[props::kLayer] = std::string(args.layer);
  return node;
}

// Where it goes: `offset` from the bone in the bone's own axes (meters), turned as the bone is,
// `size` meters a side whatever the bone's scale.
Transform placed_on(const Transform& bone, Vec3 offset, Vec3 size) {
  return {bone.position + rotate(bone.rotation, offset), bone.rotation, size};
}

mrb_value hitboxes_make(mrb_state* mrb, mrb_value) {
  const MakeArgs args = make_args(mrb);
  const ObjectId bone = alive_id(mrb, args.bone);
  const Vec3 size = size_of(mrb, args.size);
  const NodeDesc node = hitbox_node(mrb, args, size);
  const Transform world = placed_on(scene(mrb).world_transform(bone), to_vec3(mrb, args.offset), size);
  const auto& make = context(mrb).make_object;
  if (!make) mrb_raise(mrb, E_RUNTIME_ERROR, "Hitboxes are only available while a game runs");
  SpawnRequest request;
  request.world = world;
  request.parent = bone;
  return object_or_nil(mrb, paused(mrb, [&] { return make(node, request); }));
}

}  // namespace

void install_hitboxes_bindings(mrb_state* mrb) {
  RClass* hitboxes_module = mrb_define_module(mrb, "Hitboxes");
  mrb_define_module_function(mrb, hitboxes_module, "__make", hitboxes_make, MRB_ARGS_REQ(kMakeArgs));
}

}  // namespace redgem
