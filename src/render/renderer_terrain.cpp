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

}  // namespace

void Renderer::Impl::sync_terrains(const SceneDesc& desc) {
  if (!same_terrains(terrains, desc)) return upload_terrains(desc);
  for (std::size_t i = 0; i < terrains.size(); ++i) update_terrain_splat(terrains[i].textures, desc.terrains[i].splat);
  mark_terrain_parts();  // (parts uploaded since are marked too)
}

void Renderer::Impl::upload_terrains(const SceneDesc& desc) {
  release_terrains();
  for (const TerrainLayers& layers : desc.terrains) {
    const int mesh = mesh_of(desc, layers.node);
    GpuTerrain terrain{layers.name, layers.node, mesh, upload_terrain_textures(layers), {}};
    if (is_index_of(mesh, desc.meshes.size()))
      terrain.look = render_math::terrain_look(layers, desc.meshes[static_cast<std::size_t>(mesh)].bounds);
    terrains.push_back(terrain);
  }
  mark_terrain_parts();
}

// Each part of a terrain's mesh knows its terrain, so that drawing it tells the lit shader.
void Renderer::Impl::mark_terrain_parts() {
  for (auto& parts : meshes) mark_parts(parts, kNoIndex);
  for (std::size_t i = 0; i < terrains.size(); ++i) {
    if (drawn_terrain(terrains[i], meshes.size()))
      mark_parts(meshes[static_cast<std::size_t>(terrains[i].mesh)], static_cast<int>(i));
  }
}

void Renderer::Impl::release_terrains() {
  for (GpuTerrain& terrain : terrains) render_detail::release(terrain.textures);
  terrains.clear();
}

bool Renderer::Impl::has_terrains() const {
  return std::any_of(terrains.begin(), terrains.end(), [](const GpuTerrain& t) { return t.textures.loaded(); });
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
