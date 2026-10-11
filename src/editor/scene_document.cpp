#include "editor/scene_document.hpp"

#include <cstring>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <map>
#include <optional>
#include <set>

#include "core/config.hpp"
#include "core/log.hpp"
#include "editor/gltf_json.hpp"
#include "editor/model_import.hpp"
#include "scene/gltf_loader.hpp"

namespace redgem {
namespace {

using namespace gltf_json;

constexpr float kTerrainRoughness = 0.95f;        // ground: no shine
constexpr float kDefaultRoughness = 0.6f;         // of the material a new shape gets (0 mirror .. 1 matte)
constexpr std::size_t kMaxUndoSteps = 200;        // older steps are forgotten
constexpr int kFirstCopySuffix = 1;               // "Crate.001" is the first name tried after "Crate"
constexpr int kSuffixDigits = 3;                  // digits of that number
constexpr int kSuffixBufferSize = 16;             // characters: room for a dot and any int
constexpr const char* kSuffixFormat = ".%0*d";    // a dot and the number, zero-padded to those digits
constexpr int kArrayBufferTarget = 34962;         // glTF bufferView.target for vertex data
constexpr int kElementArrayBufferTarget = 34963;  // ... and for indices
constexpr int kFloatComponent = 5126;             // glTF componentType FLOAT
constexpr int kUnsignedIntComponent = 5125;       // glTF componentType UNSIGNED_INT
constexpr int kOnlyBuffer = 0;                    // a .glb keeps everything in its first buffer
constexpr int kOnlyPrimitive = 0;                 // a terrain's mesh is a single piece
constexpr float kCameraNear = 0.1f;               // meters: clip planes of a new camera
constexpr float kCameraFar = 1000.0f;
constexpr float kInfiniteRange = 0.0f;            // glTF: a light without a range reaches as far as its strength allows
constexpr const char* kTerrainCollider = "mesh";  // things stand on the ground as it is shaped

// json[key] as a list (empty when the file has none).
nlohmann::json list_of(const nlohmann::json& json, const char* key) { return json.value(key, nlohmann::json::array()); }

int count_of(const nlohmann::json& json, const char* key) {
  return json.contains(key) ? static_cast<int>(json[key].size()) : 0;
}

// json[key], made a list when it is not one yet.
nlohmann::json& array_at(nlohmann::json& json, const char* key) {
  nlohmann::json& list = json[key];
  if (!list.is_array()) list = nlohmann::json::array();
  return list;
}

int last_index(const nlohmann::json& list) { return static_cast<int>(list.size()) - 1; }

bool in_range(int index, std::size_t size) { return index >= 0 && index < static_cast<int>(size); }

void write_bounds(nlohmann::json& accessor, const Bounds& bounds) {
  accessor[kMin] = {bounds.min.x, bounds.min.y, bounds.min.z};
  accessor[kMax] = {bounds.max.x, bounds.max.y, bounds.max.z};
}

// Adds every entry of model[key] to json[key], renumbered by `renumber`.
template <typename Renumber>
void append_entries(nlohmann::json& json, const nlohmann::json& model, const char* key, Renumber renumber) {
  for (const nlohmann::json& entry : list_of(model, key)) array_at(json, key).push_back(renumber(entry));
}

NodeDesc named_node(const std::string& name, int parent) {
  NodeDesc node;
  node.name = name;
  node.parent = parent;
  return node;
}

bool same_color(const Color& a, const Color& b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

bool same_vec3(Vec3 a, Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

bool same_materials(const std::vector<MaterialDesc>& a, const std::vector<MaterialDesc>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    const MaterialDesc& x = a[i];
    const MaterialDesc& y = b[i];
    if (!same_color(x.base_color, y.base_color) || x.metallic != y.metallic || x.roughness != y.roughness ||
        !same_vec3(x.emissive, y.emissive) || x.name != y.name) {
      return false;
    }
  }
  return true;
}

bool same_light(const std::optional<LightDesc>& a, const std::optional<LightDesc>& b) {
  if (a.has_value() != b.has_value()) return false;
  return !a || (a->type == b->type && a->color.r == b->color.r && a->color.g == b->color.g &&
                a->color.b == b->color.b && a->intensity == b->intensity && a->range == b->range);
}

bool same_camera(const std::optional<CameraDesc>& a, const std::optional<CameraDesc>& b) {
  if (a.has_value() != b.has_value()) return false;
  return !a || (a->fov_y_radians == b->fov_y_radians && a->near_plane == b->near_plane && a->far_plane == b->far_plane);
}

bool same_transform(const Transform& a, const Transform& b) {
  return same_vec3(a.position, b.position) && a.rotation.x == b.rotation.x && a.rotation.y == b.rotation.y &&
         a.rotation.z == b.rotation.z && a.rotation.w == b.rotation.w && same_vec3(a.scale, b.scale);
}

bool same_nodes(const std::vector<NodeDesc>& a, const std::vector<NodeDesc>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    const NodeDesc& x = a[i];
    const NodeDesc& y = b[i];
    if (!same_transform(x.local, y.local) || x.name != y.name || x.props != y.props || x.parent != y.parent ||
        x.children != y.children || x.mesh != y.mesh || x.skin != y.skin || !same_light(x.light, y.light) ||
        !same_camera(x.camera, y.camera)) {
      return false;
    }
  }
  return true;
}

// For each old index, where `order` puts it (kNoIndex when it is left out).
std::vector<int> positions_in(const std::vector<int>& order, std::size_t count) {
  std::vector<int> position(count, kNoIndex);
  for (int i = 0; i < static_cast<int>(order.size()); ++i) position[order[i]] = i;
  return position;
}

// Where each of `children` is after a reordering, without the ones it left out.
std::vector<int> kept_children(const std::vector<int>& children, const std::vector<int>& position) {
  std::vector<int> kept;
  for (const int child : children) {
    if (position[child] != kNoIndex) kept.push_back(position[child]);
  }
  return kept;
}

void detach_from_parent(std::vector<NodeDesc>& nodes, int child) {
  const int parent = nodes[child].parent;
  if (parent == kNoIndex) return;
  std::vector<int>& siblings = nodes[parent].children;
  siblings.erase(std::remove(siblings.begin(), siblings.end(), child), siblings.end());
}

// Puts `child` under `parent` (kNoIndex = top level), placed at `local`.
void move_under(std::vector<NodeDesc>& nodes, int child, int parent, const Transform& local) {
  detach_from_parent(nodes, child);
  nodes[child].parent = parent;
  if (parent != kNoIndex) nodes[parent].children.push_back(child);
  nodes[child].local = local;
}

// Where a node has to be placed under `parent` (kNoIndex = top level) to stay where it is in the world.
Transform local_under(const std::vector<NodeDesc>& nodes, int index, int parent) {
  const Transform world = scene_document_detail::world_of(nodes, index);
  return parent == kNoIndex ? world : relative_to(scene_document_detail::world_of(nodes, parent), world);
}

bool all_copied(const std::vector<int>& joints, const std::vector<int>& copy_of) {
  return std::all_of(joints.begin(), joints.end(),
                     [&copy_of](int joint) { return joint != kNoIndex && copy_of[joint] != kNoIndex; });
}

// The current index of each source node of a list (see current_index).
std::vector<int> current_indices(const std::vector<int>& current, const std::vector<int>& sources) {
  std::vector<int> indices;
  indices.reserve(sources.size());
  for (const int source : sources) indices.push_back(current_index(current, source));
  return indices;
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

namespace scene_document_detail {

std::string numbered_name(const std::string& base, int number) {
  char buffer[kSuffixBufferSize];
  std::snprintf(buffer, sizeof(buffer), kSuffixFormat, kSuffixDigits, number);
  return base + buffer;
}

std::vector<int> subtree(const std::vector<NodeDesc>& nodes, int index) {
  std::vector<int> out;
  std::function<void(int)> visit = [&](int i) {
    out.push_back(i);
    for (const int child : nodes[i].children) visit(child);
  };
  visit(index);
  return out;
}

Transform world_of(const std::vector<NodeDesc>& nodes, int index) {
  const NodeDesc& node = nodes[index];
  return node.parent == kNoIndex ? node.local : combine(world_of(nodes, node.parent), node.local);
}

std::vector<int> order_without(int count, const std::vector<int>& removed) {
  std::vector<int> order;
  for (int i = 0; i < count; ++i) {
    if (std::find(removed.begin(), removed.end(), i) == removed.end()) order.push_back(i);
  }
  return order;
}

std::vector<int> order_with_last(int count, const std::vector<int>& last) {
  std::vector<int> order = order_without(count, last);
  order.insert(order.end(), last.begin(), last.end());
  return order;
}

std::vector<NodeDesc> nodes_in_order(const std::vector<NodeDesc>& nodes, const std::vector<int>& order) {
  const std::vector<int> position = positions_in(order, nodes.size());
  std::vector<NodeDesc> out;
  out.reserve(order.size());
  for (const int old : order) out.push_back(nodes[old]);
  for (NodeDesc& node : out) {
    if (node.parent != kNoIndex) node.parent = position[node.parent];
    node.children = kept_children(node.children, position);
  }
  return out;
}

}  // namespace scene_document_detail

using namespace scene_document_detail;

std::variant<SceneDocument, std::string> SceneDocument::open(const std::filesystem::path& path) {
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
    mark_reshaped(mesh);
  }
}

void SceneDocument::mark_reshaped(int mesh) {
  MeshDesc& target = desc_.meshes.at(static_cast<std::size_t>(mesh));
  target.bounds = mesh_bounds_of(target.primitives.front());
  changed_meshes_.insert(mesh);
  unsaved_meshes_.insert(mesh);
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
  mark_reshaped(mesh);
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
  const nlohmann::json& attributes =
      glb_.json[kMeshes][static_cast<std::size_t>(mesh)][kPrimitives][kOnlyPrimitive][kAttributes];
  const auto accessor_of = [&attributes](const char* name) { return attributes.at(name).get<std::size_t>(); };
  if (!store_stream(accessor_of(kPositionAttribute), ground.positions) ||
      !store_stream(accessor_of(kNormalAttribute), ground.normals)) {
    error =
        "cannot save the shape of '" + source.name + "': its vertices are not stored the way the editor writes them";
    return false;
  }
  write_bounds(glb_.json[kAccessors][accessor_of(kPositionAttribute)], source.bounds);
  return true;
}

bool SceneDocument::store_stream(std::size_t accessor_index, const std::vector<float>& values) {
  const nlohmann::json& accessor = glb_.json[kAccessors][accessor_index];
  const nlohmann::json& view = glb_.json[kBufferViews][accessor.at(kBufferView).get<std::size_t>()];
  const std::size_t offset = view.value(kByteOffset, std::size_t{0}) + accessor.value(kByteOffset, std::size_t{0});
  const std::size_t bytes = values.size() * sizeof(float);
  const bool packed =
      !view.contains(kByteStride) || view[kByteStride].get<std::size_t>() == config::kFloatsPerPosition * sizeof(float);
  const bool same_count = accessor.at(kCount).get<std::size_t>() * config::kFloatsPerPosition == values.size();
  if (!packed || !same_count || offset + bytes > glb_.bin.size()) return false;
  std::memcpy(glb_.bin.data() + offset, values.data(), bytes);
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
    const std::string candidate = numbered_name(base, suffix);
    if (!taken(candidate)) return candidate;
  }
}

int SceneDocument::duplicate(int index) {
  checkpoint();
  const std::vector<int> source = subtree(desc_.nodes, index);
  const int first_copy = node_count();
  std::vector<int> copy_of(desc_.nodes.size(), kNoIndex);
  for (std::size_t i = 0; i < source.size(); ++i) copy_of[source[i]] = first_copy + static_cast<int>(i);
  append_copies(source, copy_of);
  // Each copy is a node of its own in the file too, and a character copied with its bones gets
  // its own skeleton (its animation is not copied: the copy keeps the pose it was copied in).
  give_copies_own_source_nodes(first_copy, source.size());
  give_copies_own_skins(first_copy, source.size(), copy_of);
  touch();
  return first_copy;
}

void SceneDocument::append_copies(const std::vector<int>& source, const std::vector<int>& copy_of) {
  std::vector<NodeDesc>& nodes = desc_.nodes;
  const int root = source.front();
  for (const int original : source) {
    NodeDesc copy = nodes[original];
    copy.name = unique_name(copy.name);
    copy.parent = original == root ? copy.parent : copy_of[copy.parent];
    for (int& child : copy.children) child = copy_of[child];
    nodes.push_back(std::move(copy));  // names stay unique because each copy is added before the next
  }
  const int first_copy = copy_of[root];
  if (nodes[first_copy].parent != kNoIndex) nodes[nodes[first_copy].parent].children.push_back(first_copy);
}

void SceneDocument::give_copies_own_source_nodes(int first_copy, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    NodeDesc& copy = desc_.nodes[first_copy + static_cast<int>(i)];
    if (!in_range(copy.source_node, source_nodes_.size())) continue;
    source_nodes_.push_back(source_nodes_[copy.source_node]);
    copy.source_node = last_index(source_nodes_);
  }
}

