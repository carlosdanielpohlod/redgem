#include "scene/prefab_overrides.hpp"

#include <array>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <utility>

#include "scene/props.hpp"

namespace redgem::prefabs {
namespace {
using Json = nlohmann::json;
constexpr std::size_t kVectorSize = 3;
constexpr std::size_t kQuatSize = 4;
constexpr const char* kLightColor = "color";
constexpr const char* kLightIntensity = "intensity";
constexpr const char* kLightRange = "range";
constexpr const char* kLightInner = "inner_cone";
constexpr const char* kLightOuter = "outer_cone";
constexpr const char* kLightType = "type";
constexpr const char* kCameraFov = "fov";
constexpr const char* kCameraNear = "near";
constexpr const char* kCameraFar = "far";
constexpr std::array<const char*, 3> kLightTypes = {"directional", "point", "spot"};

[[noreturn]] void malformed(const std::string& what) { throw std::runtime_error("prefab_overrides: " + what); }

std::vector<float> floats(const Json& value, std::size_t count, const char* name) {
  const bool numbers = value.is_array() && value.size() == count;
  if (!numbers) malformed(std::string(name) + " needs " + std::to_string(count) + " numbers");
  std::vector<float> result;
  for (const auto& item : value) {
    if (!item.is_number()) malformed(std::string(name) + " needs numbers");
    result.push_back(item.get<float>());
  }
  return result;
}
Vec3 vec3(const Json& value, const char* name) {
  const auto v = floats(value, kVectorSize, name);
  return {v[kXOffset], v[kYOffset], v[kZOffset]};
}
Quat quat(const Json& value) {
  const auto v = floats(value, kQuatSize, field::kRotation);
  return {v[kXOffset], v[kYOffset], v[kZOffset], v[kWOffset]};
}
float number(const Json& entry, const char* key, float fallback) {
  const auto found = entry.find(key);
  if (found == entry.end()) return fallback;
  if (!found->is_number()) malformed(std::string(key) + " needs a number");
  return found->get<float>();
}

PropertyValue property(const Json& value) {
  if (value.is_boolean()) return value.get<bool>();
  if (value.is_number()) return value.get<double>();
  if (value.is_string()) return value.get<std::string>();
  malformed("a property must be a number, text or a toggle");
}
Json property_json(const PropertyValue& value) {
  return std::visit([](const auto& held) { return Json(held); }, value);
}

LightType light_type(const Json& entry) {
  const auto name = entry.value(kLightType, std::string(kLightTypes.front()));
  for (std::size_t i = 0; i < kLightTypes.size(); ++i) {
    if (name == kLightTypes[i]) return static_cast<LightType>(i);
  }
  malformed("unknown light type " + name);
}
LightDesc light(const Json& entry) {
  if (!entry.is_object()) malformed("light needs an object");
  LightDesc result;
  result.type = light_type(entry);
  const Vec3 color = entry.contains(kLightColor) ? vec3(entry[kLightColor], kLightColor) : Vec3::one();
  result.color = {color.x, color.y, color.z, 1.0f};
  result.intensity = number(entry, kLightIntensity, result.intensity);
  result.range = number(entry, kLightRange, result.range);
  result.inner_cone = number(entry, kLightInner, result.inner_cone);
  result.outer_cone = number(entry, kLightOuter, result.outer_cone);
  return result;
}
Json light_json(const LightDesc& value) {
  return {{kLightType, kLightTypes.at(static_cast<std::size_t>(value.type))},
          {kLightColor, {value.color.r, value.color.g, value.color.b}},
          {kLightIntensity, value.intensity},
          {kLightRange, value.range},
          {kLightInner, value.inner_cone},
          {kLightOuter, value.outer_cone}};
}
CameraDesc camera(const Json& entry) {
  if (!entry.is_object()) malformed("camera needs an object");
  return {number(entry, kCameraFov, 0.0f), number(entry, kCameraNear, 0.0f), number(entry, kCameraFar, 0.0f)};
}
Json camera_json(const CameraDesc& value) {
  return {{kCameraFov, value.fov_y_radians}, {kCameraNear, value.near_plane}, {kCameraFar, value.far_plane}};
}

void read_props(const Json& entry, PartOverride& part) {
  if (!entry.is_object()) malformed("props needs an object");
  for (const auto& [key, value] : entry.items()) part.props[key] = property(value);
}
void read_unset(const Json& entry, PartOverride& part) {
  if (!entry.is_array()) malformed("unset needs a list of names");
  for (const auto& name : entry) {
    if (!name.is_string()) malformed("unset needs a list of names");
    part.unset.insert(name.get<std::string>());
  }
}
// A transform field; false when `key` names none.
bool read_transform_field(const std::string& key, const Json& value, PartOverride& part) {
  if (key == field::kPosition) part.position = vec3(value, field::kPosition);
  if (key == field::kRotation) part.rotation = quat(value);
  if (key == field::kScale) part.scale = vec3(value, field::kScale);
  return key == field::kPosition || key == field::kRotation || key == field::kScale;
}
bool read_component_field(const std::string& key, const Json& value, PartOverride& part) {
  if (key == field::kLight) part.light = light(value);
  if (key == field::kCamera) part.camera = camera(value);
  return key == field::kLight || key == field::kCamera;
}
void read_field(const std::string& key, const Json& value, PartOverride& part) {
  if (read_transform_field(key, value, part) || read_component_field(key, value, part)) return;
  if (key == field::kProps) return read_props(value, part);
  if (key == field::kUnset) return read_unset(value, part);
  if (key == field::kRemoved && value.is_boolean()) return void(part.removed = value.get<bool>());
  malformed("unknown field " + key);
}
PartOverride read_part(const Json& entry) {
  if (!entry.is_object()) malformed("a part needs an object");
  PartOverride part;
  for (const auto& [key, value] : entry.items()) read_field(key, value, part);
  return part;
}

Json vector_json(Vec3 value) { return {value.x, value.y, value.z}; }
void write_transform(Json& out, const PartOverride& part) {
  if (part.position) out[field::kPosition] = vector_json(*part.position);
  if (part.rotation) out[field::kRotation] = {part.rotation->x, part.rotation->y, part.rotation->z, part.rotation->w};
  if (part.scale) out[field::kScale] = vector_json(*part.scale);
}
Json part_json(const PartOverride& part) {
  Json out = Json::object();
  write_transform(out, part);
  for (const auto& [key, value] : part.props) out[field::kProps][key] = property_json(value);
  if (!part.unset.empty()) out[field::kUnset] = part.unset;
  if (part.light) out[field::kLight] = light_json(*part.light);
  if (part.camera) out[field::kCamera] = camera_json(*part.camera);
  if (part.removed) out[field::kRemoved] = true;
  return out;
}

void apply_props(PropertyMap& target, const PartOverride& change) {
  for (const auto& name : change.unset) target.erase(name);
  for (const auto& [key, value] : change.props) target.insert_or_assign(key, value);
}
void apply_components(NodeDesc& part, const PartOverride& change) {
  if (change.light && part.light) part.light = change.light;
  if (change.camera && part.camera) part.camera = change.camera;
}

bool same_light(const std::optional<LightDesc>& a, const std::optional<LightDesc>& b) {
  if (a.has_value() != b.has_value()) return false;
  return !a || light_json(*a) == light_json(*b);
}
bool same_camera(const std::optional<CameraDesc>& a, const std::optional<CameraDesc>& b) {
  if (a.has_value() != b.has_value()) return false;
  return !a || camera_json(*a) == camera_json(*b);
}
Json transform_json(const PartOverride& part) {
  Json out = Json::object();
  write_transform(out, part);
  return out;
}
bool same_transform(const PartOverride& a, const PartOverride& b) { return transform_json(a) == transform_json(b); }

bool revert_prop(PartOverride& part, const std::string& key) {
  const bool had = part.props.erase(key) + part.unset.erase(key) > 0;
  return had;
}
template <typename T>
bool drop(std::optional<T>& value) {
  const bool had = value.has_value();
  value.reset();
  return had;
}
bool revert_transform(PartOverride& part, const std::string& name) {
  if (name == field::kPosition) return drop(part.position);
  if (name == field::kRotation) return drop(part.rotation);
  return name == field::kScale && drop(part.scale);
}
bool revert_named(PartOverride& part, const std::string& name) {
  const std::string prefix = field::kPropPrefix;
  if (name.rfind(prefix, 0) == 0) return revert_prop(part, name.substr(prefix.size()));
  if (name == field::kLight) return drop(part.light);
  if (name == field::kCamera) return drop(part.camera);
  if (name == field::kRemoved) return std::exchange(part.removed, false);
  return revert_transform(part, name);
}
}  // namespace

bool PartOverride::empty() const {
  const bool no_transform = !position && !rotation && !scale;
  const bool no_components = !light && !camera;
  return no_transform && no_components && props.empty() && unset.empty() && !removed;
}
bool PartOverride::operator==(const PartOverride& other) const {
  const bool same_props = props == other.props && unset == other.unset && removed == other.removed;
  return same_props && same_transform(*this, other) && same_light(light, other.light) &&
         same_camera(camera, other.camera);
}

Overrides parse_overrides(const std::string& text) {
  const Json json = Json::parse(text, nullptr, false);
  if (!json.is_object()) malformed("needs a JSON object");
  Overrides result;
  for (const auto& [path, entry] : json.items()) {
    if (path.empty()) malformed("a part path cannot be empty");
    auto part = read_part(entry);
    if (!part.empty()) result.emplace(path, std::move(part));
  }
  return result;
}
std::string overrides_text(const Overrides& overrides) {
  Json out = Json::object();
  for (const auto& [path, part] : overrides) out[path] = part_json(part);
  return out.dump();
}
Overrides read_overrides(const PropertyMap& placement) {
  const auto text = get_string(placement, props::kPrefabOverrides);
  if (!text) {
    if (placement.count(props::kPrefabOverrides) != 0) malformed("needs text");
    return {};
  }
  return parse_overrides(*text);
}
void write_overrides(PropertyMap& placement, const Overrides& overrides) {
  if (overrides.empty()) {
    placement.erase(props::kPrefabOverrides);
    return;
  }
  placement[props::kPrefabOverrides] = overrides_text(overrides);
}

void apply_override(NodeDesc& part, const PartOverride& change) {
  if (change.position) part.local.position = *change.position;
  if (change.rotation) part.local.rotation = *change.rotation;
  if (change.scale) part.local.scale = *change.scale;
  apply_props(part.props, change);
  apply_components(part, change);
}

std::vector<std::string> overridden_fields(const PartOverride& change) {
  std::vector<std::string> names;
  const auto add_if = [&names](bool present, const char* name) {
    if (present) names.emplace_back(name);
  };
  add_if(change.position.has_value(), field::kPosition);
  add_if(change.rotation.has_value(), field::kRotation);
  add_if(change.scale.has_value(), field::kScale);
  for (const auto& entry : change.props) names.push_back(field::kPropPrefix + entry.first);
  for (const auto& name : change.unset) names.push_back(field::kPropPrefix + name);
  add_if(change.light.has_value(), field::kLight);
  add_if(change.camera.has_value(), field::kCamera);
  add_if(change.removed, field::kRemoved);
  return names;
}
bool revert_field(Overrides& overrides, const std::string& path, const std::string& name) {
  const auto found = overrides.find(path);
  if (found == overrides.end()) return false;
  if (name.empty()) {
    overrides.erase(found);
    return true;
  }
  const bool had = revert_named(found->second, name);
  if (found->second.empty()) overrides.erase(found);
  return had;
}

std::string child_path(const std::string& parent, const std::string& identity) {
  return parent.empty() ? identity : parent + kPathSeparator + identity;
}

}  // namespace redgem::prefabs
