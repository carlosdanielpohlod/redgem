// Scatter natives: copies of an object's mesh in great numbers, only drawn (scene/scatter.hpp).

#include <mruby.h>
#include <mruby/array.h>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "scene/ground/ground_weights.hpp"
#include "scene/scatter.hpp"
#include "scene/scatter_looks.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/bindings/terrain_lookup.hpp"
#include "script/script_host.hpp"

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
constexpr mrb_float kNoShare = 0.0;
constexpr mrb_float kWholeSpot = 1.0;
constexpr int kPlaceArgs = 16;
constexpr int kLodParts = 2;  // a template and the distance where it starts taking over
constexpr int kMostLods = 3;  // base plus three replacements keeps record memory bounded
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

// Where the ground's layer `layer` takes at least `least` of the spot (Scatter.place's layer:).
struct LayerFilter {
  const TerrainLayers* layers = nullptr;
  const MeshDesc* mesh = nullptr;
  Transform world;
  std::size_t layer = 0;
  float least = 0.0f;
};

bool enough_of_layer(const LayerFilter& filter, Vec3 point) {
  const auto weights = ground_weights_at(*filter.layers, *filter.mesh, filter.world, point);
  return weights.has_value() && (*weights)[filter.layer] >= filter.least;
}

// The probe, meeting the ground only where its layer takes enough of the spot.
GroundProbe on_layer(GroundProbe probe, const LayerFilter& filter) {
  return [probe = std::move(probe), filter](Vec3 above, float depth) -> std::optional<GroundHit> {
    std::optional<GroundHit> hit = probe(above, depth);
    if (hit.has_value() && !enough_of_layer(filter, hit->point)) return std::nullopt;
    return hit;
  };
}

// The index of the layer named `name` of the ground's layers; raises when it has none of that name.
std::size_t layer_index(mrb_state* mrb, const TerrainLayers& layers, const std::string& name) {
  for (std::size_t i = 0; i < layers.layers.size(); ++i) {
    if (layers.layers[i].name == name) return i;
  }
  mrb_raisef(mrb, E_ARGUMENT_ERROR, "the ground has no layer named %s", name.c_str());
  return 0;
}

