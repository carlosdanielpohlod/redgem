#include "editor/gltf_json.hpp"

#include <algorithm>
#include <iterator>

namespace redgem::gltf_json {
namespace {

constexpr float kUnitFactor = 1.0f;     // glTF color factors stop here
constexpr float kInfiniteRange = 0.0f;  // glTF: a light without a range reaches as far as its strength allows
constexpr float kInfiniteFar = 0.0f;    // a camera far plane of 0 means infinite
constexpr int kFirstScene = 0;          // the scene a file loads when it does not say which
constexpr std::size_t kFloatsPerVec3 = 3;

// json[key][index], when the file has such an entry and it is an object; null otherwise.
const nlohmann::json* entry_at(const nlohmann::json& json, const char* key, const nlohmann::json& index) {
  const std::optional<int> number = index_from(index);
  if (!number || !json.is_object()) return nullptr;
  const auto list = json.find(key);
  if (list == json.end() || !list->is_array() || static_cast<std::size_t>(*number) >= list->size()) return nullptr;
  const nlohmann::json& entry = (*list)[static_cast<std::size_t>(*number)];
  return entry.is_object() ? &entry : nullptr;
}

// A whole number of zero or more as a size; nullopt for anything else. (Numbers read from a file
// and numbers the editor wrote itself are kept differently by the JSON library: both are taken.)
std::optional<std::size_t> size_from(const nlohmann::json& value) {
  if (value.is_number_unsigned()) return value.get<std::size_t>();
  if (!value.is_number_integer() || value.get<std::int64_t>() < 0) return std::nullopt;
  return value.get<std::size_t>();
}

// object[key] as a size: `missing` when the object has no such key, nullopt when what it has is
// not a whole number of zero or more (a negative offset must never become a huge one).
std::optional<std::size_t> size_at(const nlohmann::json& object, const char* key, std::optional<std::size_t> missing) {
  const auto found = object.find(key);
  if (found == object.end()) return missing;
  return size_from(*found);
}

// Whether an accessor holds `float_count` floats as vectors of three.
bool holds_vec3_floats(const nlohmann::json& accessor, std::size_t float_count) {
  const auto component = accessor.find(kComponentType);
  const auto type = accessor.find(kType);
  if (component == accessor.end() || type == accessor.end() || *component != kFloatComponent || *type != kVec3Type) {
    return false;
  }
  return float_count % kFloatsPerVec3 == 0 && size_at(accessor, kCount, std::nullopt) == float_count / kFloatsPerVec3;
}

// Whether a buffer view is in the file's only buffer, its elements one right after the other.
bool packed_in_only_buffer(const nlohmann::json& view) {
  const std::size_t packed_stride = kFloatsPerVec3 * sizeof(float);
  return size_at(view, kBuffer, std::nullopt) == static_cast<std::size_t>(kOnlyBuffer) &&
         size_at(view, kByteStride, packed_stride) == packed_stride;
}

// Where `bytes` bytes start that are `offset` into a view of `view_length` bytes at `view_offset`
// of the chunk, when the view is inside the chunk and the bytes inside the view. (Compared by
// subtraction from the larger side: sums of what a file says may wrap around.)
std::optional<std::size_t> place_in_chunk(std::size_t view_offset, std::size_t view_length, std::size_t offset,
                                          std::size_t bytes, std::size_t bin_size) {
  const bool view_inside = view_offset <= bin_size && view_length <= bin_size - view_offset;
  const bool bytes_inside = offset <= view_length && bytes <= view_length - offset;
  if (!view_inside || !bytes_inside) return std::nullopt;
  return view_offset + offset;
}

bool is_scalar(const nlohmann::json& value) { return value.is_boolean() || value.is_number() || value.is_string(); }

void set_or_erase_index(nlohmann::json& out, const char* key, int value) {
  if (value == kNoIndex) {
    out.erase(key);
  } else {
    out[key] = value;
  }
}

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
  if (!out.contains(kExtensions) || !out[kExtensions].is_object()) return;
  out[kExtensions].erase(kLightsExtension);
  if (out[kExtensions].empty()) out.erase(kExtensions);
}

// Non-scalar extras (which the editor cannot show) are kept; scalar ones mirror the properties.
void write_extras(nlohmann::json& out, const PropertyMap& props) {
  nlohmann::json extras = out.contains(kExtras) && out[kExtras].is_object() ? out[kExtras] : nlohmann::json::object();
  for (auto it = extras.begin(); it != extras.end();) {
    it = is_scalar(*it) ? extras.erase(it) : std::next(it);
  }
  for (const auto& [key, value] : props) extras[key] = property_json(value);
  set_unless_empty(out, kExtras, extras);
}

}  // namespace

