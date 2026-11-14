#include "app/engine.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "app/scene_files.hpp"
#include "app/scene_loader.hpp"
#include "core/log.hpp"

namespace redgem {

bool Engine::is_loading_scene() const { return !context_.loading_scene.empty(); }
bool Engine::deterministic_scene_load() const {
  const bool offline = network_.state() == net::NetworkSession::State::Offline;
  return options_.fixed_step && !serves_a_room() && offline;
}

void Engine::advance_or_loading(float frame_seconds) {
  const FrameProfileScope phase(profiler_, ProfilePhase::Update);
  if (!is_loading_scene()) return advance_game(frame_seconds);
  context_.time.delta = 0.0f;
  const float delta =
      options_.fixed_step ? config::kFixedDeltaSeconds : std::clamp(frame_seconds, 0.0f, config::kMaxFrameSeconds);
  scripts_->draw_loading(delta);
}

void Engine::cancel_scene_load() {
  scene_load_.reset();
  drop_activation();
  context_.loading_scene.clear();
  context_.cancel_scene_load = false;
}

void Engine::request_scene(const std::string& name) {
  const bool background = context_.scene_request_async;
  cancel_scene_load();
  context_.loading_error.clear();
  if (background) return begin_scene_load(name);
  auto loaded = read_scene(options_.game_dir, name);
  if (!loaded) return;
  switch_to(name, std::move(*loaded));
  log::info("scene '" + name + "' loaded");
}

void Engine::begin_scene_load(const std::string& name) {
  context_.loading_scene = name;
  scripts_->begin_loading_screen();
  SceneLoader& loader = options_.scene_loader ? *options_.scene_loader : shared_scene_loader();
  scene_load_ = loader.submit(options_.game_dir, name, presenter_->draws_pictures());
  if (!scene_load_) return finish_scene_load({false, "scene loading queue is full", {}, {}});
  // Offline fixed-step tools wait here, then activate after exactly one loading frame.
  if (deterministic_scene_load()) scene_load_->wait_until_ready();
}

void Engine::poll_scene_load() {
  if (activation_) return advance_activation();
  if (!scene_load_) return;
  auto loaded = scene_load_->take();
  if (!loaded) return;
  for (const auto& [level, text] : loaded->messages) log::write(level, text);
  finish_scene_load(std::move(loaded->loaded));
}

// The scene read is made ready over the next frames; offline fixed-step runs make it ready in this
// one, all at once, so that it starts in the same frame every time.
void Engine::finish_scene_load(LoadResult loaded) {
  const std::string name = context_.loading_scene;
  scene_load_.reset();
  for (const auto& message : loaded.warnings) log::warning(message);
  if (!loaded.ok) return fail_scene_load(name, std::move(loaded.error));
  activation_ =
      std::make_unique<SceneActivation>(name, std::move(loaded.scene), new_physics_world(), reserve_generation());
  if (deterministic_scene_load()) complete_activation();
}

void Engine::fail_scene_load(const std::string& name, std::string error) {
  cancel_scene_load();
  context_.loading_error = error.empty() ? "scene read failed" : std::move(error);
  log::error("cannot load scene '" + name + "': " + context_.loading_error);
}

void Engine::advance_activation() {
  const FrameBudget budget(options_.activation_time, kActivationSecondsPerFrame);
  if (activation_->advance(budget, *presenter_, kActivationUploadBytesPerFrame)) complete_activation();
}

void Engine::complete_activation() {
  const std::unique_ptr<SceneActivation> incoming = std::move(activation_);
  incoming->make_all_bodies();  // (none are left, but when it is done all at once)
  cancel_scene_load();          // (nothing to drop: the presenter's readied assets are the scene's)
  switch_to(*incoming);
  log::info("scene '" + incoming->name() + "' loaded");
}

void Engine::drop_activation() {
  if (!activation_) return;
  activation_.reset();
  presenter_->drop_prepared_scene();
}

void Engine::forget_decoded_pictures() {
  if (scene_desc_.decoded_images.empty()) return;
  scene_desc_.decoded_images.clear();
  scene_desc_.decoded_images.shrink_to_fit();
}
}  // namespace redgem
