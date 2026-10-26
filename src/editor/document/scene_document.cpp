// The scene file: opening a .glb, and writing the edits back into it.

#include "editor/document/scene_document.hpp"

#include <exception>
#include <optional>
#include <set>
#include <utility>

#include "core/log.hpp"
#include "editor/document/glb_compact.hpp"
#include "editor/document/gltf_json.hpp"
#include "editor/document/material_textures.hpp"
#include "scene/gltf/gltf_checks.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "scene/scene_ids.hpp"

namespace redgem {

using namespace gltf_json;

namespace {

// An image's or buffer's "uri" ("" without one).
std::string uri_of(const nlohmann::json& entry) {
  return entry.is_object() ? entry.value(kUri, std::string()) : std::string();
}

// A glTF "uri" names a file next to the scene: one is given (none at all is the .glb's own binary
// chunk), and it is no data written into the JSON itself.
bool names_a_linked_file(const std::string& uri) { return !uri.empty() && gltf_checks::names_a_file(uri.c_str()); }

}  // namespace

std::variant<SceneDocument, std::string> SceneDocument::open(const std::filesystem::path& path) {
  try {
    return open_file(path);
  } catch (const std::exception& problem) {
    return unusable(path, problem);
  }
}

std::variant<SceneDocument, std::string> SceneDocument::open_file(const std::filesystem::path& path) {
  LoadResult loaded = load_gltf_scene(path.string());
  if (!loaded.ok) return loaded.error;
  for (const std::string& warning : loaded.warnings) log::warning(warning);
  std::string error;
  std::optional<GlbFile> glb = read_glb(path, error);
  if (!glb) return error;
  SceneDocument document;
  document.path_ = path;
  document.keep_file(std::move(*glb));
  document.desc_ = std::move(loaded.scene);
  document.remember_ids();
  document.remember_node_references(0, 0);
  return document;
}

// The file as loaded: saving writes the edits into it.
void SceneDocument::keep_file(GlbFile glb) {
  glb_ = std::move(glb);
  source_nodes_ = list_of(glb_.json, kNodes);
  source_skins_ = list_of(glb_.json, kSkins);
  source_animations_ = list_of(glb_.json, kAnimations);
}

SceneDocument SceneDocument::create_empty(const std::filesystem::path& path) {
  SceneDocument document;
  document.path_ = path;
  document.glb_.json = empty_document();
  document.version_ = ++document.next_version_;  // a new document is unsaved
  return document;
}

std::string SceneDocument::unusable(const std::filesystem::path& path, const std::exception& problem) {
  return path.string() + " cannot be used: " + problem.what();
}

void SceneDocument::sync_lights_and_cameras() {
  // Entries are updated in place (undo snapshots keep pointing at them); a second node using the
  // same entry (a duplicate) gets a copy of its own.
  std::set<int> claimed_lights;
  std::set<int> claimed_cameras;
  for (NodeDesc& node : desc_.nodes) {
    if (node.light) sync_light(node, *node.light, claimed_lights);
    if (node.camera) sync_camera(node, *node.camera, claimed_cameras);
  }
}

void SceneDocument::sync_light(NodeDesc& node, const LightDesc& light, std::set<int>& claimed) {
  nlohmann::json& lights = glb_.json[kExtensions][kLightsExtension][kLights];
  node.source_light = claim_entry(lights, node.source_light, claimed);
  write_light(lights[node.source_light], light);
  add_extension_used(glb_.json, kLightsExtension);
}

void SceneDocument::sync_camera(NodeDesc& node, const CameraDesc& camera, std::set<int>& claimed) {
  nlohmann::json& cameras = glb_.json[kCameras];
  node.source_camera = claim_entry(cameras, node.source_camera, claimed);
  write_camera(cameras[node.source_camera], camera);
}

nlohmann::json SceneDocument::sync_nodes() {
  add_extension_used(glb_.json, kSceneIdExtension);
  nlohmann::json nodes = nlohmann::json::array();
  nlohmann::json roots = nlohmann::json::array();
  for (int i = 0; i < node_count(); ++i) {
    const NodeDesc& node = desc_.nodes[i];
    const bool known = in_range(node.source_node, source_nodes_.size());
    nodes.push_back(
        node_json(node, known ? nlohmann::json(source_nodes_[node.source_node]) : nlohmann::json::object()));
    if (node.parent == kNoIndex) roots.push_back(i);
  }
  glb_.json[kNodes] = nodes;
  return roots;
}

void SceneDocument::sync_materials() {
  nlohmann::json& json = glb_.json;
  const nlohmann::json materials = materials_json(list_of(json, kMaterials), desc_.materials);
  if (!materials.empty()) json[kMaterials] = materials;
  // The pictures each one is drawn with (texture slots set in the Inspector).
  for (std::size_t i = 0; i < desc_.materials.size(); ++i) {
    material_textures::write_slots(json, json[kMaterials][i], desc_.materials[i]);
  }
  for (const MaterialDesc& material : desc_.materials) {
    if (needs_emissive_strength(material)) add_extension_used(json, kEmissiveStrengthExtension);
    if (material.clearcoat.factor > 0.0f) add_extension_used(json, kClearcoatExtension);
  }
}

// A .glb keeps everything in one buffer, its binary chunk; without one it has no buffers at all.
void SceneDocument::sync_buffer() {
  if (glb_.bin.empty()) {
    glb_.json.erase(kBuffers);
  } else {
    glb_.json[kBuffers] = nlohmann::json::array({{{kByteLength, glb_.bin.size()}}});
  }
}

bool SceneDocument::sync_glb(std::string& error) {
  if (desc_.nodes.empty()) {
    error = "a glTF scene needs at least one object";
    return false;
  }
  sync_lights_and_cameras();
  nlohmann::json& json = glb_.json;
  const nlohmann::json roots = sync_nodes();
  sync_materials();
  sync_buffer();
  // Only the scene the engine loads is edited; it becomes the file's single scene.
  if (has_several_scenes(json)) log::warning("saving keeps only the active glTF scene");
  keep_active_scene(json, roots);
  sync_skins_and_animations(json);
  return true;
}

std::vector<std::string> SceneDocument::linked_files() const {
  std::vector<std::string> files;
  for (const char* kind : {kImages, kBuffers}) {
    for (const nlohmann::json& entry : list_of(glb_.json, kind)) {
      const std::string uri = uri_of(entry);
      if (names_a_linked_file(uri)) files.push_back(uri);
    }
  }
  return files;
}

bool SceneDocument::save(std::string& error) { return save_as(path_, error); }

bool SceneDocument::save_as(const std::filesystem::path& path, std::string& error) {
  try {
    return save_file(path, error);
  } catch (const std::exception& problem) {
    error = unusable(path, problem);
    return false;
  }
}

bool SceneDocument::sync_for_writing(std::string& error) {
  if (!sync_glb(error)) return false;
  for (const int mesh : unsaved_meshes_) {
    if (!store_mesh_vertices(mesh, error)) return false;
  }
  return true;
}

bool SceneDocument::save_file(const std::filesystem::path& path, std::string& error) {
  if (!sync_for_writing(error)) return false;
  // What nothing uses any more (a deleted object's mesh, an import that was undone) stays out of
  // the file. It stays in the document: undo and redo can bring back what used it.
  std::optional<GlbFile> lean = without_unused(glb_);
  material_textures::forget_unused_webp(lean ? lean->json : glb_.json);
  if (!write_glb(path, lean ? *lean : glb_, error)) return false;
  unsaved_meshes_.clear();
  path_ = path;
  saved_version_ = version_;
  return true;
}

}  // namespace redgem
