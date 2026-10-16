// The tree of objects of a glTF file.

#include "core/safe_path.hpp"
#include "scene/gltf/extras_parser.hpp"
#include "scene/gltf/gltf_checks.hpp"
#include "scene/gltf/gltf_reader.hpp"

namespace redgem {
namespace {

constexpr float kOpaqueAlpha = 1.0f;

Transform read_local_transform(const cgltf_node& node) {
  if (node.has_matrix) return transform_from_matrix(node.matrix);
  Transform t;
  if (node.has_translation) t.position = vec3_from(node.translation);
  if (node.has_rotation) t.rotation = normalize(quat_from(node.rotation));
  if (node.has_scale) t.scale = vec3_from(node.scale);
  return t;
}

// The only kind of camera a scene keeps.
bool is_perspective(const cgltf_camera* camera) {
  return camera != nullptr && camera->type == cgltf_camera_type_perspective;
}

std::optional<CameraDesc> read_camera(const cgltf_camera* camera) {
  if (!is_perspective(camera)) return std::nullopt;
  const auto& perspective = camera->data.perspective;
  return CameraDesc{perspective.yfov, perspective.znear, perspective.has_zfar ? perspective.zfar : 0.0f};
}

LightType light_type_of(cgltf_light_type type) {
  if (type == cgltf_light_type_point) return LightType::Point;
  return type == cgltf_light_type_spot ? LightType::Spot : LightType::Directional;
}

LightDesc read_light(const cgltf_light& light) {
  LightDesc out;
  out.type = light_type_of(light.type);
  out.color = {light.color[kRedChannel], light.color[kGreenChannel], light.color[kBlueChannel], kOpaqueAlpha};
  out.intensity = light.intensity;
  out.range = light.range;
  return out;
}

// The scene the file opens with, or its first one; null for a file that has none.
const cgltf_scene* default_scene(const cgltf_data& data) {
  if (data.scene != nullptr) return data.scene;
  return data.scenes_count > 0 ? &data.scenes[0] : nullptr;
}

std::string skipped_property_warning(const std::string& node_name, const std::string& key) {
  return "node '" + node_name + "': property '" + key + "' is not a scalar, ignored";
}

}  // namespace

void GltfReader::load_nodes() {
  const cgltf_scene* scene = default_scene(data_);
  added_.assign(data_.nodes_count, false);
  if (scene == nullptr) return add_parentless_nodes();
  for (cgltf_size i = 0; i < scene->nodes_count; ++i) add_tree(*scene->nodes[i]);
}

void GltfReader::add_parentless_nodes() {
  for (cgltf_size i = 0; i < data_.nodes_count; ++i) {
    if (data_.nodes[i].parent == nullptr) add_tree(data_.nodes[i]);
  }
}

// A list of work to do instead of a function calling itself: a file decides how deep its nodes
// are nested, and must not decide how much stack is used.
void GltfReader::add_tree(const cgltf_node& root) {
  std::vector<PendingNode> pending = {{&root, kNoIndex, 1}};
  while (has_more_to_add(pending)) {
    const PendingNode next = pending.back();
    pending.pop_back();
    add_node(next, pending);
  }
}

void GltfReader::add_node(const PendingNode& next, std::vector<PendingNode>& pending) {
  const auto source = static_cast<std::size_t>(index_of(next.node, data_.nodes));
  if (added_[source]) return;
  if (next.depth > gltf_checks::kMaxNodeDepth) {
    problem_ = "its nodes are nested more than " + std::to_string(gltf_checks::kMaxNodeDepth) + " deep";
    return;
  }
  added_[source] = true;
  const int index = static_cast<int>(result_.scene.nodes.size());
  result_.scene.nodes.push_back(read_node(*next.node, next.parent, index));
  if (next.parent != kNoIndex) result_.scene.nodes[next.parent].children.push_back(index);
  // (Last child first: the list hands back the first child next.)
  for (cgltf_size c = next.node->children_count; c > 0; --c) {
    pending.push_back({next.node->children[c - 1], index, next.depth + 1});
  }
}

NodeDesc GltfReader::read_node(const cgltf_node& node, int parent, int index) {
  NodeDesc out;
  out.name = printable(text_or(node.name, "node_" + std::to_string(index)));
  out.parent = parent;
  out.local = read_local_transform(node);
  out.mesh = index_of(node.mesh, data_.meshes);
  out.camera = read_camera(node.camera);
  if (node.light != nullptr) out.light = read_light(*node.light);
  out.source_node = index_of(&node, data_.nodes);
  out.source_camera = index_of(node.camera, data_.cameras);
  out.source_light = index_of(node.light, data_.lights);
  out.props = read_props(node, out.name);
  return out;
}

PropertyMap GltfReader::read_props(const cgltf_node& node, const std::string& node_name) {
  if (node.extras.data == nullptr) return {};
  ExtrasParseResult parsed = parse_extras(node.extras.data);
  if (!parsed.ok) warn("node '" + node_name + "': bad extras (" + parsed.error + ")");
  for (const std::string& key : parsed.skipped_keys) warn(skipped_property_warning(node_name, key));
  return std::move(parsed.properties);
}

std::vector<int> GltfReader::node_indices() const {
  std::vector<int> indices(data_.nodes_count, kNoIndex);
  for (std::size_t i = 0; i < result_.scene.nodes.size(); ++i) {
    const int source = result_.scene.nodes[i].source_node;
    if (source != kNoIndex) indices[static_cast<std::size_t>(source)] = static_cast<int>(i);
  }
  return indices;
}

}  // namespace redgem
