// Engine: one frame of a running game, in the order its parts run.

#include "app/engine.hpp"

#include <algorithm>

#include "app/scene_bodies.hpp"
#include "app/touch_circles.hpp"
#include "core/config.hpp"
#include "core/exit_codes.hpp"

namespace redgem {

bool Engine::finished() const { return context_.quit_requested || !presenter_->open(); }

void Engine::frame(float frame_seconds) {
  read_input();
  context_.hud.clear();
  scripts_->start_pending();
  poll_network();
  answer_requests();
  follow_presenter();
  advance_game(frame_seconds);
  present();
  audio().update();
  ++context_.time.frame;
  switch_scene_if_asked();
}

void Engine::read_input() {
  input_source().show_touch_controls(context_.touch_controls);
  const SurfaceSize surface = presenter_->surface_size();
  input_source().fit_surface(static_cast<float>(surface.width), static_cast<float>(surface.height));
  input_.update(input_source());
}

InputSource& Engine::input_source() {
  if (options_.input != nullptr) return *options_.input;
  return scripted_input_;
}

void Engine::poll_network() {
  // On the real clock: the network does not pause with the game, nor hurry with its frames.
  const float seconds = network_watch_.lap();
  for (const net::NetworkEvent& event : network_.poll(seconds)) scripts_->dispatch(event);
  // A game hosted on this machine's own port answers the neighbours looking for one.
  if (!found_by_neighbours()) discovery_.stop_announcing();
  discovery_.set_players(static_cast<int>(network_.peers().size()) + 1);
  discovery_.update(seconds);
}

void Engine::answer_requests() {
  for (const net::HttpResult& result : http_.poll()) scripts_->dispatch(result);
}

bool Engine::found_by_neighbours() const { return network_.hosting() && network_.room().empty(); }

void Engine::follow_presenter() {
  const SurfaceSize size = presenter_->surface_size();
  context_.screen_width = size.width;
  context_.screen_height = size.height;
  presenter_->capture_cursor(context_.mouse_locked);
}

void Engine::advance_game(float frame_seconds) {
  // Time.scale slows or stops the simulation (0 pauses it); behaviours still get their update,
  // with a shorter delta, so menus work while the game is paused.
  const float scale = context_.time.scale;
  const int steps = fixed_steps_due(frame_seconds, scale);
  for (int i = 0; i < steps; ++i) fixed_step(config::kFixedDeltaSeconds);
  // A long stall (a dragged window, a loading hitch) counts as one long frame, not as seconds of game.
  const float unscaled =
      options_.fixed_step ? config::kFixedDeltaSeconds : std::min(frame_seconds, config::kMaxFrameSeconds);
  update_frame(unscaled * scale);
}

int Engine::fixed_steps_due(float frame_seconds, float scale) {
  if (!options_.fixed_step) return clock_.advance(frame_seconds * scale);
  pending_steps_ += scale;
  const int steps = static_cast<int>(pending_steps_);
  pending_steps_ -= static_cast<float>(steps);
  return steps;
}

void Engine::update_frame(float delta) {
  context_.time.delta = delta;
  // Every animation of the scene plays in a loop unless a script said otherwise (Animation);
  // scripts run after and may override poses.
  animations_.advance(delta, scene_);
  particles_.update(delta);
  scripts_->update(delta);
  process_destroyed();
}

bool Engine::both_alive(const ContactEvent& event) const {
  return scene_.is_alive(event.self) && scene_.is_alive(event.other);
}

void Engine::fixed_step(float dt) {
  context_.time.delta = dt;
  scripts_->fixed_update(dt);
  process_destroyed();
  bodies::push_poses(scene_, *physics_);
  physics_->step(dt);
  bodies::pull_poses(scene_, *physics_);
  for (const ContactEvent& event : physics_->drain_events()) {
    if (both_alive(event)) scripts_->dispatch(event);
  }
  process_destroyed();
  context_.time.elapsed += dt;
}

void Engine::present() {
  const TouchLayout* touch = input_source().touch_layout();
  controls_ = touch != nullptr ? control_circles(*touch) : std::vector<ControlCircle>();
  presenter_->present(
      {scene_, scene_desc_, scene_generation_, context_.hud, particles_, context_.motion_blur, controls_, images_dir_});
}

int Engine::run() {
  while (!finished()) frame(presenter_->frame_seconds());
  return kExitSuccess;
}

}  // namespace redgem
