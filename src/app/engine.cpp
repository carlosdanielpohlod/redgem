#include "scene/animation.hpp"
#include "app/engine.hpp"

#include <mruby.h>
#include <mruby/string.h>
#include <mruby/variable.h>

#include <algorithm>
#include <string>

#include "core/config.hpp"
#include "core/log.hpp"
#include "input/raylib_input_source.hpp"
#include "physics/body_config.hpp"
#include "app/game_project.hpp"
#include "render/renderer.hpp"
#include "scene/gltf_loader.hpp"

namespace redgem {
namespace {

constexpr int kExitSuccess = 0;
constexpr int kExitFailure = 1;
constexpr const char* kSystemTestGlobal = "$__redgem_system_test";
constexpr float kWheelRotationTolerance = 1e-3f;

Vec3 gravity() { return {0.0f, config::kGravityY, 0.0f}; }

Bounds default_bounds() {
  const Vec3 half = Vec3::one() * config::kDefaultColliderHalfExtent;
  Bounds bounds;
  bounds.include(-half);
  bounds.include(half);
  return bounds;
}

}  // namespace

Engine::Engine(const EngineOptions& options)
    : options_(options),
      deterministic_(options.test_script.has_value() || (options.headless && options.input == nullptr)),
      clock_(config::kFixedDeltaSeconds, config::kMaxFrameSeconds, config::kMaxFixedStepsPerFrame) {}

Engine::~Engine() {
  // Scripts reference the scene and physics through the context: tear them down first.
  scripts_.reset();
  physics_.reset();
  renderer_.reset();
}

std::variant<std::unique_ptr<Engine>, std::string> Engine::create(const EngineOptions& options) {
  std::unique_ptr<Engine> engine(new Engine(options));
  if (auto error = engine->initialize()) return *error;
  return engine;
}

std::optional<std::string> Engine::initialize() {
  if (options_.scene != nullptr) {
    scene_desc_ = *options_.scene;
  } else {
    const std::filesystem::path scene_path = options_.game_dir / config::kSceneFileName;
    LoadResult loaded = load_gltf_scene(scene_path.string());
    if (!loaded.ok) return loaded.error;
    for (const std::string& warning : loaded.warnings) log::warning(warning);
    scene_desc_ = std::move(loaded.scene);
  }
  scene_ = Scene::from_desc(scene_desc_);

  physics_ = create_jolt_physics_world(gravity());
  create_physics_bodies();

  // Until a window or the editor's view says otherwise, the game is as big as its settings say.
  const GameSettings window = read_game_settings(options_.game_dir);
  context_.screen_width = window.window_width;
  context_.screen_height = window.window_height;

  if (!options_.headless) {
    const GameSettings settings = read_game_settings(options_.game_dir);
    renderer_ = std::make_unique<Renderer>(RenderSettings{settings.window_width, settings.window_height,
                                                          settings.title, !options_.test_script});
    renderer_->upload(scene_desc_);
    renderer_->set_images_dir(options_.game_dir / config::kImagesDirName);
    context_.measure_text = [this](const std::string& text, int size) { return renderer_->text_width(text, size); };
  }
  if (!options_.headless && !options_.test_script) device_input_ = std::make_unique<RaylibInputSource>();

  if (options_.audio == nullptr) {
    owned_audio_ = options_.headless ? std::unique_ptr<AudioSystem>(std::make_unique<RecordingAudio>())
                                     : create_raylib_audio();
  }
  audio().set_sounds_dir(options_.game_dir / config::kSoundsDirName);

  context_.scene = &scene_;
  context_.audio = &audio();
  context_.physics = physics_.get();
  context_.input = &input_;
  context_.test_input = options_.test_script ? &scripted_input_ : nullptr;
  context_.time.fixed_delta = config::kFixedDeltaSeconds;
  context_.on_transform_written = [this](ObjectId id, TransformWrite write) { on_transform_written(id, write); };
  context_.spawn = [this](ObjectId source, const std::optional<Transform>& world) { return spawn(source, world); };
  context_.scene_exists = [this](const std::string& name) { return std::filesystem::exists(scene_path(name)); };
  context_.scene_name = config::kMainSceneName;

  scripts_ = std::make_unique<ScriptHost>(context_, options_.game_dir / config::kScriptsDirName);
  if (options_.test_script) {
    const RubyResult loaded_test = scripts_->load_file(*options_.test_script);
    if (!loaded_test.ok) return "cannot load system test: " + loaded_test.error;
    system_test_ = mrb_gv_get(scripts_->vm().state(), mrb_intern_cstr(scripts_->vm().state(), kSystemTestGlobal));
    if (mrb_nil_p(system_test_)) return "system test did not call SystemTest.define";
    test_status_ = TestStatus::Running;
  }
  scripts_->attach_scripts();
  scripts_->run_scene_script(context_.scene_name);
  return std::nullopt;
}

void Engine::set_screen(int width, int height, std::function<int(const std::string&, int)> measure_text) {
  context_.screen_width = width;
  context_.screen_height = height;
  context_.measure_text = std::move(measure_text);
}

AudioSystem& Engine::audio() { return options_.audio != nullptr ? *options_.audio : *owned_audio_; }

std::filesystem::path Engine::scene_path(const std::string& name) const {
  if (name == config::kMainSceneName) return options_.game_dir / config::kSceneFileName;
  return options_.game_dir / config::kScenesDirName / (name + config::kSceneExtension);
}

void Engine::load_scene(const std::string& name) {
  LoadResult loaded = load_gltf_scene(scene_path(name).string());
  if (!loaded.ok) {
    log::error("cannot load scene '" + name + "': " + loaded.error);
    return;
  }
  for (const std::string& warning : loaded.warnings) log::warning(warning);
  // The Ruby VM survives (globals, loaded classes); the objects, bodies and behaviours do not.
  scripts_->reset_scene();
  scene_desc_ = std::move(loaded.scene);
  scene_ = Scene::from_desc(scene_desc_);
  physics_ = create_jolt_physics_world(gravity());
  context_.physics = physics_.get();
  create_physics_bodies();
  if (renderer_) renderer_->upload(scene_desc_);
  context_.scene_name = name;
  ++scene_generation_;
  animation_time_ = 0.0f;
  scripts_->attach_scripts();
  scripts_->run_scene_script(name);
  log::info("scene '" + name + "' loaded");
}

void Engine::create_physics_bodies() {
  for (const ObjectId id : scene_.alive_ids()) create_body(id);
}

void Engine::create_body(ObjectId id) {
  {
    if (scene_.is_prefab(id)) return;  // templates are inert
    const GameObject& object = scene_.at(id);
    BodyConfig body = body_config_from_props(object.props);
    for (const std::string& warning : body.warnings) log::warning("object '" + object.name + "': " + warning);
    if (!body.wants_physics()) return;
    BodyDesc desc;
    desc.object = id;
    desc.config = std::move(body);
    desc.world = scene_.world_transform(id);
    if (object.mesh != kNoIndex) {
      desc.mesh = &scene_desc_.meshes[object.mesh];
      desc.bounds = desc.mesh->bounds;
    } else {
      desc.bounds = default_bounds();
    }
    if (desc.config.type == BodyType::Vehicle) {
      desc.wheels = collect_wheels(id);
      if (desc.wheels.empty()) log::warning("vehicle '" + object.name + "' has no children with wheel = true");
    }
    if (auto error = physics_->add_object(desc)) log::error("object '" + object.name + "': " + *error);
  }
}

ObjectId Engine::spawn(ObjectId source, const std::optional<Transform>& world) {
  if (!scene_.is_alive(source)) return kNoObject;
  const std::vector<ObjectId> created = scene_.clone(source);
  if (world) {
    Transform placed = *world;
    placed.scale = scene_.world_transform(created.front()).scale;
    scene_.set_world_transform(created.front(), placed);
  }
  for (const ObjectId id : created) create_body(id);
  for (const ObjectId id : created) scripts_->attach_object(id);
  return created.front();
}

std::vector<WheelDesc> Engine::collect_wheels(ObjectId vehicle) const {
  const Transform chassis = scene_.world_transform(vehicle);
  std::vector<WheelDesc> wheels;
  for (const ObjectId child : scene_.at(vehicle).children) {
    const GameObject& object = scene_.at(child);
    if (!get_bool(object.props, config::kPropWheel).value_or(false)) continue;
    if (!approx_equal(object.local.rotation, Quat::identity(), kWheelRotationTolerance)) {
      log::warning("wheel '" + object.name + "' has an unapplied rotation; apply it in Blender (Ctrl+A) so its "
                   "axle lies along X");
    }
    const Transform world = scene_.world_transform(child);
    // Wheels are modeled with their axle along X: width on X, radius in the Y/Z plane.
    const Bounds bounds = object.mesh != kNoIndex ? scene_desc_.meshes[object.mesh].bounds : default_bounds();
    const Vec3 half = hadamard(bounds.half_extents(), world.scale);
    WheelDesc wheel;
    wheel.object = child;
    wheel.center = rotate(conjugate(chassis.rotation), world.position - chassis.position);
    wheel.radius = std::max(half.y, half.z);
    wheel.width = half.x * kTwo;
    wheels.push_back(wheel);
  }
  return wheels;
}

bool Engine::finished() const {
  if (context_.quit_requested) return true;
  if (test_status_ == TestStatus::Passed || test_status_ == TestStatus::Failed) return true;
  return renderer_ != nullptr && renderer_->close_requested();
}

void Engine::frame(float frame_seconds) {
  tick_system_test();
  if (test_status_ == TestStatus::Passed || test_status_ == TestStatus::Failed) return;

  const InputSource& source = options_.input != nullptr ? *options_.input
                              : device_input_       ? *device_input_
                                                    : static_cast<const InputSource&>(scripted_input_);
  input_.update(source);
  context_.hud.clear();
  scripts_->start_pending();

  if (renderer_) {
    context_.screen_width = renderer_->screen_width();
    context_.screen_height = renderer_->screen_height();
    // The pointer belongs to the player while a test drives the game.
    if (!options_.test_script) renderer_->set_cursor_locked(context_.mouse_locked);
  }
  // Time.scale slows or stops the simulation (0 pauses it); behaviours still get their update,
  // with a shorter delta, so menus work while the game is paused.
  const float scale = context_.time.scale;
  int steps = 0;
  if (deterministic_) {
    pending_steps_ += scale;
    steps = static_cast<int>(pending_steps_);
    pending_steps_ -= static_cast<float>(steps);
  } else {
    steps = clock_.advance(frame_seconds * scale);
  }
  for (int i = 0; i < steps; ++i) fixed_step(config::kFixedDeltaSeconds);

  const float delta = (deterministic_ ? config::kFixedDeltaSeconds : frame_seconds) * scale;
  context_.time.delta = delta;
  // Every animation of the scene plays in a loop; scripts run after and may override poses.
  animation_time_ += delta;  // (restarted by load_scene, so it stays precise)
  for (const AnimationDesc& clip : scene_desc_.animations) apply_animation(clip, animation_time_, scene_);
  scripts_->update(delta);
  process_destroyed();

  render();
  audio().update();
  ++context_.time.frame;
  if (context_.requested_scene) {
    const std::string name = *context_.requested_scene;
    context_.requested_scene.reset();
    load_scene(name);
  }
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
    if (!pose) continue;
    const BodyType type = physics_->body_type(id);
    if (type == BodyType::Character) {
      scene_.set_world_position(id, pose->position);
    } else {
      Transform world = scene_.world_transform(id);
      world.position = pose->position;
      world.rotation = pose->rotation;
      scene_.set_world_transform(id, world);
    }
    if (type == BodyType::Vehicle) {
      // After the chassis moved, so wheels are expressed relative to its new pose.
      for (const WheelPose& wheel : physics_->wheel_poses(id)) {
        Transform world = scene_.world_transform(wheel.object);
        world.position = wheel.world.position;
        world.rotation = wheel.world.rotation;
        scene_.set_world_transform(wheel.object, world);
      }
    }
  }
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
  const BodyType type = physics_->body_type(id);
  const bool simulated = type == BodyType::Dynamic || type == BodyType::Character || type == BodyType::Static ||
                         type == BodyType::Vehicle;
  if (!simulated) return;
  if (write == TransformWrite::Position) {
    physics_->teleport(id, scene_.world_transform(id));
  } else {
    // Turning must not stop a moving body (e.g. a player facing where it walks while jumping).
    physics_->set_body_rotation(id, scene_.world_transform(id).rotation);
  }
}

