#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "audio/audio.hpp"
#include "core/fixed_clock.hpp"
#include "input/input.hpp"
#include "physics/physics_world.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"
#include "script/script_context.hpp"
#include "script/script_host.hpp"

namespace redgem {

class Renderer;

struct EngineOptions {
  std::filesystem::path game_dir;
  bool headless = false;  // no window and no rendering of its own (tests, editor play mode)
  std::optional<std::filesystem::path> test_script;
  std::filesystem::path screenshot_dir;
  // Editor play mode: run this in-memory scene instead of loading game_dir/scene.glb.
  const SceneDesc* scene = nullptr;
  // Editor play mode: input comes from the editor viewport. Headless engines without it step
  // deterministically (one fixed step per frame).
  const InputSource* input = nullptr;
  // Editor play mode: the editor's audio device. Otherwise the engine opens its own (or, headless,
  // records what would play).
  AudioSystem* audio = nullptr;
};

enum class TestStatus { NotRunning, Running, Passed, Failed };

// Wires scene, physics, scripts, input and rendering together and runs the game loop.
class Engine {
 public:
  // Returns the engine or an error message.
  static std::variant<std::unique_ptr<Engine>, std::string> create(const EngineOptions& options);
  ~Engine();
  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  // Runs until the window closes, a script quits or the system test ends. Returns the exit code.
  int run();
  // Copies a prefab (or any object) into the running game with physics and scripts. `world`
  // overrides the copy's position/rotation. Returns the new root object.
  ObjectId spawn(ObjectId source, const std::optional<Transform>& world);
  // Simulates and renders one frame. Deterministic engines always advance exactly one fixed step.
  void frame(float frame_seconds);

  bool finished() const;
  TestStatus test_status() const { return test_status_; }

  Scene& scene() { return scene_; }
  PhysicsWorld& physics() { return *physics_; }
  ScriptHost& scripts() { return *scripts_; }
  ScriptContext& script_context() { return context_; }
  ScriptedInputSource& scripted_input() { return scripted_input_; }
  // Scene currently running ("main" is scene.glb) and how many times a scene was (re)loaded.
  const std::string& scene_name() const { return context_.scene_name; }
  int scene_generation() const { return scene_generation_; }
  const SceneDesc& scene_desc() const { return scene_desc_; }
  AudioSystem& audio();
  // HUD drawn by scripts during the last frame.
  const std::vector<HudItem>& hud() const { return context_.hud; }
  // For whoever draws a headless engine's frames (the editor's Scene view): the size of that
  // view and how to measure the texts drawn in it. Engines with their own window know both.
  void set_screen(int width, int height, std::function<int(const std::string&, int)> measure_text);
  // Whether the game asked for the pointer to be hidden and kept (Input.mouse_locked = true).
  bool mouse_locked() const { return context_.mouse_locked; }

 private:
  explicit Engine(const EngineOptions& options);

  std::optional<std::string> initialize();
  void create_physics_bodies();
  std::filesystem::path scene_path(const std::string& name) const;
  void load_scene(const std::string& name);
  void create_body(ObjectId id);
  std::vector<WheelDesc> collect_wheels(ObjectId vehicle) const;
  void fixed_step(float dt);
  void push_poses_to_physics();
  void pull_poses_from_physics();
  void process_destroyed();
  void on_transform_written(ObjectId id, TransformWrite write);
  void tick_system_test();
  void render();

  EngineOptions options_;
  bool deterministic_ = false;
  SceneDesc scene_desc_;
  Scene scene_;
  std::unique_ptr<PhysicsWorld> physics_;
  InputState input_;
  ScriptedInputSource scripted_input_;
  std::unique_ptr<InputSource> device_input_;
  ScriptContext context_;
  std::unique_ptr<ScriptHost> scripts_;
  std::unique_ptr<Renderer> renderer_;
  std::unique_ptr<AudioSystem> owned_audio_;
  float pending_steps_ = 0.0f;  // fixed steps owed while Time.scale slows a deterministic run
  int scene_generation_ = 0;
  float animation_time_ = 0.0f;  // seconds the scene's animations have been playing
  FixedClock clock_;
  TestStatus test_status_ = TestStatus::NotRunning;
  mrb_value system_test_ = mrb_nil_value();
};

// Runs a Ruby unit test file inside a bare engine VM. Returns the process exit code.
int run_ruby_tests(const std::filesystem::path& test_file);

}  // namespace redgem
