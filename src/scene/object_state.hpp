#pragma once

#include <vector>
#include <nlohmann/json.hpp>
#include "scene/scene.hpp"

namespace redgem {
// A checkpoint command stays bounded even for a very large spawned hierarchy.
inline constexpr std::size_t kMaxCheckpointObjects = 4096;
// Local and resulting world components must stay within a useful simulation range.
inline constexpr float kMaxCheckpointComponent = 1.0e6f;

struct ObjectState {
  Transform local;
  Vec3 tint = Vec3::one();
  bool visible = true;
};
struct ObjectPlacement {
  ObjectId id = kNoObject;
  Transform world;
};

ObjectState capture_object_state(const GameObject& object);
nlohmann::json object_state_json(const ObjectState& state);
ObjectState read_object_state(const nlohmann::json& data);
// Validates every resulting pose before the caller writes anything to scene or physics.
std::vector<ObjectPlacement> plan_object_restore(const Scene& scene, ObjectId id, const ObjectState& state);
void apply_object_state(GameObject& object, const ObjectState& state);
}  // namespace redgem
