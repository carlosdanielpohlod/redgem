#pragma once

// Plain data produced by the glTF loader. No third-party types leak out of the loader.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "scene/property.hpp"
#include "scene/transform.hpp"

namespace redgem {

inline constexpr int kNoIndex = -1;
// Whether an index names one of `count` things (kNoIndex names none).
inline bool is_index_of(int index, std::size_t count) { return index >= 0 && static_cast<std::size_t>(index) < count; }
// glTF's alpha cutoff when a masked material gives none: texels less opaque than this are not drawn.
inline constexpr float kDefaultAlphaCutoff = 0.5f;
inline constexpr std::size_t kFloatsPerMatrix = 16;  // a 4x4 matrix

struct Color {
  float r = 1.0f;
  float g = 1.0f;
  float b = 1.0f;
  float a = 1.0f;
};

struct ImageData {
  std::vector<std::uint8_t> encoded;  // PNG/JPEG bytes as stored in the .glb
  std::string file_extension;         // ".png" or ".jpg", what raylib's decoder expects
};

enum class AlphaMode { Opaque, Mask, Blend };

// glTF metallic-roughness material. Image fields index SceneDesc::images (kNoIndex when absent).
struct MaterialDesc {
  std::string name;
  Color base_color;
  int base_color_image = kNoIndex;
  float metallic = 0.0f;
  float roughness = 1.0f;
  int metallic_roughness_image = kNoIndex;  // G = roughness, B = metallic (glTF convention)
  int normal_image = kNoIndex;
  float normal_scale = 1.0f;
  int occlusion_image = kNoIndex;  // R = ambient occlusion
  float occlusion_strength = 1.0f;
  Vec3 emissive = Vec3::zero();  // linear RGB, already multiplied by emissive strength
  int emissive_image = kNoIndex;
  AlphaMode alpha_mode = AlphaMode::Opaque;
  float alpha_cutoff = kDefaultAlphaCutoff;
  bool double_sided = false;
  // A blended material drawn as a fine pattern of its texels left out by how see-through they are,
  // instead of mixed with what is behind: nothing to sort, for many layers of hair cards or fur
  // (the material's custom property `transparency = "dithered"`).
  bool dithered = false;
};

struct PrimitiveDesc {
  std::vector<float> positions;        // xyz
  std::vector<float> normals;          // xyz, same count as positions
  std::vector<float> texcoords;        // uv, empty when absent
  std::vector<std::uint8_t> colors;    // rgba, empty when absent
  std::vector<std::uint32_t> indices;  // triangle list
  // Skinning (both empty on rigid meshes): for each vertex, up to 4 joints of the node's skin and
  // how much each one moves it.
  std::vector<std::uint16_t> joints;  // 4 per vertex, indices into SkinDesc::joints
  std::vector<float> weights;         // 4 per vertex
  int material = kNoIndex;
};

// Makes every joint of a primitive's vertices one of a skeleton of `joints` joints: one past its
// last becomes its first, whose bone is there (a file may name joints its skeleton does not have).
void keep_joints_within(PrimitiveDesc& primitive, std::size_t joints);

// What keeps a primitive from being drawn or collided with: no vertices, normals that are not one
// per vertex, or indices that do not make whole triangles of the vertices it has. Empty when
// nothing does. Everything that walks a primitive's indices relies on this having been checked.
std::string primitive_shape_problem(const PrimitiveDesc& primitive);

struct Bounds {
  Vec3 min = Vec3::zero();
  Vec3 max = Vec3::zero();
  bool valid = false;

  Vec3 center() const { return (min + max) * kHalf; }
  Vec3 half_extents() const { return (max - min) * kHalf; }
  void include(Vec3 point);
};

struct MeshDesc {
  std::string name;
  std::vector<PrimitiveDesc> primitives;
  Bounds bounds;  // in the mesh's local space
};

struct CameraDesc {
  float fov_y_radians = 0.0f;
  float near_plane = 0.0f;
  float far_plane = 0.0f;
};

enum class LightType { Directional, Point, Spot };

struct LightDesc {
  LightType type = LightType::Directional;
  Color color;
  float intensity = 0.0f;
  float range = 0.0f;  // meters; 0 = not given (the renderer then uses its default range)
};

// A skeleton: the nodes that deform a skinned mesh.
struct SkinDesc {
  std::vector<int> joints;  // node indices (kNoIndex for a joint whose node was deleted)
  // Per joint, the matrix that brings a vertex from the mesh's bind pose into the joint's space
  // (column-major, as in glTF).
  std::vector<std::array<float, kFloatsPerMatrix>> inverse_bind;
};

enum class AnimationPath { Translation, Rotation, Scale };

// The keyframes of one property of one node.
struct AnimationChannel {
  int node = kNoIndex;
  AnimationPath path = AnimationPath::Translation;
  std::vector<float> times;   // seconds, increasing
  std::vector<float> values;  // 3 floats per key (4 for rotations: x y z w)
};

struct AnimationDesc {
  std::string name;
  float duration = 0.0f;  // seconds
  std::vector<AnimationChannel> channels;
};

struct NodeDesc {
  std::string name;
  int parent = kNoIndex;
  std::vector<int> children;
  Transform local;
  int mesh = kNoIndex;
  int skin = kNoIndex;  // the skeleton deforming this node's mesh
  std::optional<CameraDesc> camera;
  std::optional<LightDesc> light;
  PropertyMap props;
  // Indices into the source glTF arrays, so an edited scene can be written back.
  int source_node = kNoIndex;
  int source_camera = kNoIndex;
  int source_light = kNoIndex;
};

struct SceneDesc {
  std::vector<NodeDesc> nodes;  // parents always come before their children
  std::vector<MeshDesc> meshes;
  std::vector<MaterialDesc> materials;
  std::vector<ImageData> images;
  std::vector<SkinDesc> skins;
  std::vector<AnimationDesc> animations;

  int find_node(const std::string& name) const;
};

}  // namespace redgem
