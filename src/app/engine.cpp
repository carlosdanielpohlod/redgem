#include "core/exit_codes.hpp"
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

constexpr const char* kSystemTestGlobal = "$__redgem_system_test";
constexpr float kWheelRotationTolerance = 1e-3f;
// What a system test answers to, and what its __tick says about how it is going.
constexpr const char* kTestTickMethod = "__tick";
constexpr const char* kTestFailureMethod = "failure";
constexpr const char* kTestPassed = "passed";
constexpr const char* kTestFailed = "failed";
// What runs the tests of a Ruby unit test file.
constexpr const char* kRubyTestModule = "RedGemTest";
constexpr const char* kRunAllMethod = "run_all";

Vec3 gravity() { return {0.0f, config::kGravityY, 0.0f}; }

Bounds default_bounds() {
  const Vec3 half = Vec3::one() * config::kDefaultColliderHalfExtent;
  Bounds bounds;
  bounds.include(-half);
  bounds.include(half);
  return bounds;
}

bool simulated(BodyType type) {
  return type == BodyType::Dynamic || type == BodyType::Character || type == BodyType::Static ||
         type == BodyType::Vehicle;
}

// What a Ruby unit test file runs against: a scene, physics, input and audio with nothing in them.
struct BareWorld {
  Scene scene;
  std::unique_ptr<PhysicsWorld> physics = create_jolt_physics_world(gravity());
  InputState input;
  ScriptedInputSource test_input;  // lets Ruby tests exercise the system test DSL
  RecordingAudio audio;
  ScriptContext context;

  BareWorld() {
    context.scene = &scene;
    context.physics = physics.get();
    context.input = &input;
    context.audio = &audio;
    context.test_input = &test_input;
  }
};

// Whether every test of the file a host loaded passes, without a script error.
bool ruby_tests_pass(ScriptHost& host) {
  mrb_state* mrb = host.vm().state();
  const RubyResult failures = host.vm().call(mrb_obj_value(mrb_module_get(mrb, kRubyTestModule)), kRunAllMethod);
  if (!failures.ok) {
    log::error(failures.error);
    return false;
  }
  return mrb_integer_p(failures.value) && mrb_integer(failures.value) == 0 && host.error_count() == 0;
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
  if (auto error = load_first_scene()) return error;
  build_world();
  // Until a window or the editor's view says otherwise, the game is as big as its settings say.
  const GameSettings settings = read_game_settings(options_.game_dir);
  context_.screen_width = settings.window_width;
  context_.screen_height = settings.window_height;
  if (!options_.headless) open_window(settings);
  if (!options_.headless && !options_.test_script) device_input_ = std::make_unique<RaylibInputSource>();
  open_audio();
  open_saves();
  fill_context(settings.title);
  return start_scripts();
}

