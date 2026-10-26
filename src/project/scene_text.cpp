#include "project/scene_text.hpp"
#include "scene/scene_ids.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "core/translations.hpp"
#include "editor/node_tree.hpp"
#include "project/scene_assignments.hpp"
#include "scene/props.hpp"
#include "script/script_naming.hpp"

namespace redgem::tools {
namespace {

constexpr const char* kPathSeparator = "/";
constexpr const char* kIndent = "  ";  // per level of the hierarchy
constexpr const char* kNumberFormat = "%.*f";
constexpr std::size_t kNumberBuffer = 64;
constexpr const char* kQuote = "\"";
constexpr const char* kListSeparator = ",";
// Light types, as Blender calls them.
constexpr const char* kSunName = "sun";
constexpr const char* kPointName = "point";
constexpr const char* kSpotName = "spot";
constexpr double kDecimalBase = 10.0;
// The words of a line, before each "=": what `scene set` takes back where it can.
constexpr const char* kPositionKey = "pos";
constexpr const char* kRotationKey = "rot";
constexpr const char* kScaleKey = "scale";
constexpr const char* kMeshKey = "mesh";
constexpr const char* kLightKey = "light";
constexpr const char* kScriptsKey = "scripts";
constexpr const char* kMaterialKey = "material";
constexpr const char* kTrue = "true";
constexpr const char* kFalse = "false";

// "1.500" -> "1.5", "2.000" -> "2".
std::string without_trailing_zeros(std::string text) {
  if (text.find('.') == std::string::npos) return text;
  while (text.back() == '0') text.pop_back();
  if (text.back() == '.') text.pop_back();
  return text;
}

bool is_identity(Quat rotation) { return approx_equal(rotation, Quat::identity(), kEpsilon); }
bool is_unit_scale(Vec3 scale) { return approx_equal(scale, Vec3::one(), kEpsilon); }
bool is_uniform(Vec3 scale) {
  return approx_equal(scale.x, scale.y, kEpsilon) && approx_equal(scale.x, scale.z, kEpsilon);
}

// Whether a text, as `set` takes it back, would be read as something else without quotes: nothing,
// a text with spaces, or one that reads as a number or true/false ("12").
bool needs_quotes(const std::string& text) {
  return text.empty() || text.find(' ') != std::string::npos || !std::holds_alternative<std::string>(typed_value(text));
}

// A scale as one number when it is the same on every side.
std::string scale_text(Vec3 scale, int decimals) {
  return is_uniform(scale) ? number_text(scale.x, decimals) : vector_text(scale, decimals);
}

// A script as a line names it: its class, when that names its file back.
std::string script_word(const std::string& script) {
  const std::string class_name = class_name_for_script(script);
  return script_for_class_name(class_name) == script ? class_name : script;
}

void add(std::string& line, const std::string& key, const std::string& value) { line += " " + key + "=" + value; }

// Where it is, turned and sized, as far as it is not where an object starts.
void add_placement(std::string& line, const Transform& local) {
  if (!approx_equal(local.position, Vec3::zero(), kEpsilon))
    add(line, kPositionKey, vector_text(local.position, kListDecimals));
  if (!is_identity(local.rotation)) {
    add(line, kRotationKey, vector_text(quat_to_euler_degrees(local.rotation), kListAngleDecimals));
  }
  if (!is_unit_scale(local.scale)) add(line, kScaleKey, scale_text(local.scale, kListDecimals));
}

// What it shows or is: its mesh, its light, its camera, its scripts.
void add_parts(std::string& line, const SceneDesc& scene, const NodeDesc& node) {
  const std::string mesh = shown_mesh(scene, node);
  if (!mesh.empty()) add(line, kMeshKey, mesh);
  if (node.light) add(line, kLightKey, light_name(node.light->type));
  if (node.camera) line += std::string(" ") + kCameraWord;
  const std::string scripts = scripts_text(node.props);
  if (!scripts.empty()) add(line, kScriptsKey, scripts);
}

void add_properties(std::string& line, const PropertyMap& props) {
  for (const auto& [key, value] : props) {
    if (key != props::kScript) add(line, key, property_text(value));
  }
}

// Every node, top-level ones in their order, each followed by its children: the order the editor
// saves them in. (Walked with a list of its own, not by calling itself: a file decides how deep its
// nodes are nested, and must not decide how much stack is used.)
std::vector<int> saved_order(const SceneDesc& scene) {
  std::vector<int> order;
  for (int top = 0; top < static_cast<int>(scene.nodes.size()); ++top) {
    if (scene.nodes[top].parent != kNoIndex) continue;
    const std::vector<int> family = node_tree::subtree(scene.nodes, top);
    order.insert(order.end(), family.begin(), family.end());
  }
  return order;
}

// How deep each node is under the top of the scene, from nodes in saved order (parents first).
std::vector<std::size_t> depths(const SceneDesc& scene, const std::vector<int>& order) {
  std::vector<std::size_t> depth(scene.nodes.size(), 0);
  for (const int index : order) {
    const int parent = scene.nodes[index].parent;
    if (parent != kNoIndex) depth[index] = depth[parent] + 1;
  }
  return depth;
}

// Writes the hierarchy into lines, as far as the limits let it.
class ListWriter {
 public:
  ListWriter(const SceneDesc& scene, const ListLimits& limits) : scene_(scene), limits_(limits) {}

