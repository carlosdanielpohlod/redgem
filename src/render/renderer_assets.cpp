// A scene's assets on the graphics card: its textures, materials and meshes.

#include "render/renderer_impl.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/log.hpp"
#include "render/mesh_builder.hpp"
#include "render/mesh_pieces.hpp"
#include "render/render_config.hpp"
#include "render/texture_loading.hpp"
#include "scene/scene_config.hpp"

namespace redgem {
namespace {

using namespace render_detail;
using render_math::to_raylib;

constexpr int kShortNumbers = 0x1403;  // GL_UNSIGNED_SHORT, which rlgl has no name for
// Where raylib binds every shader's vertexBoneIds (RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEIDS in
// its config.h, which its headers do not show).
constexpr int kJointsLocation = 7;

// A skinned piece's joints, where the lit shader takes them (vertexBoneIds), 16 bits each. The
// mesh keeps the buffer: releasing the mesh releases it.
void upload_joints(Mesh& mesh, const std::vector<std::uint16_t>& joints) {
  rlEnableVertexArray(mesh.vaoId);
  const int bytes = static_cast<int>(joints.size() * sizeof(std::uint16_t));
  mesh.vboId[kJointsLocation] = rlLoadVertexBuffer(joints.data(), bytes, false);
  rlSetVertexAttribute(kJointsLocation, config::kJointsPerVertex, kShortNumbers, false, 0, 0);
  rlEnableVertexAttribute(kJointsLocation);
  rlDisableVertexArray();
}

// One piece of a mesh, on the graphics card. A piece that cannot be drawn is reported and left out.
std::optional<GpuPrimitive> upload_piece(const PrimitiveDesc& piece, std::size_t number, const std::string& mesh_name) {
  std::string error;
  std::optional<Mesh> mesh = build_mesh(piece, error);
  if (!mesh) {
    log::warning("mesh '" + mesh_name + "': " + error);
    return std::nullopt;
  }
  UploadMesh(&*mesh, false);
  const std::vector<std::uint16_t> joints = skin_joints(piece);
  if (!joints.empty()) upload_joints(*mesh, joints);
  return GpuPrimitive{*mesh, piece.material, number, !joints.empty()};
}

// The parts of a mesh, on the graphics card.
std::vector<GpuPrimitive> upload_mesh(const MeshDesc& mesh_desc) {
  std::vector<GpuPrimitive> primitives;
  const std::vector<PrimitiveDesc> pieces = mesh_pieces(mesh_desc, config::kMaxVerticesPerMesh);
  for (std::size_t number = 0; number < pieces.size(); ++number) {
    if (const auto uploaded = upload_piece(pieces[number], number, mesh_desc.name)) primitives.push_back(*uploaded);
  }
  return primitives;
}

// Whether `image` is one of the scene's and was decoded. (A material may come from anywhere: it
// gets a texture only for an image the scene really has.)
bool has_texture(const std::vector<Texture2D>& textures, int image) {
  return is_index_of(image, textures.size()) && loaded(textures[static_cast<std::size_t>(image)]);
}

}  // namespace

void Renderer::Impl::upload_new_textures(const SceneDesc& desc) {
  for (std::size_t i = textures.size(); i < desc.images.size(); ++i) {
    textures.push_back(load_scene_texture(desc.images[i]));
  }
}

Material Renderer::Impl::make_material(redgem::Color base_color) const {
  Material material = LoadMaterialDefault();
  material.shader = shader;
  material.maps[MATERIAL_MAP_ALBEDO].color = to_raylib(base_color);
  return material;
}

// A material with the textures it uses. (An image that could not be decoded leaves its map empty.)
Material Renderer::Impl::load_material(const MaterialDesc& source) const {
  Material material = make_material(source.base_color);
  const std::pair<int, int> maps[] = {{MATERIAL_MAP_ALBEDO, source.base_color_image},
                                      {MATERIAL_MAP_EMISSION, source.emissive_image},
                                      {MATERIAL_MAP_NORMAL, source.normal_image},
                                      {MATERIAL_MAP_ROUGHNESS, source.metallic_roughness_image},
                                      {MATERIAL_MAP_OCCLUSION, source.occlusion_image}};
  for (const auto& [slot, image] : maps) {
    if (has_texture(textures, image)) material.maps[slot].texture = textures[static_cast<std::size_t>(image)];
  }
  return material;
}

void Renderer::Impl::upload_new_materials(const SceneDesc& desc) {
  for (std::size_t i = materials.size(); i < desc.materials.size(); ++i) {
    materials.push_back(load_material(desc.materials[i]));
  }
}

void Renderer::Impl::refresh_material_values(const SceneDesc& desc) {
  for (std::size_t i = 0; i < desc.materials.size(); ++i) {
    materials[i].maps[MATERIAL_MAP_ALBEDO].color = to_raylib(desc.materials[i].base_color);
  }
  material_factors = desc.materials;
  skins = desc.skins;
}

void Renderer::Impl::upload_new_meshes(const SceneDesc& desc) {
  for (std::size_t m = meshes.size(); m < desc.meshes.size(); ++m) {
    meshes.push_back(upload_mesh(desc.meshes[m]));
    mesh_bounds.push_back(desc.meshes[m].bounds);
  }
}

void Renderer::Impl::release_assets() {
  release_meshes();
  skins.clear();
  for (Texture2D& texture : textures) UnloadTexture(texture);
  textures.clear();
  // Materials share the shader and textures released above; only free their map arrays.
  for (Material& material : materials) MemFree(material.maps);
  materials.clear();
  material_factors.clear();
}

void Renderer::Impl::release_meshes() {
  for (auto& primitives : meshes) {
    for (GpuPrimitive& primitive : primitives) UnloadMesh(primitive.mesh);
  }
  meshes.clear();
  mesh_bounds.clear();
  see_through_meshes.clear();
  any_see_through = false;
}

}  // namespace redgem
