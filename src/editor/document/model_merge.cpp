// A model's JSON merged into a scene's, list by list, and taken out again.

#include "editor/document/model_import.hpp"

#include <algorithm>
#include <utility>

#include "editor/document/gltf_json.hpp"

namespace redgem::model_import {
namespace {

using namespace gltf_json;

// Adds every entry of model[key] to scene[key], renumbered by `renumber`.
template <typename Renumber>
void append_entries(nlohmann::json& scene, const nlohmann::json& model, const char* key, Renumber renumber) {
  for (const nlohmann::json& entry : list_of(model, key)) array_at(scene, key).push_back(renumber(entry));
}

// Shortens scene[key] to its first `count` entries.
void keep_first(nlohmann::json& scene, const char* key, int count) {
  if (!has_list(scene, key)) return;
  nlohmann::json& list = scene[key];
  while (static_cast<int>(list.size()) > count) list.erase(list.size() - 1);
}

void merge_geometry(nlohmann::json& scene, const nlohmann::json& model, const JsonOffsets& offsets) {
  using nlohmann::json;
  append_entries(scene, model, kBufferViews, [&](const json& view) { return imported_buffer_view(view, offsets); });
  append_entries(scene, model, kAccessors, [&](const json& accessor) { return imported_accessor(accessor, offsets); });
}

void merge_materials(nlohmann::json& scene, const nlohmann::json& model, const JsonOffsets& offsets,
                     const std::filesystem::path& model_dir, const std::filesystem::path& scene_dir) {
  using nlohmann::json;
  append_entries(scene, model, kImages,
                 [&](const json& image) { return imported_image(image, offsets, model_dir, scene_dir); });
  append_entries(scene, model, kSamplers, [](const json& sampler) { return sampler; });
  append_entries(scene, model, kTextures, [&](const json& texture) { return imported_texture(texture, offsets); });
  append_entries(scene, model, kMaterials, [&](const json& material) { return imported_material(material, offsets); });
}

}  // namespace

void merge_string_list(nlohmann::json& into, const nlohmann::json& from, const char* key) {
  if (!from.contains(key)) return;
  for (const nlohmann::json& name : from[key]) {
    if (!into[key].is_array()) into[key] = nlohmann::json::array();
    if (std::find(into[key].begin(), into[key].end(), name) == into[key].end()) into[key].push_back(name);
  }
}

void merge_model(nlohmann::json& scene, const nlohmann::json& model, const JsonOffsets& offsets,
                 const std::filesystem::path& model_dir, const std::filesystem::path& scene_dir) {
  merge_geometry(scene, model, offsets);
  merge_materials(scene, model, offsets, model_dir, scene_dir);
  append_entries(scene, model, kMeshes, [&](const nlohmann::json& mesh) { return imported_mesh(mesh, offsets); });
  merge_string_list(scene, model, kExtensionsUsed);
  merge_string_list(scene, model, kExtensionsRequired);
}

void drop_model(nlohmann::json& scene, const JsonOffsets& offsets) {
  const std::pair<const char*, int> lists[] = {{kBufferViews, offsets.buffer_views},
                                               {kAccessors, offsets.accessors},
                                               {kImages, offsets.images},
                                               {kSamplers, offsets.samplers},
                                               {kTextures, offsets.textures},
                                               {kMaterials, offsets.materials},
                                               {kMeshes, offsets.meshes}};
  for (const auto& [key, count] : lists) keep_first(scene, key, count);
}

}  // namespace redgem::model_import