  void write_all() {
    const std::vector<int> order = saved_order(scene_);
    const std::vector<std::size_t> depth = depths(scene_, order);
    for (const int index : order) {
      if (room_left() && !too_deep(depth[index])) write(index, depth[index]);
    }
  }
  const std::string& text() const { return text_; }
  std::size_t shown() const { return shown_; }

 private:
  void write(int index, std::size_t depth) {
    for (std::size_t level = 0; level < depth; ++level) text_ += kIndent;
    text_ += object_line(scene_, index) + "\n";
    ++shown_;
  }
  bool room_left() const { return limits_.most == 0 || shown_ < limits_.most; }
  bool too_deep(std::size_t depth) const { return depth > limits_.depth; }

  const SceneDesc& scene_;
  const ListLimits& limits_;
  std::string text_;
  std::size_t shown_ = 0;
};

std::string detail_line(const char* key, const std::string& value) { return std::string(key) + ": " + value + "\n"; }

std::string light_detail(const LightDesc& light) {
  return std::string(light_name(light.type)) +
         " color=" + vector_text({light.color.r, light.color.g, light.color.b}, kDetailDecimals) +
         " intensity=" + number_text(light.intensity, kDetailDecimals) +
         " range=" + number_text(light.range, kDetailDecimals);
}

std::string camera_detail(const CameraDesc& camera) {
  return "fov=" + number_text(radians_to_degrees(camera.fov_y_radians), kListAngleDecimals) +
         " near=" + number_text(camera.near_plane, kDetailDecimals) +
         " far=" + number_text(camera.far_plane, kDetailDecimals);
}

std::string mesh_detail(const SceneDesc& scene, const MeshDesc& mesh) {
  std::string text = mesh.name;
  for (const PrimitiveDesc& part : mesh.primitives) {
    if (is_index_of(part.material, scene.materials.size()))
      text += std::string(" ") + kMaterialKey + "=" + scene.materials[part.material].name;
  }
  return text;
}

std::string children_detail(const SceneDesc& scene, const NodeDesc& node) {
  std::string text;
  for (const int child : node.children) {
    text += (text.empty() ? "" : ", ") + object_id(scene, child) + " " + scene.nodes[child].name;
  }
  return text;
}

std::string placement_detail(const Transform& local) {
  return detail_line("position", vector_text(local.position, kDetailDecimals)) +
         detail_line("rotation", vector_text(quat_to_euler_degrees(local.rotation), kDetailDecimals)) +
         detail_line("scale", vector_text(local.scale, kDetailDecimals));
}

std::string parts_detail(const SceneDesc& scene, const NodeDesc& node) {
  std::string text;
  if (is_index_of(node.mesh, scene.meshes.size()))
    text += detail_line("mesh", mesh_detail(scene, scene.meshes[node.mesh]));
  if (node.light) text += detail_line("light", light_detail(*node.light));
  if (node.camera) text += detail_line("camera", camera_detail(*node.camera));
  return text;
}

std::string properties_detail(const PropertyMap& props) {
  std::string text;
  for (const auto& [key, value] : props) {
    if (key != props::kScript) text += std::string(kIndent) + key + "=" + property_text(value) + "\n";
  }
  return text.empty() ? text : "props:\n" + text;
}

}  // namespace

std::string object_id(const SceneDesc& scene, int index) { return kIdMark + scene_node_id(scene, index); }

std::string number_text(double value, int decimals) {
  const double scale = std::pow(kDecimalBase, decimals);
  double rounded = std::round(value * scale) / scale;
  if (rounded == 0.0) rounded = 0.0;  // (no "-0")
  char buffer[kNumberBuffer];
  std::snprintf(buffer, sizeof(buffer), kNumberFormat, decimals, rounded);
  return without_trailing_zeros(buffer);
}

std::string vector_text(Vec3 value, int decimals) {
  return number_text(value.x, decimals) + kListSeparator + number_text(value.y, decimals) + kListSeparator +
         number_text(value.z, decimals);
}

std::string property_text(const PropertyValue& value) {
  if (const bool* flag = std::get_if<bool>(&value)) return *flag ? kTrue : kFalse;
  if (const double* number = std::get_if<double>(&value)) return number_text(*number, kDetailDecimals);
  const std::string& text = std::get<std::string>(value);
  return needs_quotes(text) ? kQuote + text + kQuote : text;
}

std::string object_path(const SceneDesc& scene, int index) {
  std::string path = scene.nodes[index].name;
  for (int parent = scene.nodes[index].parent; parent != kNoIndex; parent = scene.nodes[parent].parent) {
    path.insert(0, scene.nodes[parent].name + kPathSeparator);
  }
  return path;
}

const char* light_name(LightType type) {
  if (type == LightType::Directional) return kSunName;
  return type == LightType::Point ? kPointName : kSpotName;
}

// (Most meshes are named as their object, and a list does not say it twice.)
std::string shown_mesh(const SceneDesc& scene, const NodeDesc& node) {
  if (!is_index_of(node.mesh, scene.meshes.size())) return {};
  const std::string& name = scene.meshes[node.mesh].name;
  return name == node.name ? std::string() : name;
}

std::string scripts_text(const PropertyMap& props) {
  std::string words;
  for (const std::string& script : split_script_list(get_string(props, props::kScript).value_or(""))) {
    words += (words.empty() ? "" : kListSeparator) + script_word(script);
  }
  return words;
}

std::string object_line(const SceneDesc& scene, int index) {
  const NodeDesc& node = scene.nodes[index];
  std::string line = object_id(scene, index) + " " + node.name;
  add_placement(line, node.local);
  add_parts(line, scene, node);
  add_properties(line, node.props);
  return line;
}

std::string scene_list(const SceneDesc& scene, const std::string& scene_name, const ListLimits& limits) {
  ListWriter writer(scene, limits);
  writer.write_all();
  const std::size_t total = scene.nodes.size();
  std::string text = tr("project.scene.list_header", {{"scene", scene_name}, {"count", std::to_string(total)}}) + "\n";
  text += writer.text();
  if (writer.shown() < total) {
    text += tr("project.scene.list_more", {{"count", std::to_string(total - writer.shown())}}) + "\n";
  }
  return text;
}

std::string object_detail(const SceneDesc& scene, int index) {
  const NodeDesc& node = scene.nodes[index];
  std::string text = object_id(scene, index) + " " + node.name + "\n" + detail_line("path", object_path(scene, index));
  if (node.parent != kNoIndex)
    text += detail_line("parent", object_id(scene, node.parent) + " " + scene.nodes[node.parent].name);
  text += placement_detail(node.local) + parts_detail(scene, node);
  const auto scripts = get_string(node.props, props::kScript);
  if (scripts) text += detail_line("scripts", *scripts + " (" + scripts_text(node.props) + ")");
  text += properties_detail(node.props);
  if (!node.children.empty()) text += detail_line("children", children_detail(scene, node));
  return text;
}

}  // namespace redgem::tools
