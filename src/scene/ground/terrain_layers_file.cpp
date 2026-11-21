#include "scene/ground/terrain_layers_file.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include "core/config.hpp"
#include "scene/property.hpp"

namespace redgem {
namespace {

using nlohmann::json;

constexpr const char* kLayersFileExtension = ".json";
constexpr const char* kSplatPictureInfix = ".splat";
constexpr const char* kSplatPictureExtension = ".png";
constexpr const char* kLayersKey = "layers";
constexpr const char* kNameKey = "name";
constexpr const char* kTintKey = "tint";
constexpr std::size_t kTintChannels = 3;
constexpr std::size_t kRed = 0;
constexpr std::size_t kGreen = 1;
constexpr std::size_t kBlue = 2;
constexpr float kMaxSharpness = 1.0f;
constexpr float kMaxVariation = 1.0f;
constexpr float kMinTiling = 0.01f;  // meters
constexpr float kMaxTiling = 10000.0f;
constexpr float kMaxScale = 100.0f;  // of a layer's roughness or normals
constexpr float kMaxVariationSize = 100000.0f;

// A finite number under `key`, clamped to `low`..`high`; `fallback` when there is none.
float number_of(const json& object, const char* key, float fallback, float low, float high) {
  const auto found = object.find(key);
  if (found == object.end() || !found->is_number()) return fallback;
  const auto value = found->get<double>();
  if (!std::isfinite(value)) return fallback;
  return std::clamp(static_cast<float>(value), low, high);
}

int whole_number_of(const json& object, const char* key, int fallback, int low, int high) {
  return static_cast<int>(std::lround(
      number_of(object, key, static_cast<float>(fallback), static_cast<float>(low), static_cast<float>(high))));
}

std::string text_of(const json& object, const char* key) {
  const auto found = object.find(key);
  return found != object.end() && found->is_string() ? found->get<std::string>() : std::string();
}

bool flag_of(const json& object, const char* key) {
  const auto found = object.find(key);
  return found != object.end() && found->is_boolean() && found->get<bool>();
}

bool is_tint_array(const json& value) {
  return value.is_array() && value.size() == kTintChannels &&
         std::all_of(value.begin(), value.end(), [](const json& channel) { return channel.is_number(); });
}

// "#rrggbb" as a picker shows it, or three linear channels.
std::optional<Vec3> tint_of(const json& layer) {
  const auto found = layer.find(kTintKey);
  if (found == layer.end()) return Vec3{1.0f, 1.0f, 1.0f};
  if (found->is_string()) return parse_hex_color(found->get<std::string>());
  if (!is_tint_array(*found)) return std::nullopt;
  const json& rgb = *found;
  return Vec3{rgb[kRed].get<float>(), rgb[kGreen].get<float>(), rgb[kBlue].get<float>()};
}

void read_pictures(const json& object, TerrainLayer& layer) {
  layer.albedo = text_of(object, "albedo");
  layer.normal = text_of(object, "normal");
  layer.roughness = text_of(object, "roughness");
  layer.occlusion = text_of(object, "occlusion");
  layer.height = text_of(object, "height");
}

void read_looks(const json& object, TerrainLayer& layer) {
  layer.roughness_scale = number_of(object, "roughness_scale", 1.0f, 0.0f, kMaxScale);
  layer.normal_scale = number_of(object, "normal_scale", 1.0f, 0.0f, kMaxScale);
  layer.tiling = number_of(object, "tiling", kDefaultLayerTiling, kMinTiling, kMaxTiling);
  layer.sharpness = number_of(object, "sharpness", kDefaultLayerSharpness, 0.0f, kMaxSharpness);
  layer.variation = number_of(object, "variation", 0.0f, 0.0f, kMaxVariation);
  layer.stochastic = flag_of(object, "stochastic");
  layer.triplanar = flag_of(object, "triplanar");
}

// One layer of the file, or what is wrong with it.
std::string read_layer(const json& object, TerrainLayer& layer) {
  if (!object.is_object()) return "a layer is not an object";
  layer.name = text_of(object, kNameKey);
  if (layer.name.empty()) return "a layer has no name";
  const auto tint = tint_of(object);
  if (!tint) return "layer '" + layer.name + "' has a tint that is neither \"#rrggbb\" nor [r, g, b]";
  layer.tint = *tint;
  read_pictures(object, layer);
  read_looks(object, layer);
  return {};
}

bool has_layer_count(const json& layers) {
  return layers.is_array() && !layers.empty() && layers.size() <= static_cast<std::size_t>(kMaxTerrainLayers);
}

std::string read_layers(const json& layers, std::vector<TerrainLayer>& into) {
  if (!has_layer_count(layers)) return "\"layers\" must list 1 to " + std::to_string(kMaxTerrainLayers) + " layers";
  for (const json& object : layers) {
    TerrainLayer layer;
    const std::string problem = read_layer(object, layer);
    if (!problem.empty()) return problem;
    into.push_back(layer);
  }
  return {};
}

std::string repeated_name(const std::vector<TerrainLayer>& layers) {
  for (std::size_t i = 0; i < layers.size(); ++i) {
    const auto same = [&](const TerrainLayer& other) { return other.name == layers[i].name; };
    if (std::count_if(layers.begin(), layers.end(), same) > 1) return layers[i].name;
  }
  return {};
}

}  // namespace

std::string terrain_layers_path(const std::string& name) {
  return std::string(config::kTerrainsDirName) + "/" + name + kLayersFileExtension;
}

std::string splat_picture_path(const std::string& name, int picture) {
  return std::string(config::kTerrainsDirName) + "/" + name + kSplatPictureInfix + std::to_string(picture) +
         kSplatPictureExtension;
}

std::string read_terrain_layers(const json& file, TerrainLayers& into) {
  if (!file.is_object()) return "the file does not hold a JSON object";
  std::vector<TerrainLayer> layers;
  const auto found = file.find(kLayersKey);
  std::string problem = read_layers(found == file.end() ? json() : *found, layers);
  if (!problem.empty()) return problem;
  const std::string repeated = repeated_name(layers);
  if (!repeated.empty()) return "two layers are called '" + repeated + "'";
  into.layers = std::move(layers);
  into.splat_size = whole_number_of(file, "splat_size", kDefaultSplatSize, kMinSplatSize, kMaxSplatSize);
  into.picture_size =
      whole_number_of(file, "picture_size", kDefaultLayerPictureSize, kMinLayerPictureSize, kMaxLayerPictureSize);
  into.variation_size = number_of(file, "variation_size", kDefaultVariationSize, kMinTiling, kMaxVariationSize);
  return {};
}

int layer_named(const TerrainLayers& layers, const std::string& name) {
  for (std::size_t i = 0; i < layers.layers.size(); ++i) {
    if (layers.layers[i].name == name) return static_cast<int>(i);
  }
  return kNoLayer;
}

}  // namespace redgem
