// Terrains with ground layers: their pictures and weights on the graphics card, and what the lit
// shader is told of them as their parts are drawn (see shaders/lit_terrain.hpp).

#include <algorithm>

#include "render/renderer_impl.hpp"

namespace redgem {
namespace {

using namespace render_detail;

// Whether a pass draws with the lit shader, which blends the layers (the shadow map's and the
// motion pass's have their own, which need nothing of them).
bool lit_pass(render_math::AlphaPass pass) {
  return pass == render_math::AlphaPass::Opaque || pass == render_math::AlphaPass::SeeThrough;
}

// Whether the terrains on the card are the scene's: as many, for the same objects and files.
bool same_terrains(const std::vector<GpuTerrain>& uploaded, const SceneDesc& desc) {
  if (uploaded.size() != desc.terrains.size()) return false;
  for (std::size_t i = 0; i < uploaded.size(); ++i) {
    const bool same = uploaded[i].node == desc.terrains[i].node && uploaded[i].name == desc.terrains[i].name;
    if (!same) return false;
  }
  return true;
}

// Whether a terrain is drawn with its layers: they are on the card, over a mesh that is.
bool drawn_terrain(const GpuTerrain& terrain, std::size_t meshes) {
  return terrain.textures.loaded() && is_index_of(terrain.mesh, meshes);
}

void mark_parts(std::vector<GpuPrimitive>& parts, int terrain) {
  for (GpuPrimitive& part : parts) part.terrain = terrain;
}

int mesh_of(const SceneDesc& desc, int node) {
  return is_index_of(node, desc.nodes.size()) ? desc.nodes[static_cast<std::size_t>(node)].mesh : kNoIndex;
}

// Whether no other object draws the terrain's mesh: the layers go with the mesh's parts, so a mesh
// shared with a crate or a copy of the terrain would draw them on those too.
bool own_mesh(const SceneDesc& desc, int mesh) {
  const auto users =
      std::count_if(desc.nodes.begin(), desc.nodes.end(), [mesh](const NodeDesc& node) { return node.mesh == mesh; });
  return users == 1;
}

// The look of a terrain's layers over its mesh, of no more layers than its pictures on the card.
render_math::TerrainLook look_of(const TerrainLayers& layers, const SceneDesc& desc, const GpuTerrain& terrain) {
  if (!is_index_of(terrain.mesh, desc.meshes.size())) return {};
  render_math::TerrainLook look =
      render_math::terrain_look(layers, desc.meshes[static_cast<std::size_t>(terrain.mesh)].bounds);
  look.layers = std::min(look.layers, terrain.textures.layers);
  return look;
}

}  // namespace

void Renderer::Impl::sync_terrains(const SceneDesc& desc) {
  if (!same_terrains(terrains, desc)) return upload_terrains(desc);
  for (std::size_t i = 0; i < terrains.size(); ++i) sync_terrain(terrains[i], desc.terrains[i], desc);
  mark_terrain_parts(desc);  // (parts uploaded since are marked too)
}

// Only the weights that changed, for terrains already on the card (what a script painted).
void Renderer::Impl::follow_terrain_weights(const SceneDesc& desc) {
  if (!same_terrains(terrains, desc)) return;  // (the next upload or sync takes them whole)
  for (std::size_t i = 0; i < terrains.size(); ++i) update_terrain_splat(terrains[i].textures, desc.terrains[i].splat);
}

// A terrain already on the card follows its layers: new weights, settings and mesh. Its pictures
// are sent once (they are freed after), or again only while none could be sent.
void Renderer::Impl::sync_terrain(GpuTerrain& terrain, const TerrainLayers& layers, const SceneDesc& desc) {
  if (!terrain.textures.loaded()) terrain.textures = upload_terrain_textures(layers);
  update_terrain_splat(terrain.textures, layers.splat);
  terrain.mesh = mesh_of(desc, layers.node);
  terrain.look = look_of(layers, desc, terrain);
}

void Renderer::Impl::upload_terrains(const SceneDesc& desc) {
  release_terrains();
  for (const TerrainLayers& layers : desc.terrains) {
    GpuTerrain terrain{layers.name, layers.node, mesh_of(desc, layers.node), upload_terrain_textures(layers), {}};
    terrain.look = look_of(layers, desc, terrain);
    terrains.push_back(terrain);
  }
  mark_terrain_parts(desc);
}

// Each part of a terrain's mesh knows its terrain, so that drawing it tells the lit shader.
void Renderer::Impl::mark_terrain_parts(const SceneDesc& desc) {
  for (auto& parts : meshes) mark_parts(parts, kNoIndex);
  for (std::size_t i = 0; i < terrains.size(); ++i) {
    if (drawn_terrain(terrains[i], meshes.size()) && own_mesh(desc, terrains[i].mesh))
      mark_parts(meshes[static_cast<std::size_t>(terrains[i].mesh)], static_cast<int>(i));
  }
}

void Renderer::Impl::release_terrains() {
  for (GpuTerrain& terrain : terrains) render_detail::release(terrain.textures);
  terrains.clear();
}

// Whether the lit shader needs the layers: a terrain has them on the card, and the card has the
// texture units they are bound to (most phones have 16: their terrains are drawn plain).
bool Renderer::Impl::has_terrains() const {
  const bool any =
      std::any_of(terrains.begin(), terrains.end(), [](const GpuTerrain& t) { return t.textures.loaded(); });
  return any && has_texture_units(kTerrainSplatSlot + 1);
}

void Renderer::Impl::use_terrain(int terrain, ObjectPass& pass) {
  if (!lit_features.terrain || !lit_pass(pass.alpha) || terrain == pass.last_terrain) return;
  pass.last_terrain = terrain;
  if (!is_index_of(terrain, terrains.size())) return send_terrain_layers(0);
  const GpuTerrain& drawn = terrains[static_cast<std::size_t>(terrain)];
  bind_terrain_textures(drawn.textures, kTerrainTextureSlot);
  send_terrain_look(drawn.look);
}

// Every pass that told the lit shader of a terrain tells it of none when it ends: what is drawn
// with it after (see-through surfaces, the reflections) is no terrain.
void Renderer::Impl::end_terrains(ObjectPass& pass) {
  if (is_index_of(pass.last_terrain, terrains.size())) send_terrain_layers(0);
  pass.last_terrain = kNoTerrainYet;
}

void Renderer::Impl::send_terrain_layers(int layers) const {
  rlSetUniform(terrain_uniforms.layers, &layers, SHADER_UNIFORM_INT, 1);
}

void Renderer::Impl::send_terrain_look(const render_math::TerrainLook& look) const {
  const TerrainUniforms& at = terrain_uniforms;
  constexpr int kLayers = kMaxTerrainLayers;
  send_terrain_layers(look.layers);
  rlSetUniform(at.area, look.area.data(), SHADER_UNIFORM_VEC4, 1);
  rlSetUniform(at.tiling, look.tiling.data(), SHADER_UNIFORM_FLOAT, kLayers);
  rlSetUniform(at.sharpness, look.sharpness.data(), SHADER_UNIFORM_FLOAT, kLayers);
  rlSetUniform(at.variation, look.variation.data(), SHADER_UNIFORM_FLOAT, kLayers);
  rlSetUniform(at.stochastic, look.stochastic.data(), SHADER_UNIFORM_FLOAT, kLayers);
  rlSetUniform(at.triplanar, look.triplanar.data(), SHADER_UNIFORM_FLOAT, kLayers);
  rlSetUniform(at.variation_scale, &look.variation_scale, SHADER_UNIFORM_FLOAT, 1);
}

}  // namespace redgem