std::optional<int> index_from(const nlohmann::json& value) {
  const std::optional<std::size_t> size = size_from(value);
  if (!size || *size > kMaxIndex) return std::nullopt;
  return static_cast<int>(*size);
}

std::optional<std::size_t> vec3_stream_offset(const nlohmann::json& gltf, std::size_t accessor_index,
                                              std::size_t float_count, std::size_t bin_size) {
  const nlohmann::json* accessor = entry_at(gltf, kAccessors, accessor_index);
  if (accessor == nullptr || !holds_vec3_floats(*accessor, float_count)) return std::nullopt;
  const nlohmann::json* view = entry_at(gltf, kBufferViews, accessor->value(kBufferView, nlohmann::json()));
  if (view == nullptr || !packed_in_only_buffer(*view)) return std::nullopt;
  const std::optional<std::size_t> view_offset = size_at(*view, kByteOffset, 0);
  const std::optional<std::size_t> view_length = size_at(*view, kByteLength, std::nullopt);
  const std::optional<std::size_t> offset = size_at(*accessor, kByteOffset, 0);
  if (!view_offset || !view_length || !offset) return std::nullopt;
  return place_in_chunk(*view_offset, *view_length, *offset, float_count * sizeof(float), bin_size);
}

nlohmann::json empty_document() {
  return {{kAsset, {{kVersion, kGltfVersion}, {kGenerator, kGeneratorName}}},
          {kScene, kFirstScene},
          {kScenes, nlohmann::json::array({{{kName, kDefaultSceneName}, {kNodes, nlohmann::json::array()}}})},
          {kNodes, nlohmann::json::array()}};
}

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

namespace {

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

}  // namespace

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

void add_extension_used(nlohmann::json& json, const char* extension) {
  nlohmann::json& used = json[kExtensionsUsed];
  if (!used.is_array()) used = nlohmann::json::array();
  if (std::find(used.begin(), used.end(), extension) == used.end()) used.push_back(extension);
}

void set_unless_empty(nlohmann::json& object, const char* key, const nlohmann::json& value) {
  if (value.empty()) {
    object.erase(key);
  } else {
    object[key] = value;
  }
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
  const bool known = source >= 0 && source < static_cast<int>(entries.size());
  if (!known || claimed.count(source) != 0) {
    entries.push_back(known ? nlohmann::json(entries[source]) : nlohmann::json::object());
    source = static_cast<int>(entries.size()) - 1;
  }
  claimed.insert(source);
  return source;
}

int current_index(const std::vector<int>& current, int source) {
  return source >= 0 && source < static_cast<int>(current.size()) ? current[source] : kNoIndex;
}

std::optional<nlohmann::json> skin_with_current_nodes(nlohmann::json skin, const std::vector<int>& current) {
  bool intact = true;
  for (nlohmann::json& joint : skin[kJoints]) {
    const int index = current_index(current, index_from(joint).value_or(kNoIndex));
    intact = intact && index != kNoIndex;
    joint = index;
  }
  if (!intact) return std::nullopt;
  if (!skin.contains(kSkeleton)) return skin;
  const int root = current_index(current, index_from(skin[kSkeleton]).value_or(kNoIndex));
  set_or_erase_index(skin, kSkeleton, root);
  return skin;
}

std::optional<nlohmann::json> animation_with_current_nodes(nlohmann::json animation, const std::vector<int>& current) {
  nlohmann::json channels = nlohmann::json::array();
  for (nlohmann::json channel : animation[kChannels]) {
    if (!channel.contains(kTarget) || !channel[kTarget].contains(kNode)) continue;
    const int index = current_index(current, index_from(channel[kTarget][kNode]).value_or(kNoIndex));
    if (index == kNoIndex) continue;
    channel[kTarget][kNode] = index;
    channels.push_back(std::move(channel));
  }
  if (channels.empty()) return std::nullopt;
  animation[kChannels] = channels;
  return animation;
}

bool has_several_scenes(const nlohmann::json& json) { return json.contains(kScenes) && json[kScenes].size() > 1; }

void keep_active_scene(nlohmann::json& json, const nlohmann::json& roots) {
  // (The file's own "scene" may be anything: a number that names none of its scenes, or no number.)
  const nlohmann::json* active = entry_at(json, kScenes, json.value(kScene, nlohmann::json(kFirstScene)));
  nlohmann::json scene = active != nullptr ? *active : nlohmann::json::object();
  scene[kNodes] = roots;
  json[kScenes] = nlohmann::json::array({scene});
  json[kScene] = kFirstScene;
}

}  // namespace redgem::gltf_json
