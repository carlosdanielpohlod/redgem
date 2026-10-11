#include "editor/scene_document.hpp"

#include <cstring>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string_view>

#include "core/config.hpp"
#include "core/log.hpp"
#include "scene/gltf_loader.hpp"

namespace redgem {
namespace {

constexpr float kTerrainRoughness = 0.95f;  // ground: no shine
constexpr std::size_t kMaxUndoSteps = 200;
constexpr int kFirstCopySuffix = 1;
constexpr int kSuffixDigits = 3;
constexpr int kSuffixBufferSize = 16;
constexpr const char* kLightsExtension = "KHR_lights_punctual";
constexpr const char* kEmissiveStrengthExtension = "KHR_materials_emissive_strength";
constexpr float kUnitFactor = 1.0f;
constexpr int kArrayBufferTarget = 34962;         // glTF bufferView.target for vertex data
constexpr int kElementArrayBufferTarget = 34963;  // ... and for indices
constexpr int kFloatComponent = 5126;             // glTF componentType FLOAT
constexpr int kUnsignedIntComponent = 5125;       // glTF componentType UNSIGNED_INT
constexpr std::size_t kBinAlignment = 4;
constexpr float kDefaultRoughness = 0.6f;
constexpr float kCameraNear = 0.1f;
constexpr float kCameraFar = 1000.0f;
constexpr std::string_view kTextureSuffix = "Texture";
constexpr const char* kDataUriPrefix = "data:";
constexpr float kInfiniteRange = 0.0f;  // glTF: a light without a range reaches as far as its strength allows
constexpr float kInfiniteFar = 0.0f;    // a camera far plane of 0 means infinite

nlohmann::json property_json(const PropertyValue& value) {
  if (const auto* flag = std::get_if<bool>(&value)) return *flag;
  if (const auto* number = std::get_if<double>(&value)) return *number;
  return std::get<std::string>(value);
}

bool is_scalar(const nlohmann::json& value) { return value.is_boolean() || value.is_number() || value.is_string(); }

// The node as written on save: its original glTF JSON (unknown fields such as weights or other
// extensions survive) with the editable fields replaced.
nlohmann::json node_json(const NodeDesc& node, const nlohmann::json& original) {
  nlohmann::json out = original.is_object() ? original : nlohmann::json::object();
  out.erase("matrix");  // TRS below replaces it
  out["name"] = node.name;
  out["translation"] = {node.local.position.x, node.local.position.y, node.local.position.z};
  const Quat rotation = normalize(node.local.rotation);
  out["rotation"] = {rotation.x, rotation.y, rotation.z, rotation.w};
  out["scale"] = {node.local.scale.x, node.local.scale.y, node.local.scale.z};
  auto set_index = [&out](const char* key, int value) {
    if (value == kNoIndex) {
      out.erase(key);
    } else {
      out[key] = value;
    }
  };
  set_index("mesh", node.mesh);
  set_index("camera", node.source_camera);
  if (node.source_light != kNoIndex) {
    out["extensions"][kLightsExtension]["light"] = node.source_light;
  } else if (out.contains("extensions") && out["extensions"].is_object()) {
    out["extensions"].erase(kLightsExtension);
    if (out["extensions"].empty()) out.erase("extensions");
  }
  if (node.children.empty()) {
    out.erase("children");
  } else {
    out["children"] = node.children;
  }
  // Non-scalar extras (which the editor cannot show) are kept; scalar ones mirror the properties.
  nlohmann::json extras = out.contains("extras") && out["extras"].is_object() ? out["extras"] : nlohmann::json::object();
  for (auto it = extras.begin(); it != extras.end();) {
    it = is_scalar(*it) ? extras.erase(it) : std::next(it);
  }
  for (const auto& [key, value] : node.props) extras[key] = property_json(value);
  if (extras.empty()) {
    out.erase("extras");
  } else {
    out["extras"] = extras;
  }
  return out;
}

bool same_color(const Color& a, const Color& b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

bool same_materials(const std::vector<MaterialDesc>& a, const std::vector<MaterialDesc>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    const MaterialDesc& x = a[i];
    const MaterialDesc& y = b[i];
    if (!same_color(x.base_color, y.base_color) || x.metallic != y.metallic || x.roughness != y.roughness ||
        x.emissive.x != y.emissive.x || x.emissive.y != y.emissive.y || x.emissive.z != y.emissive.z ||
        x.name != y.name) {
      return false;
    }
  }
  return true;
}

nlohmann::json material_json(const MaterialDesc& material, const nlohmann::json& original) {
  nlohmann::json out = original.is_object() ? original : nlohmann::json::object();
  out["name"] = material.name;
  const Color& c = material.base_color;
  out["pbrMetallicRoughness"]["baseColorFactor"] = {c.r, c.g, c.b, c.a};
  out["pbrMetallicRoughness"]["metallicFactor"] = material.metallic;
  out["pbrMetallicRoughness"]["roughnessFactor"] = material.roughness;
  // glTF factors stop at 1; brighter emission goes through KHR_materials_emissive_strength.
  const float strength = std::max({material.emissive.x, material.emissive.y, material.emissive.z, kUnitFactor});
  const Vec3 factor = material.emissive / strength;
  out["emissiveFactor"] = {factor.x, factor.y, factor.z};
  if (strength > kUnitFactor) {
    out["extensions"][kEmissiveStrengthExtension]["emissiveStrength"] = strength;
  } else if (out.contains("extensions")) {
    out["extensions"].erase(kEmissiveStrengthExtension);
  }
  return out;
}

void add_extension_used(nlohmann::json& json, const char* extension) {
  nlohmann::json& used = json["extensionsUsed"];
  if (!used.is_array()) used = nlohmann::json::array();
  if (std::find(used.begin(), used.end(), extension) == used.end()) used.push_back(extension);
}

bool same_light(const std::optional<LightDesc>& a, const std::optional<LightDesc>& b) {
  if (a.has_value() != b.has_value()) return false;
  return !a || (a->type == b->type && a->color.r == b->color.r && a->color.g == b->color.g && a->color.b == b->color.b &&
                a->intensity == b->intensity && a->range == b->range);
}

bool same_camera(const std::optional<CameraDesc>& a, const std::optional<CameraDesc>& b) {
  if (a.has_value() != b.has_value()) return false;
  return !a || (a->fov_y_radians == b->fov_y_radians && a->near_plane == b->near_plane && a->far_plane == b->far_plane);
}

bool same_nodes(const std::vector<NodeDesc>& a, const std::vector<NodeDesc>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    const NodeDesc& x = a[i];
    const NodeDesc& y = b[i];
    const bool same_local = x.local.position.x == y.local.position.x && x.local.position.y == y.local.position.y &&
                            x.local.position.z == y.local.position.z && x.local.rotation.x == y.local.rotation.x &&
                            x.local.rotation.y == y.local.rotation.y && x.local.rotation.z == y.local.rotation.z &&
                            x.local.rotation.w == y.local.rotation.w && x.local.scale.x == y.local.scale.x &&
                            x.local.scale.y == y.local.scale.y && x.local.scale.z == y.local.scale.z;
    if (!same_local || x.name != y.name || x.props != y.props || x.parent != y.parent || x.children != y.children ||
        x.mesh != y.mesh || x.skin != y.skin || !same_light(x.light, y.light) || !same_camera(x.camera, y.camera)) {
      return false;
    }
  }
  return true;
}

// Indices of `index` and all its descendants, parents first.
std::vector<int> subtree(const std::vector<NodeDesc>& nodes, int index) {
  std::vector<int> out;
  std::function<void(int)> visit = [&](int i) {
    out.push_back(i);
    for (const int child : nodes[i].children) visit(child);
  };
  visit(index);
  return out;
}

}  // namespace

