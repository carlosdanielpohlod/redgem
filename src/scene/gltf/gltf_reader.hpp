#pragma once

// Turns a parsed and checked glTF file into a scene description. One class, implemented part by
// part; only those parts and the loader include this:
//   gltf_reader_materials.cpp   pictures and materials
//   gltf_reader_meshes.cpp      meshes and their parts
//   gltf_reader_nodes.cpp       the tree of objects
//   gltf_reader_skins.cpp       skeletons
//   gltf_reader_animations.cpp  animation clips

#include <cgltf.h>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "scene/gltf/gltf_loader.hpp"

namespace redgem {

// Where each channel sits in a color a file stores as floats.
inline constexpr int kRedChannel = 0;
inline constexpr int kGreenChannel = 1;
inline constexpr int kBlueChannel = 2;
inline constexpr int kAlphaChannel = 3;

// A file's own text, or `fallback` where it gives none.
inline std::string text_or(const char* text, const std::string& fallback) { return text != nullptr ? text : fallback; }

class GltfReader {
 public:
  // `image_files`: the file each picture names, by its number (see gltf_checks::named_files).
  GltfReader(const cgltf_data& data, std::vector<std::filesystem::path> image_files, LoadResult& result)
      : data_(data), image_files_(std::move(image_files)), result_(result) {}

  // Fills the result; returns what made the file unusable, or an empty text.
  std::string run() {
    load_images();
    load_materials();
    load_meshes();
    load_nodes();
    load_skins();
    keep_joints_within_skins();
    load_animations();
    return problem_;
  }

 private:
  // A node waiting to be added to the scene: under which parent, and how deep that puts it.
  struct PendingNode {
    const cgltf_node* node = nullptr;
    int parent = kNoIndex;
    int depth = 0;
  };

  template <typename T>
  static int index_of(const T* item, const T* first) {
    return item == nullptr ? kNoIndex : static_cast<int>(item - first);
  }
  void warn(std::string warning) { result_.warnings.push_back(std::move(warning)); }
  // (Once the file is found unusable, what is left of it is not read.)
  bool has_more_to_add(const std::vector<PendingNode>& pending) const { return !pending.empty() && problem_.empty(); }

  // Pictures and materials.
  // A picture, from the scene's data or from `file`, the one it names (empty for none it may read).
  ImageData load_image(const cgltf_image& image, const std::filesystem::path& file) const;
  void load_images();
  int image_of(const cgltf_texture_view& view) const;
  void read_pbr(const cgltf_material& material, MaterialDesc& out) const;
  // The pictures laid over the base color, and how strongly the first two count.
  void read_detail_textures(const cgltf_material& material, MaterialDesc& out) const;
  MaterialDesc load_material(const cgltf_material& material) const;
  void load_materials();

  // Meshes.
  void load_meshes();
  MeshDesc load_mesh(const cgltf_mesh& mesh);
  // A part of a mesh, checked here once so that everything using the scene can trust it: its
  // vertices are whole, its triangles are whole and name only vertices it has. Anything else is
  // left out with a warning.
  std::optional<PrimitiveDesc> load_primitive(const cgltf_primitive& primitive, const std::string& mesh_name);
  std::nullopt_t dropped(const std::string& mesh_name, const std::string& why);
  // A mesh exported without normals is lit as if it faced up everywhere.
  void ensure_normals(PrimitiveDesc& primitive, const std::string& mesh_name);

  // The tree of objects.
  void load_nodes();
  void add_parentless_nodes();
  // Adds a top-level node and everything under it, parents first and children in the file's order.
  void add_tree(const cgltf_node& root);
  // Adds one node and queues its children. A node is added once, however often the file lists it.
  void add_node(const PendingNode& next, std::vector<PendingNode>& pending);
  NodeDesc read_node(const cgltf_node& node, int parent, int index);
  // A node's extras (Blender custom properties), with a warning for each thing left out.
  PropertyMap read_props(const cgltf_node& node, const std::string& node_name);
  // glTF node index -> index in the scene description (nodes are stored parents-first).
  std::vector<int> node_indices() const;

  // Skeletons.
  void load_skins();
  SkinDesc load_skin(const cgltf_skin& skin, const std::vector<int>& nodes) const;
  // A vertex naming a joint past its skeleton's last follows the skeleton's first (see
  // keep_joints_within), for every skinned object's mesh.
  void keep_joints_within_skins();
  void warn_of_unused_parts(const cgltf_skin& skin, cgltf_size index);

  // Animation clips.
  void load_animations();
  AnimationDesc load_animation(const cgltf_animation& animation, cgltf_size index, const std::vector<int>& nodes);
  std::optional<AnimationChannel> load_channel(const cgltf_animation_channel& channel, const std::string& clip_name,
                                               const std::vector<int>& nodes);
  // What a channel animates; nullopt (warned about when it is worth it) for one that cannot be played.
  std::optional<AnimationPath> playable_path(const cgltf_animation_channel& channel, const std::string& clip_name);

  const cgltf_data& data_;
  std::vector<std::filesystem::path> image_files_;
  LoadResult& result_;
  std::vector<bool> added_;  // by glTF node index: already in the scene
  std::string problem_;      // what made the file unusable, when something did
};

}  // namespace redgem
