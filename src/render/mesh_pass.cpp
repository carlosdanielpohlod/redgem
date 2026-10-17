#include "render/mesh_pass.hpp"

#include <raymath.h>
#include <rlgl.h>

#include "render/render_math.hpp"
#include "render/shader_programs.hpp"
#include "scene/scene_config.hpp"

namespace redgem::render_detail {
namespace {

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

// Where the mesh is, for the shader: its own matrix, the one for its normals, and the whole way to the screen.
void send_matrices(const MeshPass& pass, const Matrix& model) {
  const Shader& shader = pass.shader;
  if (takes(shader, SHADER_LOC_MATRIX_MODEL)) rlSetUniformMatrix(shader.locs[SHADER_LOC_MATRIX_MODEL], model);
  if (takes(shader, SHADER_LOC_MATRIX_NORMAL)) {
    rlSetUniformMatrix(shader.locs[SHADER_LOC_MATRIX_NORMAL], MatrixTranspose(MatrixInvert(model)));
  }
  rlSetUniformMatrix(shader.locs[SHADER_LOC_MATRIX_MVP], MatrixMultiply(model, pass.view_projection));
}

}  // namespace

MeshPass begin_mesh_pass(const Shader& shader) {
  rlDrawRenderBatchActive();  // whatever raylib still had queued goes first
  const MeshPass pass{shader, MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection())};
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

void draw_mesh(const MeshPass& pass, const Mesh& mesh, const Matrix& model) {
  send_matrices(pass, model);
  rlEnableVertexArray(mesh.vaoId);
  if (mesh.indices != nullptr) {
    rlDrawVertexArrayElements(0, mesh.triangleCount * config::kVerticesPerTriangle, nullptr);
  } else {
    rlDrawVertexArray(0, mesh.vertexCount);
  }
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
