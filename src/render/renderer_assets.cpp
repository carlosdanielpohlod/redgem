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
#include "render/morph_shapes.hpp"
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

// Whether a piece has shape keys the morph texels had no room for.
bool lost_shape_keys(const PrimitiveDesc& piece, const GpuPrimitive& part) {
  return !piece.targets.empty() && part.morph_first < 0.0f;
}

// One piece of a mesh, on the graphics card, its shape keys' deltas added to `morph_deltas`. A
// piece that cannot be drawn is reported and left out.
std::optional<GpuPrimitive> upload_piece(const PrimitiveDesc& piece, std::size_t number, const std::string& mesh_name,
                                         std::vector<float>& morph_deltas) {
  std::string error;
  std::optional<Mesh> mesh = build_mesh(piece, error);
  if (!mesh) {
    log::warning("mesh '" + mesh_name + "': " + error);
    return std::nullopt;
  }
  UploadMesh(&*mesh, false);
  const std::vector<std::uint16_t> joints = skin_joints(piece);
  if (!joints.empty()) upload_joints(*mesh, joints);
  GpuPrimitive part{*mesh, piece.material, number, !joints.empty()};
  part.morph_first = render_math::append_morph_deltas(morph_deltas, piece, render_math::kMaxTexels);
  if (lost_shape_keys(piece, part)) log::warning("mesh '" + mesh_name + "': too many shape keys to draw, left out");
  return part;
}

// Keep the source primitive identity when a large primitive becomes several GPU pieces.
void append_gpu_primitive(std::vector<GpuPrimitive>& parts, const PrimitiveDesc& source, std::size_t original,
                          const std::string& name, std::size_t& number, std::vector<float>& morph_deltas) {
  for (const auto& piece : split_primitive(source, config::kMaxVerticesPerMesh)) {
    auto uploaded = upload_piece(piece, number++, name, morph_deltas);
    if (!uploaded) continue;
    uploaded->primitive = original;
    parts.push_back(*uploaded);
  }
}

// The parts of a mesh, on the graphics card.
std::vector<GpuPrimitive> upload_mesh(const MeshDesc& mesh_desc, std::vector<float>& morph_deltas) {
  std::vector<GpuPrimitive> primitives;
  std::size_t number = 0;
  for (std::size_t index = 0; index < mesh_desc.primitives.size(); ++index) {
    append_gpu_primitive(primitives, mesh_desc.primitives[index], index, mesh_desc.name, number, morph_deltas);
  }
  return primitives;
}

// Whether `image` is one of the scene's and was decoded. (A material may come from anywhere: it
// gets a texture only for an image the scene really has.)
bool has_texture(const std::vector<Texture2D>& textures, int image) {
  return is_index_of(image, textures.size()) && loaded(textures[static_cast<std::size_t>(image)]);
}

// What a map of a material without a picture there shows: raylib's white texel for the color (so
// the color shows as it is), nothing for the others (the shader is told they have none).
Texture2D blank_map(int slot) {
  if (slot != MATERIAL_MAP_ALBEDO) return Texture2D{};
  return Texture2D{rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
}

template <typename Element>
std::size_t bytes_of(const std::vector<Element>& elements) {
  return elements.size() * sizeof(Element);
}

// Roughly what a mesh sends to the graphics card: its vertices and indices.
std::size_t mesh_bytes(const MeshDesc& mesh) {
  std::size_t bytes = 0;
  for (const PrimitiveDesc& piece : mesh.primitives) {
    bytes += bytes_of(piece.positions) + bytes_of(piece.normals) + bytes_of(piece.texcoords) +
             bytes_of(piece.colors) + bytes_of(piece.indices) + bytes_of(piece.joints) + bytes_of(piece.weights);
  }
  return bytes;
}

bool has_decoded(const SceneDesc& desc, std::size_t image) {
  return image < desc.decoded_images.size() && !desc.decoded_images[image].empty();
}

// Roughly what a picture sends: its pixels, or its file's bytes when it was not decoded yet.
std::size_t picture_bytes(const SceneDesc& desc, std::size_t image) {
  if (has_decoded(desc, image)) return desc.decoded_images[image].pixels.size();
  return desc.images[image].encoded.size();
}

}  // namespace

Texture2D Renderer::scene_texture(int image) const {
  const auto& textures = impl_->textures;
  if (!has_texture(textures, image)) return {};
  return textures[static_cast<std::size_t>(image)];
}

std::uint64_t Renderer::scene_upload_count() const { return impl_->scene_uploads; }

void Renderer::begin_scene_texture_use() { impl_->scene_textures_in_use = true; }

void Renderer::end_scene_texture_use() {
  impl_->scene_textures_in_use = false;
  for (const Texture2D& texture : impl_->retired_scene_textures) UnloadTexture(texture);
  impl_->retired_scene_textures.clear();
}

void Renderer::Impl::upload_new_textures(const SceneDesc& desc) {
  for (std::size_t i = textures.size(); i < desc.images.size(); ++i) textures.push_back(scene_picture(desc, i));
}

Texture2D Renderer::Impl::scene_picture(const SceneDesc& desc, std::size_t image) {
  static const DecodedPicture kNotDecoded;
  const bool decoded = image < desc.decoded_images.size();
  return load_scene_texture(desc.images[image], decoded ? desc.decoded_images[image] : kNotDecoded);
}

