// Part of SceneDocument: copying nodes out as a .glb of their own, and pasting such a .glb in.
// Copy cuts an outline of the scene (its JSON and descriptions, no vertices or pictures) down to
// the nodes and takes from the binary chunk only what they reach; paste takes them in the way a
// model is imported, so that what they are made of comes along whichever scene they came from.

#include "editor/document/scene_document.hpp"

#include <exception>
#include <fstream>
#include <random>
#include <utility>

#include "editor/document/glb_compact.hpp"
#include "editor/document/glb_images.hpp"
#include "editor/document/gltf_json.hpp"
#include "editor/document/model_import.hpp"
#include "editor/node_tree.hpp"
#include "scene/gltf/gltf_loader.hpp"

namespace redgem {
namespace {

using namespace node_tree;
using gltf_json::kBuffers;
using gltf_json::kByteLength;

constexpr const char* kPasteFolderPrefix = "redgem-paste-";  // a folder of its own for each paste
constexpr const char* kPasteFileName = "clipboard.glb";      // the file a paste is read from, for a moment
constexpr const char* kClipboardName = "the clipboard";      // what errors call what is pasted

bool write_file(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  return file && file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

// What a paste that cannot be done returns, with why in `error`.
std::vector<int> refused(std::string& error, std::string why) {
  error = std::move(why);
  return {};
}

// A folder only this user can read, in the system's temporary folder, for the file a paste is
// read from (the loader reads files): gone once the paste is done with it, however it ended. A
// random number keeps two editors that paste at once apart.
struct PasteFolder {
  std::filesystem::path path =
      std::filesystem::temp_directory_path() / (kPasteFolderPrefix + std::to_string(std::random_device{}()));
  bool made = false;

  PasteFolder() {
    std::error_code error;
    made = std::filesystem::create_directory(path, error);
    if (made) std::filesystem::permissions(path, std::filesystem::perms::owner_all, error);
  }
  ~PasteFolder() {
    std::error_code ignored;
    if (made) std::filesystem::remove_all(path, ignored);
  }
  PasteFolder(const PasteFolder&) = delete;
  PasteFolder& operator=(const PasteFolder&) = delete;
};

// A copied mesh without its vertices: what writing the clip's JSON needs of it.
MeshDesc without_vertices(const MeshDesc& mesh) {
  MeshDesc outline;
  outline.name = mesh.name;
  outline.bounds = mesh.bounds;
  outline.primitives.resize(mesh.primitives.size());
  for (std::size_t i = 0; i < mesh.primitives.size(); ++i) outline.primitives[i].material = mesh.primitives[i].material;
  return outline;
}

// The clip's file, when it holds something that can be pasted: objects, in one buffer. nullopt,
// with why in `error`, otherwise.
std::optional<GlbFile> pasteable_clip(const std::filesystem::path& path, const SceneDesc& clip, std::string& error) {
  if (clip.nodes.empty()) {
    error = std::string(kClipboardName) + " holds no objects";
    return std::nullopt;
  }
  std::optional<GlbFile> source = read_glb(path, error);
  if (!source) return std::nullopt;
  if (!model_import::has_several_buffers(source->json)) return source;
  error = std::string(kClipboardName) + " keeps its data in several buffers, which cannot be pasted";
  return std::nullopt;
}

}  // namespace

std::optional<std::vector<std::uint8_t>> SceneDocument::copy_nodes(const std::vector<int>& roots, std::string& error) {
  try {
    return clipboard_glb(roots, error);
  } catch (const std::exception& problem) {
    error = unusable(path_, problem);
    return std::nullopt;
  }
}

std::optional<std::vector<std::uint8_t>> SceneDocument::clipboard_glb(const std::vector<int>& roots,
                                                                      std::string& error) {
  // Reshaped terrains are written into the binary chunk first, which the clip takes its bytes from.
  for (const int mesh : unsaved_meshes_) {
    if (!store_mesh_vertices(mesh, error)) return std::nullopt;
  }
  SceneDocument clip = outline();
  clip.keep_only(roots);
  if (!clip.sync_glb(error)) return std::nullopt;
  // The outline keeps no bytes of its own: its JSON points into the scene's binary chunk.
  if (!glb_.bin.empty()) clip.glb_.json[kBuffers] = nlohmann::json::array({{{kByteLength, glb_.bin.size()}}});
  // (Only what the clip's nodes reach is taken from the scene's binary chunk.)
  GlbFile lean = without_unused(clip.glb_.json, glb_.bin).value_or(GlbFile{clip.glb_.json, glb_.bin});
  glb_images::embed_linked_images(lean, path_.parent_path());
  return encode_glb(lean, error);
}

SceneDocument SceneDocument::outline() const {
  SceneDocument copy;
  copy.path_ = path_;
  copy.glb_.json = glb_.json;
  copy.source_nodes_ = source_nodes_;
  copy.source_skins_ = source_skins_;
  copy.source_animations_ = source_animations_;
  copy.skin_joint_sources_ = skin_joint_sources_;
  copy.channel_node_sources_ = channel_node_sources_;
  copy.desc_.nodes = desc_.nodes;
  copy.desc_.materials = desc_.materials;
  copy.desc_.skins = desc_.skins;
  copy.desc_.animations = desc_.animations;
  for (const MeshDesc& mesh : desc_.meshes) copy.desc_.meshes.push_back(without_vertices(mesh));
  return copy;
}

void SceneDocument::keep_only(const std::vector<int>& roots) {
  std::vector<Transform> worlds;
  worlds.reserve(roots.size());
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
  if (!can_be_parent(parent)) return refused(error, "invalid parent object");
  const PasteFolder folder;
  const std::filesystem::path file = folder.path / kPasteFileName;
  if (!folder.made) return refused(error, "cannot make " + folder.path.string());
  if (!write_file(file, glb)) return refused(error, "cannot write " + file.string());
  try {
    return paste_file(file, parent, error);
  } catch (const std::exception& problem) {
    return refused(error, std::string(kClipboardName) + " cannot be pasted: " + problem.what());
  }
}

std::vector<int> SceneDocument::paste_file(const std::filesystem::path& path, int parent, std::string& error) {
  const LoadResult loaded = load_gltf_scene(path.string());
  if (!loaded.ok) return refused(error, loaded.error);
  const std::optional<GlbFile> source = pasteable_clip(path, loaded.scene, error);
  if (!source) return {};
  return add_pasted(loaded.scene, *source, path, parent);
}

std::vector<int> SceneDocument::add_pasted(const SceneDesc& clip, const GlbFile& source,
                                           const std::filesystem::path& path, int parent) {
  // Only now that something will be added is the scene changed, and an undo step recorded.
  const PasteMark mark = paste_mark();
  const model_import::JsonOffsets offsets = merge_file(source, path);
  checkpoint();
  try {
    return append_pasted(clip, offsets, parent);
  } catch (const std::exception&) {
    take_back_paste(mark, offsets);
    throw;
  }
}

SceneDocument::PasteMark SceneDocument::paste_mark() const {
  return {glb_.bin.size(),         source_animations_.size(),  desc_.meshes.size(),
          desc_.materials.size(),  desc_.images.size(),        desc_.skins.size(),
          desc_.animations.size(), skin_joint_sources_.size(), channel_node_sources_.size()};
}

void SceneDocument::take_back_paste(const PasteMark& mark, const model_import::JsonOffsets& offsets) {
  Snapshot before = pop_step(undo_);
  restore(before);  // the nodes as they were (and every material's values)
  desc_.meshes.resize(mark.meshes);
  desc_.materials.resize(mark.materials);
  desc_.images.resize(mark.images);
  desc_.skins.resize(mark.skins);
  desc_.animations.resize(mark.animations);
  skin_joint_sources_.resize(mark.skin_joints);
  channel_node_sources_.resize(mark.channels);
  forget_model(offsets, mark.bin, mark.source_animations);
  refresh_node_references();
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
