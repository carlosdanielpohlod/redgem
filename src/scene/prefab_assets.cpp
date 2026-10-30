#include "scene/prefab_import.hpp"
#include <algorithm>
#include "scene/gltf/gltf_budget.hpp"

namespace redgem::prefabs {
namespace {
template <typename T>
std::uint64_t bytes(const std::vector<T>& values) {
  return values.size() * sizeof(T);
}
std::uint64_t property_bytes(const PropertyMap& props) {
  std::uint64_t size = 0;
  for (const auto& entry : props) {
    size += sizeof(entry) + entry.first.size();
    if (const auto* text = std::get_if<std::string>(&entry.second)) size += text->size();
  }
  return size;
}
std::uint64_t node_bytes(const NodeDesc& node) {
  return sizeof(NodeDesc) + node.name.size() + std::max<std::size_t>(kIdentityBytes, node.stable_id.size()) +
         bytes(node.children) + property_bytes(node.props);
}
std::uint64_t primitive_bytes(const PrimitiveDesc& primitive) {
  return bytes(primitive.positions) + bytes(primitive.normals) + bytes(primitive.texcoords) + bytes(primitive.colors) +
         bytes(primitive.indices) + bytes(primitive.joints) + bytes(primitive.weights);
}
int shifted(int index, int offset) { return index == kNoIndex ? kNoIndex : index + offset; }
void move_material(SceneDesc& target, MaterialDesc material, int offset) {
  for (int* image : {&material.base_color_image, &material.metallic_roughness_image, &material.normal_image,
                     &material.occlusion_image, &material.emissive_image, &material.clearcoat.factor_image,
                     &material.clearcoat.roughness_image, &material.clearcoat.normal_image})
    *image = shifted(*image, offset);
  target.materials.push_back(std::move(material));
}
void move_mesh(SceneDesc& target, MeshDesc mesh, int offset) {
  for (auto& primitive : mesh.primitives) primitive.material = shifted(primitive.material, offset);
  target.meshes.push_back(std::move(mesh));
}
}  // namespace
std::uint64_t instance_bytes(const SceneDesc& scene) {
  std::uint64_t size = 0;
  for (const auto& node : scene.nodes) size += node_bytes(node);
  for (const auto& skin : scene.skins) size += sizeof(SkinDesc) + bytes(skin.joints) + bytes(skin.inverse_bind);
  for (const auto& animation : scene.animations) {
    size += sizeof(AnimationDesc) + animation.name.size() + bytes(animation.channels);
    for (const auto& channel : animation.channels) size += bytes(channel.times) + bytes(channel.values);
  }
  return size;
}
std::uint64_t asset_bytes(const SceneDesc& scene) {
  std::uint64_t size = bytes(scene.images) + bytes(scene.materials) + bytes(scene.meshes);
  for (const auto& image : scene.images) size += bytes(image.encoded);
  for (const auto& mesh : scene.meshes) {
    size += mesh.name.size() + bytes(mesh.primitives);
    for (const auto& primitive : mesh.primitives) size += primitive_bytes(primitive);
  }
  return size + gltf_budget::decoded_bytes(scene.images);
}
void transfer_assets(SceneDesc& target, Source& source) {
  if (source.transferred) return;
  source.assets = {static_cast<int>(target.images.size()), static_cast<int>(target.materials.size()),
                   static_cast<int>(target.meshes.size())};
  for (auto& image : source.scene.images) target.images.push_back(std::move(image));
  for (auto& material : source.scene.materials) move_material(target, std::move(material), source.assets.images);
  for (auto& mesh : source.scene.meshes) move_mesh(target, std::move(mesh), source.assets.materials);
  source.transferred = true;
}
}  // namespace redgem::prefabs
