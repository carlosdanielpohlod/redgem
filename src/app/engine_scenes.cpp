// Engine: what a running game's scene is made of. Scenes that take each other's place, objects
// spawned into the one that runs and objects destroyed in it.

#include "app/engine.hpp"

#include <string>

#include "app/scene_bodies.hpp"
#include "app/scene_water.hpp"
#include "app/scene_files.hpp"
#include "core/log.hpp"
#include "physics/physics_config.hpp"

namespace redgem {

Vec3 Engine::gravity() { return {0.0f, config::kGravityY, 0.0f}; }

void Engine::make_bodies() {
  physics_ = create_jolt_physics_world(gravity(), collision_layers_);
  bodies::create_all({scene_, scene_desc_}, *physics_);
  update_scene_water(water_, scene_, scene_desc_, *physics_, 0.0f);
}

void Engine::switch_scene_if_asked() {
  if (!context_.requested_scene) return;
  const std::string name = *context_.requested_scene;
  context_.requested_scene.reset();
  std::optional<SceneDesc> scene = read_scene(options_.game_dir, name);
  if (!scene) return;
  switch_to(name, std::move(*scene));
  log::info("scene '" + name + "' loaded");
}

void Engine::switch_to(const std::string& name, SceneDesc desc) {
  // The Ruby VM survives (globals, loaded classes); the objects, bodies and behaviours do not.
  scripts_->reset_scene();
  scripts_->scene_was_reset();
  replace_world(std::move(desc));
  context_.scene_name = name;
  ++scene_generation_;
  animations_.reset(scene_desc_.animations, scene_);
  particles_.clear();
  particle_emitters_.clear();
  scripts_->attach_scripts();
  scripts_->run_scene_script(name);
  network_watch_.restart();  // the time the scene took to load is not the network's
}

void Engine::replace_world(SceneDesc desc) {
  scene_desc_ = std::move(desc);
  scene_ = Scene::from_desc(scene_desc_);
  water_.reset(scene_desc_);
  make_bodies();
  context_.physics = physics_.get();
}

void Engine::place_spawned(ObjectId id, const std::optional<Transform>& world) {
  if (!world) return;
  Transform placed = *world;
  placed.scale = scene_.world_transform(id).scale;
  scene_.set_world_transform(id, placed);
}

ObjectId Engine::spawn(ObjectId source, const std::optional<Transform>& world, bool physics) {
  if (!scene_.is_alive(source)) return kNoObject;
  const std::vector<ObjectId> created = scene_.clone(source);
  place_spawned(created.front(), world);
  if (physics) {
    for (const ObjectId id : created) bodies::create({scene_, scene_desc_}, *physics_, id);
  }
  update_scene_water(water_, scene_, scene_desc_, *physics_, 0.0f);
  for (const ObjectId id : created) scripts_->attach_object(id);
  return created.front();
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
      physics_->remove_object(*id);
      scene_.finalize_destroy(*id);
    }
  }
}

}  // namespace redgem
