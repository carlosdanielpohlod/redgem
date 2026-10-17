#pragma once

// The engine's own draw call. raylib's DrawMesh sets the whole pipeline up again for every mesh
// (every matrix, every texture slot, bound and unbound); a scene is thousands of meshes per pass,
// so here what belongs to the pass is set once, what belongs to a material only when the material
// changes, and every copy of a mesh the pass draws goes in one call: each finds where it is in
// the frame's texels by the number of its record (see instance_records.hpp).

#include <raylib.h>

#include <cstddef>
#include <vector>

namespace redgem::render_detail {

// The texture maps materials use (base color, emission, normal, metallic-roughness, occlusion).
inline constexpr int kMaterialMaps[] = {MATERIAL_MAP_ALBEDO, MATERIAL_MAP_EMISSION, MATERIAL_MAP_NORMAL,
                                        MATERIAL_MAP_ROUGHNESS, MATERIAL_MAP_OCCLUSION};

// Meshes being drawn through one shader, from the view raylib is in (between BeginMode3D and
// EndMode3D).
struct MeshPass {
  // cppcheck-suppress uninitMemberVarNoCtor ; a reference: whoever makes one has to give it
  const Shader& shader;
  int instance_slot = -1;  // the vertex attribute each copy's record number goes to
};

// The record numbers of every copy a frame's passes draw, pass after pass, on the graphics card.
struct SlotBuffer {
  unsigned int id = 0;
  std::size_t capacity = 0;  // numbers it has room for
  std::size_t used = 0;      // by this frame's passes so far
};
// Puts `slots` after what the frame's earlier passes put there, and says where they start.
std::size_t append_slots(SlotBuffer& buffer, const std::vector<float>& slots);
void release_slots(SlotBuffer& buffer);

// Copies of a mesh: `count` record numbers from `first` on in `buffer`.
struct InstanceRun {
  unsigned int buffer = 0;
  std::size_t first = 0;
  std::size_t count = 0;
};

// Sets what every mesh of the pass shares: the shader, the view and each texture's slot.
MeshPass begin_mesh_pass(const Shader& shader, int instance_slot);
void bind_material(const MeshPass& pass, const Material& material);
void draw_instances(const MeshPass& pass, const Mesh& mesh, const InstanceRun& run);
// Leaves the state raylib expects: nothing bound.
void end_mesh_pass();

}  // namespace redgem::render_detail
