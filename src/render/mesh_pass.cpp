#include "render/mesh_pass.hpp"

#include <rlgl.h>

#include <algorithm>

#include "render/render_math.hpp"
#include "render/shader_programs.hpp"
#include "scene/scene_config.hpp"

namespace redgem::render_detail {
namespace {

constexpr std::size_t kGrowth = 2;  // how much bigger a buffer of numbers is made when one is too small
constexpr int kOnePerCopy = 1;      // the vertex attribute divisor: the next number for every copy

// Whether the pass's shader samples that kind of texture.
bool samples(const MeshPass& pass, int map) { return takes(pass.shader, SHADER_LOC_MAP_DIFFUSE + map); }

// Each kind of texture has its own slot, the same for every material.
void name_texture_slots(const MeshPass& pass) {
  for (const int map : kMaterialMaps) {
    if (samples(pass, map)) rlSetUniform(pass.shader.locs[SHADER_LOC_MAP_DIFFUSE + map], &map, SHADER_UNIFORM_INT, 1);
  }
}

void bind_texture(int slot, unsigned int texture) {
  rlActiveTextureSlot(slot);
  if (texture != 0) {
    rlEnableTexture(texture);
  } else {
    rlDisableTexture();
  }
}

// Room for at least `slots` numbers, in a new buffer when the old one is too small. What the
// frame's earlier passes put in the old one was drawn already: a buffer lives on while draws use it.
void make_room(SlotBuffer& buffer, std::size_t slots) {
  if (slots <= buffer.capacity) return;
  release_slots(buffer);
  buffer.capacity = std::max(slots, buffer.capacity * kGrowth);
  buffer.id = rlLoadVertexBuffer(nullptr, static_cast<int>(buffer.capacity * sizeof(float)), true);
}

}  // namespace

std::size_t append_slots(SlotBuffer& buffer, const std::vector<float>& slots) {
  const std::size_t first = buffer.used;
  if (slots.empty()) return first;  // a pass that sees nothing
  make_room(buffer, first + slots.size());
  const auto bytes = static_cast<int>(slots.size() * sizeof(float));
  rlUpdateVertexBuffer(buffer.id, slots.data(), bytes, static_cast<int>(first * sizeof(float)));
  buffer.used = first + slots.size();
  return first;
}

void release_slots(SlotBuffer& buffer) {
  if (buffer.id != 0) rlUnloadVertexBuffer(buffer.id);
  buffer.id = 0;
}

MeshPass begin_mesh_pass(const Shader& shader, int instance_slot) {
  rlDrawRenderBatchActive();  // whatever raylib still had queued goes first
  const MeshPass pass{shader, instance_slot};
  rlEnableShader(shader.id);
  if (takes(shader, SHADER_LOC_MATRIX_VIEW)) {
    rlSetUniformMatrix(shader.locs[SHADER_LOC_MATRIX_VIEW], rlGetMatrixModelview());
  }
  if (takes(shader, SHADER_LOC_MATRIX_PROJECTION)) {
    rlSetUniformMatrix(shader.locs[SHADER_LOC_MATRIX_PROJECTION], rlGetMatrixProjection());
  }
  name_texture_slots(pass);
  return pass;
}

void bind_material(const MeshPass& pass, const Material& material) {
  for (const int map : kMaterialMaps) {
    if (samples(pass, map)) bind_texture(map, material.maps[map].texture.id);
  }
  if (!takes(pass.shader, SHADER_LOC_COLOR_DIFFUSE)) return;
  const auto color = render_math::color_floats(material.maps[MATERIAL_MAP_DIFFUSE].color);
  rlSetUniform(pass.shader.locs[SHADER_LOC_COLOR_DIFFUSE], color.data(), SHADER_UNIFORM_VEC4, 1);
}

void draw_instances(const MeshPass& pass, const Mesh& mesh, const InstanceRun& run) {
  const auto location = static_cast<unsigned int>(pass.instance_slot);
  const auto copies = static_cast<int>(run.count);
  rlEnableVertexArray(mesh.vaoId);
  rlEnableVertexBuffer(run.buffer);
  rlSetVertexAttribute(location, 1, RL_FLOAT, false, 0, static_cast<int>(run.first * sizeof(float)));
  rlSetVertexAttributeDivisor(location, kOnePerCopy);
  rlEnableVertexAttribute(location);
  if (mesh.indices != nullptr) {
    rlDrawVertexArrayElementsInstanced(0, mesh.triangleCount * config::kVerticesPerTriangle, nullptr, copies);
  } else {
    rlDrawVertexArrayInstanced(0, mesh.vertexCount, copies);
  }
  // Whatever else draws the mesh's vertex array (raylib's own calls) finds no copies in it.
  rlDisableVertexAttribute(location);
}

void end_mesh_pass() {
  rlDisableVertexArray();
  for (const int map : kMaterialMaps) {
    rlActiveTextureSlot(map);
    rlDisableTexture();
  }
  rlActiveTextureSlot(0);
  rlDisableShader();
}

}  // namespace redgem::render_detail