std::optional<std::string> Engine::load_first_scene() {
  if (options_.scene != nullptr) {
    scene_desc_ = *options_.scene;
    return std::nullopt;
  }
  const std::filesystem::path scene_path = options_.game_dir / config::kSceneFileName;
  LoadResult loaded = load_gltf_scene(scene_path.string());
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

void Engine::open_window(GameSettings settings) {
  // A screen that is not a window has its own shape: the game keeps its height and gets the
  // width that fills it.
  if (options_.screen_aspect > 0.0f) {
    settings.window_width =
        static_cast<int>(std::lround(static_cast<float>(settings.window_height) * options_.screen_aspect));
    context_.screen_width = settings.window_width;
  }
  renderer_ = std::make_unique<Renderer>(
      RenderSettings{settings.window_width, settings.window_height, settings.title, !options_.test_script});
  renderer_->upload(scene_desc_);
  renderer_->set_images_dir(options_.game_dir / config::kImagesDirName);
  context_.measure_text = [this](const std::string& text, int size) { return renderer_->text_width(text, size); };
}

void Engine::open_audio() {
  if (options_.audio == nullptr) {
    owned_audio_ =
        options_.headless ? std::unique_ptr<AudioSystem>(std::make_unique<RecordingAudio>()) : create_raylib_audio();
  }
  audio().set_sounds_dir(options_.game_dir / config::kSoundsDirName);
}

void Engine::open_saves() {
  std::filesystem::path save_file = options_.save_file;
  if (save_file.empty() && options_.test_script) {
    // A test starts from no saves at all, and leaves the player's alone.
    save_file = test_save_file_for(options_.game_dir);
    std::error_code ignored;
    std::filesystem::remove(save_file, ignored);
  }
  save_ = std::make_unique<SaveStore>(save_file.empty() ? save_file_for(options_.game_dir) : save_file);
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
  context_.test_input = options_.test_script ? &scripted_input_ : nullptr;
  context_.time.fixed_delta = config::kFixedDeltaSeconds;
  context_.scene_name = config::kMainSceneName;
  connect_context();
}

void Engine::connect_context() {
  context_.on_transform_written = [this](ObjectId id, TransformWrite write) { on_transform_written(id, write); };
  context_.spawn = [this](ObjectId source, const std::optional<Transform>& world, bool physics) {
    return spawn(source, world, physics);
  };
  context_.scene_exists = [this](const std::string& name) { return std::filesystem::exists(scene_path(name)); };
}

std::optional<std::string> Engine::start_scripts() {
  scripts_ = std::make_unique<ScriptHost>(context_, options_.game_dir / config::kScriptsDirName);
  if (auto error = load_system_test()) return error;
  scripts_->attach_scripts();
  scripts_->run_scene_script(context_.scene_name);
  return std::nullopt;
}

std::optional<std::string> Engine::load_system_test() {
  if (!options_.test_script) return std::nullopt;
  const RubyResult loaded_test = scripts_->load_file(*options_.test_script);
  if (!loaded_test.ok) return "cannot load system test: " + loaded_test.error;
  system_test_ = mrb_gv_get(scripts_->vm().state(), mrb_intern_cstr(scripts_->vm().state(), kSystemTestGlobal));
  if (mrb_nil_p(system_test_)) return "system test did not call SystemTest.define";
  test_status_ = TestStatus::Running;
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
  if (renderer_) renderer_->upload(scene_desc_);
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
  return object.mesh != kNoIndex ? scene_desc_.meshes[object.mesh].bounds : default_bounds();
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
    if (!get_bool(object.props, config::kPropWheel).value_or(false)) continue;
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

bool Engine::test_ended() const { return test_status_ == TestStatus::Passed || test_status_ == TestStatus::Failed; }

bool Engine::finished() const {
  if (context_.quit_requested || test_ended()) return true;
  return renderer_ != nullptr && renderer_->close_requested();
}

void Engine::frame(float frame_seconds) {
  tick_system_test();
  if (test_ended()) return;
  read_input();
  context_.hud.clear();
  scripts_->start_pending();
  poll_network(frame_seconds);
  follow_window();
  advance_game(frame_seconds);
  render();
  audio().update();
  ++context_.time.frame;
  switch_scene_if_asked();
}

void Engine::read_input() {
  if (device_input_) device_input_->show_touch_controls(context_.touch_controls);
  input_.update(input_source());
}

const InputSource& Engine::input_source() const {
  if (options_.input != nullptr) return *options_.input;
  if (device_input_) return *device_input_;
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

void Engine::follow_window() {
  if (!renderer_) return;
  context_.screen_width = renderer_->screen_width();
  context_.screen_height = renderer_->screen_height();
  // The pointer belongs to the player while a test drives the game.
  if (!options_.test_script) renderer_->set_cursor_locked(context_.mouse_locked);
}

void Engine::advance_game(float frame_seconds) {
  // Time.scale slows or stops the simulation (0 pauses it); behaviours still get their update,
  // with a shorter delta, so menus work while the game is paused.
  const float scale = context_.time.scale;
  const int steps = fixed_steps_due(frame_seconds, scale);
  for (int i = 0; i < steps; ++i) fixed_step(config::kFixedDeltaSeconds);
  // A long stall (a dragged window, a loading hitch) counts as one long frame, not as seconds of game.
  const float unscaled =
      deterministic_ ? config::kFixedDeltaSeconds : std::min(frame_seconds, config::kMaxFrameSeconds);
  update_frame(unscaled * scale);
}

int Engine::fixed_steps_due(float frame_seconds, float scale) {
  if (!deterministic_) return clock_.advance(frame_seconds * scale);
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

void Engine::fail_test(const std::string& why) {
  log::error(why);
  test_status_ = TestStatus::Failed;
}

void Engine::tick_system_test() {
  if (test_status_ != TestStatus::Running) return;
  if (context_.time.frame >= config::kMaxTestFrames) {
    fail_test("system test timed out after " + std::to_string(config::kMaxTestFrames) + " frames");
    return;
  }
  const RubyResult result = scripts_->vm().call(system_test_, kTestTickMethod);
  if (!result.ok) {
    fail_test("system test crashed: " + result.error);
  } else if (!mrb_symbol_p(result.value)) {
    fail_test("SystemTest#__tick must return a Symbol");
  } else {
    heed_test_status(mrb_sym_name(scripts_->vm().state(), mrb_symbol(result.value)));
  }
}

void Engine::heed_test_status(const std::string& status) {
  if (status == kTestPassed) test_status_ = TestStatus::Passed;
  if (status != kTestFailed) return;
  const RubyResult failure = scripts_->vm().call(system_test_, kTestFailureMethod);
  const bool readable = failure.ok && mrb_string_p(failure.value);
  fail_test("system test failed: " + std::string(readable ? RSTRING_PTR(failure.value) : "unknown failure"));
}

void Engine::render() {
  std::vector<std::filesystem::path> screenshots;
  screenshots.reserve(context_.screenshot_requests.size());
  for (const std::string& name : context_.screenshot_requests) {
    screenshots.push_back(options_.screenshot_dir / (name + config::kScreenshotExtension));
  }
  context_.screenshot_requests.clear();
  if (renderer_) {
    renderer_->set_motion_blur(context_.motion_blur);
    renderer_->draw(scene_, context_.hud, screenshots, &particles_,
                    device_input_ ? device_input_->touch_layout() : nullptr);
  }
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
  BareWorld world;
  ScriptHost host(world.context, test_file.parent_path());
  const RubyResult loaded = host.load_file(test_file);
  if (!loaded.ok) {
    log::error(loaded.error);
    return kExitFailure;
  }
  return ruby_tests_pass(host) ? kExitSuccess : kExitFailure;
}

}  // namespace redgem
