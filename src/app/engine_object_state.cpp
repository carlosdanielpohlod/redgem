#include "app/engine.hpp"
#include <stdexcept>
#include "scene/object_state.hpp"

namespace redgem {
namespace {
using Json = nlohmann::json;
constexpr int kCheckpointSchema = 1;
constexpr std::size_t kCheckpointFields = 12;

ObjectId checkpoint_target(const Scene& scene, ObjectHandle handle) {
  const auto id = scene.resolve(handle);
  if (id == kNoObject) throw std::runtime_error("checkpoint target is no longer alive");
  if (scene.at(id).pending_destroy) throw std::runtime_error("checkpoint target is pending destruction");
  return id;
}
std::string source_identity(const Scene& scene, ObjectId id) {
  if (id == kNoObject) return {};
  const auto& object = scene.at(id);
  const auto source = object.copy_of == kNoObject ? id : object.copy_of;
  const auto identity = scene.at(source).stable_id;
  if (identity.empty()) throw std::invalid_argument("checkpoint source has no scene-file identity");
  return identity;
}
Json checkpoint_header(const Scene& scene, const PhysicsWorld& physics, const std::string& name, ObjectId id) {
  const auto& object = scene.at(id);
  return {{"schema", kCheckpointSchema},
          {"scene", name},
          {"source", source_identity(scene, id)},
          {"copy", object.copy_of != kNoObject},
          {"parent", source_identity(scene, object.parent)},
          {"body", body_type_name(physics.body_type(id))},
          {"trigger", physics.is_trigger(id)}};
}
void require_header(const Json& data, const Json& expected) {
  const bool shaped = data.is_object() && data.size() == kCheckpointFields;
  if (!shaped) throw std::invalid_argument("invalid object checkpoint fields");
  if (!data.at("schema").is_number_integer()) throw std::invalid_argument("invalid object checkpoint schema");
  for (auto entry = expected.begin(); entry != expected.end(); ++entry) {
    if (data.at(entry.key()) != entry.value())
      throw std::invalid_argument("checkpoint does not match this scene, source, parent or physics body");
  }
}
bool same_scale(Vec3 left, Vec3 right) { return left.x == right.x && left.y == right.y && left.z == right.z; }
void require_collider_scales(const GameObject& target, const ObjectState& state, const PhysicsWorld& physics,
                             const std::vector<ObjectPlacement>& placements) {
  if (same_scale(target.local.scale, state.local.scale)) return;
  for (const auto& placement : placements) {
    if (physics.has_object(placement.id)) throw std::invalid_argument("checkpoint cannot rescale an existing collider");
  }
}
void stop_checkpoint_kinematic(PhysicsWorld& physics, ObjectId id) {
  if (physics.body_type(id) != BodyType::Kinematic) return;
  physics.set_velocity(id, Vec3::zero());
  physics.set_angular_velocity(id, Vec3::zero());
}
}  // namespace

nlohmann::json Engine::capture_object_checkpoint(ObjectHandle handle) {
  const auto id = checkpoint_target(scene_, handle);
  auto result = object_state_json(capture_object_state(scene_.at(id)));
  result.update(checkpoint_header(scene_, *physics_, context_.scene_name, id));
  return result;
}
void Engine::restore_object_checkpoint(ObjectHandle handle, const nlohmann::json& data) {
  const auto id = checkpoint_target(scene_, handle);
  require_header(data, checkpoint_header(scene_, *physics_, context_.scene_name, id));
  const auto state = read_object_state(data);
  const auto placements = plan_object_restore(scene_, id, state);
  require_collider_scales(scene_.at(id), state, *physics_, placements);
  apply_object_state(scene_.at(id), state);
  for (const auto& placement : placements) {
    physics_->teleport(placement.id, placement.world);
    stop_checkpoint_kinematic(*physics_, placement.id);
    water_.forget(scene_.handle(placement.id));
  }
}
void Engine::connect_object_checkpoints() {
  context_.capture_object_state = [this](ObjectHandle object) { return capture_object_checkpoint(object); };
  context_.restore_object_state = [this](ObjectHandle object, const nlohmann::json& data) {
    restore_object_checkpoint(object, data);
  };
}
}  // namespace redgem