void Engine::tick_system_test() {
  if (test_status_ != TestStatus::Running) return;
  if (context_.time.frame >= config::kMaxTestFrames) {
    log::error("system test timed out after " + std::to_string(config::kMaxTestFrames) + " frames");
    test_status_ = TestStatus::Failed;
    return;
  }
  const RubyResult result = scripts_->vm().call(system_test_, "__tick");
  if (!result.ok) {
    log::error("system test crashed: " + result.error);
    test_status_ = TestStatus::Failed;
    return;
  }
  mrb_state* mrb = scripts_->vm().state();
  if (!mrb_symbol_p(result.value)) {
    log::error("SystemTest#__tick must return a Symbol");
    test_status_ = TestStatus::Failed;
    return;
  }
  const std::string status = mrb_sym_name(mrb, mrb_symbol(result.value));
  if (status == "passed") {
    test_status_ = TestStatus::Passed;
  } else if (status == "failed") {
    const RubyResult failure = scripts_->vm().call(system_test_, "failure");
    const bool readable = failure.ok && mrb_string_p(failure.value);
    log::error("system test failed: " + std::string(readable ? RSTRING_PTR(failure.value) : "unknown failure"));
    test_status_ = TestStatus::Failed;
  }
}

void Engine::render() {
  std::vector<std::filesystem::path> screenshots;
  for (const std::string& name : context_.screenshot_requests) {
    screenshots.push_back(options_.screenshot_dir / (name + config::kScreenshotExtension));
  }
  context_.screenshot_requests.clear();
  if (renderer_) renderer_->draw(scene_, context_.hud, screenshots);
}

