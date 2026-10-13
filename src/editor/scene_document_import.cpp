// Part of SceneDocument: a model from another .glb taken into the scene, under a new object.

#include "editor/scene_document.hpp"

#include <exception>
#include <optional>

#include "editor/glb_geometry.hpp"
#include "editor/gltf_json.hpp"
#include "editor/model_import.hpp"
#include "editor/node_tree.hpp"
#include "render/gpu_profile.hpp"
#include "scene/gltf_loader.hpp"

namespace redgem {
namespace {

using namespace gltf_json;

int count_of(const nlohmann::json& json, const char* key) {
  return json.contains(key) ? static_cast<int>(json[key].size()) : 0;
}

// A model's .glb, when it is one the editor can take in.
std::optional<GlbFile> read_importable_glb(const std::filesystem::path& path, std::string& error) {
  std::optional<GlbFile> source = read_glb(path, error);
  if (!source) return std::nullopt;
  if (!model_import::has_several_buffers(source->json)) return source;
  error = "models with several buffers are not supported (export a single .glb from Blender)";
  return std::nullopt;
}

// Where a model's entries start in the scene description: the same places as in the glTF JSON
// (the loader keeps glTF order for these), and after the skins the description has.
model_import::DescOffsets description_offsets(const model_import::JsonOffsets& json_offsets, std::size_t skins) {
  model_import::DescOffsets offsets;
  offsets.images = json_offsets.images;
  offsets.materials = json_offsets.materials;
  offsets.meshes = json_offsets.meshes;
  offsets.source_nodes = json_offsets.nodes;
  offsets.skins = static_cast<int>(skins);
  return offsets;
}

}  // namespace

int SceneDocument::import_model(const std::filesystem::path& path, std::string& error, int parent) {
  try {
    return import_model_file(path, error, parent);
  } catch (const std::exception& problem) {
    error = unusable(path, problem);
    return kNoIndex;
  }
}

int SceneDocument::import_model_file(const std::filesystem::path& path, std::string& error, int parent) {
  if (!can_be_parent(parent)) {
    error = "invalid parent object";
    return kNoIndex;
  }
  const LoadResult loaded = load_gltf_scene(path.string(), gpu_profile::kMaxSkinJoints);
  if (!loaded.ok) {
    error = loaded.error;
    return kNoIndex;
  }
  const std::optional<GlbFile> source = read_importable_glb(path, error);
  if (!source) return kNoIndex;
  return append_model(loaded.scene, *source, path, parent);
}

model_import::JsonOffsets SceneDocument::import_offsets() const {
  // Offsets of everything the imported file references by index.
  model_import::JsonOffsets offsets;
  offsets.buffer_views = count_of(glb_.json, kBufferViews);
  offsets.accessors = count_of(glb_.json, kAccessors);
  offsets.images = count_of(glb_.json, kImages);
  offsets.samplers = count_of(glb_.json, kSamplers);
  offsets.textures = count_of(glb_.json, kTextures);
  offsets.materials = material_count();
  offsets.meshes = static_cast<int>(desc_.meshes.size());
  offsets.nodes = static_cast<int>(source_nodes_.size());
  offsets.skins = static_cast<int>(source_skins_.size());
  return offsets;
}

int SceneDocument::append_model(const SceneDesc& scene, const GlbFile& source, const std::filesystem::path& path,
                                int parent) {
  model_import::JsonOffsets offsets = import_offsets();
  const std::size_t bin_size = glb_.bin.size();
  const std::size_t animations = source_animations_.size();
  offsets.bin = glb_geometry::append_binary(glb_, source.bin.data(), source.bin.size());
  try {
    append_model_sources(source.json, offsets);
    model_import::merge_model(glb_.json, source.json, offsets, path.parent_path(), path_.parent_path());
  } catch (const std::exception&) {
    forget_model(offsets, bin_size, animations);
    throw;
  }
  checkpoint();
  return append_model_description(scene, offsets, path.stem().string(), parent);
}

void SceneDocument::append_model_sources(const nlohmann::json& model, const model_import::JsonOffsets& offsets) {
  using nlohmann::json;
  for (const json& node : list_of(model, kNodes)) source_nodes_.push_back(model_import::imported_node(node, offsets));
  for (const json& skin : list_of(model, kSkins)) source_skins_.push_back(model_import::imported_skin(skin, offsets));
  for (const json& animation : list_of(model, kAnimations)) {
    source_animations_.push_back(model_import::imported_animation(animation, offsets));
  }
}

void SceneDocument::forget_model(const model_import::JsonOffsets& offsets, std::size_t bin_size,
                                 std::size_t animations) {
  glb_.bin.resize(bin_size);
  model_import::drop_model(glb_.json, offsets);
  while (static_cast<int>(source_nodes_.size()) > offsets.nodes) source_nodes_.erase(source_nodes_.size() - 1);
  while (static_cast<int>(source_skins_.size()) > offsets.skins) source_skins_.erase(source_skins_.size() - 1);
  while (source_animations_.size() > animations) source_animations_.erase(source_animations_.size() - 1);
}

int SceneDocument::append_model_description(const SceneDesc& model, const model_import::JsonOffsets& json_offsets,
                                            const std::string& name, int parent) {
  model_import::DescOffsets offsets = description_offsets(json_offsets, desc_.skins.size());
  append_model_assets(model, offsets);
  offsets.root = append_node(node_tree::named_node(name, parent));
  offsets.nodes = node_count();
  append_model_nodes(model, offsets);
  touch();
  return offsets.root;
}

void SceneDocument::append_model_assets(const SceneDesc& model, const model_import::DescOffsets& offsets) {
  for (const ImageData& image : model.images) desc_.images.push_back(image);
  for (const MaterialDesc& material : model.materials) {
    desc_.materials.push_back(model_import::imported_material(material, offsets));
  }
  for (const MeshDesc& mesh : model.meshes) desc_.meshes.push_back(model_import::imported_mesh(mesh, offsets));
}

void SceneDocument::append_model_nodes(const SceneDesc& model, const model_import::DescOffsets& offsets) {
  for (const NodeDesc& imported : model.nodes) {
    NodeDesc node = model_import::imported_node(imported, offsets);
    node.name = unique_name(node.name);
    desc_.nodes.push_back(std::move(node));
    if (desc_.nodes.back().parent == offsets.root) desc_.nodes[offsets.root].children.push_back(node_count() - 1);
  }
  // The imported skeletons and animations point at the imported nodes (now further on in the list).
  const std::size_t first_animation = desc_.animations.size();
  for (const SkinDesc& skin : model.skins) desc_.skins.push_back(model_import::imported_skin(skin, offsets));
  for (const AnimationDesc& animation : model.animations) {
    desc_.animations.push_back(model_import::imported_animation(animation, offsets));
  }
  remember_node_references(static_cast<std::size_t>(offsets.skins), first_animation);
}

}  // namespace redgem