std::variant<SceneDocument, std::string> SceneDocument::open(const std::filesystem::path& path) {
  LoadResult loaded = load_gltf_scene(path.string());
  if (!loaded.ok) return loaded.error;
  for (const std::string& warning : loaded.warnings) log::warning(warning);
  std::string error;
  std::optional<GlbFile> glb = read_glb(path, error);
  if (!glb) return error;
  SceneDocument document;
  document.path_ = path;
  document.glb_ = std::move(*glb);
  document.source_nodes_ = document.glb_.json.value("nodes", nlohmann::json::array());
  document.source_skins_ = document.glb_.json.value("skins", nlohmann::json::array());
  document.source_animations_ = document.glb_.json.value("animations", nlohmann::json::array());
  document.desc_ = std::move(loaded.scene);
  document.remember_node_references(0, 0);
  return document;
}

void SceneDocument::checkpoint() {
  undo_.push_back(snapshot());
  if (undo_.size() > kMaxUndoSteps) undo_.erase(undo_.begin());
  redo_.clear();
  changed_since_checkpoint_ = false;
}

SceneDocument::Snapshot SceneDocument::snapshot() const {
  Snapshot state{desc_.nodes, desc_.materials, version_, {}};
  for (int i = 0; i < node_count(); ++i) {
    if (!terrain_grid(i)) continue;
    const PrimitiveDesc& ground = desc_.meshes[static_cast<std::size_t>(desc_.nodes[i].mesh)].primitives.front();
    state.shapes[desc_.nodes[i].mesh] = {ground.positions, ground.normals};
  }
  return state;
}

void SceneDocument::restore_shapes(const std::map<int, MeshShape>& shapes) {
  for (const auto& [mesh, shape] : shapes) {
    MeshDesc& target = desc_.meshes.at(static_cast<std::size_t>(mesh));
    PrimitiveDesc& ground = target.primitives.front();
    if (ground.positions == shape.positions) continue;
    ground.positions = shape.positions;
    ground.normals = shape.normals;
    target.bounds = mesh_bounds_of(ground);
    changed_meshes_.insert(mesh);
    unsaved_meshes_.insert(mesh);
  }
}

std::optional<TerrainGrid> SceneDocument::terrain_grid(int node) const {
  if (node < 0 || node >= node_count()) return std::nullopt;
  const NodeDesc& desc = desc_.nodes[static_cast<std::size_t>(node)];
  if (!get_bool(desc.props, config::kPropTerrain).value_or(false) || desc.mesh == kNoIndex) return std::nullopt;
  const TerrainGrid grid{static_cast<float>(get_number(desc.props, config::kPropTerrainSize).value_or(0.0)),
                         static_cast<int>(get_number(desc.props, config::kPropTerrainCells).value_or(0.0))};
  const MeshDesc& mesh = desc_.meshes.at(static_cast<std::size_t>(desc.mesh));
  if (mesh.primitives.size() != 1 || !grid.fits(mesh.primitives.front())) return std::nullopt;
  return grid;
}

bool SceneDocument::sculpt_terrain(int node, const TerrainBrush& brush) {
  const auto grid = terrain_grid(node);
  if (!grid) return false;
  const int mesh = desc_.nodes[static_cast<std::size_t>(node)].mesh;
  MeshDesc& target = desc_.meshes[static_cast<std::size_t>(mesh)];
  if (!redgem::sculpt_terrain(target.primitives.front(), *grid, brush)) return false;
  target.bounds = mesh_bounds_of(target.primitives.front());
  changed_meshes_.insert(mesh);
  unsaved_meshes_.insert(mesh);
  touch();
  return true;
}

std::vector<int> SceneDocument::take_changed_meshes() {
  std::vector<int> changed(changed_meshes_.begin(), changed_meshes_.end());
  changed_meshes_.clear();
  return changed;
}

bool SceneDocument::store_mesh_vertices(int mesh, std::string& error) {
  const MeshDesc& source = desc_.meshes.at(static_cast<std::size_t>(mesh));
  const PrimitiveDesc& ground = source.primitives.front();
  const nlohmann::json& attributes = glb_.json["meshes"][static_cast<std::size_t>(mesh)]["primitives"][0]["attributes"];
  nlohmann::json& accessors = glb_.json["accessors"];
  const std::pair<const char*, const std::vector<float>*> streams[] = {{"POSITION", &ground.positions},
                                                                      {"NORMAL", &ground.normals}};
  for (const auto& [name, values] : streams) {
    const nlohmann::json& accessor = accessors[attributes.at(name).get<std::size_t>()];
    const nlohmann::json& view = glb_.json["bufferViews"][accessor.at("bufferView").get<std::size_t>()];
    const std::size_t offset = view.value("byteOffset", std::size_t{0}) + accessor.value("byteOffset", std::size_t{0});
    const std::size_t bytes = values->size() * sizeof(float);
    const bool packed = !view.contains("byteStride") ||
                        view["byteStride"].get<std::size_t>() == config::kFloatsPerPosition * sizeof(float);
    if (!packed || accessor.at("count").get<std::size_t>() * config::kFloatsPerPosition != values->size() ||
        offset + bytes > glb_.bin.size()) {
      error = "cannot save the shape of '" + source.name + "': its vertices are not stored the way the editor writes them";
      return false;
    }
    std::memcpy(glb_.bin.data() + offset, values->data(), bytes);
  }
  nlohmann::json& position = accessors[attributes.at("POSITION").get<std::size_t>()];
  position["min"] = {source.bounds.min.x, source.bounds.min.y, source.bounds.min.z};
  position["max"] = {source.bounds.max.x, source.bounds.max.y, source.bounds.max.z};
  return true;
}

