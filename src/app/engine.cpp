#include "core/exit_codes.hpp"
#include "scene/animation.hpp"
#include "app/engine.hpp"

#include <algorithm>
#include <string>

#include "core/config.hpp"
#include "physics/physics_config.hpp"
#include "scene/props.hpp"
#include "core/log.hpp"
#include "core/safe_path.hpp"
#include "physics/body_config.hpp"
#include "app/save_location.hpp"
#include "render/gpu_profile.hpp"
#include "scene/gltf_loader.hpp"

namespace redgem {
namespace {

constexpr float kWheelRotationTolerance = 1e-3f;

Vec3 gravity() { return {0.0f, config::kGravityY, 0.0f}; }

bool simulated(BodyType type) {
  return type == BodyType::Dynamic || type == BodyType::Character || type == BodyType::Static ||
         type == BodyType::Vehicle;
}

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
  std::unique_ptr<Engine> engine(new Engine(options));
  if (auto error = engine->initialize()) return *error;
  return engine;
}

std::optional<std::string> Engine::initialize() {
  if (auto error = load_first_scene()) return error;
  build_world();
  const GameSettings settings = read_game_settings(options_.game_dir);
  open_presenter(settings);
  open_audio();
  open_saves();
  fill_context(settings.title);
  start_scripts();
  return std::nullopt;
}

std::optional<std::string> Engine::load_first_scene() {
  if (options_.scene != nullptr) {
    scene_desc_ = *options_.scene;
    return std::nullopt;
  }
  const std::filesystem::path scene_path = options_.game_dir / config::kSceneFileName;
  LoadResult loaded = load_gltf_scene(scene_path.string(), gpu_profile::kMaxSkinJoints);
  if (!loaded.ok) return loaded.error;
  for (const std::string& warning : loaded.warnings) log::warning(warning);
  scene_desc_ = std::move(loaded.scene);
  return std::nullopt;
}

void Engine::build_world() {
  scene_ = Scene::from_desc(scene_desc_);
  animations_.reset(scene_desc_.animations, scene_);
  particles_.set_gravity(gravity());
  physics_ = create_jolt_physics_world(gravity());
  create_physics_bodies();
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
  context_.on_transform_written = [this](ObjectId id, TransformWrite write) { on_transform_written(id, write); };
  context_.spawn = [this](ObjectId source, const std::optional<Transform>& world, bool physics) {
    return spawn(source, world, physics);
  };
  context_.scene_exists = [this](const std::string& name) {
    return safe_scene_name(name) && std::filesystem::exists(scene_path(name));
  };
}

void Engine::start_scripts() {
  scripts_ = std::make_unique<ScriptHost>(context_, options_.game_dir / config::kScriptsDirName);
  scripts_->attach_scripts();
  scripts_->run_scene_script(context_.scene_name);
}

AudioSystem& Engine::audio() { return options_.audio != nullptr ? *options_.audio : *owned_audio_; }

std::filesystem::path Engine::scene_path(const std::string& name) const {
  if (name == config::kMainSceneName) return options_.game_dir / config::kSceneFileName;
  return options_.game_dir / config::kScenesDirName / (name + config::kSceneExtension);
}

bool Engine::safe_scene_name(const std::string& name) {
  if (safe_asset_name(name)) return true;
  warn_of_unsafe_asset_name(name);
  return false;
}

void Engine::load_scene(const std::string& name) {
  if (!safe_scene_name(name)) return;
  LoadResult loaded = load_gltf_scene(scene_path(name).string(), gpu_profile::kMaxSkinJoints);
  if (!loaded.ok) {
    log::error("cannot load scene '" + name + "': " + loaded.error);
    return;
  }
  for (const std::string& warning : loaded.warnings) log::warning(warning);
  switch_to(name, std::move(loaded.scene));
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
  scripts_->attach_scripts();
  scripts_->run_scene_script(name);
}

void Engine::replace_world(SceneDesc desc) {
  scene_desc_ = std::move(desc);
  scene_ = Scene::from_desc(scene_desc_);
  physics_ = create_jolt_physics_world(gravity());
  context_.physics = physics_.get();
  create_physics_bodies();
}

void Engine::create_physics_bodies() {
  for (const ObjectId id : scene_.alive_ids()) create_body(id);
}

void Engine::create_body(ObjectId id) {
  if (scene_.is_prefab(id)) return;  // templates are inert
  const GameObject& object = scene_.at(id);
  BodyConfig body = body_config_from_props(object.props);
  for (const std::string& warning : body.warnings) log::warning("object '" + object.name + "': " + warning);
  if (!body.wants_physics()) return;
  const BodyDesc desc = describe_body(id, std::move(body));
  if (auto error = physics_->add_object(desc)) log::error("object '" + object.name + "': " + *error);
}

BodyDesc Engine::describe_body(ObjectId id, BodyConfig config) const {
  const GameObject& object = scene_.at(id);
  BodyDesc desc;
  desc.object = id;
  desc.config = std::move(config);
  desc.world = scene_.world_transform(id);
  if (object.mesh != kNoIndex) desc.mesh = &scene_desc_.meshes[object.mesh];
  desc.bounds = bounds_of(object);
  if (desc.config.type != BodyType::Vehicle) return desc;
  desc.wheels = collect_wheels(id);
  if (desc.wheels.empty()) log::warning("vehicle '" + object.name + "' has no children with wheel = true");
  return desc;
}

Bounds Engine::bounds_of(const GameObject& object) const {
  return object.mesh != kNoIndex ? scene_desc_.meshes[object.mesh].bounds : default_collider_bounds();
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
    for (const ObjectId id : created) create_body(id);
  }
  for (const ObjectId id : created) scripts_->attach_object(id);
  return created.front();
}

