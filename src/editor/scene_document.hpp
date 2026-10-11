#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include "editor/glb_file.hpp"
#include "scene/primitives.hpp"
#include "scene/scene_desc.hpp"
#include "scene/terrain.hpp"

namespace redgem {

namespace model_import {
struct JsonOffsets;
struct DescOffsets;
}  // namespace model_import

// The scene being edited: the loaded glTF plus editor changes, with undo/redo and saving back
// into the same .glb. Existing meshes and textures are kept as they are; created primitives and
// imported models append theirs.
class SceneDocument {
 public:
  static std::variant<SceneDocument, std::string> open(const std::filesystem::path& path);
  // A document with no objects yet, saved to `path` on save() (new games start from it).
  static SceneDocument create_empty(const std::filesystem::path& path);

  const SceneDesc& desc() const { return desc_; }
  const NodeDesc& node(int index) const { return desc_.nodes.at(index); }
  int node_count() const { return static_cast<int>(desc_.nodes.size()); }
  const std::filesystem::path& path() const { return path_; }
  // True when the nodes or materials differ from what is on disk (undoing back to the saved
  // state clears it).
  bool dirty() const { return version_ != saved_version_; }

  // Records an undo step. Edits below call it themselves unless `record` is false, so a gizmo
  // drag can call checkpoint() once and then stream unrecorded transform updates.
  void checkpoint();
  void set_name(int index, const std::string& name);
  void set_local(int index, const Transform& local, bool record = true);
  void set_prop(int index, const std::string& key, const PropertyValue& value, bool record = true);
  // Light / camera settings of a node that has one (ignored otherwise).
  void set_light(int index, const LightDesc& light, bool record = true);
  void set_camera(int index, const CameraDesc& camera, bool record = true);
  void remove_prop(int index, const std::string& key, bool record = true);
  // Copies the node and its children next to it. Returns the index of the copy. A skinned
  // character copied together with its bones gets its own skeleton, in the pose it had.
  int duplicate(int index);
  // Removes the node and its children. Indices of later nodes shift down.
  void remove(int index);

  // Creating objects. Each returns the new node index and records one undo step.
  int add_empty(const std::string& name, int parent = kNoIndex);
  // A built-in shape with its own material of `color`, plus a fitting static collider.
  int add_primitive(PrimitiveShape shape, const std::string& name, Color color, int parent = kNoIndex);
  int add_light(LightType type, const std::string& name, Color color, float intensity, int parent = kNoIndex);
  int add_camera(const std::string& name, float fov_y_radians, int parent = kNoIndex);
  // A terrain: flat ground of `color` that can be sculpted, with a collider that follows its shape.
  int add_terrain(const std::string& name, const TerrainGrid& grid, Color color, int parent = kNoIndex);
  // The grid of a terrain node; nullopt when the node is not a terrain made here.
  std::optional<TerrainGrid> terrain_grid(int node) const;
  // One brush stroke on a terrain, in the terrain's own space. Like a gizmo drag, it is not
  // recorded by itself: call checkpoint() once when a stroke begins. False when nothing changed.
  bool sculpt_terrain(int node, const TerrainBrush& brush);
  // Meshes reshaped since the last call (the renderer has to upload their vertices again).
  std::vector<int> take_changed_meshes();
  // Moves a node (and its children) under `new_parent` (kNoIndex = scene root), keeping its world
  // placement. Returns the node's new index (nodes keep parents before children). Refuses cycles
  // and invalid indices (returns `index` unchanged).
  int reparent(int index, int new_parent);
  // Adds every object of another .glb (meshes, materials, textures) under a new empty named after
  // the file, itself under `parent`. Returns that node, or kNoIndex with `error` set. Skeletons
  // and animations come along. Models using several buffers are refused; morph targets and the
  // model's lights/cameras are dropped.
  int import_model(const std::filesystem::path& path, std::string& error, int parent = kNoIndex);

  // Materials (shared by every object whose mesh uses them).
  int material_count() const { return static_cast<int>(desc_.materials.size()); }
  const MaterialDesc& material(int index) const { return desc_.materials.at(index); }
  // Material of the first part of the node's mesh, or kNoIndex.
  int material_of(int node) const;
  // How many nodes show this material.
  int material_users(int material) const;
  void set_material(int index, const MaterialDesc& material, bool record = true);

  // Drops the last checkpoint if nothing changed since (a click on a gizmo or field without moving it).
  void drop_checkpoint_if_unchanged();

  bool can_undo() const { return !undo_.empty(); }
  bool can_redo() const { return !redo_.empty(); }
  bool undo();
  bool redo();

  bool save(std::string& error);
  bool save_as(const std::filesystem::path& path, std::string& error);

  // A name not used by any node: "Crate" -> "Crate.001" (Blender style).
  std::string unique_name(const std::string& base) const;

 private:
  // The vertices of a mesh that can be reshaped (a terrain).
  struct MeshShape {
    std::vector<float> positions;
    std::vector<float> normals;
  };
  struct Snapshot {
    std::vector<NodeDesc> nodes;
    std::vector<MaterialDesc> materials;
    long long version = 0;
    std::map<int, MeshShape> shapes;  // of every terrain, by mesh index
  };