void SceneDocument::touch() {
  // The first change after a checkpoint makes a new state; later unrecorded changes (a drag) extend it.
  if (!changed_since_checkpoint_) {
    version_ = ++next_version_;
    changed_since_checkpoint_ = true;
  }
}

void SceneDocument::drop_checkpoint_if_unchanged() {
  if (undo_.empty() || !same_nodes(undo_.back().nodes, desc_.nodes) ||
      !same_materials(undo_.back().materials, desc_.materials)) {
    return;
  }
  version_ = undo_.back().version;
  undo_.pop_back();
  changed_since_checkpoint_ = false;
}

void SceneDocument::set_name(int index, const std::string& name) {
  checkpoint();
  desc_.nodes.at(index).name = name;
  touch();
}

void SceneDocument::set_local(int index, const Transform& local, bool record) {
  if (record) checkpoint();
  desc_.nodes.at(index).local = local;
  touch();
}

void SceneDocument::set_prop(int index, const std::string& key, const PropertyValue& value, bool record) {
  if (record) checkpoint();
  desc_.nodes.at(index).props[key] = value;
  touch();
}

void SceneDocument::remove_prop(int index, const std::string& key, bool record) {
  if (record) checkpoint();
  desc_.nodes.at(index).props.erase(key);
  touch();
}

std::string SceneDocument::unique_name(const std::string& base) const {
  auto taken = [this](const std::string& name) {
    return std::any_of(desc_.nodes.begin(), desc_.nodes.end(), [&](const NodeDesc& n) { return n.name == name; });
  };
  if (!taken(base)) return base;
  for (int suffix = kFirstCopySuffix;; ++suffix) {
    char buffer[kSuffixBufferSize];
    std::snprintf(buffer, sizeof(buffer), ".%0*d", kSuffixDigits, suffix);
    const std::string candidate = base + buffer;
    if (!taken(candidate)) return candidate;
  }
}

int SceneDocument::duplicate(int index) {
  checkpoint();
  std::vector<NodeDesc>& nodes = desc_.nodes;
  const std::vector<int> source = subtree(nodes, index);
  const int first_copy = static_cast<int>(nodes.size());
  std::vector<int> remap(nodes.size(), kNoIndex);
  for (std::size_t i = 0; i < source.size(); ++i) remap[source[i]] = first_copy + static_cast<int>(i);
  for (const int original : source) {
    NodeDesc copy = nodes[original];
    copy.name = unique_name(copy.name);
    copy.parent = original == index ? copy.parent : remap[copy.parent];
    for (int& child : copy.children) child = remap[child];
    nodes.push_back(std::move(copy));  // names stay unique because each copy is added before the next
  }
  if (nodes[first_copy].parent != kNoIndex) nodes[nodes[first_copy].parent].children.push_back(first_copy);
  // Each copy is a node of its own in the file too, and a character copied with its bones gets
  // its own skeleton (its animation is not copied: the copy keeps the pose it was copied in).
  for (std::size_t i = 0; i < source.size(); ++i) {
    NodeDesc& copy = nodes[first_copy + static_cast<int>(i)];
    if (copy.source_node < 0 || copy.source_node >= static_cast<int>(source_nodes_.size())) continue;
    source_nodes_.push_back(source_nodes_[copy.source_node]);
    copy.source_node = static_cast<int>(source_nodes_.size()) - 1;
  }
  std::map<int, int> copied_skins;
  for (std::size_t i = 0; i < source.size(); ++i) {
    NodeDesc& copy = nodes[first_copy + static_cast<int>(i)];
    if (copy.skin < 0 || copy.skin >= static_cast<int>(desc_.skins.size())) continue;
    if (copied_skins.count(copy.skin) == 0) {
      SkinDesc skin = desc_.skins[copy.skin];
      const bool whole = std::all_of(skin.joints.begin(), skin.joints.end(),
                                     [&remap](int joint) { return joint != kNoIndex && remap[joint] != kNoIndex; });
      if (!whole) continue;  // bones outside the copy stay shared with the original
      nlohmann::json json = source_skins_[copy.skin];
      std::vector<int> joint_sources;
      for (std::size_t j = 0; j < skin.joints.size(); ++j) {
        skin.joints[j] = remap[skin.joints[j]];
        joint_sources.push_back(nodes[skin.joints[j]].source_node);
        json["joints"][j] = joint_sources.back();
      }
      json.erase("skeleton");
      copied_skins[copy.skin] = static_cast<int>(desc_.skins.size());
      desc_.skins.push_back(std::move(skin));
      source_skins_.push_back(std::move(json));
      skin_joint_sources_.push_back(std::move(joint_sources));
    }
    copy.skin = copied_skins[copy.skin];
  }
  touch();
  return first_copy;
}

void SceneDocument::remove(int index) {
  checkpoint();
  std::vector<NodeDesc>& nodes = desc_.nodes;
  const std::vector<int> doomed_list = subtree(nodes, index);
  const std::set<int> doomed(doomed_list.begin(), doomed_list.end());
  std::vector<int> remap(nodes.size(), kNoIndex);
  std::vector<NodeDesc> kept;
  for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
    if (doomed.count(i) != 0) continue;
    remap[i] = static_cast<int>(kept.size());
    kept.push_back(nodes[i]);
  }
  for (NodeDesc& node : kept) {
    if (node.parent != kNoIndex) node.parent = remap[node.parent];
    std::vector<int> children;
    for (const int child : node.children) {
      if (remap[child] != kNoIndex) children.push_back(remap[child]);
    }
    node.children = std::move(children);
  }
  nodes = std::move(kept);
  refresh_node_references();
  touch();
}