int Engine::run() {
  while (!finished()) frame(renderer_ ? renderer_->frame_seconds() : config::kFixedDeltaSeconds);
  if (test_status_ == TestStatus::NotRunning) return kExitSuccess;
  if (test_status_ == TestStatus::Passed && scripts_->error_count() == 0) {
    log::info("system test passed");
    return kExitSuccess;
  }
  if (scripts_->error_count() > 0) log::error(std::to_string(scripts_->error_count()) + " script error(s)");
  return kExitFailure;
}

int run_ruby_tests(const std::filesystem::path& test_file) {
  Scene scene;
  std::unique_ptr<PhysicsWorld> physics = create_jolt_physics_world(gravity());
  InputState input;
  ScriptedInputSource test_input;  // lets Ruby tests exercise the system test DSL
  ScriptContext context;
  context.scene = &scene;
  context.physics = physics.get();
  context.input = &input;
  RecordingAudio audio;
  context.audio = &audio;
  context.test_input = &test_input;
  ScriptHost host(context, test_file.parent_path());
  const RubyResult loaded = host.load_file(test_file);
  if (!loaded.ok) {
    log::error(loaded.error);
    return kExitFailure;
  }
  mrb_state* mrb = host.vm().state();
  const RubyResult failures = host.vm().call(mrb_obj_value(mrb_module_get(mrb, "RedGemTest")), "run_all");
  if (!failures.ok) {
    log::error(failures.error);
    return kExitFailure;
  }
  const bool passed = mrb_integer_p(failures.value) && mrb_integer(failures.value) == 0 && host.error_count() == 0;
  return passed ? kExitSuccess : kExitFailure;
}

}  // namespace redgem