// layer:, least: checked against the ground, before any copies are placed.
LayerFilter layer_filter(mrb_state* mrb, mrb_value ground, mrb_value layer, mrb_float least) {
  if (mrb_nil_p(ground)) mrb_raise(mrb, E_ARGUMENT_ERROR, "Scatter.place on a layer needs a ground");
  const GameObject& terrain = alive_object(mrb, ground);
  LayerFilter filter;
  filter.layers = terrain_layers_of(mrb, terrain);
  filter.mesh = terrain_mesh_of(mrb, terrain);
  if (filter.layers == nullptr || filter.mesh == nullptr)
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Scatter.place on a layer needs a ground with ground layers");
  filter.world = scene(mrb).world_transform(terrain.id);
  filter.layer = layer_index(mrb, *filter.layers, to_name(mrb, layer));
  filter.least = static_cast<float>(least);
  return filter;
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
  mrb_value lods;
  mrb_value layer;
  mrb_float least = 0.0;
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

// The numbers Scatter.__place is given for where and how big the copies are, before they are checked.
struct PlaceNumbers {
  mrb_float count = 0.0;
  mrb_float seed = 0.0;
  mrb_float smallest = 1.0;
  mrb_float largest = 1.0;
  mrb_bool align = false;
};

// count, seed, smallest, largest, align: how many copies are placed, and how.
void set_request(mrb_state* mrb, ScatterRequest& request, const PlaceNumbers& numbers) {
  request.count = static_cast<std::size_t>(from_to(mrb, numbers.count, kOneCopy, kMaxScatteredCopies, "the count"));
  request.seed = static_cast<std::uint32_t>(from_to(mrb, numbers.seed, 0.0, kLargestSeed, "the seed"));
  request.smallest = static_cast<float>(more_than(mrb, numbers.smallest, kNoScale, "the smallest scale"));
  request.largest = static_cast<float>(at_least(mrb, numbers.largest, numbers.smallest, "the largest scale"));
  request.align = numbers.align;
}

// Scatter.__place(template, count, corner, corner, ground, seed, smallest, largest, align, layer,
//                 least, tints, tint_variation, fade, normals_up, lods)
PlaceArgs place_args(mrb_state* mrb) {
  PlaceArgs args{};
  mrb_value first, second, tints;
  PlaceNumbers numbers;
  mrb_float variation = 0.0, fade = 0.0, normals_up = 0.0;
  mrb_get_args(mrb, "ofooofffbofofffo", &args.source, &numbers.count, &first, &second, &args.ground, &numbers.seed,
               &numbers.smallest, &numbers.largest, &numbers.align, &args.layer, &args.least, &tints, &variation, &fade,
               &normals_up, &args.lods);
  set_looks(mrb, args, tints, variation, fade, normals_up);
  set_request(mrb, args.request, numbers);
  args.least = from_to(mrb, args.least, kNoShare, kWholeSpot, "least");
  set_box(mrb, args.request, first, second);
  return args;
}

// One replacement, checked before any copies are placed.
ScatterLod lod_arg(mrb_state* mrb, mrb_value pair, float previous, float end, float fade) {
  if (!is_list(pair)) mrb_raise(mrb, E_ARGUMENT_ERROR, "each lod must be a [template, distance] pair");
  if (RARRAY_LEN(pair) != kLodParts) mrb_raise(mrb, E_ARGUMENT_ERROR, "each lod must have two entries");
  const auto distance = static_cast<float>(mrb_as_float(mrb, mrb_ary_ref(mrb, pair, 1)));
  const auto from = static_cast<float>(more_than(mrb, distance, previous + fade, "lod distance"));
  if (from + fade >= end) mrb_raise(mrb, E_ARGUMENT_ERROR, "lod transition exceeds visible_to");
  return {template_arg(mrb, mrb_ary_ref(mrb, pair, 0)).mesh, from};
}

// Validates the whole chain before placing any copies, so errors change no scene.
std::vector<ScatterLod> lods_arg(mrb_state* mrb, const PlaceArgs& args, const GameObject& object) {
  std::vector<ScatterLod> lods;
  if (mrb_nil_p(args.lods)) return lods;
  if (!is_list(args.lods)) mrb_raise(mrb, E_ARGUMENT_ERROR, "lods must be a list of [template, distance] pairs");
  if (RARRAY_LEN(args.lods) > kMostLods) mrb_raise(mrb, E_ARGUMENT_ERROR, "lods holds at most three replacements");
  float previous = object.visible_from;
  for (mrb_int i = 0; i < RARRAY_LEN(args.lods); ++i) {
    lods.push_back(lod_arg(mrb, mrb_ary_ref(mrb, args.lods, i), previous, object.visible_to, args.fade));
    previous = lods.back().from;
  }
  return lods;
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

// What copies stand on: the ground, and only its layer when one is named.
GroundProbe probe_of(mrb_state* mrb, const PlaceArgs& args) {
  if (mrb_nil_p(args.layer)) return ground_of(mrb, args.ground);
  const LayerFilter filter = layer_filter(mrb, args.ground, args.layer, args.least);
  return on_layer(ground_of(mrb, args.ground), filter);
}

// -> [the set's id, how many copies were placed].
mrb_value scatter_place(mrb_state* mrb, mrb_value) {
  // (Placing thousands of copies is the engine's time, not the script's.)
  const BudgetPause pause(script_budget(mrb));
  const PlaceArgs args = place_args(mrb);
  const GameObject& object = template_arg(mrb, args.source);
  const auto lods = lods_arg(mrb, args, object);
  std::vector<Transform> places = scatter_places(args.request, probe_of(mrb, args));
  const auto placed = static_cast<mrb_int>(places.size());
  ScatterSet set = set_of(mrb, object, args, std::move(places));
  set.lods = lods;
  const int id = scene(mrb).scattered().add(std::move(set));
  if (id == 0)
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "a scene holds at most %d scatter drawing records, including LODs", kMostCopies);
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