bool SceneDocument::undo() {
  if (undo_.empty()) return false;
  redo_.push_back(snapshot());
  desc_.nodes = std::move(undo_.back().nodes);
  refresh_node_references();
  restore_materials(undo_.back().materials);
  restore_shapes(undo_.back().shapes);
  version_ = undo_.back().version;
  undo_.pop_back();
  changed_since_checkpoint_ = false;
  return true;
}

bool SceneDocument::redo() {
  if (redo_.empty()) return false;
  undo_.push_back(snapshot());
  desc_.nodes = std::move(redo_.back().nodes);
  refresh_node_references();
  restore_materials(redo_.back().materials);
  restore_shapes(redo_.back().shapes);
  version_ = redo_.back().version;
  redo_.pop_back();
  changed_since_checkpoint_ = false;
  return true;
}

void SceneDocument::set_light(int index, const LightDesc& light, bool record) {
  NodeDesc& node = desc_.nodes.at(index);
  if (!node.light) return;
  if (record) checkpoint();
  node.light = light;
  touch();
}

void SceneDocument::set_camera(int index, const CameraDesc& camera, bool record) {
  NodeDesc& node = desc_.nodes.at(index);
  if (!node.camera) return;
  if (record) checkpoint();
  node.camera = camera;
  touch();
}

void SceneDocument::sync_lights_and_cameras() {
  nlohmann::json& json = glb_.json;
  // Entries are updated in place (undo snapshots keep pointing at them); a second node using the
  // same entry (a duplicate) gets a copy of its own.
  std::set<int> claimed_lights;
  std::set<int> claimed_cameras;
  for (NodeDesc& node : desc_.nodes) {
    if (node.light) {
      nlohmann::json& lights = json["extensions"][kLightsExtension]["lights"];
      if (!lights.is_array()) lights = nlohmann::json::array();
      const bool known = node.source_light >= 0 && node.source_light < static_cast<int>(lights.size());
      if (!known || claimed_lights.count(node.source_light) != 0) {
        lights.push_back(known ? lights[node.source_light] : nlohmann::json::object());
        node.source_light = static_cast<int>(lights.size()) - 1;
      }
      claimed_lights.insert(node.source_light);
      nlohmann::json& entry = lights[node.source_light];
      const LightDesc& light = *node.light;
      entry["type"] = light.type == LightType::Directional ? "directional"
                      : light.type == LightType::Spot      ? "spot"
                                                           : "point";
      entry["color"] = {light.color.r, light.color.g, light.color.b};
      entry["intensity"] = light.intensity;
      if (light.range > kInfiniteRange) {
        entry["range"] = light.range;
      } else {
        entry.erase("range");
      }
      add_extension_used(json, kLightsExtension);
    }
    if (node.camera) {
      nlohmann::json& cameras = json["cameras"];
      if (!cameras.is_array()) cameras = nlohmann::json::array();
      const bool known = node.source_camera >= 0 && node.source_camera < static_cast<int>(cameras.size());
      if (!known || claimed_cameras.count(node.source_camera) != 0) {
        cameras.push_back(known ? cameras[node.source_camera] : nlohmann::json::object());
        node.source_camera = static_cast<int>(cameras.size()) - 1;
      }
      claimed_cameras.insert(node.source_camera);
      nlohmann::json& entry = cameras[node.source_camera];
      entry["type"] = "perspective";
      entry["perspective"]["yfov"] = node.camera->fov_y_radians;
      entry["perspective"]["znear"] = node.camera->near_plane;
      if (node.camera->far_plane > kInfiniteFar) {
        entry["perspective"]["zfar"] = node.camera->far_plane;
      } else {
        entry["perspective"].erase("zfar");  // glTF: no zfar = infinite projection
      }
    }
  }
}

bool SceneDocument::sync_glb(std::string& error) {
  if (desc_.nodes.empty()) {
    error = "a glTF scene needs at least one object";
    return false;
  }
  sync_lights_and_cameras();
  nlohmann::json& json = glb_.json;
  const nlohmann::json& original_nodes = source_nodes_;
  nlohmann::json nodes = nlohmann::json::array();
  nlohmann::json roots = nlohmann::json::array();
  for (int i = 0; i < node_count(); ++i) {
    const NodeDesc& node = desc_.nodes[i];
    const bool known = node.source_node >= 0 && node.source_node < static_cast<int>(original_nodes.size());
    nodes.push_back(node_json(node, known ? original_nodes[node.source_node] : nlohmann::json::object()));
    if (node.parent == kNoIndex) roots.push_back(i);
  }
  json["nodes"] = nodes;
  nlohmann::json materials = json.contains("materials") ? json["materials"] : nlohmann::json::array();
  for (int i = 0; i < material_count(); ++i) {
    const nlohmann::json original = i < static_cast<int>(materials.size()) ? materials[i] : nlohmann::json::object();
    if (i < static_cast<int>(materials.size())) {
      materials[i] = material_json(desc_.materials[i], original);
    } else {
      materials.push_back(material_json(desc_.materials[i], original));
    }
  }
  if (!materials.empty()) json["materials"] = materials;
  for (const auto& material : desc_.materials) {
    if (std::max({material.emissive.x, material.emissive.y, material.emissive.z}) > kUnitFactor) {
      add_extension_used(json, kEmissiveStrengthExtension);
    }
  }
  if (glb_.bin.empty()) {
    json.erase("buffers");
  } else {
    json["buffers"] = nlohmann::json::array({{{"byteLength", glb_.bin.size()}}});
  }
  // Only the scene the engine loads is edited; it becomes the file's single scene.
  const int active = json.value("scene", 0);
  nlohmann::json scene = json.contains("scenes") && active < static_cast<int>(json["scenes"].size())
                             ? json["scenes"][active]
                             : nlohmann::json::object();
  if (json.contains("scenes") && json["scenes"].size() > 1) {
    log::warning("saving keeps only the active glTF scene");
  }
  scene["nodes"] = roots;
  json["scenes"] = nlohmann::json::array({scene});
  json["scene"] = 0;
  sync_skins_and_animations(json);
  return true;
}