bool Renderer::Impl::staged_all(const SceneDesc& desc) const {
  return staged.textures.size() >= desc.images.size() && staged.meshes.size() >= desc.meshes.size();
}

std::size_t Renderer::Impl::stage_next(const SceneDesc& desc) {
  const std::size_t image = staged.textures.size();
  if (image < desc.images.size()) {
    staged.textures.push_back(scene_picture(desc, image));
    return picture_bytes(desc, image);
  }
  const MeshDesc& mesh = desc.meshes[staged.meshes.size()];
  staged.meshes.push_back(upload_mesh(mesh, staged.morph_deltas));
  return mesh_bytes(mesh);
}

bool Renderer::Impl::staged_fits(const SceneDesc& desc) const {
  return staged.textures.size() <= desc.images.size() && staged.meshes.size() <= desc.meshes.size();
}

void Renderer::Impl::adopt_staged(const SceneDesc& desc) {
  if (!staged_fits(desc)) return release_staged();
  textures = std::move(staged.textures);
  meshes = std::move(staged.meshes);
  morph_deltas = std::move(staged.morph_deltas);
  for (std::size_t m = 0; m < meshes.size(); ++m) mesh_bounds.push_back(render_math::drawn_bounds(desc.meshes[m]));
  staged = {};
}

void Renderer::Impl::release_staged() {
  for (const Texture2D& texture : staged.textures) UnloadTexture(texture);
  for (auto& primitives : staged.meshes) {
    for (GpuPrimitive& primitive : primitives) UnloadMesh(primitive.mesh);
  }
  staged = {};
}

Material Renderer::Impl::make_material(redgem::Color base_color) const {
  Material material = LoadMaterialDefault();
  material.shader = shader;
  material.maps[MATERIAL_MAP_ALBEDO].color = to_raylib(base_color);
  return material;
}

// A material with its textures; declared images that could not be decoded use a checkerboard.
Material Renderer::Impl::load_material(const MaterialDesc& source) const {
  Material material = make_material(source.base_color);
  bind_textures(material, source);
  return material;
}

// Each map shows the texture of its image, or what a material without one shows. (Done again as
// the editor gives a slot another picture, or takes it away.)
void Renderer::Impl::bind_textures(Material& material, const MaterialDesc& source) const {
  const std::pair<int, int> maps[] = {{MATERIAL_MAP_ALBEDO, source.base_color_image},
                                      {MATERIAL_MAP_EMISSION, source.emissive_image},
                                      {MATERIAL_MAP_NORMAL, source.normal_image},
                                      {MATERIAL_MAP_ROUGHNESS, source.metallic_roughness_image},
                                      {MATERIAL_MAP_OCCLUSION, source.occlusion_image},
                                      {render_detail::kCoatFactorMap, source.clearcoat.factor_image},
                                      {render_detail::kCoatRoughnessMap, source.clearcoat.roughness_image},
                                      {render_detail::kCoatNormalMap, source.clearcoat.normal_image}};
  for (const auto& [slot, image] : maps) {
    material.maps[slot].texture =
        has_texture(textures, image) ? textures[static_cast<std::size_t>(image)] : blank_map(slot);
  }
}

void Renderer::Impl::upload_new_materials(const SceneDesc& desc) {
  for (std::size_t i = materials.size(); i < desc.materials.size(); ++i) {
    materials.push_back(load_material(desc.materials[i]));
  }
}

void Renderer::Impl::refresh_material_values(const SceneDesc& desc) {
  for (std::size_t i = 0; i < desc.materials.size(); ++i) {
    materials[i].maps[MATERIAL_MAP_ALBEDO].color = to_raylib(desc.materials[i].base_color);
    bind_textures(materials[i], desc.materials[i]);
  }
  material_factors = desc.materials;
  skins = desc.skins;
}

void Renderer::Impl::upload_new_meshes(const SceneDesc& desc) {
  for (std::size_t m = meshes.size(); m < desc.meshes.size(); ++m) {
    meshes.push_back(upload_mesh(desc.meshes[m], morph_deltas));
    mesh_bounds.push_back(render_math::drawn_bounds(desc.meshes[m]));
  }
  if (morph_deltas.size() != morph_texture_floats) remake_morph_texture();
}

void Renderer::Impl::remake_morph_texture() {
  if (morph_texture != 0) rlUnloadTexture(morph_texture);
  const std::size_t row_floats = static_cast<std::size_t>(render_math::kTexelsPerRow) * render_math::kFloatsPerTexel;
  const std::size_t rows = (morph_deltas.size() + row_floats - 1) / row_floats;
  std::vector<float> whole_rows = morph_deltas;
  whole_rows.resize(rows * row_floats);  // (a texture is whole rows)
  morph_texture = rlLoadTexture(whole_rows.data(), render_math::kTexelsPerRow, static_cast<int>(rows),
                                render_math::kTexelFormat, render_math::kTexelLevels);
  morph_texture_floats = morph_deltas.size();
}

void Renderer::Impl::release_assets() {
  release_meshes();
  skins.clear();
  if (scene_textures_in_use) {
    retired_scene_textures.insert(retired_scene_textures.end(), textures.begin(), textures.end());
  } else {
    for (const Texture2D& texture : textures) UnloadTexture(texture);
  }
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
  if (morph_texture != 0) rlUnloadTexture(morph_texture);
  morph_texture = 0;
  morph_deltas.clear();
  morph_texture_floats = 0;
  see_through_meshes.clear();
  any_see_through = false;
}

}  // namespace redgem
