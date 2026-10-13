// One entry of the file at a time: a node, a material, a light or a camera as the editor writes it.

#include "editor/gltf_json.hpp"

#include <algorithm>
#include <iterator>

namespace redgem::gltf_json {
namespace {

constexpr float kUnitFactor = 1.0f;     // glTF color factors stop here
constexpr float kInfiniteRange = 0.0f;  // glTF: a light without a range reaches as far as its strength allows
constexpr float kInfiniteFar = 0.0f;    // a camera far plane of 0 means infinite

bool is_scalar(const nlohmann::json& value) { return value.is_boolean() || value.is_number() || value.is_string(); }

// TRS replaces a matrix the node may have had.
void write_transform(nlohmann::json& out, const Transform& local) {
  out.erase(kMatrix);
  out[kTranslation] = {local.position.x, local.position.y, local.position.z};
  const Quat rotation = normalize(local.rotation);
  out[kRotation] = {rotation.x, rotation.y, rotation.z, rotation.w};
  out[kScale] = {local.scale.x, local.scale.y, local.scale.z};
}

void write_light_reference(nlohmann::json& out, int light) {
  if (light != kNoIndex) {
    out[kExtensions][kLightsExtension][kLight] = light;
    return;
  }
  if (!has_object(out, kExtensions)) return;
  out[kExtensions].erase(kLightsExtension);
  if (out[kExtensions].empty()) out.erase(kExtensions);
}

// Non-scalar extras (which the editor cannot show) are kept; scalar ones mirror the properties.
void write_extras(nlohmann::json& out, const PropertyMap& props) {
  nlohmann::json extras = has_object(out, kExtras) ? out[kExtras] : nlohmann::json::object();
  for (auto it = extras.begin(); it != extras.end();) {
    it = is_scalar(*it) ? extras.erase(it) : std::next(it);
  }
  for (const auto& [key, value] : props) extras[key] = property_json(value);
  set_unless_empty(out, kExtras, extras);
}

// glTF factors stop at 1; brighter emission goes through KHR_materials_emissive_strength.
void write_emission(nlohmann::json& out, const MaterialDesc& material) {
  const float strength = emissive_strength(material);
  const Vec3 factor = material.emissive / strength;
  out[kEmissiveFactor] = {factor.x, factor.y, factor.z};
  if (needs_emissive_strength(material)) {
    out[kExtensions][kEmissiveStrengthExtension][kEmissiveStrength] = strength;
  } else if (out.contains(kExtensions)) {
    out[kExtensions].erase(kEmissiveStrengthExtension);
  }
}

// A node gets a new entry when it came from none, or when another node took the one it came from.
bool needs_new_entry(bool known, const std::set<int>& claimed, int source) {
  return !known || claimed.count(source) != 0;
}

}  // namespace

nlohmann::json property_json(const PropertyValue& value) {
  if (const auto* flag = std::get_if<bool>(&value)) return *flag;
  if (const auto* number = std::get_if<double>(&value)) return *number;
  return std::get<std::string>(value);
}

nlohmann::json node_json(const NodeDesc& node, const nlohmann::json& original) {
  nlohmann::json out = original.is_object() ? original : nlohmann::json::object();
  out[kName] = node.name;
  write_transform(out, node.local);
  set_or_erase_index(out, kMesh, node.mesh);
  set_or_erase_index(out, kCamera, node.source_camera);
  write_light_reference(out, node.source_light);
  set_unless_empty(out, kChildren, node.children);
  write_extras(out, node.props);
  return out;
}

float emissive_strength(const MaterialDesc& material) {
  return std::max({material.emissive.x, material.emissive.y, material.emissive.z, kUnitFactor});
}

bool needs_emissive_strength(const MaterialDesc& material) { return emissive_strength(material) > kUnitFactor; }

nlohmann::json material_json(const MaterialDesc& material, const nlohmann::json& original) {
  nlohmann::json out = original.is_object() ? original : nlohmann::json::object();
  out[kName] = material.name;
  const Color& c = material.base_color;
  out[kPbr][kBaseColorFactor] = {c.r, c.g, c.b, c.a};
  out[kPbr][kMetallicFactor] = material.metallic;
  out[kPbr][kRoughnessFactor] = material.roughness;
  write_emission(out, material);
  return out;
}

nlohmann::json materials_json(nlohmann::json existing, const std::vector<MaterialDesc>& materials) {
  for (std::size_t i = 0; i < materials.size(); ++i) {
    if (i < existing.size()) {
      existing[i] = material_json(materials[i], nlohmann::json(existing[i]));
    } else {
      existing.push_back(material_json(materials[i], nlohmann::json::object()));
    }
  }
  return existing;
}

const char* light_type_name(LightType type) {
  if (type == LightType::Directional) return kDirectionalLight;
  return type == LightType::Spot ? kSpotLight : kPointLight;
}

void write_light(nlohmann::json& entry, const LightDesc& light) {
  entry[kType] = light_type_name(light.type);
  entry[kColor] = {light.color.r, light.color.g, light.color.b};
  entry[kIntensity] = light.intensity;
  if (light.range > kInfiniteRange) {
    entry[kRange] = light.range;
  } else {
    entry.erase(kRange);
  }
}

void write_camera(nlohmann::json& entry, const CameraDesc& camera) {
  entry[kType] = kPerspectiveCamera;
  entry[kPerspective][kYfov] = camera.fov_y_radians;
  entry[kPerspective][kZnear] = camera.near_plane;
  if (camera.far_plane > kInfiniteFar) {
    entry[kPerspective][kZfar] = camera.far_plane;
  } else {
    entry[kPerspective].erase(kZfar);  // glTF: no zfar = infinite projection
  }
}

int claim_entry(nlohmann::json& entries, int source, std::set<int>& claimed) {
  if (!entries.is_array()) entries = nlohmann::json::array();
  const bool known = in_range(source, entries.size());
  if (needs_new_entry(known, claimed, source)) {
    entries.push_back(known ? nlohmann::json(entries[source]) : nlohmann::json::object());
    source = static_cast<int>(entries.size()) - 1;
  }
  claimed.insert(source);
  return source;
}

}  // namespace redgem::gltf_json
