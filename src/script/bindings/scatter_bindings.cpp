// Scatter natives: copies of an object's mesh in great numbers, only drawn (scene/scatter.hpp).

#include <mruby.h>
#include <mruby/array.h>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "scene/scatter.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr Vec3 kDown{0.0f, -1.0f, 0.0f};
// Meters above the area's higher corner the rays to the ground start, and below its lower corner
// they still look: with a ground, the corners' heights hardly matter.
constexpr float kReach = 1000.0f;
constexpr mrb_float kLargestSeed = 4294967295.0;  // seeds are 32-bit
constexpr mrb_float kOneCopy = 1.0;
constexpr mrb_float kNoScale = 0.0;
constexpr int kPlaceArgs = 9;
constexpr int kAnswerParts = 2;  // what Scatter.__place answers: an id and a count
constexpr auto kMostCopies = static_cast<int>(kMaxScatteredCopies);

// The object whose mesh is scattered: a GameObject, or the object of that name, with a mesh.
const GameObject& template_arg(mrb_state* mrb, mrb_value source) {
  const std::string name = is_game_object(mrb, source) ? std::string() : to_name(mrb, source);
  const ObjectId id = name.empty() ? alive_id(mrb, source) : scene(mrb).find(name);
  if (id == kNoObject) mrb_raisef(mrb, E_ARGUMENT_ERROR, "no object named %s to scatter", name.c_str());
  const GameObject& object = scene(mrb).at(id);
  if (object.mesh == kNoIndex) mrb_raisef(mrb, E_ARGUMENT_ERROR, "%s has no mesh to scatter", object.name.c_str());
  return object;
}

// Whether a ray down met the ground copies are to stand on before anything else.
bool met_the_ground(const std::optional<RaycastHit>& hit, ObjectId ground) {
  return hit.has_value() && hit->object == ground;
}

// What copies stand on: where a ray straight down meets `object` (nothing where it meets something
// else first). An empty probe without one.
GroundProbe ground_of(mrb_state* mrb, mrb_value object) {
  if (mrb_nil_p(object)) return {};
  const ObjectId ground = alive_id(mrb, object);
  const PhysicsWorld& world = required(mrb, context(mrb).physics, "Scatter.place on a ground needs the game's physics");
  return [&world, ground](Vec3 above, float depth) -> std::optional<GroundHit> {
    const Vec3 start{above.x, above.y + kReach, above.z};
    const std::optional<RaycastHit> hit = world.raycast(start, kDown, depth + kReach * kTwo, kNoObject);
    if (!met_the_ground(hit, ground)) return std::nullopt;
    return GroundHit{hit->point, hit->normal};
  };
}

// The box between two corners, whichever way round they are given.
void set_box(mrb_state* mrb, ScatterRequest& request, mrb_value first, mrb_value second) {
  const Vec3 a = to_vec3(mrb, first);
  const Vec3 b = to_vec3(mrb, second);
  request.low = {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
  request.high = {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}

// What Scatter.__place is given: the template, what copies stand on, and how they are placed.
struct PlaceArgs {
  mrb_value source;
  mrb_value ground;
  ScatterRequest request;
};

// Scatter.__place(template, count, corner, corner, ground, seed, smallest, largest, align)
PlaceArgs place_args(mrb_state* mrb) {
  PlaceArgs args{};
  mrb_value first, second;
  mrb_float count = 0.0, seed = 0.0, smallest = 1.0, largest = 1.0;
  mrb_bool align = false;
  mrb_get_args(mrb, "ofooofffb", &args.source, &count, &first, &second, &args.ground, &seed, &smallest, &largest,
               &align);
  args.request.count = static_cast<std::size_t>(from_to(mrb, count, kOneCopy, kMaxScatteredCopies, "the count"));
  args.request.seed = static_cast<std::uint32_t>(from_to(mrb, seed, 0.0, kLargestSeed, "the seed"));
  args.request.smallest = static_cast<float>(more_than(mrb, smallest, kNoScale, "the smallest scale"));
  args.request.largest = static_cast<float>(at_least(mrb, largest, smallest, "the largest scale"));
  args.request.align = align;
  set_box(mrb, args.request, first, second);
  return args;
}

// -> [the set's id, how many copies were placed].
mrb_value scatter_place(mrb_state* mrb, mrb_value) {
  const PlaceArgs args = place_args(mrb);
  const GameObject& object = template_arg(mrb, args.source);
  std::vector<Transform> places = scatter_places(args.request, ground_of(mrb, args.ground));
  const auto placed = static_cast<mrb_int>(places.size());
  const int id = scene(mrb).scattered().add(
      {0, object.mesh, object.tint, object.visible_from, object.visible_to, object.sways, std::move(places)});
  if (id == 0) mrb_raisef(mrb, E_ARGUMENT_ERROR, "a scene holds at most %d scattered copies", kMostCopies);
  const mrb_value answer[] = {mrb_int_value(mrb, id), mrb_int_value(mrb, placed)};
  return mrb_ary_new_from_values(mrb, kAnswerParts, answer);
}

mrb_value scatter_remove(mrb_state* mrb, mrb_value) {
  mrb_int id = 0;
  mrb_get_args(mrb, "i", &id);
  return mrb_bool_value(scene(mrb).scattered().remove(static_cast<int>(id)));
}

mrb_value scatter_clear(mrb_state* mrb, mrb_value) {
  scene(mrb).scattered().clear();
  return mrb_nil_value();
}

}  // namespace

void install_scatter_bindings(mrb_state* mrb) {
  RClass* scatter = mrb_define_class(mrb, "Scatter", mrb->object_class);
  mrb_define_class_method(mrb, scatter, "__place", scatter_place, MRB_ARGS_REQ(kPlaceArgs));
  mrb_define_class_method(mrb, scatter, "__remove", scatter_remove, MRB_ARGS_REQ(1));
  mrb_define_class_method(mrb, scatter, "clear", scatter_clear, MRB_ARGS_NONE());
}

}  // namespace redgem