void SceneDocument::give_copies_own_skins(int first_copy, std::size_t count, const std::vector<int>& copy_of) {
  std::map<int, int> copied_skins;
  for (std::size_t i = 0; i < count; ++i) {
    NodeDesc& copy = desc_.nodes[first_copy + static_cast<int>(i)];
    if (!in_range(copy.skin, desc_.skins.size())) continue;
    if (copied_skins.count(copy.skin) == 0) {
      const std::optional<int> skin = copy_skin(copy.skin, copy_of);
      if (!skin) continue;  // bones outside the copy stay shared with the original
      copied_skins[copy.skin] = *skin;
    }
    copy.skin = copied_skins[copy.skin];
  }
}

std::optional<int> SceneDocument::copy_skin(int original, const std::vector<int>& copy_of) {
  SkinDesc skin = desc_.skins[original];
  if (!all_copied(skin.joints, copy_of)) return std::nullopt;
  nlohmann::json json = source_skins_[original];
  std::vector<int> joint_sources = point_at_copies(skin, json, copy_of);
  json.erase(kSkeleton);
  desc_.skins.push_back(std::move(skin));
  source_skins_.push_back(std::move(json));
  skin_joint_sources_.push_back(std::move(joint_sources));
  return static_cast<int>(desc_.skins.size()) - 1;
}