std::vector<int> SceneDocument::current_node_indices() const {
  std::vector<int> current(source_nodes_.size(), kNoIndex);
  for (int i = 0; i < node_count(); ++i) {
    const int source = desc_.nodes[i].source_node;
    // A duplicated node shares its source with the original: the first one keeps the skeleton.
    if (source >= 0 && source < static_cast<int>(current.size()) && current[source] == kNoIndex) current[source] = i;
  }
  return current;
}

void SceneDocument::remember_node_references(std::size_t first_skin, std::size_t first_animation) {
  const auto source_of = [this](int node) { return node == kNoIndex ? kNoIndex : desc_.nodes.at(node).source_node; };
  for (std::size_t s = first_skin; s < desc_.skins.size(); ++s) {
    std::vector<int> sources;
    for (const int joint : desc_.skins[s].joints) sources.push_back(source_of(joint));
    skin_joint_sources_.push_back(std::move(sources));
  }
  for (std::size_t a = first_animation; a < desc_.animations.size(); ++a) {
    std::vector<int> sources;
    for (const AnimationChannel& channel : desc_.animations[a].channels) sources.push_back(source_of(channel.node));
    channel_node_sources_.push_back(std::move(sources));
  }
}

void SceneDocument::refresh_node_references() {
  if (desc_.skins.empty() && desc_.animations.empty()) return;
  const std::vector<int> current = current_node_indices();
  const auto now = [&current](int source) {
    return source >= 0 && source < static_cast<int>(current.size()) ? current[source] : kNoIndex;
  };
  for (std::size_t s = 0; s < desc_.skins.size(); ++s) {
    for (std::size_t j = 0; j < desc_.skins[s].joints.size(); ++j) desc_.skins[s].joints[j] = now(skin_joint_sources_[s][j]);
  }
  for (std::size_t a = 0; a < desc_.animations.size(); ++a) {
    for (std::size_t c = 0; c < desc_.animations[a].channels.size(); ++c) {
      desc_.animations[a].channels[c].node = now(channel_node_sources_[a][c]);
    }
  }
}

void SceneDocument::sync_skins_and_animations(nlohmann::json& json) const {
  const std::vector<int> current = current_node_indices();
  const auto now = [&current](const nlohmann::json& source) {
    const int index = source.get<int>();
    return index >= 0 && index < static_cast<int>(current.size()) ? current[index] : kNoIndex;
  };
  // Skins: one that lost a joint (a deleted bone, or a whole deleted or undone character) is left
  // out, and the meshes that used it are saved rigid; the others keep working.
  nlohmann::json skins = nlohmann::json::array();
  std::vector<int> written(source_skins_.size(), kNoIndex);  // skin index -> its index in the file
  for (std::size_t s = 0; s < source_skins_.size(); ++s) {
    nlohmann::json skin = source_skins_[s];
    bool intact = true;
    for (nlohmann::json& joint : skin["joints"]) {
      const int index = now(joint);
      intact = intact && index != kNoIndex;
      joint = index;
    }
    if (!intact) continue;
    if (skin.contains("skeleton")) {
      const int root = now(skin["skeleton"]);
      if (root == kNoIndex) {
        skin.erase("skeleton");
      } else {
        skin["skeleton"] = root;
      }
    }
    written[s] = static_cast<int>(skins.size());
    skins.push_back(std::move(skin));
  }
  for (int i = 0; i < node_count(); ++i) {
    const int skin = desc_.nodes[i].skin;
    const int in_file = skin >= 0 && skin < static_cast<int>(written.size()) ? written[skin] : kNoIndex;
    if (in_file == kNoIndex) {
      if (skin != kNoIndex) log::warning("'" + desc_.nodes[i].name + "' lost part of its skeleton: saved without it");
      json["nodes"][i].erase("skin");
    } else {
      json["nodes"][i]["skin"] = in_file;
    }
  }
  if (skins.empty()) {
    json.erase("skins");
  } else {
    json["skins"] = skins;
  }
  // Animations: channels of deleted nodes are dropped.
  nlohmann::json animations = nlohmann::json::array();
  for (nlohmann::json animation : source_animations_) {
    nlohmann::json channels = nlohmann::json::array();
    for (nlohmann::json channel : animation["channels"]) {
      if (!channel.contains("target") || !channel["target"].contains("node")) continue;
      const int index = now(channel["target"]["node"]);
      if (index == kNoIndex) continue;
      channel["target"]["node"] = index;
      channels.push_back(std::move(channel));
    }
    if (channels.empty()) continue;
    animation["channels"] = channels;
    animations.push_back(std::move(animation));
  }
  if (animations.empty()) {
    json.erase("animations");
  } else {
    json["animations"] = animations;
  }
}

SceneDocument SceneDocument::create_empty(const std::filesystem::path& path) {
  SceneDocument document;
  document.path_ = path;
  document.glb_.json = {{"asset", {{"version", "2.0"}, {"generator", "RedGem"}}},
                        {"scene", 0},
                        {"scenes", nlohmann::json::array({{{"name", "Scene"}, {"nodes", nlohmann::json::array()}}})},
                        {"nodes", nlohmann::json::array()}};
  document.version_ = ++document.next_version_;  // a new document is unsaved
  return document;
}

void SceneDocument::restore_materials(const std::vector<MaterialDesc>& snapshot) {
  // Materials are only ever appended, so a snapshot may be shorter: newer ones stay as they are.
  for (std::size_t i = 0; i < snapshot.size() && i < desc_.materials.size(); ++i) desc_.materials[i] = snapshot[i];
}

int SceneDocument::append_node(NodeDesc node) {
  node.name = unique_name(node.name);
  const int index = node_count();
  const int parent = node.parent;
  desc_.nodes.push_back(std::move(node));
  if (parent != kNoIndex) desc_.nodes.at(parent).children.push_back(index);
  touch();
  return index;
}

int SceneDocument::append_buffer_view(const void* data, std::size_t bytes, int target) {
  glb_.bin.resize((glb_.bin.size() + kBinAlignment - 1) / kBinAlignment * kBinAlignment, 0);
  const std::size_t offset = glb_.bin.size();
  const auto* begin = static_cast<const std::uint8_t*>(data);
  glb_.bin.insert(glb_.bin.end(), begin, begin + bytes);
  nlohmann::json& views = glb_.json["bufferViews"];
  if (!views.is_array()) views = nlohmann::json::array();
  views.push_back({{"buffer", 0}, {"byteOffset", offset}, {"byteLength", bytes}, {"target", target}});
  return static_cast<int>(views.size()) - 1;
}

