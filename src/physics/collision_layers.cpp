#include "physics/collision_layers.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

namespace redgem {
namespace {

// What game.json's "physics" section says.
constexpr const char* kLayersKey = "layers";
constexpr const char* kCollidesKey = "collides";
constexpr const char* kSection = "game.json physics: ";
constexpr const char* kNameSeparator = ", ";

bool listed_text(const nlohmann::json& entry) { return entry.is_string() && !entry.get<std::string>().empty(); }

void add_layer(CollisionLayers& layers, const nlohmann::json& entry, std::vector<std::string>& warnings) {
  if (!listed_text(entry)) {
    warnings.push_back(std::string(kSection) + "a layer must be a name, not " + entry.dump());
    return;
  }
  const std::string name = entry.get<std::string>();
  if (layers.find(name)) return;  // "default" said again, or a layer twice: it is there already
  if (!layers.add(name)) {
    warnings.push_back(std::string(kSection) + "at most " + std::to_string(kMaxCollisionLayers) +
                       " layers; '" + name + "' is left out");
  }
}

void read_layers(const nlohmann::json& physics, CollisionLayersReading& reading) {
  if (!physics.contains(kLayersKey)) return;
  const nlohmann::json& listed = physics[kLayersKey];
  if (!listed.is_array()) {
    reading.warnings.push_back(std::string(kSection) + "\"layers\" must be a list of names");
    return;
  }
  for (const nlohmann::json& entry : listed) add_layer(reading.layers, entry, reading.warnings);
}

// The layer a name in "collides" stands for; a warning when it stands for none.
std::optional<std::size_t> named_layer(const CollisionLayersReading& reading, const nlohmann::json& name,
                                       std::vector<std::string>& warnings) {
  const std::optional<std::size_t> layer =
      name.is_string() ? reading.layers.find(name.get<std::string>()) : std::nullopt;
  if (!layer) {
    warnings.push_back(std::string(kSection) + "\"collides\" names " + name.dump() +
                       ", which is no layer (layers: " + reading.layers.names_listed() + ")");
  }
  return layer;
}

LayerMask mask_of(CollisionLayersReading& reading, const nlohmann::json& names) {
  LayerMask mask = 0;
  if (!names.is_array()) {
    reading.warnings.push_back(std::string(kSection) + "what a layer collides with must be a list of layers");
    return mask;
  }
  for (const nlohmann::json& name : names) {
    if (const auto layer = named_layer(reading, name, reading.warnings)) mask |= layer_bit(*layer);
  }
  return mask;
}

void read_collides(const nlohmann::json& physics, CollisionLayersReading& reading) {
  if (!physics.contains(kCollidesKey)) return;
  const nlohmann::json& rules = physics[kCollidesKey];
  if (!rules.is_object()) {
    reading.warnings.push_back(std::string(kSection) + "\"collides\" must map layers to lists of layers");
    return;
  }
  for (const auto& [name, others] : rules.items()) {
    const std::optional<std::size_t> layer = named_layer(reading, name, reading.warnings);
    if (layer) reading.layers.let_in(*layer, mask_of(reading, others));
  }
}

}  // namespace

CollisionLayers::CollisionLayers() : names_{kDefaultLayerName} { lets_in_.fill(kAllLayers); }

std::optional<std::size_t> CollisionLayers::find(const std::string& name) const {
  const auto it = std::find(names_.begin(), names_.end(), name);
  if (it == names_.end()) return std::nullopt;
  return static_cast<std::size_t>(it - names_.begin());
}

bool CollisionLayers::collide(std::size_t a, std::size_t b) const {
  return (lets_in_.at(a) & layer_bit(b)) != 0 && (lets_in_.at(b) & layer_bit(a)) != 0;
}

std::string CollisionLayers::names_listed() const {
  std::string listed;
  for (const std::string& name : names_) listed += (listed.empty() ? "" : kNameSeparator) + name;
  return listed;
}

bool CollisionLayers::add(const std::string& name) {
  if (names_.size() >= kMaxCollisionLayers || find(name)) return false;
  names_.push_back(name);
  return true;
}

CollisionLayersReading collision_layers_from_json(const nlohmann::json& physics) {
  CollisionLayersReading reading;
  if (physics.is_null()) return reading;
  if (!physics.is_object()) {
    reading.warnings.push_back(std::string(kSection) + "the section must be an object");
    return reading;
  }
  read_layers(physics, reading);
  read_collides(physics, reading);
  return reading;
}

}  // namespace redgem
