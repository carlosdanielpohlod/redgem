#include "project/scene_snapshot.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "project/scene_fingerprint.hpp"
#include "scene/scene_ids.hpp"

namespace redgem::tools::snapshot {
namespace {

constexpr double kFloatTolerance = 0.00001;
constexpr const char* kPropertyPrefix = "props.";

Json vector(Vec3 value) { return {value.x, value.y, value.z}; }
Json color(Color value) { return {value.r, value.g, value.b, value.a}; }

bool negative_half(Quat value) {
  if (value.w != 0.0f) return value.w < 0.0f;
  if (value.x != 0.0f) return value.x < 0.0f;
  if (value.y != 0.0f) return value.y < 0.0f;
  return value.z < 0.0f;
}

Json rotation(Quat value) {
  const float sign = negative_half(value) ? -1.0f : 1.0f;
  return {value.x * sign, value.y * sign, value.z * sign, value.w * sign};
}

Json image(const std::vector<std::string>& images, int index) {
  return is_index_of(index, images.size()) ? Json(images[index]) : Json(nullptr);
}

Json water(const WaterDesc& value) {
  return {{"enabled", value.enabled},
          {"height", value.wave_height},
          {"wavelength", value.wavelength},
          {"speed", value.speed},
          {"absorption", vector(value.absorption)},
          {"foam", value.foam_width},
          {"refraction", value.refraction}};
}

Json clearcoat(const ClearcoatDesc& value, const std::vector<std::string>& images) {
  return {{"factor", value.factor},
          {"roughness", value.roughness},
          {"normal_scale", value.normal_scale},
          {"factor_image", image(images, value.factor_image)},
          {"roughness_image", image(images, value.roughness_image)},
          {"normal_image", image(images, value.normal_image)}};
}

Json surface_parameters(const MaterialDesc& value) {
  return {{"name", value.name},
          {"color", color(value.base_color)},
          {"metallic", value.metallic},
          {"roughness", value.roughness},
          {"normal_scale", value.normal_scale},
          {"occlusion", value.occlusion_strength},
          {"emissive", vector(value.emissive)},
          {"alpha", static_cast<int>(value.alpha_mode)},
          {"cutoff", value.alpha_cutoff},
          {"double_sided", value.double_sided},
          {"dithered", value.dithered}};
}

Json texture_parameters(const MaterialDesc& value, const std::vector<std::string>& images) {
  return {{"color", image(images, value.base_color_image)},
          {"surface", image(images, value.metallic_roughness_image)},
          {"normal", image(images, value.normal_image)},
          {"occlusion", image(images, value.occlusion_image)},
          {"emissive", image(images, value.emissive_image)}};
}

Json material(const MaterialDesc& value, const std::vector<std::string>& images) {
  Json out = surface_parameters(value);
  out["images"] = texture_parameters(value, images);
  out["clearcoat"] = clearcoat(value.clearcoat, images);
  out["water"] = water(value.water);
  return out;
}

bool track_before(const Json& a, const Json& b) {
  if (a["clip"] != b["clip"]) return a["clip"] < b["clip"];
  return a["path"] < b["path"];
}

Json track(const AnimationDesc& clip, const AnimationChannel& channel) {
  return {{"clip", clip.name},
          {"duration", clip.duration},
          {"path", static_cast<int>(channel.path)},
          {"keys", channel_key(channel)}};
}

std::vector<Json> node_animations(const SceneDesc& scene) {
  std::vector<Json> nodes(scene.nodes.size(), Json::array());
  for (const AnimationDesc& clip : scene.animations) {
    for (const AnimationChannel& channel : clip.channels) {
      if (!is_index_of(channel.node, nodes.size())) continue;
      nodes[channel.node].push_back(track(clip, channel));
    }
  }
  for (Json& tracks : nodes) std::stable_sort(tracks.begin(), tracks.end(), track_before);
  return nodes;
}

Json node_ids(const SceneDesc& scene, const std::vector<int>& indices) {
  Json ids = Json::array();
  for (const int index : indices) {
    ids.push_back(is_index_of(index, scene.nodes.size()) ? Json(scene_node_id(scene, index)) : Json(nullptr));
  }
  return ids;
}

Json camera(const std::optional<CameraDesc>& value) {
  if (!value) return nullptr;
  return {{"fov", radians_to_degrees(value->fov_y_radians)}, {"near", value->near_plane}, {"far", value->far_plane}};
}

Json light(const std::optional<LightDesc>& value) {
  if (!value) return nullptr;
  return {{"type", static_cast<int>(value->type)},
          {"color", color(value->color)},
          {"intensity", value->intensity},
          {"range", value->range}};
}

bool same_value(const Json& a, const Json& b);

bool same_array(const Json& a, const Json& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (!same_value(a[i], b[i])) return false;
  }
  return true;
}

bool same_object(const Json& a, const Json& b) {
  if (a.size() != b.size()) return false;
  for (const auto& [key, value] : a.items()) {
    const auto other = b.find(key);
    if (other == b.end() || !same_value(value, *other)) return false;
  }
  return true;
}

bool same_value(const Json& a, const Json& b) {
  if (a.is_number_float() && b.is_number_float()) {
    const double first = a.get<double>(), second = b.get<double>();
    return std::fabs(first - second) <= kFloatTolerance;
  }
  if (a.type() != b.type()) return false;
  if (a.is_array()) return same_array(a, b);
  if (a.is_object()) return same_object(a, b);
  return a == b;
}

void properties(Fields& fields, const PropertyMap& props) {
  for (const auto& [key, value] : props) {
    fields[std::string(kPropertyPrefix) + key] = std::visit([](const auto& item) { return Json(item); }, value);
  }
}

}  // namespace