std::vector<int> SceneDocument::point_at_copies(SkinDesc& skin, nlohmann::json& json,
                                                const std::vector<int>& copy_of) const {
  std::vector<int> joint_sources;
  for (std::size_t j = 0; j < skin.joints.size(); ++j) {
    skin.joints[j] = copy_of[skin.joints[j]];
    joint_sources.push_back(desc_.nodes[skin.joints[j]].source_node);
    json[kJoints][j] = joint_sources.back();
  }
  return joint_sources;
}

void SceneDocument::remove(int index) {
  checkpoint();
  const std::vector<int> kept = order_without(node_count(), subtree(desc_.nodes, index));
  desc_.nodes = nodes_in_order(desc_.nodes, kept);
  refresh_node_references();
  touch();
}

void SceneDocument::restore(Snapshot& state) {
  desc_.nodes = std::move(state.nodes);
  refresh_node_references();
  restore_materials(state.materials);
  restore_shapes(state.shapes);
  version_ = state.version;
  changed_since_checkpoint_ = false;
}

bool SceneDocument::undo() {
  if (undo_.empty()) return false;
  redo_.push_back(snapshot());
  restore(undo_.back());
  undo_.pop_back();
  return true;
}

bool SceneDocument::redo() {
  if (redo_.empty()) return false;
  undo_.push_back(snapshot());
  restore(redo_.back());
  redo_.pop_back();
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
  for (const MaterialDesc& material : desc_.materials) {
    if (needs_emissive_strength(material)) add_extension_used(json, kEmissiveStrengthExtension);
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

std::vector<int> SceneDocument::current_node_indices() const {
  std::vector<int> current(source_nodes_.size(), kNoIndex);
  for (int i = 0; i < node_count(); ++i) {
    const int source = desc_.nodes[i].source_node;
    // A duplicated node shares its source with the original: the first one keeps the skeleton.
    if (source >= 0 && source < static_cast<int>(current.size()) && current[source] == kNoIndex) current[source] = i;
  }
  return current;
}

int SceneDocument::source_of(int node) const { return node == kNoIndex ? kNoIndex : desc_.nodes.at(node).source_node; }

void SceneDocument::remember_node_references(std::size_t first_skin, std::size_t first_animation) {
  for (std::size_t s = first_skin; s < desc_.skins.size(); ++s) remember_joints(desc_.skins[s]);
  for (std::size_t a = first_animation; a < desc_.animations.size(); ++a) remember_channels(desc_.animations[a]);
}

void SceneDocument::remember_joints(const SkinDesc& skin) {
  std::vector<int> sources;
  sources.reserve(skin.joints.size());
  for (const int joint : skin.joints) sources.push_back(source_of(joint));
  skin_joint_sources_.push_back(std::move(sources));
}

void SceneDocument::remember_channels(const AnimationDesc& animation) {
  std::vector<int> sources;
  sources.reserve(animation.channels.size());
  for (const AnimationChannel& channel : animation.channels) sources.push_back(source_of(channel.node));
  channel_node_sources_.push_back(std::move(sources));
}

void SceneDocument::refresh_node_references() {
  if (desc_.skins.empty() && desc_.animations.empty()) return;
  const std::vector<int> current = current_node_indices();
  for (std::size_t s = 0; s < desc_.skins.size(); ++s) {
    desc_.skins[s].joints = current_indices(current, skin_joint_sources_[s]);
  }
  for (std::size_t a = 0; a < desc_.animations.size(); ++a) {
    const std::vector<int> nodes = current_indices(current, channel_node_sources_[a]);
    std::vector<AnimationChannel>& channels = desc_.animations[a].channels;
    for (std::size_t c = 0; c < channels.size(); ++c) channels[c].node = nodes[c];
  }
}

void SceneDocument::sync_skins_and_animations(nlohmann::json& json) const {
  const std::vector<int> current = current_node_indices();
  sync_skins(json, current);
  sync_animations(json, current);
}

// A skin that lost a joint (a deleted bone, or a whole deleted or undone character) is left out,
// and the meshes that used it are saved rigid; the others keep working.
void SceneDocument::sync_skins(nlohmann::json& json, const std::vector<int>& current) const {
  nlohmann::json skins = nlohmann::json::array();
  std::vector<int> written(source_skins_.size(), kNoIndex);  // skin index -> its index in the file
  for (std::size_t s = 0; s < source_skins_.size(); ++s) {
    std::optional<nlohmann::json> skin = skin_with_current_nodes(source_skins_[s], current);
    if (!skin) continue;
    written[s] = static_cast<int>(skins.size());
    skins.push_back(std::move(*skin));
  }
  sync_node_skins(json, written);
  set_unless_empty(json, kSkins, skins);
}

// Channels of deleted nodes are dropped.
void SceneDocument::sync_animations(nlohmann::json& json, const std::vector<int>& current) const {
  nlohmann::json animations = nlohmann::json::array();
  for (const nlohmann::json& animation : source_animations_) {
    std::optional<nlohmann::json> kept = animation_with_current_nodes(animation, current);
    if (kept) animations.push_back(std::move(*kept));
  }
  set_unless_empty(json, kAnimations, animations);
}

void SceneDocument::sync_node_skins(nlohmann::json& json, const std::vector<int>& written) const {
  for (int i = 0; i < node_count(); ++i) {
    const int skin = desc_.nodes[i].skin;
    const int in_file = in_range(skin, written.size()) ? written[skin] : kNoIndex;
    if (in_file != kNoIndex) {
      json[kNodes][i][kSkin] = in_file;
      continue;
    }
    if (skin != kNoIndex) log::warning("'" + desc_.nodes[i].name + "' lost part of its skeleton: saved without it");
    json[kNodes][i].erase(kSkin);
  }
}

SceneDocument SceneDocument::create_empty(const std::filesystem::path& path) {
  SceneDocument document;
  document.path_ = path;
  document.glb_.json = empty_document();
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

std::size_t SceneDocument::append_binary(const std::uint8_t* data, std::size_t bytes) {
  glb_.bin.resize(glb_padded_size(glb_.bin.size()), 0);
  const std::size_t offset = glb_.bin.size();
  glb_.bin.insert(glb_.bin.end(), data, data + bytes);
  return offset;
}

int SceneDocument::append_buffer_view(const void* data, std::size_t bytes, int target) {
  const std::size_t offset = append_binary(static_cast<const std::uint8_t*>(data), bytes);
  nlohmann::json& views = array_at(glb_.json, kBufferViews);
  views.push_back({{kBuffer, kOnlyBuffer}, {kByteOffset, offset}, {kByteLength, bytes}, {kTarget, target}});
  return last_index(views);
}

int SceneDocument::append_accessor(int view, int component, std::size_t count, const char* type) {
  nlohmann::json& accessors = array_at(glb_.json, kAccessors);
  accessors.push_back({{kBufferView, view}, {kComponentType, component}, {kCount, count}, {kType, type}});
  return last_index(accessors);
}

int SceneDocument::append_vertex_stream(const std::vector<float>& values, std::size_t vertices, const char* type) {
  const int view = append_buffer_view(values.data(), values.size() * sizeof(float), kArrayBufferTarget);
  return append_accessor(view, kFloatComponent, vertices, type);
}

int SceneDocument::append_mesh(const std::string& name, const PrimitiveDesc& primitive) {
  MeshDesc mesh;
  mesh.name = name;
  mesh.primitives.push_back(primitive);
  mesh.bounds = mesh_bounds_of(primitive);
  const nlohmann::json attributes = append_attributes(primitive, mesh.bounds);
  const int indices = append_indices(primitive.indices);
  nlohmann::json json_primitive = {{kAttributes, attributes}, {kIndices, indices}};
  if (primitive.material != kNoIndex) json_primitive[kMaterial] = primitive.material;
  array_at(glb_.json, kMeshes).push_back({{kName, name}, {kPrimitives, nlohmann::json::array({json_primitive})}});
  desc_.meshes.push_back(std::move(mesh));
  return static_cast<int>(desc_.meshes.size()) - 1;
}

nlohmann::json SceneDocument::append_attributes(const PrimitiveDesc& primitive, const Bounds& bounds) {
  const std::size_t vertices = primitive.positions.size() / config::kFloatsPerPosition;
  const int position = append_vertex_stream(primitive.positions, vertices, kVec3Type);
  write_bounds(glb_.json[kAccessors][position], bounds);
  const int normal = append_vertex_stream(primitive.normals, vertices, kVec3Type);
  nlohmann::json attributes = {{kPositionAttribute, position}, {kNormalAttribute, normal}};
  if (!primitive.texcoords.empty()) {
    attributes[kTexcoordAttribute] = append_vertex_stream(primitive.texcoords, vertices, kVec2Type);
  }
  return attributes;
}

int SceneDocument::append_indices(const std::vector<std::uint32_t>& indices) {
  const int view =
      append_buffer_view(indices.data(), indices.size() * sizeof(std::uint32_t), kElementArrayBufferTarget);
  return append_accessor(view, kUnsignedIntComponent, indices.size(), kScalarType);
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
  node.props[config::kPropCollider] = std::string(primitive_collider(shape));
  return append_node(std::move(node));
}

int SceneDocument::add_terrain(const std::string& name, const TerrainGrid& grid, Color color, int parent) {
  checkpoint();
  PrimitiveDesc geometry = make_terrain(grid);
  geometry.material = append_plain_material(name, color, kTerrainRoughness);
  NodeDesc node = named_node(name, parent);
  node.mesh = append_mesh(name, geometry);
  node.props[config::kPropTerrain] = true;
  node.props[config::kPropTerrainSize] = static_cast<double>(grid.size);
  node.props[config::kPropTerrainCells] = static_cast<double>(grid.cells);
  node.props[config::kPropCollider] = std::string(kTerrainCollider);
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

bool SceneDocument::can_reparent(int index, int new_parent) const {
  const int count = node_count();
  if (index < 0 || index >= count || new_parent < kNoIndex || new_parent >= count) return false;
  const std::vector<int> moving = subtree(desc_.nodes, index);
  const bool into_itself = std::find(moving.begin(), moving.end(), new_parent) != moving.end();
  return !into_itself && desc_.nodes.at(index).parent != new_parent;
}

int SceneDocument::reparent(int index, int new_parent) {
  if (!can_reparent(index, new_parent)) return index;
  checkpoint();
  std::vector<NodeDesc>& nodes = desc_.nodes;
  const Transform local = local_under(nodes, index, new_parent);
  // Rebuild the list: everything else in order, then the moved subtree last (after its new parent).
  const std::vector<int> order = order_with_last(node_count(), subtree(nodes, index));
  const std::vector<int> position = positions_in(order, nodes.size());
  std::vector<NodeDesc> reordered = nodes_in_order(nodes, order);
  const int moved = position[index];
  move_under(reordered, moved, new_parent == kNoIndex ? kNoIndex : position[new_parent], local);
  nodes = std::move(reordered);
  refresh_node_references();
  touch();
  return moved;
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

void SceneDocument::append_model_json(const nlohmann::json& model, const model_import::JsonOffsets& offsets,
                                      const std::filesystem::path& model_dir) {
  append_model_geometry(model, offsets);
  append_model_materials(model, offsets, model_dir);
  append_entries(glb_.json, model, kMeshes,
                 [&](const nlohmann::json& mesh) { return model_import::imported_mesh(mesh, offsets); });
  append_model_sources(model, offsets);
  model_import::merge_string_list(glb_.json, model, kExtensionsUsed);
  model_import::merge_string_list(glb_.json, model, kExtensionsRequired);
}

void SceneDocument::append_model_geometry(const nlohmann::json& model, const model_import::JsonOffsets& offsets) {
  using nlohmann::json;
  append_entries(glb_.json, model, kBufferViews,
                 [&](const json& view) { return model_import::imported_buffer_view(view, offsets); });
  append_entries(glb_.json, model, kAccessors,
                 [&](const json& accessor) { return model_import::imported_accessor(accessor, offsets); });
}

void SceneDocument::append_model_materials(const nlohmann::json& model, const model_import::JsonOffsets& offsets,
                                           const std::filesystem::path& model_dir) {
  using nlohmann::json;
  const std::filesystem::path scene_dir = path_.parent_path();
  json& out = glb_.json;
  append_entries(out, model, kImages,
                 [&](const json& image) { return model_import::imported_image(image, offsets, model_dir, scene_dir); });
  append_entries(out, model, kSamplers, [](const json& sampler) { return sampler; });
  append_entries(out, model, kTextures,
                 [&](const json& texture) { return model_import::imported_texture(texture, offsets); });
  append_entries(out, model, kMaterials,
                 [&](const json& material) { return model_import::imported_material(material, offsets); });
}

// Nodes, skeletons and animations come along too, kept apart like the scene's own (saving
// numbers them again).
void SceneDocument::append_model_sources(const nlohmann::json& model, const model_import::JsonOffsets& offsets) {
  using nlohmann::json;
  for (const json& node : list_of(model, kNodes)) source_nodes_.push_back(model_import::imported_node(node, offsets));
  for (const json& skin : list_of(model, kSkins)) source_skins_.push_back(model_import::imported_skin(skin, offsets));
  for (const json& animation : list_of(model, kAnimations)) {
    source_animations_.push_back(model_import::imported_animation(animation, offsets));
  }
}

int SceneDocument::append_model_description(const SceneDesc& model, const model_import::JsonOffsets& json_offsets,
                                            const std::string& name, int parent) {
  model_import::DescOffsets offsets = description_offsets(json_offsets, desc_.skins.size());
  append_model_assets(model, offsets);
  offsets.root = append_node(named_node(name, parent));
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
  const std::optional<GlbFile> source = read_importable_glb(path, error);
  if (!source) return kNoIndex;
  return append_model(loaded.scene, *source, path, parent);
}

int SceneDocument::append_model(const SceneDesc& scene, const GlbFile& source, const std::filesystem::path& path,
                                int parent) {
  model_import::JsonOffsets offsets = import_offsets();
  offsets.bin = append_binary(source.bin.data(), source.bin.size());
  append_model_json(source.json, offsets, path.parent_path());
  checkpoint();
  return append_model_description(scene, offsets, path.stem().string(), parent);
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
