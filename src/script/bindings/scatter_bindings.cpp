// Scatter natives: copies of an object's mesh in great numbers, only drawn (scene/scatter.hpp).

#include <mruby.h>
#include <mruby/array.h>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "scene/scatter.hpp"
#include "scene/scatter_looks.hpp"
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
constexpr mrb_float kNoVariation = 0.0;
constexpr mrb_float kFullVariation = 1.0;
constexpr mrb_float kNoFade = 0.0;
constexpr mrb_float kAsModelled = 0.0;  // normals as the mesh has them
constexpr mrb_float kStraightUp = 1.0;
constexpr int kPlaceArgs = 13;
constexpr auto kMostTints = static_cast<int>(kMaxScatterTints);
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

// What Scatter.__place is given: the template, what copies stand on, how they are placed and how
// they look.
struct PlaceArgs {
  mrb_value source;
  mrb_value ground;
  ScatterRequest request;
  ScatterLooks looks;
  float fade = 0.0f;
  float normals_up = 0.0f;
};

bool is_list(mrb_value value) { return mrb_array_p(value); }

// A list of up to kMaxScatterTints colors (each [r, g, b] or a Vector3), or none for nil.
std::vector<Vec3> tints_arg(mrb_state* mrb, mrb_value list) {
  std::vector<Vec3> tints;
  if (mrb_nil_p(list)) return tints;
  if (!is_list(list)) mrb_raise(mrb, E_ARGUMENT_ERROR, "tints must be a list of colors");
  const mrb_int count = RARRAY_LEN(list);
  if (count > kMostTints) mrb_raisef(mrb, E_ARGUMENT_ERROR, "tints holds at most %d colors", kMostTints);
  for (mrb_int i = 0; i < count; ++i) tints.push_back(to_vec3(mrb, mrb_ary_ref(mrb, list, i)));
  return tints;
}

// tints, tint_variation, fade, normals_up: how the copies look.
void set_looks(mrb_state* mrb, PlaceArgs& args, mrb_value tints, mrb_float variation, mrb_float fade,
               mrb_float normals_up) {
  args.looks.tints = tints_arg(mrb, tints);
  args.looks.tint_variation =
      static_cast<float>(from_to(mrb, variation, kNoVariation, kFullVariation, "tint_variation"));
  args.fade = static_cast<float>(at_least(mrb, fade, kNoFade, "fade"));
  args.normals_up = static_cast<float>(from_to(mrb, normals_up, kAsModelled, kStraightUp, "normals_up"));
}

// Scatter.__place(template, count, corner, corner, ground, seed, smallest, largest, align, tints,
//                 tint_variation, fade, normals_up)
PlaceArgs place_args(mrb_state* mrb) {
  PlaceArgs args{};
  mrb_value first, second, tints;
  mrb_float count = 0.0, seed = 0.0, smallest = 1.0, largest = 1.0, variation = 0.0, fade = 0.0, normals_up = 0.0;
  mrb_bool align = false;
  mrb_get_args(mrb, "ofooofffbofff", &args.source, &count, &first, &second, &args.ground, &seed, &smallest, &largest,
               &align, &tints, &variation, &fade, &normals_up);
  set_looks(mrb, args, tints, variation, fade, normals_up);
  args.request.count = static_cast<std::size_t>(from_to(mrb, count, kOneCopy, kMaxScatteredCopies, "the count"));
  args.request.seed = static_cast<std::uint32_t>(from_to(mrb, seed, 0.0, kLargestSeed, "the seed"));
  args.request.smallest = static_cast<float>(more_than(mrb, smallest, kNoScale, "the smallest scale"));
  args.request.largest = static_cast<float>(at_least(mrb, largest, smallest, "the largest scale"));
  args.request.align = align;
  set_box(mrb, args.request, first, second);
  return args;
}

// The set of copies of `object`: its mesh and look, at `places` scaled by its own scale.
ScatterSet set_of(mrb_state* mrb, const GameObject& object, const PlaceArgs& args, std::vector<Transform> places) {
  ScatterSet set;
  set.mesh = object.mesh;
  set.tint = object.tint;
  set.visible_from = object.visible_from;
  set.visible_to = object.visible_to;
  set.cast_shadow = object.cast_shadow;
  set.sways = object.sways;
  scale_places(places, scene(mrb).world_transform(object.id).scale);
  set.tints = scatter_tints(args.looks, args.request.seed, places.size());
  set.places = std::move(places);
  set.fade = args.fade;
  set.normals_up = args.normals_up;
  return set;
}

// -> [the set's id, how many copies were placed].
mrb_value scatter_place(mrb_state* mrb, mrb_value) {
  const PlaceArgs args = place_args(mrb);
  const GameObject& object = template_arg(mrb, args.source);
  std::vector<Transform> places = scatter_places(args.request, ground_of(mrb, args.ground));
  const auto placed = static_cast<mrb_int>(places.size());
  const int id = scene(mrb).scattered().add(set_of(mrb, object, args, std::move(places)));
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
