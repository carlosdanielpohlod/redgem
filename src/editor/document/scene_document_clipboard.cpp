// Part of SceneDocument: copying nodes out as a .glb of their own, and pasting such a .glb in.
// Copy cuts a copy of the scene down to the nodes; paste takes them in the way a model is
// imported, so that what they are made of comes along whichever scene they came from.

#include "editor/document/scene_document.hpp"

#include <exception>
#include <fstream>
#include <random>

#include "editor/document/glb_compact.hpp"
#include "editor/document/glb_images.hpp"
#include "editor/document/model_import.hpp"
#include "editor/node_tree.hpp"
#include "scene/gltf/gltf_loader.hpp"

namespace redgem {
namespace {

using namespace node_tree;

constexpr const char* kPasteFilePrefix = "redgem-paste-";  // the file a paste is read from, for a moment
constexpr const char* kPasteFileExtension = ".glb";
constexpr const char* kClipboardName = "the clipboard";  // what errors call what is pasted

// A file of its own for each paste, in the system's temporary folder (a random number keeps two
// editors that paste at once apart).
std::filesystem::path paste_file_path() {
  const std::string name = kPasteFilePrefix + std::to_string(std::random_device{}()) + kPasteFileExtension;
  return std::filesystem::temp_directory_path() / name;
}

bool write_file(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  return file && file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

// The temporary file is gone once the paste is done with it, however it ended.
struct RemovedAfter {
  std::filesystem::path path;
  ~RemovedAfter() {
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
  }
};

}  // namespace

std::optional<std::vector<std::uint8_t>> SceneDocument::copy_nodes(const std::vector<int>& roots,
                                                                   std::string& error) const {
  try {
    return clipboard_glb(roots, error);
  } catch (const std::exception& problem) {
    error = unusable(path_, problem);
    return std::nullopt;
  }
}

std::optional<std::vector<std::uint8_t>> SceneDocument::clipboard_glb(const std::vector<int>& roots,
                                                                      std::string& error) const {
  SceneDocument clip = without_history();
  clip.keep_only(roots);
  if (!clip.sync_for_writing(error)) return std::nullopt;
  GlbFile lean = without_unused(clip.glb_).value_or(clip.glb_);
  glb_images::embed_linked_images(lean, path_.parent_path());
  return encode_glb(lean, error);
}

SceneDocument SceneDocument::without_history() const {
  SceneDocument copy;
  copy.path_ = path_;
  copy.glb_ = glb_;
  copy.source_nodes_ = source_nodes_;
  copy.source_skins_ = source_skins_;
  copy.source_animations_ = source_animations_;
  copy.skin_joint_sources_ = skin_joint_sources_;
  copy.channel_node_sources_ = channel_node_sources_;
  copy.desc_ = desc_;
  copy.unsaved_meshes_ = unsaved_meshes_;
  return copy;
}

void SceneDocument::keep_only(const std::vector<int>& roots) {
  std::vector<Transform> worlds;
  for (const int root : roots) worlds.push_back(world_of(desc_.nodes, root));
  const std::vector<int> order = subtrees(desc_.nodes, roots);
  const std::vector<int> position = positions_in(order, desc_.nodes.size());
  desc_.nodes = nodes_in_order(desc_.nodes, order);
  for (std::size_t i = 0; i < roots.size(); ++i) {
    NodeDesc& root = desc_.nodes[position[roots[i]]];
    root.parent = kNoIndex;
    root.local = worlds[i];
  }
  refresh_node_references();
}

std::vector<int> SceneDocument::paste_nodes(const std::vector<std::uint8_t>& glb, int parent, std::string& error) {
  if (!can_be_parent(parent)) {
    error = "invalid parent object";
    return {};
  }
  const RemovedAfter file{paste_file_path()};
  if (!write_file(file.path, glb)) {
    error = "cannot write " + file.path.string();
    return {};
  }
  try {
    return paste_file(file.path, parent, error);
  } catch (const std::exception& problem) {
    error = std::string(kClipboardName) + " cannot be pasted: " + problem.what();
    return {};
  }
}

std::vector<int> SceneDocument::paste_file(const std::filesystem::path& path, int parent, std::string& error) {
  const LoadResult loaded = load_gltf_scene(path.string());
  if (!loaded.ok) {
    error = loaded.error;
    return {};
  }
  const std::optional<GlbFile> source = read_glb(path, error);
  if (!source) return {};
  const model_import::JsonOffsets offsets = merge_file(*source, path);
  checkpoint();
  return append_pasted(loaded.scene, offsets, parent);
}

std::vector<int> SceneDocument::append_pasted(const SceneDesc& clip, const model_import::JsonOffsets& json_offsets,
                                              int parent) {
  model_import::DescOffsets offsets = model_import::description_offsets(json_offsets, desc_.skins.size());
  append_model_assets(clip, offsets);
  offsets.root = kNoIndex;  // the clip's top-level nodes stay at the top level, until placed below
  offsets.nodes = node_count();
  append_model_nodes(clip, offsets);
  keep_lights_and_cameras(clip, offsets.nodes);
  touch();
  return place_pasted_roots(offsets.nodes, parent);
}

void SceneDocument::keep_lights_and_cameras(const SceneDesc& clip, int first) {
  // (An imported model's lights and cameras are left out; copied objects keep theirs. Saving
  // gives each its own entry in the file.)
  for (std::size_t i = 0; i < clip.nodes.size(); ++i) {
    NodeDesc& pasted = desc_.nodes[first + static_cast<int>(i)];
    pasted.light = clip.nodes[i].light;
    pasted.camera = clip.nodes[i].camera;
  }
}

std::vector<int> SceneDocument::place_pasted_roots(int first, int parent) {
  std::vector<int> roots;
  for (int i = first; i < node_count(); ++i) {
    if (desc_.nodes[i].parent != kNoIndex) continue;
    if (parent != kNoIndex) move_under(desc_.nodes, i, parent, local_under(desc_.nodes, i, parent));
    roots.push_back(i);
  }
  return roots;
}

}  // namespace redgem
