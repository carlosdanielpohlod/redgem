#pragma once

// Renumbering of a model that is added to a scene. Everything in a glTF file points at other
// things by their position in a list, so what an imported file brings has to be shifted past what
// the scene already holds. Pure functions over JSON and scene descriptions.

#include <cstddef>
#include <filesystem>

#include <nlohmann/json.hpp>

#include "scene/scene_desc.hpp"

namespace redgem::model_import {

// How many of each thing the scene's file holds before the import: what the model's own indices
// are shifted by.
struct JsonOffsets {
  int buffer_views = 0;
  int accessors = 0;
  int images = 0;
  int samplers = 0;
  int textures = 0;
  int materials = 0;
  int meshes = 0;
  int nodes = 0;
  int skins = 0;
  std::size_t bin = 0;  // bytes: where the model's binary data starts in the scene's
};

// The same for the scene description, whose node and skin lists are numbered on their own.
struct DescOffsets {
  int images = 0;
  int materials = 0;
  int meshes = 0;
  int skins = 0;
  int nodes = 0;         // index the model's first node gets
  int source_nodes = 0;  // JsonOffsets::nodes
  int root = kNoIndex;   // the object the model's top-level nodes go under
};

// True when a file keeps its data in more than one buffer (which the editor cannot merge).
bool has_several_buffers(const nlohmann::json& model);

// Adds `offset` to object[key] when the object has that key.
void shift_index(nlohmann::json& object, const char* key, int offset);
// Shifts every texture reference ({"index": n} under a key ending in "Texture") in a material,
// including the ones inside material extensions (specular, clearcoat, transmission...).
void shift_texture_refs(nlohmann::json& value, int offset);
// Adds the names of from[key] that into[key] does not list yet.
void merge_string_list(nlohmann::json& into, const nlohmann::json& from, const char* key);

// Each kind of entry of the model's JSON, renumbered for the scene's file.
nlohmann::json imported_buffer_view(nlohmann::json view, const JsonOffsets& offsets);
nlohmann::json imported_accessor(nlohmann::json accessor, const JsonOffsets& offsets);
// External image files stay where they are: the scene points at them from its own folder.
nlohmann::json imported_image(nlohmann::json image, const JsonOffsets& offsets, const std::filesystem::path& model_dir,
                              const std::filesystem::path& scene_dir);
nlohmann::json imported_texture(nlohmann::json texture, const JsonOffsets& offsets);
nlohmann::json imported_material(nlohmann::json material, const JsonOffsets& offsets);
// Morph targets are not supported: they are dropped with their weights and names.
nlohmann::json imported_mesh(nlohmann::json mesh, const JsonOffsets& offsets);
// The model's cameras and lights are not imported; children are rebuilt on save.
nlohmann::json imported_node(nlohmann::json node, const JsonOffsets& offsets);
nlohmann::json imported_skin(nlohmann::json skin, const JsonOffsets& offsets);
nlohmann::json imported_animation(nlohmann::json animation, const JsonOffsets& offsets);

// The same entries of the scene description.
MaterialDesc imported_material(const MaterialDesc& original, const DescOffsets& offsets);
MeshDesc imported_mesh(MeshDesc mesh, const DescOffsets& offsets);
// The node keeps its place in the model's tree; the model's top-level nodes go under the root.
NodeDesc imported_node(NodeDesc node, const DescOffsets& offsets);
SkinDesc imported_skin(SkinDesc skin, const DescOffsets& offsets);
AnimationDesc imported_animation(AnimationDesc animation, const DescOffsets& offsets);

}  // namespace redgem::model_import