std::vector<WheelDesc> Engine::collect_wheels(ObjectId vehicle) const {
  const Transform chassis = scene_.world_transform(vehicle);
  std::vector<WheelDesc> wheels;
  for (const ObjectId child : scene_.at(vehicle).children) {
    const GameObject& object = scene_.at(child);
    if (!get_bool(object.props, props::kWheel).value_or(false)) continue;
    if (!approx_equal(object.local.rotation, Quat::identity(), kWheelRotationTolerance)) {
      log::warning("wheel '" + object.name +
                   "' has an unapplied rotation; apply it in Blender (Ctrl+A) so its "
                   "axle lies along X");
    }
    wheels.push_back(describe_wheel(child, chassis));
  }
  return wheels;
}

WheelDesc Engine::describe_wheel(ObjectId child, const Transform& chassis) const {
  const Transform world = scene_.world_transform(child);
  // Wheels are modeled with their axle along X: width on X, radius in the Y/Z plane.
  const Vec3 half = hadamard(bounds_of(scene_.at(child)).half_extents(), world.scale);
  WheelDesc wheel;
  wheel.object = child;
  wheel.center = rotate(conjugate(chassis.rotation), world.position - chassis.position);
  wheel.radius = std::max(half.y, half.z);
  wheel.width = half.x * kTwo;
  return wheel;
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
  load_scene(name);
}

void Engine::fixed_step(float dt) {
  context_.time.delta = dt;
  scripts_->fixed_update(dt);
  process_destroyed();
  push_poses_to_physics();
  physics_->step(dt);
  pull_poses_from_physics();
  for (const ContactEvent& event : physics_->drain_events()) {
    if (scene_.is_alive(event.self) && scene_.is_alive(event.other)) scripts_->dispatch(event);
  }
  process_destroyed();
  context_.time.elapsed += dt;
}

void Engine::push_poses_to_physics() {
  for (const ObjectId id : scene_.alive_ids()) {
    if (!physics_->has_object(id)) continue;
    if (physics_->body_type(id) == BodyType::Kinematic || physics_->is_trigger(id)) {
      physics_->set_target_pose(id, scene_.world_transform(id));
    }
  }
}

void Engine::pull_poses_from_physics() {
  for (const ObjectId id : scene_.alive_ids()) {
    const std::optional<Transform> pose = physics_->simulated_pose(id);
    if (pose) follow_body(id, *pose, physics_->body_type(id));
  }
}

void Engine::follow_body(ObjectId id, const Transform& pose, BodyType type) {
  if (type == BodyType::Character) {
    scene_.set_world_position(id, pose.position);
  } else {
    move_to(id, pose);
  }
  if (type != BodyType::Vehicle) return;
  // After the chassis moved, so wheels are expressed relative to its new pose.
  for (const WheelPose& wheel : physics_->wheel_poses(id)) move_to(wheel.object, wheel.world);
}

void Engine::move_to(ObjectId id, const Transform& pose) {
  Transform world = scene_.world_transform(id);
  world.position = pose.position;
  world.rotation = pose.rotation;
  scene_.set_world_transform(id, world);
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

void Engine::on_transform_written(ObjectId id, TransformWrite write) {
  // Kinematic bodies and triggers already follow the scene every step.
  if (!simulated(physics_->body_type(id))) return;
  if (write == TransformWrite::Position) {
    physics_->teleport(id, scene_.world_transform(id));
  } else {
    // Turning must not stop a moving body (e.g. a player facing where it walks while jumping).
    physics_->set_body_rotation(id, scene_.world_transform(id).rotation);
  }
}

void Engine::present() {
  const TouchLayout* touch = input_source().touch_layout();
  controls_ = touch != nullptr ? control_circles(*touch) : std::vector<ControlCircle>();
  presenter_->present(
      {scene_, scene_desc_, scene_generation_, context_.hud, particles_, context_.motion_blur, controls_, images_dir_});
}

std::vector<ControlCircle> control_circles(const TouchLayout& layout) {
  const auto button = [](const TouchCircle& at) {
    return ControlCircle{at.x, at.y, at.radius,
                         at.pressed ? ControlCircle::Look::Pressed : ControlCircle::Look::Resting};
  };
  const TouchCircle& base = layout.stick_base;
  return {{base.x, base.y, base.radius, ControlCircle::Look::Base},
          button(layout.stick_knob),
          button(layout.a),
          button(layout.b),
          button(layout.start)};
}

int Engine::run() {
  while (!finished()) frame(presenter_->frame_seconds());
  return kExitSuccess;
}

}  // namespace redgem
