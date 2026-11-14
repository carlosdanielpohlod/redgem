// The natives of crowds: the velocity a character steers by among others and the bodies that move
// (Navigation.steer, ruby/engine/navigation.rb), and those bodies as obstacles a path goes around.

#include <mruby.h>
#include <mruby/array.h>

#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include "navigation/nav_steering.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/navigation_crowd.hpp"
#include "script/bindings/physics_args.hpp"
#include "script/script_context.hpp"

namespace redgem::binding {
namespace {

constexpr int kSteerArgs = 7;  // position, velocity, preferred, radius, speed, crowd, ignore
constexpr mrb_int kEntryPosition = 0;
constexpr mrb_int kEntryVelocity = 1;
constexpr mrb_int kEntryRadius = 2;
constexpr mrb_int kEntrySize = 3;

// Whether the scene's grid is still to be baked, by an engine that can.
bool bake_due(const Navigation& navigation, const ScriptContext& game) {
  return !navigation.baked() && static_cast<bool>(game.bake_navigation);
}

const PhysicsWorld& physics(mrb_state* mrb) {
  return required(mrb, context(mrb).physics, "Navigation is only available in a running game");
}

// Whether a body moves: anything but the level.
bool moves(BodyType type) { return type != BodyType::Static && type != BodyType::None; }

// The radius of a disc on the ground around a box: through its corners.
float disc_radius(const Bounds& bounds) {
  const Vec3 half = bounds.half_extents();
  return std::sqrt(half.x * half.x + half.z * half.z);
}

// The box around an object's body when that body moves.
std::optional<Bounds> moving_bounds(const PhysicsWorld& world, ObjectId object) {
  if (!moves(world.body_type(object))) return std::nullopt;
  return world.body_bounds(object);
}

// How far around a character steering looks for bodies that move: as far as one coming at it as
// fast as it walks is within the horizon.
float body_reach(const SteeringAgent& agent) { return agent.radius + nav::kSteeringHorizon * agent.max_speed * kTwo; }

std::vector<SteeringObstacle> bodies_near(const PhysicsWorld& world, const SteeringAgent& agent,
                                          const QueryFilter& filter) {
  QueryShape sphere;
  sphere.radius = body_reach(agent);
  std::vector<SteeringObstacle> found;
  for (const RaycastHit& hit : world.overlap(sphere, agent.position, filter)) {
    const std::optional<Bounds> box = moving_bounds(world, hit.object);
    if (box) found.push_back({box->center(), world.velocity(hit.object), disc_radius(*box), false});
  }
  return found;
}

bool crowd_entry_shaped(mrb_value entry) { return mrb_array_p(entry) && RARRAY_LEN(entry) == kEntrySize; }

// [position, velocity, radius]: a character that steers too.
SteeringObstacle crowd_entry(mrb_state* mrb, mrb_value entry) {
  if (!crowd_entry_shaped(entry)) mrb_raise(mrb, E_TYPE_ERROR, "a crowd entry is [position, velocity, radius]");
  const mrb_float radius = mrb_as_float(mrb, mrb_ary_ref(mrb, entry, kEntryRadius));
  return {to_vec3(mrb, mrb_ary_ref(mrb, entry, kEntryPosition)), to_vec3(mrb, mrb_ary_ref(mrb, entry, kEntryVelocity)),
          static_cast<float>(at_least(mrb, radius, 0.0, "a crowd entry's radius")), true};
}

std::vector<SteeringObstacle> crowd_arg(mrb_state* mrb, mrb_value crowd) {
  if (!mrb_array_p(crowd)) mrb_raise(mrb, E_TYPE_ERROR, "the crowd must be a list");
  std::vector<SteeringObstacle> others;
  others.reserve(static_cast<std::size_t>(RARRAY_LEN(crowd)));
  for (mrb_int i = 0; i < RARRAY_LEN(crowd); ++i) others.push_back(crowd_entry(mrb, mrb_ary_ref(mrb, crowd, i)));
  return others;
}

// What __steer is given.
struct SteerArgs {
  mrb_value position{}, velocity{}, preferred{}, crowd{}, ignore{};
  mrb_float radius = 0.0, speed = 0.0;
};

SteeringAgent agent_of(mrb_state* mrb, const SteerArgs& args) {
  return {to_vec3(mrb, args.position), to_vec3(mrb, args.velocity), to_vec3(mrb, args.preferred),
          static_cast<float>(at_least(mrb, args.radius, 0.0, "Navigation.steer's radius")),
          static_cast<float>(at_least(mrb, args.speed, 0.0, "Navigation.steer's speed"))};
}

// __steer(position, velocity, preferred, radius, speed, crowd, ignore): the velocity to walk at.
mrb_value navigation_steer(mrb_state* mrb, mrb_value) {
  SteerArgs args;
  mrb_get_args(mrb, "oooffoo", &args.position, &args.velocity, &args.preferred, &args.radius, &args.speed, &args.crowd,
               &args.ignore);
  const SteeringAgent agent = agent_of(mrb, args);
  std::vector<SteeringObstacle> others = crowd_arg(mrb, args.crowd);
  const std::vector<SteeringObstacle> bodies =
      bodies_near(physics(mrb), agent, {kAllLayers, objects_arg(mrb, args.ignore), true});
  others.insert(others.end(), bodies.begin(), bodies.end());
  return vec3_value(mrb, baked_navigation(mrb).steer(agent, others));
}

}  // namespace

const Navigation& baked_navigation(mrb_state* mrb) {
  ScriptContext& game = context(mrb);
  const Navigation& found = required(mrb, game.navigation, "Navigation is only available in a running game");
  if (bake_due(found, game)) game.bake_navigation();
  return found;
}

std::vector<NavObstacle> obstacles_arg(mrb_state* mrb, mrb_value objects) {
  const PhysicsWorld& world = physics(mrb);
  std::vector<NavObstacle> obstacles;
  for (const ObjectId object : objects_arg(mrb, objects)) {
    const std::optional<Bounds> box = world.body_bounds(object);
    if (box) obstacles.push_back({box->center(), disc_radius(*box), box->min.y, box->max.y});
    if (obstacles.size() == kMaxAvoidedBodies) break;
  }
  return obstacles;
}

void install_navigation_crowd_bindings(mrb_state* mrb, RClass* module) {
  mrb_define_module_function(mrb, module, "__steer", navigation_steer, MRB_ARGS_REQ(kSteerArgs));
}

}  // namespace redgem::binding
