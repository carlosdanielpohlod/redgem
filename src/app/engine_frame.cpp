// Engine: one frame of a running game, in the order its parts run.

#include "app/engine.hpp"

#include <algorithm>

#include "app/scene_bodies.hpp"
#include "app/debug_labels.hpp"
#include "app/profile_overlay.hpp"
#include "app/scene_water.hpp"
#include "app/touch_circles.hpp"
#include "core/config.hpp"
#include "core/exit_codes.hpp"

namespace redgem {

bool Engine::finished() const { return context_.quit_requested || !presenter_->open(); }

void Engine::frame(float frame_seconds) {
  profiler_.begin_frame();
  settle_physics();
  prepare_frame();
  poll_network();
  answer_requests();
  follow_presenter();
  advance_or_loading(frame_seconds);
  present();
  audio().update(frame_seconds);
  ++context_.time.frame;
  keep_bindings();
  switch_scene_if_asked();
  profiler_.finish_frame();
}

void Engine::prepare_frame() {
  const FrameProfileScope phase(profiler_, ProfilePhase::Input);
  read_input();
  hand_on_capture();
  context_.hud.clear();
  debug_draw_.clear();
  scripts_->vm().set_sampling(profiler_.enabled());
  if (!is_loading_scene()) scripts_->start_pending();
}

void Engine::read_input() {
  input_source().show_touch_controls(context_.touch_controls);
  const SurfaceSize surface = presenter_->surface_size();
  input_source().fit_surface(static_cast<float>(surface.width), static_cast<float>(surface.height));
  // (The text fields of the frame before: this one draws its own.)
  input_source().request_text_input(context_.text_input);
  context_.text_input = {};
  input_.update(input_source());
  context_.keyboard_visible = input_source().soft_keyboard_shown();
}

void Engine::hand_on_capture() {
  if (const auto caught = input_.take_captured()) scripts_->dispatch(*caught);
}

InputSource& Engine::input_source() {
  if (options_.input != nullptr) return *options_.input;
  return scripted_input_;
}

void Engine::poll_network() {
  const FrameProfileScope phase(profiler_, ProfilePhase::Network);
  // On the real clock: the network does not pause with the game, nor hurry with its frames. A
  // long stall (a dragged window, a debugger) counts as one long frame, as it does for the game,
  // so that it does not run out the time the network gives its answers.
  const float lap = network_watch_.lap();
  const float seconds = std::min(lap, config::kMaxFrameSeconds);
  network_.forgive_stall(lap - seconds);  // (what the others sent while this one stood still)
  for (const net::NetworkEvent& event : network_.poll(seconds)) scripts_->dispatch(event);
  // A game hosted on this machine's own port answers the neighbours looking for one.
  if (!found_by_neighbours()) discovery_.stop_announcing();
  discovery_.set_players(static_cast<int>(network_.peers().size()) + 1);
  discovery_.update(seconds);
}

void Engine::answer_requests() {
  const FrameProfileScope phase(profiler_, ProfilePhase::Http);
  for (const net::HttpResult& result : http_.poll()) scripts_->dispatch(result);
}

bool Engine::found_by_neighbours() const { return network_.hosting() && network_.room().empty(); }

bool Engine::captures_cursor() const { return context_.mouse_locked && !is_loading_scene(); }

void Engine::read_surface_sizes() {
  const SurfaceSize size = presenter_->surface_size();
  context_.screen_width = size.width;
  context_.screen_height = size.height;
  const SurfaceSize picture = presenter_->picture_size();
  context_.picture_width = picture.width;
  context_.picture_height = picture.height;
}

void Engine::follow_presenter() {
  read_surface_sizes();
  presenter_->capture_cursor(captures_cursor());
}

void Engine::advance_game(float frame_seconds) {
  // Time.scale slows or stops the simulation (0 pauses it); behaviours still get their update,
  // with a shorter delta, so menus work while the game is paused.
  const float scale = context_.time.scale;
  run_fixed_steps(fixed_steps_due(frame_seconds, scale));
  // A long stall (a dragged window, a loading hitch) counts as one long frame, not as seconds of game.
  const float unscaled =
      options_.fixed_step ? config::kFixedDeltaSeconds : std::min(frame_seconds, config::kMaxFrameSeconds);
  context_.time.unscaled_delta = unscaled;
  update_frame(unscaled * scale);
  begin_prepared_step();  // (drawn meanwhile: see frame())
}

void Engine::run_fixed_steps(int steps) {
  const int in_place = overlaps_physics() ? std::max(steps - 1, 0) : steps;
  for (int i = 0; i < in_place; ++i) fixed_step(config::kFixedDeltaSeconds);
  if (in_place == steps) return;
  prepare_fixed_step(config::kFixedDeltaSeconds);
  step_prepared_ = true;
}

void Engine::begin_prepared_step() {
  if (!step_prepared_) return;
  overlapped_->begin_step(config::kFixedDeltaSeconds);
  step_prepared_ = false;
  step_in_flight_ = true;
}

void Engine::settle_physics() {
  if (!step_in_flight_) return;
  const FrameProfileScope phase(profiler_, ProfilePhase::Physics);
  step_in_flight_ = false;
  overlapped_->finish_step();
  // (Behaviours added since the step began start before they hear of its contacts, as they would
  // with the step taken in place, after the next frame's start.)
  if (!is_loading_scene()) scripts_->start_pending();
  finish_fixed_step(config::kFixedDeltaSeconds);
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
  if (context_.renders_particles) particle_emitters_.advance(delta, scene_, particles_);
}

bool Engine::both_alive(const ContactEvent& event) const {
  return scene_.is_alive(event.self) && scene_.is_alive(event.other);
}

void Engine::fixed_step(float dt) {
  prepare_fixed_step(dt);
  const FrameProfileScope phase(profiler_, ProfilePhase::Physics);
  physics_->step(dt);
  finish_fixed_step(dt);
}

void Engine::prepare_fixed_step(float dt) {
  const FrameProfileScope phase(profiler_, ProfilePhase::Physics);
  context_.time.delta = dt;
  scripts_->fixed_update(dt);
  process_destroyed();
  update_scene_water(water_, scene_, scene_desc_, *physics_, 0.0f);  // prime newly spawned or teleported bodies
  bodies::push_poses(scene_, *physics_);
}

void Engine::finish_fixed_step(float dt) {
  context_.time.delta = dt;  // (the contacts' callbacks read the step's)
  bodies::pull_poses(scene_, *physics_);
  update_scene_water(water_, scene_, scene_desc_, *physics_, dt);
  for (const ContactEvent& event : physics_->drain_events()) {
    if (both_alive(event)) scripts_->dispatch(event);
  }
  process_destroyed();
  context_.time.elapsed += dt;
}

void Engine::present() {
  const FrameProfileScope phase(profiler_, ProfilePhase::Presentation);
  append_debug_labels(context_, debug_draw_);
  append_profile_overlay(context_, {presenter_->gpu_timings(), &scripts_->vm().sampler().profile()});
  const TouchLayout* touch = input_source().touch_layout();
  controls_ = touch != nullptr ? control_circles(*touch) : std::vector<ControlCircle>();
  presenter_->present({scene_, scene_desc_, scene_generation_, context_.hud, particles_, context_.motion_blur,
                       controls_, images_dir_, context_.stop_recording_requested, &water_.snapshot(),
                       shows_profile_stats(context_), &debug_draw_});
}

int Engine::run() {
  while (!finished()) frame(presenter_->frame_seconds());
  return kExitSuccess;
}

}  // namespace redgem
