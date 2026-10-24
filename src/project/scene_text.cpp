#include "project/scene_text.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "core/translations.hpp"
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
constexpr const char* kCameraWord = "camera";
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

// A scale as one number when it is the same on every side.
std::string scale_text(Vec3 scale, int decimals) {
  return is_uniform(scale) ? number_text(scale.x, decimals) : vector_text(scale, decimals);
}

const char* light_name(LightType type) {
  if (type == LightType::Directional) return kSunName;
  return type == LightType::Point ? kPointName : kSpotName;
}

// The classes of the scripts an object has: "Coin,Spin".
std::string script_classes(const PropertyMap& props) {
  std::string classes;
  const auto list = get_string(props, props::kScript);
  for (const std::string& script : split_script_list(list.value_or(""))) {
    classes += (classes.empty() ? "" : kListSeparator) + class_name_for_script(script);
  }
  return classes;
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

// Whether an object shows a mesh named otherwise than itself (most are named alike, and a list
// does not say it twice).
bool shows_other_mesh(const SceneDesc& scene, const NodeDesc& node) {
  return is_index_of(node.mesh, scene.meshes.size()) && scene.meshes[node.mesh].name != node.name;
}

// What it shows or is: its mesh, its light, its camera, its scripts.
void add_parts(std::string& line, const SceneDesc& scene, const NodeDesc& node) {
  if (shows_other_mesh(scene, node)) add(line, kMeshKey, scene.meshes[node.mesh].name);
  if (node.light) add(line, kLightKey, light_name(node.light->type));
  if (node.camera) line += std::string(" ") + kCameraWord;
  const std::string scripts = script_classes(node.props);
  if (!scripts.empty()) add(line, kScriptsKey, scripts);
}

void add_properties(std::string& line, const PropertyMap& props) {
  for (const auto& [key, value] : props) {
    if (key != props::kScript) add(line, key, property_text(value));
  }
}

// Walks the hierarchy into lines, as far as the limits let it.
class ListWriter {
 public:
  ListWriter(const SceneDesc& scene, const ListLimits& limits) : scene_(scene), limits_(limits) {}

  void write(int index, std::size_t depth) {
    if (!room_left() || too_deep(depth)) return;
    std::string indent;
    for (std::size_t level = 0; level < depth; ++level) indent += kIndent;
    text_ += indent + object_line(scene_, index) + "\n";
    ++shown_;
    for (const int child : scene_.nodes[index].children) write(child, depth + 1);
  }
  const std::string& text() const { return text_; }
  std::size_t shown() const { return shown_; }

 private:
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
    text += (text.empty() ? "" : ", ") + object_id(child) + " " + scene.nodes[child].name;
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

void add_in_saved_order(const SceneDesc& scene, int index, std::vector<int>& order) {
  order.push_back(index);
  for (const int child : scene.nodes[index].children) add_in_saved_order(scene, child, order);
}

}  // namespace

int saved_id(const SceneDesc& scene, int index) {
  std::vector<int> order;
  for (int top = 0; top < static_cast<int>(scene.nodes.size()); ++top) {
    if (scene.nodes[top].parent == kNoIndex) add_in_saved_order(scene, top, order);
  }
  const auto found = std::find(order.begin(), order.end(), index);
  return found == order.end() ? kNoIndex : static_cast<int>(found - order.begin());
}

std::string object_id(int index) { return kIdMark + std::to_string(index); }

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
  const bool needs_quotes = text.empty() || text.find(' ') != std::string::npos;
  return needs_quotes ? kQuote + text + kQuote : text;
}

std::string object_path(const SceneDesc& scene, int index) {
  std::string path = scene.nodes[index].name;
  for (int parent = scene.nodes[index].parent; parent != kNoIndex; parent = scene.nodes[parent].parent) {
    path.insert(0, scene.nodes[parent].name + kPathSeparator);
  }
  return path;
}

std::string object_line(const SceneDesc& scene, int index) {
  const NodeDesc& node = scene.nodes[index];
  std::string line = object_id(index) + " " + node.name;
  add_placement(line, node.local);
  add_parts(line, scene, node);
  add_properties(line, node.props);
  return line;
}

std::string scene_list(const SceneDesc& scene, const std::string& scene_name, const ListLimits& limits) {
  ListWriter writer(scene, limits);
  for (std::size_t index = 0; index < scene.nodes.size(); ++index) {
    if (scene.nodes[index].parent == kNoIndex) writer.write(static_cast<int>(index), 0);
  }
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
  std::string text = object_id(index) + " " + node.name + "\n" + detail_line("path", object_path(scene, index));
  if (node.parent != kNoIndex)
    text += detail_line("parent", object_id(node.parent) + " " + scene.nodes[node.parent].name);
  text += placement_detail(node.local) + parts_detail(scene, node);
  const auto scripts = get_string(node.props, props::kScript);
  if (scripts) text += detail_line("scripts", *scripts + " (" + script_classes(node.props) + ")");
  text += properties_detail(node.props);
  if (!node.children.empty()) text += detail_line("children", children_detail(scene, node));
  return text;
}

}  // namespace redgem::tools
