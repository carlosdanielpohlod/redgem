// Engine: what a running game's scene is made of. Scenes that take each other's place, objects
// spawned into the one that runs and objects destroyed in it.

#include "app/engine.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "app/scene_bodies.hpp"
#include "app/scene_joints.hpp"
#include "app/scene_water.hpp"
#include "app/scene_files.hpp"
#include "core/log.hpp"
#include "physics/physics_config.hpp"
#include "scene/property.hpp"
#include "scene/props.hpp"

namespace redgem {

Vec3 Engine::gravity() { return {0.0f, config::kGravityY, 0.0f}; }

std::unique_ptr<PhysicsWorld> Engine::new_physics_world() const {
  std::unique_ptr<PhysicsWorld> world =
      create_jolt_physics_world(gravity(), collision_layers_, max_bodies_, physics_threads_);
  if (!config::solves_in_parallel(physics_threads_)) return world;
  return std::make_unique<OverlappedPhysicsWorld>(std::move(world));
}

void Engine::adopt_physics(std::unique_ptr<PhysicsWorld> world) {
  // (A step still under way was the old world's: it ends with it, and is never finished.)
  step_prepared_ = false;
  step_in_flight_ = false;
  physics_ = std::move(world);
  overlapped_ = dynamic_cast<OverlappedPhysicsWorld*>(physics_.get());
}

void Engine::make_bodies() {
  adopt_physics(nullptr);  // (the old world goes first, threads and all: a large world is never held twice)
  adopt_physics(new_physics_world());
  bodies::create_all({scene_, scene_desc_}, *physics_);
  settle_world();
}

void Engine::settle_world() {
  joints::create_all({scene_, scene_desc_}, *physics_, context_.scene_joints);
  find_water();
  navigation_.forget();  // the grid was the old scene's
  if (navigation_.settings().bake_on_load) bake_navigation();
  context_.physics = physics_.get();
}

void Engine::bake_navigation() {
  physics_->optimize_broad_phase();  // the bake's many rays and overlaps search it
  const NavBakeReport& report = navigation_.bake(*physics_);
  log::info("navigation: baked " + std::to_string(report.cells) + " walkable cells of " +
            std::to_string(report.columns) + " columns in " + std::to_string(report.milliseconds) + " ms");
  if (!report.cells_enlarged) return;
  log::warning("navigation: the level is too large for its cell size; cells of " + std::to_string(report.cell_size) +
               " m are used instead");
}

void Engine::switch_scene_if_asked() {
  const FrameProfileScope phase(profiler_, ProfilePhase::Scene);
  if (context_.quit_requested) {
    context_.requested_scene.reset();
    return cancel_scene_load();
  }
  if (context_.cancel_scene_load) cancel_scene_load();
  if (!context_.requested_scene) return poll_scene_load();
  const std::string name = *context_.requested_scene;
  context_.requested_scene.reset();
  request_scene(name);
}

void Engine::switch_to(const std::string& name, SceneDesc desc) {
  SceneActivation incoming(name, std::move(desc), new_physics_world(), reserve_generation());
  incoming.make_all_bodies();
  switch_to(incoming);
}

void Engine::switch_to(SceneActivation& incoming) {
  // The Ruby VM survives (globals, loaded classes); the objects, bodies and behaviours do not.
  scripts_->reset_scene();
  scripts_->scene_was_reset();
  install_world(incoming);
  context_.scene_name = incoming.name();
  scene_generation_ = incoming.generation();
  animations_.reset(scene_desc_.animations, scene_);
  head_aims_.clear();
  particles_.clear();
  hitboxes_.clear();  // (the objects are gone with the scene)
  particle_emitters_.clear();
  // Every object is in place by now: each script's first update sees the whole scene.
  scripts_->attach_scripts();
  scripts_->run_scene_script(incoming.name());
  network_watch_.restart();  // the time the scene took to load is not the network's
}

void Engine::install_world(SceneActivation& incoming) {
  runtime_prefabs_.clear();
  runtime_prefab_bytes_ = 0;
  scene_desc_ = std::move(incoming.desc());
  scene_ = std::move(incoming.scene());
  adopt_physics(incoming.take_physics());
  water_.reset(scene_desc_);
  settle_world();
}

void Engine::place_spawned(ObjectId id, const std::optional<Transform>& world) {
  if (!world) return;
  Transform placed = *world;
  placed.scale = scene_.world_transform(id).scale;
  scene_.set_world_transform(id, placed);
}

void Engine::shape_spawned(ObjectId id, const SpawnRequest& request) {
  if (!request.name.empty()) scene_.at(id).name = request.name;
  place_spawned(id, request.world);
  // (After it is placed: the place asked for is in the world, and parenting keeps it there.)
  if (scene_.is_alive(request.parent)) scene_.set_parent(id, request.parent, true);
}

ObjectId Engine::spawn(const SpawnRequest& request) {
  if (!scene_.is_alive(request.source)) return kNoObject;
  return finish_spawn(scene_.clone_pairs(request.source), request);
}

ObjectId Engine::make_object(const NodeDesc& node, const SpawnRequest& request) {
  const ObjectId id = scene_.instantiate_rigid({node}, 0, scene_desc_).front();
  finish_spawn({{kNoObject, id}}, request);
  if (get_bool(node.props, props::kHitbox).value_or(false)) hitboxes_.push_back(scene_.handle(id));
  return id;
}

void Engine::snap_hitboxes() {
  const auto gone = [this](ObjectHandle hitbox) { return scene_.resolve(hitbox) == kNoObject; };
  hitboxes_.erase(std::remove_if(hitboxes_.begin(), hitboxes_.end(), gone), hitboxes_.end());
  for (const ObjectHandle hitbox : hitboxes_) {
    const ObjectId id = scene_.resolve(hitbox);
    physics_->teleport(id, scene_.world_transform(id));
  }
}

ObjectId Engine::finish_spawn(const std::vector<std::pair<ObjectId, ObjectId>>& copies, const SpawnRequest& request) {
  shape_spawned(copies.front().second, request);
  if (request.physics) {
    for (const auto& [original, id] : copies) bodies::create({scene_, scene_desc_}, *physics_, id);
    // (Once every body of the copy is there: a joint may hold two of its parts.)
    joints::create_copies({scene_, scene_desc_}, *physics_, copies, context_.scene_joints);
  }
  update_scene_water(water_, scene_, scene_desc_, *physics_, 0.0f);
  for (const auto& [original, id] : copies) scripts_->attach_object(id);
  return copies.front().second;
}

void Engine::process_destroyed() {
  // on_destroy callbacks may destroy more objects, so repeat until nothing is pending.
  for (std::vector<ObjectId> pending = scene_.pending_destroy_ids(); !pending.empty();
       pending = scene_.pending_destroy_ids()) {
    // Every object of the batch (e.g. a parent and its children) runs on_destroy while the
    // whole batch is still alive, then the batch is torn down.
    for (const ObjectId id : pending) scripts_->on_destroyed(id);
    for (const ObjectId id : pending) scripts_->detach(id);
    // Children first, so each parent is torn down with nothing left under it.
    for (auto id = pending.rbegin(); id != pending.rend(); ++id) {
      joints::forget(context_.scene_joints, scene_.handle(*id));
      physics_->remove_object(*id);
      scene_.finalize_destroy(*id);
    }
  }
}

}  // namespace redgem
