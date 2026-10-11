#include "editor/model_import.hpp"

#include <algorithm>
#include <string>
#include <string_view>

#include "editor/gltf_json.hpp"

namespace redgem::model_import {
namespace {

using namespace gltf_json;

constexpr std::string_view kTextureSuffix = "Texture";  // "baseColorTexture", "normalTexture"...
constexpr std::string_view kDataUriPrefix = "data:";    // an image stored inside the JSON itself
constexpr int kOnlyBuffer = 0;                          // a .glb keeps everything in its first buffer

bool ends_with(std::string_view text, std::string_view suffix) {
  return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool starts_with(std::string_view text, std::string_view prefix) { return text.rfind(prefix, 0) == 0; }

int shifted(int index, int offset) { return index == kNoIndex ? kNoIndex : index + offset; }

}  // namespace

bool has_several_buffers(const nlohmann::json& model) {
  return model.value(kBuffers, nlohmann::json::array()).size() > 1;
}

void shift_index(nlohmann::json& object, const char* key, int offset) {
  if (object.contains(key)) object[key] = object[key].get<int>() + offset;
}

void shift_texture_refs(nlohmann::json& value, int offset) {
  if (!value.is_object()) return;
  for (auto& [key, child] : value.items()) {
    if (ends_with(key, kTextureSuffix) && child.is_object()) shift_index(child, kIndex, offset);
    shift_texture_refs(child, offset);
  }
}

void merge_string_list(nlohmann::json& into, const nlohmann::json& from, const char* key) {
  if (!from.contains(key)) return;
  for (const nlohmann::json& name : from[key]) {
    if (!into[key].is_array()) into[key] = nlohmann::json::array();
    if (std::find(into[key].begin(), into[key].end(), name) == into[key].end()) into[key].push_back(name);
  }
}

nlohmann::json imported_buffer_view(nlohmann::json view, const JsonOffsets& offsets) {
  view[kBuffer] = kOnlyBuffer;
  view[kByteOffset] = view.value(kByteOffset, 0) + offsets.bin;
  return view;
}

nlohmann::json imported_accessor(nlohmann::json accessor, const JsonOffsets& offsets) {
  shift_index(accessor, kBufferView, offsets.buffer_views);
  if (accessor.contains(kSparse)) {
    shift_index(accessor[kSparse][kIndices], kBufferView, offsets.buffer_views);
    shift_index(accessor[kSparse][kValues], kBufferView, offsets.buffer_views);
  }
  return accessor;
}

nlohmann::json imported_image(nlohmann::json image, const JsonOffsets& offsets, const std::filesystem::path& model_dir,
                              const std::filesystem::path& scene_dir) {
  shift_index(image, kBufferView, offsets.buffer_views);
  if (!image.contains(kUri)) return image;
  const std::string uri = image[kUri].get<std::string>();
  if (starts_with(uri, kDataUriPrefix)) return image;
  image[kUri] = (model_dir / uri).lexically_relative(scene_dir).generic_string();
  return image;
}

nlohmann::json imported_texture(nlohmann::json texture, const JsonOffsets& offsets) {
  shift_index(texture, kSampler, offsets.samplers);
  shift_index(texture, kSource, offsets.images);
  if (!texture.contains(kExtensions)) return texture;
  for (auto& [name, extension] : texture[kExtensions].items()) shift_index(extension, kSource, offsets.images);
  return texture;
}

nlohmann::json imported_material(nlohmann::json material, const JsonOffsets& offsets) {
  shift_texture_refs(material, offsets.textures);
  return material;
}

nlohmann::json imported_mesh(nlohmann::json mesh, const JsonOffsets& offsets) {
  for (nlohmann::json& primitive : mesh[kPrimitives]) {
    for (auto& [name, accessor] : primitive[kAttributes].items()) accessor = accessor.get<int>() + offsets.accessors;
    shift_index(primitive, kIndices, offsets.accessors);
    shift_index(primitive, kMaterial, offsets.materials);
    primitive.erase(kTargets);
  }
  mesh.erase(kWeights);
  if (mesh.contains(kExtras) && mesh[kExtras].is_object()) mesh[kExtras].erase(kTargetNames);
  return mesh;
}

nlohmann::json imported_node(nlohmann::json node, const JsonOffsets& offsets) {
  shift_index(node, kMesh, offsets.meshes);
  shift_index(node, kSkin, offsets.skins);
  node.erase(kCamera);
  node.erase(kWeights);
  node.erase(kExtensions);
  node.erase(kChildren);
  return node;
}

nlohmann::json imported_skin(nlohmann::json skin, const JsonOffsets& offsets) {
  for (nlohmann::json& joint : skin[kJoints]) joint = joint.get<int>() + offsets.nodes;
  shift_index(skin, kSkeleton, offsets.nodes);
  shift_index(skin, kInverseBindMatrices, offsets.accessors);
  return skin;
}

nlohmann::json imported_animation(nlohmann::json animation, const JsonOffsets& offsets) {
  for (nlohmann::json& sampler : animation[kSamplers]) {
    shift_index(sampler, kInput, offsets.accessors);
    shift_index(sampler, kOutput, offsets.accessors);
  }
  for (nlohmann::json& channel : animation[kChannels]) {
    if (channel.contains(kTarget)) shift_index(channel[kTarget], kNode, offsets.nodes);
  }
  return animation;
}

MaterialDesc imported_material(MaterialDesc material, const DescOffsets& offsets) {
  for (int* image : {&material.base_color_image, &material.metallic_roughness_image, &material.normal_image,
                     &material.occlusion_image, &material.emissive_image}) {
    *image = shifted(*image, offsets.images);
  }
  return material;
}

MeshDesc imported_mesh(MeshDesc mesh, const DescOffsets& offsets) {
  for (PrimitiveDesc& primitive : mesh.primitives) primitive.material = shifted(primitive.material, offsets.materials);
  return mesh;
}

NodeDesc imported_node(NodeDesc node, const DescOffsets& offsets) {
  node.parent = node.parent == kNoIndex ? offsets.root : node.parent + offsets.nodes;
  for (int& child : node.children) child += offsets.nodes;
  node.mesh = shifted(node.mesh, offsets.meshes);
  node.skin = shifted(node.skin, offsets.skins);
  node.camera.reset();
  node.light.reset();
  node.source_camera = kNoIndex;
  node.source_light = kNoIndex;
  node.source_node = shifted(node.source_node, offsets.source_nodes);
  return node;
}

SkinDesc imported_skin(SkinDesc skin, const DescOffsets& offsets) {
  for (int& joint : skin.joints) joint = shifted(joint, offsets.nodes);
  return skin;
}

AnimationDesc imported_animation(AnimationDesc animation, const DescOffsets& offsets) {
  for (AnimationChannel& channel : animation.channels) channel.node += offsets.nodes;
  return animation;
}

}  // namespace redgem::model_import