int SceneDocument::append_mesh(const std::string& name, const PrimitiveDesc& primitive) {
  nlohmann::json& accessors = glb_.json["accessors"];
  if (!accessors.is_array()) accessors = nlohmann::json::array();
  const std::size_t vertices = primitive.positions.size() / config::kFloatsPerPosition;
  auto accessor = [&](const void* data, std::size_t bytes, int target, int component, std::size_t count,
                      const char* type) {
    nlohmann::json entry = {{"bufferView", append_buffer_view(data, bytes, target)},
                            {"componentType", component}, {"count", count}, {"type", type}};
    accessors.push_back(entry);
    return static_cast<int>(accessors.size()) - 1;
  };
  MeshDesc mesh;
  mesh.name = name;
  mesh.primitives.push_back(primitive);
  for (std::size_t v = 0; v < vertices; ++v) {
    const float* p = &primitive.positions[v * config::kFloatsPerPosition];
    mesh.bounds.include({p[0], p[1], p[2]});
  }
  const int position = accessor(primitive.positions.data(), primitive.positions.size() * sizeof(float),
                                kArrayBufferTarget, kFloatComponent, vertices, "VEC3");
  accessors[position]["min"] = {mesh.bounds.min.x, mesh.bounds.min.y, mesh.bounds.min.z};
  accessors[position]["max"] = {mesh.bounds.max.x, mesh.bounds.max.y, mesh.bounds.max.z};
  const int normal = accessor(primitive.normals.data(), primitive.normals.size() * sizeof(float),
                              kArrayBufferTarget, kFloatComponent, vertices, "VEC3");
  nlohmann::json attributes = {{"POSITION", position}, {"NORMAL", normal}};
  if (!primitive.texcoords.empty()) {
    attributes["TEXCOORD_0"] = accessor(primitive.texcoords.data(), primitive.texcoords.size() * sizeof(float),
                                        kArrayBufferTarget, kFloatComponent, vertices, "VEC2");
  }
  const int indices = accessor(primitive.indices.data(), primitive.indices.size() * sizeof(std::uint32_t),
                               kElementArrayBufferTarget, kUnsignedIntComponent, primitive.indices.size(), "SCALAR");
  nlohmann::json json_primitive = {{"attributes", attributes}, {"indices", indices}};
  if (primitive.material != kNoIndex) json_primitive["material"] = primitive.material;
  nlohmann::json& meshes = glb_.json["meshes"];
  if (!meshes.is_array()) meshes = nlohmann::json::array();
  meshes.push_back({{"name", name}, {"primitives", nlohmann::json::array({json_primitive})}});
  desc_.meshes.push_back(std::move(mesh));
  return static_cast<int>(desc_.meshes.size()) - 1;
}

int SceneDocument::append_material(const MaterialDesc& material) {
  desc_.materials.push_back(material);
  nlohmann::json& materials = glb_.json["materials"];
  if (!materials.is_array()) materials = nlohmann::json::array();
  while (static_cast<int>(materials.size()) < material_count()) materials.push_back(nlohmann::json::object());
  return material_count() - 1;
}

int SceneDocument::add_empty(const std::string& name, int parent) {
  checkpoint();
  NodeDesc node;
  node.name = name;
  node.parent = parent;
  return append_node(std::move(node));
}

int SceneDocument::add_primitive(PrimitiveShape shape, const std::string& name, Color color, int parent) {
  checkpoint();
  MaterialDesc material;
  material.name = name;
  material.base_color = color;
  material.roughness = kDefaultRoughness;
  PrimitiveDesc geometry = make_primitive(shape);
  geometry.material = append_material(material);
  NodeDesc node;
  node.name = name;
  node.parent = parent;
  node.mesh = append_mesh(name, geometry);
  node.props[config::kPropCollider] = std::string(primitive_collider(shape));
  return append_node(std::move(node));
}

int SceneDocument::add_terrain(const std::string& name, const TerrainGrid& grid, Color color, int parent) {
  checkpoint();
  MaterialDesc material;
  material.name = name;
  material.base_color = color;
  material.roughness = kTerrainRoughness;
  PrimitiveDesc geometry = make_terrain(grid);
  geometry.material = append_material(material);
  NodeDesc node;
  node.name = name;
  node.parent = parent;
  node.mesh = append_mesh(name, geometry);
  node.props[config::kPropTerrain] = true;
  node.props[config::kPropTerrainSize] = static_cast<double>(grid.size);
  node.props[config::kPropTerrainCells] = static_cast<double>(grid.cells);
  node.props[config::kPropCollider] = std::string("mesh");  // things stand on the ground as it is shaped
  return append_node(std::move(node));
}

int SceneDocument::add_light(LightType type, const std::string& name, Color color, float intensity, int parent) {
  checkpoint();
  nlohmann::json& lights = glb_.json["extensions"][kLightsExtension]["lights"];
  if (!lights.is_array()) lights = nlohmann::json::array();
  const char* kind = type == LightType::Directional ? "directional" : type == LightType::Spot ? "spot" : "point";
  lights.push_back({{"name", name}, {"type", kind}, {"color", {color.r, color.g, color.b}}, {"intensity", intensity}});
  add_extension_used(glb_.json, kLightsExtension);
  NodeDesc node;
  node.name = name;
  node.parent = parent;
  node.light = LightDesc{type, color, intensity, kInfiniteRange};
  node.source_light = static_cast<int>(lights.size()) - 1;
  return append_node(std::move(node));
}

int SceneDocument::add_camera(const std::string& name, float fov_y_radians, int parent) {
  checkpoint();
  nlohmann::json& cameras = glb_.json["cameras"];
  if (!cameras.is_array()) cameras = nlohmann::json::array();
  cameras.push_back({{"name", name}, {"type", "perspective"},
                     {"perspective", {{"yfov", fov_y_radians}, {"znear", kCameraNear}, {"zfar", kCameraFar}}}});
  NodeDesc node;
  node.name = name;
  node.parent = parent;
  node.camera = CameraDesc{fov_y_radians, kCameraNear, kCameraFar};
  node.source_camera = static_cast<int>(cameras.size()) - 1;
  return append_node(std::move(node));
}

