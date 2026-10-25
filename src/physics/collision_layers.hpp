#pragma once

// Which bodies collide with which: every body is on one named layer, and game.json says which
// layers meet. Its "physics" section:
//   "physics": {
//     "layers": ["default", "player", "enemy", "bullet", "pickup"],
//     "collides": {"bullet": ["enemy", "default"]}
//   }
// "default" is always the first layer (it is added when the list leaves it out) and is where an
// object without a `layer` property goes. A layer "collides" says nothing of meets every layer;
// two layers meet only when each of them lets the other in, so the bullet above passes through
// players and pickups, and they through it.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace redgem {

inline constexpr std::size_t kMaxCollisionLayers = 16;
inline constexpr const char* kDefaultLayerName = "default";
inline constexpr std::size_t kDefaultLayer = 0;  // where kDefaultLayerName always is

// A set of layers, one bit each (bit i for layer i).
using LayerMask = std::uint32_t;
inline constexpr LayerMask kAllLayers = ~LayerMask{0};
inline LayerMask layer_bit(std::size_t layer) { return LayerMask{1} << layer; }

class CollisionLayers {
 public:
  // Just "default", which collides with itself.
  CollisionLayers();

  std::size_t count() const { return names_.size(); }
  const std::string& name(std::size_t layer) const { return names_.at(layer); }
  std::optional<std::size_t> find(const std::string& name) const;
  // Whether bodies on these two layers collide (the same answer either way round).
  bool collide(std::size_t a, std::size_t b) const;
  // The layers' names, comma separated, for messages.
  std::string names_listed() const;

  // Building one (collision_layers_from_json does). A layer past kMaxCollisionLayers, or one
  // already there, is not added: false.
  bool add(const std::string& name);
  // What a layer lets in; every layer until set.
  void let_in(std::size_t layer, LayerMask others) { lets_in_.at(layer) = others; }

 private:
  std::vector<std::string> names_;
  std::array<LayerMask, kMaxCollisionLayers> lets_in_;
};

// game.json's "physics" section, and what is wrong with it (each problem is left out, the rest
// is used).
struct CollisionLayersReading {
  CollisionLayers layers;
  std::vector<std::string> warnings;
};
CollisionLayersReading collision_layers_from_json(const nlohmann::json& physics);

}  // namespace redgem
