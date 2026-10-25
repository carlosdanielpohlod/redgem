#pragma once

// What can collide with what. Every body is on one of the game's collision layers (see
// physics/collision_layers.hpp) and either never moves or may move. Jolt sees each pair of those
// as an object layer of its own: things that never move do not collide with each other, and the
// rest collide when their layers meet. The broad phase keeps two trees, one for what never moves
// and one for the rest, as Jolt advises: few broad phase layers, the finer rules on object layers.

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

#include <cstddef>

#include "physics/collision_layers.hpp"

namespace redgem::jolt {

namespace layers {
// An object layer is a collision layer times two, plus one for what may move.
inline constexpr JPH::ObjectLayer kMovingBit = 1;
inline constexpr JPH::ObjectLayer kLayerShift = 1;
inline constexpr JPH::uint kCount = kMaxCollisionLayers << kLayerShift;

inline JPH::ObjectLayer object_layer(std::size_t layer, bool moving) {
  return static_cast<JPH::ObjectLayer>((layer << kLayerShift) | (moving ? kMovingBit : 0));
}
inline std::size_t collision_layer(JPH::ObjectLayer layer) { return layer >> kLayerShift; }
inline bool moves(JPH::ObjectLayer layer) { return (layer & kMovingBit) != 0; }
}  // namespace layers

namespace broad_phase_layers {
inline constexpr JPH::BroadPhaseLayer kNonMoving(0);
inline constexpr JPH::BroadPhaseLayer kMoving(1);
inline constexpr JPH::uint kCount = 2;
}  // namespace broad_phase_layers

class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
 public:
  JPH::uint GetNumBroadPhaseLayers() const override { return broad_phase_layers::kCount; }
  JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
    return layers::moves(layer) ? broad_phase_layers::kMoving : broad_phase_layers::kNonMoving;
  }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
  const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
    return layer == broad_phase_layers::kNonMoving ? "NonMoving" : "Moving";
  }
#endif
};

class ObjectVsBroadPhase final : public JPH::ObjectVsBroadPhaseLayerFilter {
 public:
  bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broad) const override {
    return layers::moves(layer) || broad == broad_phase_layers::kMoving;
  }
};

class ObjectPairs final : public JPH::ObjectLayerPairFilter {
 public:
  explicit ObjectPairs(const CollisionLayers& rules) : rules_(rules) {}
  bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
    return either_moves(a, b) && rules_.collide(layers::collision_layer(a), layers::collision_layer(b));
  }

 private:
  static bool either_moves(JPH::ObjectLayer a, JPH::ObjectLayer b) { return layers::moves(a) || layers::moves(b); }
  const CollisionLayers& rules_;
};

// What a query (a ray, a cast, an overlap) looks at: bodies on the layers of a mask.
class LayersInMask final : public JPH::ObjectLayerFilter {
 public:
  explicit LayersInMask(LayerMask mask) : mask_(mask) {}
  bool ShouldCollide(JPH::ObjectLayer layer) const override {
    return (mask_ & layer_bit(layers::collision_layer(layer))) != 0;
  }

 private:
  LayerMask mask_;
};

// What a trigger on `layer` notices: moving bodies on a layer that meets its own.
class MovingThatMeet final : public JPH::ObjectLayerFilter {
 public:
  MovingThatMeet(const CollisionLayers& rules, std::size_t layer) : rules_(rules), layer_(layer) {}
  bool ShouldCollide(JPH::ObjectLayer other) const override {
    return layers::moves(other) && rules_.collide(layer_, layers::collision_layer(other));
  }

 private:
  const CollisionLayers& rules_;
  std::size_t layer_;
};

}  // namespace redgem::jolt