namespace {

Transform world_of(const std::vector<NodeDesc>& nodes, int index) {
  const NodeDesc& node = nodes[index];
  return node.parent == kNoIndex ? node.local : combine(world_of(nodes, node.parent), node.local);
}

}  // namespace

int SceneDocument::reparent(int index, int new_parent) {
  std::vector<NodeDesc>& nodes = desc_.nodes;
  const int count = node_count();
  if (index < 0 || index >= count || new_parent < kNoIndex || new_parent >= count) return index;
  const std::vector<int> moving = subtree(nodes, index);
  if (new_parent != kNoIndex && std::find(moving.begin(), moving.end(), new_parent) != moving.end()) return index;
  if (nodes.at(index).parent == new_parent) return index;
  checkpoint();
  const Transform world = world_of(nodes, index);
  const Transform parent_world = new_parent == kNoIndex ? Transform{} : world_of(nodes, new_parent);
  // Rebuild the list: everything else in order, then the moved subtree last (after its new parent).
  std::vector<int> order;
  for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
    if (std::find(moving.begin(), moving.end(), i) == moving.end()) order.push_back(i);
  }
  order.insert(order.end(), moving.begin(), moving.end());
  std::vector<int> remap(nodes.size(), kNoIndex);
  for (int i = 0; i < static_cast<int>(order.size()); ++i) remap[order[i]] = i;
  std::vector<NodeDesc> reordered;
  for (const int old : order) reordered.push_back(nodes[old]);
  for (NodeDesc& node : reordered) {
    if (node.parent != kNoIndex) node.parent = remap[node.parent];
    for (int& child : node.children) child = remap[child];
  }
  const int moved = remap[index];
  const int old_parent = reordered[moved].parent;
  if (old_parent != kNoIndex) {
    auto& siblings = reordered[old_parent].children;
    siblings.erase(std::remove(siblings.begin(), siblings.end(), moved), siblings.end());
  }
  const int parent = new_parent == kNoIndex ? kNoIndex : remap[new_parent];
  reordered[moved].parent = parent;
  if (parent != kNoIndex) reordered[parent].children.push_back(moved);
  reordered[moved].local = parent == kNoIndex ? world : relative_to(parent_world, world);
  nodes = std::move(reordered);
  refresh_node_references();
  touch();
  return moved;
}

namespace {

// Shifts every texture reference ({"index": n} under a key ending in "Texture") in a material,
// including the ones inside material extensions (specular, clearcoat, transmission...).
void shift_texture_refs(nlohmann::json& value, int offset) {
  if (!value.is_object()) return;
  for (auto& [key, child] : value.items()) {
    const bool texture_ref = key.size() >= kTextureSuffix.size() &&
                             key.compare(key.size() - kTextureSuffix.size(), kTextureSuffix.size(), kTextureSuffix) == 0;
    if (texture_ref && child.is_object() && child.contains("index")) {
      child["index"] = child["index"].get<int>() + offset;
    }
    shift_texture_refs(child, offset);
  }
}

void merge_string_list(nlohmann::json& into, const nlohmann::json& from, const char* key) {
  if (!from.contains(key)) return;
  for (const nlohmann::json& name : from[key]) {
    if (!into[key].is_array()) into[key] = nlohmann::json::array();
    if (std::find(into[key].begin(), into[key].end(), name) == into[key].end()) into[key].push_back(name);
  }
}

}  // namespace