SceneView::SceneView(const SceneDesc& scene) : scene_(scene), animations_(node_animations(scene)) {
  for (const auto& picture : scene.images) images_.push_back(picture.file_extension + ":" + image_key(picture));
  for (const auto& mesh : scene.meshes) meshes_.push_back(mesh_key(mesh));
  for (const auto& surface : scene.materials) materials_.push_back(material(surface, images_));
  for (const auto& skin : scene.skins)
    skins_.push_back({{"joints", node_ids(scene_, skin.joints)}, {"bind", bind_key(skin)}});
}

Json SceneView::material_list(const NodeDesc& node) const {
  Json list = Json::array();
  if (!is_index_of(node.mesh, scene_.meshes.size())) return list;
  for (const auto& primitive : scene_.meshes[node.mesh].primitives) {
    list.push_back(is_index_of(primitive.material, materials_.size()) ? materials_[primitive.material] : Json(nullptr));
  }
  return list;
}

Json SceneView::skeleton(const NodeDesc& node) const {
  return is_index_of(node.skin, skins_.size()) ? skins_[node.skin] : Json(nullptr);
}

void SceneView::parts(Fields& fields, const NodeDesc& node) const {
  const bool has_mesh = is_index_of(node.mesh, scene_.meshes.size());
  fields["mesh"] = has_mesh ? Json(scene_.meshes[node.mesh].name) : Json(nullptr);
  fields["geometry"] = has_mesh ? Json(meshes_[node.mesh]) : Json(nullptr);
  fields["materials"] = material_list(node);
  fields["skin"] = skeleton(node);
  fields["camera"] = camera(node.camera);
  fields["light"] = light(node.light);
}

Fields SceneView::fields(int index) const {
  const NodeDesc& node = scene_.nodes[index];
  Fields fields{{"name", node.name},
                {"position", vector(node.local.position)},
                {"rotation", rotation(node.local.rotation)},
                {"scale", vector(node.local.scale)},
                {"children", node_ids(scene_, node.children)}};
  fields["parent"] =
      is_index_of(node.parent, scene_.nodes.size()) ? Json(scene_node_id(scene_, node.parent)) : Json(nullptr);
  fields["animations"] = animations_[index];
  properties(fields, node.props);
  parts(fields, node);
  return fields;
}

bool property_field(const std::string& field) { return field.rfind(kPropertyPrefix, 0) == 0; }

bool same_field(const std::string& field, const Json& before, const Json& after) {
  if (property_field(field)) return before == after;
  return same_value(before, after);
}

}  // namespace redgem::tools::snapshot
