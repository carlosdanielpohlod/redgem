#include "scene/object_state.hpp"
#include <cmath>
#include <stdexcept>

namespace redgem {
namespace {
using Json = nlohmann::json;
constexpr std::size_t kVectorParts = 3;
constexpr std::size_t kQuaternionParts = 4;
enum Component : std::size_t { kVectorX, kVectorY, kVectorZ, kTurnW };
constexpr std::size_t kStateFields = 5;

void require_state(bool valid, const char* reason) {
  if (!valid) throw std::invalid_argument(reason);
}
bool finite_component(float value) { return std::isfinite(value) && std::fabs(value) <= kMaxCheckpointComponent; }
bool finite_vector(Vec3 value) {
  return finite_component(value.x) && finite_component(value.y) && finite_component(value.z);
}
float number(const Json& value) {
  require_state(value.is_number(), "checkpoint components must be numbers");
  const auto result = value.get<double>();
  const bool valid = std::isfinite(result) && std::fabs(result) <= kMaxCheckpointComponent;
  require_state(valid, "checkpoint component is outside the simulation range");
  return static_cast<float>(result);
}
void require_parts(const Json& data, std::size_t count) {
  require_state(data.is_array() && data.size() == count, "wrong number of checkpoint vector components");
}
Vec3 vector(const Json& data) {
  require_parts(data, kVectorParts);
  return {number(data[kVectorX]), number(data[kVectorY]), number(data[kVectorZ])};
}
Quat rotation(const Json& data) {
  require_parts(data, kQuaternionParts);
  const Quat value{number(data[kVectorX]), number(data[kVectorY]), number(data[kVectorZ]), number(data[kTurnW])};
  const double norm =
      double(value.x) * value.x + double(value.y) * value.y + double(value.z) * value.z + double(value.w) * value.w;
  require_state(norm > double(kEpsilon) * kEpsilon, "checkpoint rotation must not be zero");
  const auto inverse = static_cast<float>(1.0 / std::sqrt(norm));
  return {value.x * inverse, value.y * inverse, value.z * inverse, value.w * inverse};
}
Json vector_json(Vec3 value) { return Json::array({value.x, value.y, value.z}); }
void require_pose(const Transform& value) {
  const bool valid = finite_vector(value.position) && finite_vector(value.scale);
  require_state(valid, "checkpoint produces a pose outside the simulation range");
  (void)rotation(Json::array({value.rotation.x, value.rotation.y, value.rotation.z, value.rotation.w}));
}
void add_placement(std::vector<ObjectPlacement>& out, ObjectId id, const Transform& world) {
  require_state(out.size() < kMaxCheckpointObjects, "checkpoint hierarchy is too large");
  require_pose(world);
  out.push_back({id, world});
}
void require_live(const Scene& scene, ObjectId id) {
  require_state(scene.is_alive(id), "checkpoint target is no longer alive");
  require_state(!scene.at(id).pending_destroy, "checkpoint target is pending destruction");
}
void add_children(const Scene& scene, const ObjectPlacement& parent, std::vector<ObjectPlacement>& out) {
  for (const auto child : scene.at(parent.id).children) {
    if (!scene.is_alive(child)) continue;
    require_live(scene, child);
    require_state(scene.at(child).parent == parent.id, "checkpoint hierarchy is inconsistent");
    add_placement(out, child, combine(parent.world, scene.at(child).local));
  }
}
}  // namespace

ObjectState capture_object_state(const GameObject& object) {
  const ObjectState state{object.local, object.tint, object.visible};
  return read_object_state(object_state_json(state));
}
Json object_state_json(const ObjectState& state) {
  const auto& turn = state.local.rotation;
  return {{"local_position", vector_json(state.local.position)},
          {"local_scale", vector_json(state.local.scale)},
          {"local_rotation", Json::array({turn.x, turn.y, turn.z, turn.w})},
          {"tint", vector_json(state.tint)},
          {"visible", state.visible}};
}
ObjectState read_object_state(const Json& data) {
  require_state(data.is_object() && data.size() >= kStateFields, "checkpoint must be an object state");
  ObjectState result;
  result.local.position = vector(data.at("local_position"));
  result.local.scale = vector(data.at("local_scale"));
  result.local.rotation = rotation(data.at("local_rotation"));
  result.tint = vector(data.at("tint"));
  require_state(data.at("visible").is_boolean(), "checkpoint visibility must be a boolean");
  result.visible = data.at("visible").get<bool>();
  return result;
}
std::vector<ObjectPlacement> plan_object_restore(const Scene& scene, ObjectId id, const ObjectState& state) {
  require_live(scene, id);
  const auto parent = scene.at(id).parent;
  const auto world = parent == kNoObject ? state.local : combine(scene.world_transform(parent), state.local);
  std::vector<ObjectPlacement> result;
  add_placement(result, id, world);
  for (std::size_t index = 0; index < result.size(); ++index) {
    const auto placement = result[index];  // appending children may reallocate the plan
    add_children(scene, placement, result);
  }
  return result;
}
void apply_object_state(GameObject& object, const ObjectState& state) {
  object.local = state.local;
  object.tint = state.tint;
  object.visible = state.visible;
}
}  // namespace redgem
