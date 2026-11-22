// The Terrain module's natives: which ground layer is where on a terrain with ground layers
// (footsteps, what grows there), and its weights painted again by rules while the game runs. The
// weights live in the scene's description, which the renderer follows every frame.

#include <mruby.h>
#include <mruby/array.h>

#include <string>
#include <vector>

#include "scene/ground/ground_rules.hpp"
#include "scene/ground/ground_weights.hpp"
#include "scene/ground/splat_map.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/bindings/message_values.hpp"
#include "script/bindings/terrain_lookup.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr ValueHolder kRulesHolder{"Terrain.paint_by_rules", "a rule's", "take", kAnyNumber};

SceneDesc& scene_desc(mrb_state* mrb) {
  return required(mrb, context(mrb).scene_desc, "Terrain and Scatter.place(layer:) need a running game");
}

mrb_value weights_value(mrb_state* mrb, const LayerWeights& weights, std::size_t layers) {
  mrb_value array = mrb_ary_new_capa(mrb, static_cast<mrb_int>(layers));
  for (std::size_t i = 0; i < layers; ++i) mrb_ary_push(mrb, array, mrb_float_value(mrb, weights[i]));
  return array;
}

// __layers(object): the names of its layers, in order; nil without layers.
mrb_value terrain_layers(mrb_state* mrb, mrb_value) {
  const TerrainLayers* layers = terrain_layers_of(mrb, alive_object(mrb, single_arg(mrb)));
  if (layers == nullptr) return mrb_nil_value();
  mrb_value names = mrb_ary_new_capa(mrb, static_cast<mrb_int>(layers->layers.size()));
  for (const TerrainLayer& layer : layers->layers) mrb_ary_push(mrb, names, text_value(mrb, layer.name));
  return names;
}

// __weights_at(object, position): each layer's share where the point is, seen from above; nil
// without layers, or past the edges of the terrain.
mrb_value terrain_weights_at(mrb_state* mrb, mrb_value) {
  mrb_value object = mrb_nil_value();
  mrb_value position = mrb_nil_value();
  mrb_get_args(mrb, "oo", &object, &position);
  const GameObject& terrain = alive_object(mrb, object);
  const TerrainLayers* layers = terrain_layers_of(mrb, terrain);
  const MeshDesc* mesh = terrain_mesh_of(mrb, terrain);
  if (layers == nullptr || mesh == nullptr) return mrb_nil_value();
  const auto weights =
      ground_weights_at(*layers, *mesh, scene(mrb).world_transform(terrain.id), to_vec3(mrb, position));
  return weights ? weights_value(mrb, *weights, layers->layers.size()) : mrb_nil_value();
}

// The rules a script gave (an Array of Hashes, as the layers file has them), or the file's.
std::vector<GroundRule> rules_of(mrb_state* mrb, mrb_value given, const TerrainLayers& layers) {
  if (mrb_nil_p(given)) return layers.rules;
  std::vector<GroundRule> rules;
  const std::string problem = read_ground_rules(json_from(mrb, given, kRulesHolder), layers, rules);
  if (!problem.empty()) mrb_raisef(mrb, E_ARGUMENT_ERROR, "Terrain.paint_by_rules: %s", problem.c_str());
  return rules;
}

// __paint(object, rules): paints the rules (nil: the layers file's) over its weights; how many
// texels changed.
mrb_value terrain_paint(mrb_state* mrb, mrb_value) {
  mrb_value object = mrb_nil_value();
  mrb_value given = mrb_nil_value();
  mrb_get_args(mrb, "oo", &object, &given);
  const GameObject& terrain = alive_object(mrb, object);
  TerrainLayers* layers = terrain_layers_of(mrb, terrain);
  const MeshDesc* mesh = terrain_mesh_of(mrb, terrain);
  if (layers == nullptr) mrb_raise(mrb, E_ARGUMENT_ERROR, "Terrain.paint_by_rules: the object has no ground layers");
  if (mesh == nullptr) mrb_raise(mrb, E_ARGUMENT_ERROR, "Terrain.paint_by_rules: the object has no mesh to paint over");
  const std::vector<GroundRule> rules = rules_of(mrb, given, *layers);
  const std::size_t changed = paused(mrb, [&] { return paint_ground(*layers, rules, *mesh); });
  return mrb_int_value(mrb, static_cast<mrb_int>(changed));
}

}  // namespace

TerrainLayers* terrain_layers_of(mrb_state* mrb, const GameObject& object) {
  // (An object of the loaded scene has its node's index for its id, see Scene.)
  const auto node = static_cast<int>(object.copy_of != kNoObject ? object.copy_of : object.id);
  for (TerrainLayers& layers : scene_desc(mrb).terrains) {
    if (layers.node == node) return &layers;
  }
  return nullptr;
}

const MeshDesc* terrain_mesh_of(mrb_state* mrb, const GameObject& object) {
  const SceneDesc& desc = scene_desc(mrb);
  return is_index_of(object.mesh, desc.meshes.size()) ? &desc.meshes[static_cast<std::size_t>(object.mesh)] : nullptr;
}

void install_terrain_bindings(mrb_state* mrb) {
  RClass* terrain = mrb_define_module(mrb, "Terrain");
  mrb_define_module_function(mrb, terrain, "__layers", terrain_layers, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, terrain, "__weights_at", terrain_weights_at, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, terrain, "__paint", terrain_paint, MRB_ARGS_REQ(2));
}

}  // namespace redgem
