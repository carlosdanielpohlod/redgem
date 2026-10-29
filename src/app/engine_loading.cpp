#include "app/engine.hpp"

#include <algorithm>

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
  scene_load_ = loader.submit(options_.game_dir, name);
  if (!scene_load_) return finish_scene_load({false, "scene loading queue is full", {}, {}});
  // Offline fixed-step tools wait here, then activate after exactly one loading frame.
  if (deterministic_scene_load()) scene_load_->wait_until_ready();
}

void Engine::poll_scene_load() {
  if (!scene_load_) return;
  auto loaded = scene_load_->take();
  if (!loaded) return;
  for (const auto& [level, text] : loaded->messages) log::write(level, text);
  finish_scene_load(std::move(loaded->loaded));
}

void Engine::finish_scene_load(LoadResult loaded) {
  const std::string name = context_.loading_scene;
  cancel_scene_load();
  for (const auto& message : loaded.warnings) log::warning(message);
  if (!loaded.ok) {
    context_.loading_error = loaded.error.empty() ? "scene read failed" : std::move(loaded.error);
    log::error("cannot load scene '" + name + "': " + context_.loading_error);
    return;
  }
  switch_to(name, std::move(loaded.scene));
  log::info("scene '" + name + "' loaded");
}
}  // namespace redgem
