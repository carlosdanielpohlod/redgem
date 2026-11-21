#include "scene/ground/ground_rules.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "scene/ground/splat_map.hpp"
#include "scene/ground/terrain_layers_file.hpp"

namespace redgem {
namespace {

using nlohmann::json;

constexpr float kMinNoiseSize = 0.01f;  // meters
constexpr float kMaxNoiseSize = 100000.0f;
constexpr float kMaxSlope = 90.0f;  // degrees
constexpr int kNoiseOctaves = 3;
constexpr float kOctaveShare = 0.5f;  // of the octave before
constexpr float kOctaveScale = 2.0f;
// A hash of a lattice point and a seed (a 32-bit mix), as a share from 0 to 1.
constexpr std::uint32_t kPrimeX = 0x8da6b343U;
constexpr std::uint32_t kPrimeZ = 0xd8163841U;
constexpr std::uint32_t kPrimeSeed = 0xcb1ab31fU;
constexpr std::uint32_t kMixShift = 15;
constexpr std::uint32_t kMixFactor = 0x2c1b3c6dU;
constexpr float kLargestHash = 4294967295.0f;
constexpr float kSmoothA = 3.0f;  // of the smoothstep 3t² - 2t³
constexpr float kSmoothB = 2.0f;

// Whether a range leaves anything out (an open one needs no value to say all is in).
bool bounded(const SoftRange& range) { return std::isfinite(range.min) || std::isfinite(range.max); }

float smooth(float t) { return t * t * (kSmoothA - kSmoothB * t); }

// From 0 below `low` to 1 above `high`, smoothly; a step at `low` when they are one.
float rise(float low, float high, float value) {
  if (high <= low) return value >= low ? 1.0f : 0.0f;
  return smooth(std::clamp((value - low) / (high - low), 0.0f, 1.0f));
}

float lattice(int x, int z, std::uint32_t seed) {
  std::uint32_t hash =
      static_cast<std::uint32_t>(x) * kPrimeX ^ static_cast<std::uint32_t>(z) * kPrimeZ ^ seed * kPrimeSeed;
  hash ^= hash >> kMixShift;
  hash *= kMixFactor;
  hash ^= hash >> kMixShift;
  return static_cast<float>(hash) / kLargestHash;
}

float value_noise(float x, float z, std::uint32_t seed) {
  const float cell_x = std::floor(x);
  const float cell_z = std::floor(z);
  const int ix = static_cast<int>(cell_x);
  const int iz = static_cast<int>(cell_z);
  const float sx = smooth(x - cell_x);
  const float sz = smooth(z - cell_z);
  const float near = lattice(ix, iz, seed) + (lattice(ix + 1, iz, seed) - lattice(ix, iz, seed)) * sx;
  const float far = lattice(ix, iz + 1, seed) + (lattice(ix + 1, iz + 1, seed) - lattice(ix, iz + 1, seed)) * sx;
  return near + (far - near) * sz;
}

// --- Reading -------------------------------------------------------------------------------------

std::optional<float> finite_number(const json& object, const char* key) {
  const auto found = object.find(key);
  if (found == object.end() || !found->is_number()) return std::nullopt;
  const auto value = found->get<double>();
  if (!std::isfinite(value)) return std::nullopt;
  return static_cast<float>(value);
}

float number_or(const json& object, const char* key, float fallback, float low, float high) {
  return std::clamp(finite_number(object, key).value_or(fallback), low, high);
}

bool has_key(const json& object, const char* key) { return object.find(key) != object.end(); }

// A range under `key`: {min, max, soft}, every one optional. What is wrong with it, or "".
std::string read_range(const json& rule, const char* key, SoftRange& into) {
  if (!has_key(rule, key)) return {};
  const json& range = rule.at(key);
  if (!range.is_object()) return std::string("\"") + key + "\" must be an object of min, max and soft";
  into.min = finite_number(range, "min").value_or(-std::numeric_limits<float>::infinity());
  into.max = finite_number(range, "max").value_or(std::numeric_limits<float>::infinity());
  into.soft = std::max(finite_number(range, "soft").value_or(0.0f), 0.0f);
  if (into.min > into.max) return std::string("\"") + key + "\" has a min above its max";
  return {};
}

std::string read_ranges(const json& rule, GroundRule& into) {
  for (const auto& [key, range] :
       {std::pair{"height", &into.height}, std::pair{"slope", &into.slope}, std::pair{"noise", &into.noise}}) {
    std::string problem = read_range(rule, key, *range);
    if (!problem.empty()) return problem;
  }
  return {};
}

std::uint32_t seed_of(const json& rule) {
  const auto found = rule.find("seed");
  return found != rule.end() && found->is_number_unsigned() ? found->get<std::uint32_t>() : 0U;
}

int layer_of(const json& rule, const TerrainLayers& layers) {
  const auto found = rule.find("layer");
  return found != rule.end() && found->is_string() ? layer_named(layers, found->get<std::string>()) : kNoLayer;
}

std::string read_rule(const json& rule, const TerrainLayers& layers, GroundRule& into) {
  if (!rule.is_object()) return "a rule is not an object";
  into.layer = layer_of(rule, layers);
  if (into.layer == kNoLayer) return "a rule names no layer of the file (\"layer\": \"<name>\")";
  into.noise_size = number_or(rule, "noise_size", kDefaultNoiseSize, kMinNoiseSize, kMaxNoiseSize);
  into.strength = number_or(rule, "strength", 1.0f, 0.0f, 1.0f);
  into.seed = seed_of(rule);
  return read_ranges(rule, into);
}

// --- Painting ------------------------------------------------------------------------------------

// What a terrain's rules are painted over: its map, and the ground under each texel.
struct Canvas {
  SplatArea area;
  std::vector<GroundPoint> ground;
};

LayerWeights painted_texel(const LayerWeights& before, const std::vector<GroundRule>& rules, const GroundPoint& point,
                           GroundPlace place) {
  LayerWeights weights = before;
  for (const GroundRule& rule : rules)
    weights = toward_layer(weights, rule.layer, rule_share(rule, point, place.x, place.z));
  return weights;
}

using TexelBytes = std::array<std::uint8_t, kMaxTerrainLayers>;

TexelBytes bytes_at(const SplatMap& splat, std::size_t from) {
  TexelBytes bytes{};
  std::copy_n(splat.weights.begin() + static_cast<std::ptrdiff_t>(from), bytes.size(), bytes.begin());
  return bytes;
}

// Paints one texel; whether its bytes changed.
bool paint_texel(SplatMap& splat, const std::vector<GroundRule>& rules, const Canvas& canvas, int column, int row) {
  const std::size_t texel =
      static_cast<std::size_t>(row) * static_cast<std::size_t>(splat.size) + static_cast<std::size_t>(column);
  const GroundPoint& point = canvas.ground[texel];
  if (!point.covered) return false;
  const std::size_t from = texel * static_cast<std::size_t>(kMaxTerrainLayers);
  const TexelBytes before = bytes_at(splat, from);
  const GroundPlace place = texel_place(canvas.area, splat.size, column, row);
  set_texel(splat, column, row, painted_texel(texel_weights(splat, column, row), rules, point, place));
  return bytes_at(splat, from) != before;
}

std::size_t paint_row(SplatMap& splat, const std::vector<GroundRule>& rules, const Canvas& canvas, int row) {
  std::size_t changed = 0;
  for (int column = 0; column < splat.size; ++column) changed += paint_texel(splat, rules, canvas, column, row) ? 1 : 0;
  return changed;
}

}  // namespace

float range_share(const SoftRange& range, float value) {
  const float half = range.soft * kHalf;
  return rise(range.min - half, range.min + half, value) * (1.0f - rise(range.max - half, range.max + half, value));
}

float ground_noise(float x, float z, float size, std::uint32_t seed) {
  float total = 0.0f;
  float share = 1.0f;
  float shares = 0.0f;
  float scale = 1.0f / std::max(size, kMinNoiseSize);
  for (int octave = 0; octave < kNoiseOctaves; ++octave) {
    total += value_noise(x * scale, z * scale, seed + static_cast<std::uint32_t>(octave)) * share;
    shares += share;
    share *= kOctaveShare;
    scale *= kOctaveScale;
  }
  return total / shares;
}

float rule_share(const GroundRule& rule, const GroundPoint& point, float x, float z) {
  const float in_height = range_share(rule.height, point.height);
  const float in_slope = range_share(rule.slope, std::min(point.slope, kMaxSlope));
  const float in_noise =
      bounded(rule.noise) ? range_share(rule.noise, ground_noise(x, z, rule.noise_size, rule.seed)) : 1.0f;
  return in_height * in_slope * in_noise * rule.strength;
}

std::string read_ground_rules(const json& rules, const TerrainLayers& layers, std::vector<GroundRule>& into) {
  if (!rules.is_array()) return "\"rules\" must be a list";
  std::vector<GroundRule> read;
  for (const json& rule : rules) {
    GroundRule one;
    const std::string problem = read_rule(rule, layers, one);
    if (!problem.empty()) return problem;
    read.push_back(one);
  }
  into = std::move(read);
  return {};
}

std::size_t paint_ground(TerrainLayers& layers, const std::vector<GroundRule>& rules, const MeshDesc& mesh) {
  SplatMap& splat = layers.splat;
  if (!well_formed(splat) || rules.empty()) return 0;
  const Canvas canvas{splat_area(mesh.bounds), survey_ground(mesh, splat.size)};
  std::size_t changed = 0;
  for (int row = 0; row < splat.size; ++row) changed += paint_row(splat, rules, canvas, row);
  if (changed > 0) ++splat.version;
  return changed;
}

}  // namespace redgem
