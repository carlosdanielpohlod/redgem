// Part of SceneDocument: objects made in the editor. Empties, shapes, terrains, lights and
// cameras, each with what it needs in the file.

#include "editor/document/scene_document.hpp"

#include "editor/document/glb_geometry.hpp"
#include "editor/document/gltf_json.hpp"
#include "editor/node_tree.hpp"
#include "scene/props.hpp"

namespace redgem {
namespace {

using namespace gltf_json;
using node_tree::named_node;

constexpr float kTerrainRoughness = 0.95f;  // ground: no shine
constexpr float kDefaultRoughness = 0.6f;   // of the material a new shape gets (0 mirror .. 1 matte)
constexpr float kCameraNear = 0.1f;         // meters: clip planes of a new camera
constexpr float kCameraFar = 1000.0f;
constexpr float kInfiniteRange = 0.0f;  // glTF: a light without a range reaches as far as its strength allows
constexpr const char* kTerrainCollider = props::kColliderMesh;  // things stand on the ground as it is shaped

}  // namespace

int SceneDocument::append_node(NodeDesc node) {
  node.name = unique_name(node.name);
  const int index = node_count();
  const int parent = node.parent;
  desc_.nodes.push_back(std::move(node));
  if (parent != kNoIndex) desc_.nodes.at(parent).children.push_back(index);
  touch();
  return index;
}

int SceneDocument::append_mesh(const std::string& name, const PrimitiveDesc& primitive) {
  MeshDesc mesh;
  mesh.name = name;
  mesh.primitives.push_back(primitive);
  mesh.bounds = mesh_bounds_of(primitive);
  glb_geometry::append_mesh(glb_, name, primitive, mesh.bounds);
  desc_.meshes.push_back(std::move(mesh));
  return static_cast<int>(desc_.meshes.size()) - 1;
}

int SceneDocument::append_material(const MaterialDesc& material) {
  desc_.materials.push_back(material);
  nlohmann::json& materials = array_at(glb_.json, kMaterials);
  while (static_cast<int>(materials.size()) < material_count()) materials.push_back(nlohmann::json::object());
  return material_count() - 1;
}

int SceneDocument::append_plain_material(const std::string& name, Color color, float roughness) {
  MaterialDesc material;
  material.name = name;
  material.base_color = color;
  material.roughness = roughness;
  return append_material(material);
}

int SceneDocument::add_empty(const std::string& name, int parent) {
  checkpoint();
  return append_node(named_node(name, parent));
}

int SceneDocument::add_primitive(PrimitiveShape shape, const std::string& name, Color color, int parent) {
  checkpoint();
  PrimitiveDesc geometry = make_primitive(shape);
  geometry.material = append_plain_material(name, color, kDefaultRoughness);
  NodeDesc node = named_node(name, parent);
  node.mesh = append_mesh(name, geometry);
  node.props[props::kCollider] = std::string(primitive_collider(shape));
  return append_node(std::move(node));
}

int SceneDocument::add_terrain(const std::string& name, const TerrainGrid& grid, Color color, int parent) {
  checkpoint();
  PrimitiveDesc geometry = make_terrain(grid);
  geometry.material = append_plain_material(name, color, kTerrainRoughness);
  NodeDesc node = named_node(name, parent);
  node.mesh = append_mesh(name, geometry);
  node.props[props::kTerrain] = true;
  node.props[props::kTerrainSize] = static_cast<double>(grid.size);
  node.props[props::kTerrainCells] = static_cast<double>(grid.cells);
  node.props[props::kCollider] = std::string(kTerrainCollider);
  return append_node(std::move(node));
}

int SceneDocument::add_light(LightType type, const std::string& name, Color color, float intensity, int parent) {
  checkpoint();
  nlohmann::json& lights = array_at(glb_.json[kExtensions][kLightsExtension], kLights);
  lights.push_back(
      {{kName, name}, {kType, light_type_name(type)}, {kColor, {color.r, color.g, color.b}}, {kIntensity, intensity}});
  add_extension_used(glb_.json, kLightsExtension);
  NodeDesc node = named_node(name, parent);
  node.light = LightDesc{type, color, intensity, kInfiniteRange};
  node.source_light = last_index(lights);
  return append_node(std::move(node));
}

int SceneDocument::add_camera(const std::string& name, float fov_y_radians, int parent) {
  checkpoint();
  nlohmann::json& cameras = array_at(glb_.json, kCameras);
  cameras.push_back({{kName, name},
                     {kType, kPerspectiveCamera},
                     {kPerspective, {{kYfov, fov_y_radians}, {kZnear, kCameraNear}, {kZfar, kCameraFar}}}});
  NodeDesc node = named_node(name, parent);
  node.camera = CameraDesc{fov_y_radians, kCameraNear, kCameraFar};
  node.source_camera = last_index(cameras);
  return append_node(std::move(node));
}

}  // namespace redgem
