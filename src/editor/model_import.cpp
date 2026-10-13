#include "editor/model_import.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "editor/gltf_json.hpp"

namespace redgem::model_import {
namespace {

using namespace gltf_json;

constexpr std::string_view kTextureSuffix = "Texture";  // "baseColorTexture", "normalTexture"...
constexpr std::string_view kDataUriPrefix = "data:";    // an image stored inside the JSON itself
constexpr const char* kParentStep = "..";               // how a path says "one folder up"
// How deep a material's extensions are walked for texture references (they nest a few levels).
constexpr int kMaxMaterialDepth = 64;

bool ends_with(std::string_view text, std::string_view suffix) {
  return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool starts_with(std::string_view text, std::string_view prefix) { return text.rfind(prefix, 0) == 0; }

// An index the model gives, moved past what the scene already holds; null when it is no index
// (see gltf_json::index_from), so that nothing made up is written into the scene.
nlohmann::json shifted_json(const nlohmann::json& index, int offset) {
  const std::optional<int> number = index_from(index);
  return number ? nlohmann::json(*number + offset) : nlohmann::json();
}

// The same for an index a mesh or a skeleton cannot do without: the import stops (the editor
// reports it) instead of going on with a model that points at nothing.
nlohmann::json shifted_required(const nlohmann::json& index, int offset) {
  const std::optional<int> number = index_from(index);
  if (!number) throw std::invalid_argument("the model holds something that is not an index where glTF wants one");
  return *number + offset;
}

// A value of a material that may hold texture references, not too deep to be looked into.
bool walkable(const nlohmann::json& value, int depth) { return value.is_object() && depth <= kMaxMaterialDepth; }

// {"index": n} under a key ending in "Texture".
bool texture_reference(const std::string& key, const nlohmann::json& child) {
  return ends_with(key, kTextureSuffix) && child.is_object();
}

// A path that does not stay inside the folder it is relative to.
bool leaves_folder(const std::filesystem::path& relative) {
  return relative.empty() || relative.is_absolute() || *relative.begin() == kParentStep;
}

// The image names no file the scene can read.
bool no_picture(const std::string& uri, const std::string& from_scene) { return uri.empty() || from_scene.empty(); }

// Where a buffer view starts in the model's own data. (An offset that is not a whole number of
// zero or more is no offset: the view starts at its data.)
std::size_t own_offset(const nlohmann::json& view) {
  const auto offset = view.find(kByteOffset);
  return offset == view.end() ? 0 : size_from(*offset).value_or(0);
}

void shift_texture_refs_within(nlohmann::json& value, int offset, int depth) {
  if (!walkable(value, depth)) return;
  for (auto& [key, child] : value.items()) {
    if (texture_reference(key, child)) shift_index(child, kIndex, offset);
    shift_texture_refs_within(child, offset, depth + 1);
  }
}

// Where the scene finds a file the model names from its own folder; empty when that is outside
// the scene's folder, which a scene's pictures never are.
std::string uri_from_scene(const std::filesystem::path& file, const std::filesystem::path& scene_dir) {
  const std::filesystem::path relative = file.lexically_normal().lexically_relative(scene_dir.lexically_normal());
  if (leaves_folder(relative)) return {};
  return relative.generic_string();
}

}  // namespace

bool has_several_buffers(const nlohmann::json& model) {
  return model.value(kBuffers, nlohmann::json::array()).size() > 1;
}

void shift_index(nlohmann::json& object, const char* key, int offset) {
  if (!object.contains(key)) return;
  const nlohmann::json moved = shifted_json(object[key], offset);
  // (What is there but is no index is taken out: left in, it would be read as some index later.)
  if (moved.is_null()) {
    object.erase(key);
  } else {
    object[key] = moved;
  }
}

void shift_texture_refs(nlohmann::json& value, int offset) { shift_texture_refs_within(value, offset, 0); }

nlohmann::json imported_buffer_view(nlohmann::json view, const JsonOffsets& offsets) {
  view[kBuffer] = kOnlyBuffer;
  view[kByteOffset] = own_offset(view) + offsets.bin;
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
  const std::string uri = image[kUri].is_string() ? image[kUri].get<std::string>() : std::string();
  if (starts_with(uri, kDataUriPrefix)) return image;
  const std::string from_scene = uri_from_scene(model_dir / uri, scene_dir);
  // (A picture outside the scene's folder is not the scene's to read: the image comes without one.)
  if (no_picture(uri, from_scene)) {
    image.erase(kUri);
  } else {
    image[kUri] = from_scene;
  }
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
    for (auto& [name, accessor] : primitive[kAttributes].items())
      accessor = shifted_required(accessor, offsets.accessors);
    shift_index(primitive, kIndices, offsets.accessors);
    shift_index(primitive, kMaterial, offsets.materials);
    primitive.erase(kTargets);
  }
  mesh.erase(kWeights);
  if (has_object(mesh, kExtras)) mesh[kExtras].erase(kTargetNames);
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
  for (nlohmann::json& joint : skin[kJoints]) joint = shifted_required(joint, offsets.nodes);
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

}  // namespace redgem::model_import
