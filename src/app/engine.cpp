#include "app/engine.hpp"

#include <algorithm>
#include <string>

#include "app/save_location.hpp"
#include "app/scene_bodies.hpp"
#include "app/scene_files.hpp"
#include "app/touch_circles.hpp"
#include "core/config.hpp"
#include "core/exit_codes.hpp"
#include "core/log.hpp"
#include "physics/physics_config.hpp"
#include "scene/animation.hpp"

namespace redgem {
namespace {

// What follows the word that a game's id cannot be used.
constexpr const char* kSavedByFolderNote = ". Until it is put right, the game's save goes by where its folder is";

Vec3 gravity() { return {0.0f, config::kGravityY, 0.0f}; }

}  // namespace

Engine::Engine(const EngineOptions& options)
    : options_(options),
      images_dir_(options.game_dir / config::kImagesDirName),
      presenter_(options.presenter),
      clock_(config::kFixedDeltaSeconds, config::kMaxFrameSeconds, config::kMaxFixedStepsPerFrame) {}

Engine::~Engine() {
  // Scripts reference the scene and physics through the context: tear them down first.
  scripts_.reset();
  physics_.reset();
}

std::variant<std::unique_ptr<Engine>, std::string> Engine::create(const EngineOptions& options) {
  if (options.scene != nullptr) return create(options, SceneDesc(*options.scene));
  auto scene = read_game_scene(options.game_dir);
  if (auto* error = std::get_if<std::string>(&scene)) return *error;
  return create(options, std::move(std::get<SceneDesc>(scene)));
}

std::unique_ptr<Engine> Engine::create(const EngineOptions& options, SceneDesc scene) {
  std::unique_ptr<Engine> engine(new Engine(options));
  engine->scene_desc_ = std::move(scene);
  engine->initialize();
  return engine;
}

void Engine::initialize() {
  build_world();
  const GameManifest manifest = read_game_manifest(options_.game_dir);
  if (!manifest.id_error.empty()) log::warning(manifest.id_error + kSavedByFolderNote);
  open_presenter(manifest.settings);
  open_audio();
  open_saves();
  fill_context(manifest.settings.title);
  start_scripts();
}

void Engine::build_world() {
  scene_ = Scene::from_desc(scene_desc_);
  animations_.reset(scene_desc_.animations, scene_);
  particles_.set_gravity(gravity());
  physics_ = create_jolt_physics_world(gravity());
  bodies::create_all({scene_, scene_desc_}, *physics_);
}

void Engine::open_presenter(const GameSettings& settings) {
  if (presenter_ == nullptr) {
    // Shown by nobody, the game is as big as its settings say.
    own_presenter_ = std::make_unique<NullPresenter>(SurfaceSize{settings.window_width, settings.window_height});
    presenter_ = own_presenter_.get();
  }
  const SurfaceSize size = presenter_->surface_size();
  context_.screen_width = size.width;
  context_.screen_height = size.height;
  context_.measure_text = [this](const std::string& text, int size) { return presenter_->text_width(text, size); };
}

void Engine::open_audio() {
  if (options_.audio == nullptr) owned_audio_ = std::make_unique<NullAudio>();
  audio().set_sounds_dir(options_.game_dir / config::kSoundsDirName);
}

void Engine::open_saves() {
  save_ =
      std::make_unique<SaveStore>(options_.save_file.empty() ? save_file_for(options_.game_dir) : options_.save_file);
}

void Engine::fill_context(const std::string& game_title) {
  context_.save = save_.get();
  context_.animations = &animations_;
  context_.particles = &particles_;
  context_.network = &network_;
  context_.discovery = &discovery_;
  context_.game_title = game_title;
  context_.scene = &scene_;
  context_.audio = &audio();
  context_.physics = physics_.get();
  context_.input = &input_;
  context_.time.fixed_delta = config::kFixedDeltaSeconds;
  context_.scene_name = config::kMainSceneName;
  connect_context();
}

void Engine::connect_context() {
  context_.on_transform_written = [this](ObjectId id, TransformWrite write) {
    bodies::follow_write(scene_, *physics_, id, write);
  };
  context_.spawn = [this](ObjectId source, const std::optional<Transform>& world, bool physics) {
    return spawn(source, world, physics);
  };
  context_.scene_exists = [this](const std::string& name) {
    return safe_scene_name(name) && std::filesystem::exists(scene_file(options_.game_dir, name));
  };
}

void Engine::start_scripts() {
  scripts_ = std::make_unique<ScriptHost>(context_, options_.game_dir / config::kScriptsDirName);
  scripts_->attach_scripts();
  scripts_->run_scene_script(context_.scene_name);
}

AudioSystem& Engine::audio() { return options_.audio != nullptr ? *options_.audio : *owned_audio_; }

void Engine::switch_to(const std::string& name, SceneDesc desc) {
  // The Ruby VM survives (globals, loaded classes); the objects, bodies and behaviours do not.
  scripts_->reset_scene();
  scripts_->scene_was_reset();
  replace_world(std::move(desc));
  context_.scene_name = name;
  ++scene_generation_;
  animations_.reset(scene_desc_.animations, scene_);
  particles_.clear();
  scripts_->attach_scripts();
  scripts_->run_scene_script(name);
}

void Engine::replace_world(SceneDesc desc) {
  scene_desc_ = std::move(desc);
  scene_ = Scene::from_desc(scene_desc_);
  physics_ = create_jolt_physics_world(gravity());
  context_.physics = physics_.get();
  bodies::create_all({scene_, scene_desc_}, *physics_);
}

ObjectId Engine::spawn(ObjectId source, const std::optional<Transform>& world, bool physics) {
  if (!scene_.is_alive(source)) return kNoObject;
  const std::vector<ObjectId> created = scene_.clone(source);
  if (world) {
    Transform placed = *world;
    placed.scale = scene_.world_transform(created.front()).scale;
    scene_.set_world_transform(created.front(), placed);
  }
  if (physics) {
    for (const ObjectId id : created) bodies::create({scene_, scene_desc_}, *physics_, id);
  }
  for (const ObjectId id : created) scripts_->attach_object(id);
  return created.front();
}

bool Engine::finished() const { return context_.quit_requested || !presenter_->open(); }

void Engine::frame(float frame_seconds) {
  read_input();
  context_.hud.clear();
  scripts_->start_pending();
  poll_network(frame_seconds);
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

void Engine::poll_network(float frame_seconds) {
  // On the real clock: the network does not pause with the game.
  for (const net::NetworkEvent& event : network_.poll(frame_seconds)) scripts_->dispatch(event);
  // A game hosted on this machine's own port answers the neighbours looking for one.
  if (!network_.hosting() || !network_.room().empty()) discovery_.stop_announcing();
  discovery_.set_players(static_cast<int>(network_.peers().size()) + 1);
  discovery_.update(frame_seconds);
}

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

void Engine::switch_scene_if_asked() {
  if (!context_.requested_scene) return;
  const std::string name = *context_.requested_scene;
  context_.requested_scene.reset();
  std::optional<SceneDesc> scene = read_scene(options_.game_dir, name);
  if (!scene) return;
  switch_to(name, std::move(*scene));
  log::info("scene '" + name + "' loaded");
}

void Engine::fixed_step(float dt) {
  context_.time.delta = dt;
  scripts_->fixed_update(dt);
  process_destroyed();
  bodies::push_poses(scene_, *physics_);
  physics_->step(dt);
  bodies::pull_poses(scene_, *physics_);
  for (const ContactEvent& event : physics_->drain_events()) {
    if (scene_.is_alive(event.self) && scene_.is_alive(event.other)) scripts_->dispatch(event);
  }
  process_destroyed();
  context_.time.elapsed += dt;
}

void Engine::process_destroyed() {
  // on_destroy callbacks may destroy more objects, so repeat until nothing is pending.
  for (std::vector<ObjectId> pending = scene_.pending_destroy_ids(); !pending.empty();
       pending = scene_.pending_destroy_ids()) {
    // Every object of the batch (e.g. a parent and its children) runs on_destroy while the
    // whole batch is still alive, then the batch is torn down.
    for (const ObjectId id : pending) scripts_->on_destroyed(id);
    for (const ObjectId id : pending) scripts_->detach(id);
    for (const ObjectId id : pending) {
      physics_->remove_object(id);
      scene_.finalize_destroy(id);
    }
  }
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