int SceneDocument::import_model(const std::filesystem::path& path, std::string& error, int parent) {
  if (parent < kNoIndex || parent >= node_count()) {
    error = "invalid parent object";
    return kNoIndex;
  }
  const LoadResult loaded = load_gltf_scene(path.string());
  if (!loaded.ok) {
    error = loaded.error;
    return kNoIndex;
  }
  std::optional<GlbFile> source = read_glb(path, error);
  if (!source) return kNoIndex;
  const nlohmann::json& in = source->json;
  if (in.value("buffers", nlohmann::json::array()).size() > 1) {
    error = "models with several buffers are not supported (export a single .glb from Blender)";
    return kNoIndex;
  }
  nlohmann::json& json = glb_.json;
  auto count = [&json](const char* key) { return json.contains(key) ? static_cast<int>(json[key].size()) : 0; };
  // Offsets of everything the imported file references by index.
  const int view_offset = count("bufferViews");
  const int accessor_offset = count("accessors");
  const int image_offset = count("images");
  const int sampler_offset = count("samplers");
  const int texture_offset = count("textures");
  const int material_offset = material_count();
  const int mesh_offset = static_cast<int>(desc_.meshes.size());
  const int node_offset = static_cast<int>(source_nodes_.size());
  const int skin_offset = static_cast<int>(source_skins_.size());
  glb_.bin.resize((glb_.bin.size() + kBinAlignment - 1) / kBinAlignment * kBinAlignment, 0);
  const std::size_t bin_offset = glb_.bin.size();
  glb_.bin.insert(glb_.bin.end(), source->bin.begin(), source->bin.end());

  auto append = [&json](const char* key, nlohmann::json item) {
    if (!json[key].is_array()) json[key] = nlohmann::json::array();
    json[key].push_back(std::move(item));
  };
  auto shift = [](nlohmann::json& object, const char* key, int offset) {
    if (object.contains(key)) object[key] = object[key].get<int>() + offset;
  };
  for (nlohmann::json view : in.value("bufferViews", nlohmann::json::array())) {
    view["buffer"] = 0;
    view["byteOffset"] = view.value("byteOffset", 0) + bin_offset;
    append("bufferViews", view);
  }
  for (nlohmann::json accessor : in.value("accessors", nlohmann::json::array())) {
    shift(accessor, "bufferView", view_offset);
    if (accessor.contains("sparse")) {
      shift(accessor["sparse"]["indices"], "bufferView", view_offset);
      shift(accessor["sparse"]["values"], "bufferView", view_offset);
    }
    append("accessors", accessor);
  }
  for (nlohmann::json image : in.value("images", nlohmann::json::array())) {
    shift(image, "bufferView", view_offset);
    // External image files stay where they are: point at them from the scene's folder.
    if (image.contains("uri") && image["uri"].get<std::string>().rfind(kDataUriPrefix, 0) != 0) {
      const std::filesystem::path file = path.parent_path() / image["uri"].get<std::string>();
      image["uri"] = file.lexically_relative(path_.parent_path()).generic_string();
    }
    append("images", image);
  }
  for (nlohmann::json sampler : in.value("samplers", nlohmann::json::array())) append("samplers", sampler);
  for (nlohmann::json texture : in.value("textures", nlohmann::json::array())) {
    shift(texture, "sampler", sampler_offset);
    shift(texture, "source", image_offset);
    if (texture.contains("extensions")) {
      for (auto& [name, extension] : texture["extensions"].items()) shift(extension, "source", image_offset);
    }
    append("textures", texture);
  }
  for (nlohmann::json material : in.value("materials", nlohmann::json::array())) {
    shift_texture_refs(material, texture_offset);
    append("materials", material);
  }
  for (nlohmann::json mesh : in.value("meshes", nlohmann::json::array())) {
    for (nlohmann::json& primitive : mesh["primitives"]) {
      for (auto& [name, accessor] : primitive["attributes"].items()) accessor = accessor.get<int>() + accessor_offset;
      shift(primitive, "indices", accessor_offset);
      shift(primitive, "material", material_offset);
      primitive.erase("targets");  // morph targets are not supported: drop them with their weights
    }
    mesh.erase("weights");
    if (mesh.contains("extras") && mesh["extras"].is_object()) mesh["extras"].erase("targetNames");
    append("meshes", mesh);
  }
  for (nlohmann::json node : in.value("nodes", nlohmann::json::array())) {
    shift(node, "mesh", mesh_offset);
    shift(node, "skin", skin_offset);
    node.erase("camera");
    node.erase("weights");
    node.erase("extensions");  // lights of the model are not imported
    node.erase("children");    // rebuilt from the scene description on save
    source_nodes_.push_back(std::move(node));
  }
  // Skeletons and animations come along, renumbered like everything else.
  for (nlohmann::json skin : in.value("skins", nlohmann::json::array())) {
    for (nlohmann::json& joint : skin["joints"]) joint = joint.get<int>() + node_offset;
    shift(skin, "skeleton", node_offset);
    shift(skin, "inverseBindMatrices", accessor_offset);
    source_skins_.push_back(std::move(skin));
  }
  for (nlohmann::json animation : in.value("animations", nlohmann::json::array())) {
    for (nlohmann::json& sampler : animation["samplers"]) {
      shift(sampler, "input", accessor_offset);
      shift(sampler, "output", accessor_offset);
    }
    for (nlohmann::json& channel : animation["channels"]) {
      if (channel.contains("target")) shift(channel["target"], "node", node_offset);
    }
    source_animations_.push_back(std::move(animation));
  }
  merge_string_list(json, in, "extensionsUsed");
  merge_string_list(json, in, "extensionsRequired");

  // The scene description mirrors the same arrays (the loader keeps glTF order for these).
  checkpoint();
  for (const ImageData& image : loaded.scene.images) desc_.images.push_back(image);
  for (MaterialDesc material : loaded.scene.materials) {
    for (int* image : {&material.base_color_image, &material.metallic_roughness_image, &material.normal_image,
                       &material.occlusion_image, &material.emissive_image}) {
      if (*image != kNoIndex) *image += image_offset;
    }
    desc_.materials.push_back(material);
  }
  for (MeshDesc mesh : loaded.scene.meshes) {
    for (PrimitiveDesc& primitive : mesh.primitives) {
      if (primitive.material != kNoIndex) primitive.material += material_offset;
    }
    desc_.meshes.push_back(std::move(mesh));
  }
  const int first_skin = static_cast<int>(desc_.skins.size());
  NodeDesc holder;
  holder.name = path.stem().string();
  holder.parent = parent;
  const int root = append_node(std::move(holder));
  const int first = node_count();
  for (const NodeDesc& imported : loaded.scene.nodes) {
    NodeDesc node = imported;
    node.parent = imported.parent == kNoIndex ? root : imported.parent + first;
    for (int& child : node.children) child += first;
    if (node.mesh != kNoIndex) node.mesh += mesh_offset;
    if (node.skin != kNoIndex) node.skin += first_skin;
    node.camera.reset();
    node.light.reset();
    node.source_camera = kNoIndex;
    node.source_light = kNoIndex;
    node.source_node = imported.source_node == kNoIndex ? kNoIndex : imported.source_node + node_offset;
    node.name = unique_name(node.name);
    desc_.nodes.push_back(std::move(node));
    if (desc_.nodes.back().parent == root) desc_.nodes[root].children.push_back(node_count() - 1);
  }
  // The imported skeletons and animations point at the imported nodes (now `first` further on).
  const std::size_t first_animation = desc_.animations.size();
  for (SkinDesc skin : loaded.scene.skins) {
    for (int& joint : skin.joints) joint = joint == kNoIndex ? kNoIndex : joint + first;
    desc_.skins.push_back(std::move(skin));
  }
  for (AnimationDesc animation : loaded.scene.animations) {
    for (AnimationChannel& channel : animation.channels) channel.node += first;
    desc_.animations.push_back(std::move(animation));
  }
  remember_node_references(static_cast<std::size_t>(first_skin), first_animation);
  touch();
  return root;
}

int SceneDocument::material_of(int node) const {
  const NodeDesc& n = desc_.nodes.at(node);
  if (n.mesh == kNoIndex || desc_.meshes.at(n.mesh).primitives.empty()) return kNoIndex;
  return desc_.meshes.at(n.mesh).primitives.front().material;
}

int SceneDocument::material_users(int material) const {
  int users = 0;
  for (int i = 0; i < node_count(); ++i) users += material_of(i) == material ? 1 : 0;
  return users;
}

void SceneDocument::set_material(int index, const MaterialDesc& material, bool record) {
  if (record) checkpoint();
  desc_.materials.at(index) = material;
  touch();
}

bool SceneDocument::save(std::string& error) { return save_as(path_, error); }

bool SceneDocument::save_as(const std::filesystem::path& path, std::string& error) {
  if (!sync_glb(error)) return false;
  for (const int mesh : unsaved_meshes_) {
    if (!store_mesh_vertices(mesh, error)) return false;
  }
  if (!write_glb(path, glb_, error)) return false;
  unsaved_meshes_.clear();
  path_ = path;
  saved_version_ = version_;
  return true;
}

}  // namespace redgem