  SceneDocument() = default;
  void touch();
  Snapshot snapshot() const;
  // Goes back to a snapshot (its nodes are moved out of it).
  void restore(Snapshot& state);
  // Puts the terrains back as a snapshot had them.
  void restore_shapes(const std::map<int, MeshShape>& shapes);
  // A mesh whose vertices changed: new bounds, and the renderer and the next save get to know.
  void mark_reshaped(int mesh);
  // Writes a reshaped mesh's vertices over its old ones in the binary chunk (same count, so the
  // file does not grow).
  bool store_mesh_vertices(int mesh, std::string& error);
  bool sync_glb(std::string& error);
  // Writes every node's light and camera into the glTF arrays (copies get their own entry).
  void sync_lights_and_cameras();
  // Writes the nodes in editor order; returns the numbers of the top-level ones.
  nlohmann::json sync_nodes();
  void sync_materials();
  // source_node number -> current node index (kNoIndex for deleted nodes).
  std::vector<int> current_node_indices() const;
  // Remembers which source nodes the description's skins and animations (from `first_skin` /
  // `first_animation` on) point at, given they currently hold description indices.
  void remember_node_references(std::size_t first_skin, std::size_t first_animation);
  // Points the description's skins and animations at the nodes' current indices. Call after
  // anything that reorders, removes or restores nodes.
  void refresh_node_references();
  // Writes skins and animations into the glTF JSON with current node numbers.
  void sync_skins_and_animations(nlohmann::json& json) const;
  // Points every node at its skin's number in the file (`written`: skin index -> that number, or
  // kNoIndex for a skin left out).
  void sync_node_skins(nlohmann::json& json, const std::vector<int>& written) const;
  // The copies made by duplicate() (`count` nodes from `first_copy` on) become nodes of their own
  // in the file, with skeletons of their own. `copy_of`: node index -> its copy, or kNoIndex.
  void give_copies_own_source_nodes(int first_copy, std::size_t count);
  void give_copies_own_skins(int first_copy, std::size_t count, const std::vector<int>& copy_of);
  // Index of a copy of a skin that moves the copied bones; nullopt when some of its bones were
  // not copied.
  std::optional<int> copy_skin(int original, const std::vector<int>& copy_of);
  // The three steps of import_model: where the model's entries will start, its glTF JSON, and its
  // scene description under a new object called `name`.
  model_import::JsonOffsets import_offsets() const;
  void append_model_json(const nlohmann::json& model, const model_import::JsonOffsets& offsets,
                         const std::filesystem::path& model_dir);
  int append_model_description(const SceneDesc& model, const model_import::JsonOffsets& json_offsets,
                               const std::string& name, int parent);
  void append_model_nodes(const SceneDesc& model, const model_import::DescOffsets& offsets);
  int append_node(NodeDesc node);
  void restore_materials(const std::vector<MaterialDesc>& snapshot);
  int append_material(const MaterialDesc& material);
  // A material with just a color and a roughness (what new shapes and terrains get).
  int append_plain_material(const std::string& name, Color color, float roughness);
  int append_mesh(const std::string& name, const PrimitiveDesc& primitive);
  // Appends raw bytes to the binary chunk, aligned; returns where they start.
  std::size_t append_binary(const std::uint8_t* data, std::size_t bytes);
  // Appends raw bytes to the binary chunk as a new buffer view; returns its index.
  int append_buffer_view(const void* data, std::size_t bytes, int target);
  // Describes a buffer view's elements; returns the accessor's index.
  int append_accessor(int view, int component, std::size_t count, const char* type);
  // One float stream of a mesh (positions, normals, texture coordinates) as a new accessor.
  int append_vertex_stream(const std::vector<float>& values, std::size_t vertices, const char* type);

  std::filesystem::path path_;
  GlbFile glb_;
  // glTF JSON of every node as loaded or imported, indexed by NodeDesc::source_node. Saving
  // rewrites glb_.json["nodes"] in editor order, so this copy is what keeps those indices valid.
  nlohmann::json source_nodes_ = nlohmann::json::array();
  // Skeletons and animations as loaded or imported (glTF JSON, node references in source_node
  // numbering). Saving writes them back with the nodes' current numbers.
  nlohmann::json source_skins_ = nlohmann::json::array();
  nlohmann::json source_animations_ = nlohmann::json::array();
  // The same references for the scene description: per skin its joints, per animation its
  // channels' nodes, as source_node numbers. desc_ holds them as current node indices.
  std::vector<std::vector<int>> skin_joint_sources_;
  std::vector<std::vector<int>> channel_node_sources_;
  SceneDesc desc_;
  std::vector<Snapshot> undo_;
  std::vector<Snapshot> redo_;
  // Every distinct state of the nodes gets a version; the saved one is remembered.
  long long version_ = 0;
  long long next_version_ = 0;
  long long saved_version_ = 0;
  bool changed_since_checkpoint_ = false;
  std::set<int> changed_meshes_;  // reshaped since the renderer last asked
  std::set<int> unsaved_meshes_;  // reshaped since the last save
};

// Pieces of SceneDocument that need no document. Declared here for the unit tests.
namespace scene_document_detail {

// "Crate" and 1 -> "Crate.001".
std::string numbered_name(const std::string& base, int number);
// Indices of `index` and all its descendants, parents first.
std::vector<int> subtree(const std::vector<NodeDesc>& nodes, int index);
// Where a node is in the world, following its parents.
Transform world_of(const std::vector<NodeDesc>& nodes, int index);
// The indices 0..count-1 in order without `removed`, and with `last` moved to the end.
std::vector<int> order_without(int count, const std::vector<int>& removed);
std::vector<int> order_with_last(int count, const std::vector<int>& last);
// The nodes listed by `order` (old indices), with parents and children renumbered to match.
// Children left out of the order are dropped from their parent.
std::vector<NodeDesc> nodes_in_order(const std::vector<NodeDesc>& nodes, const std::vector<int>& order);

}  // namespace scene_document_detail

}  // namespace redgem
