#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "audio/audio.hpp"
#include "core/fixed_clock.hpp"
#include "input/input.hpp"
#include "input/touch_controls.hpp"
#include "net/lan_discovery.hpp"
#include "net/network_session.hpp"
#include "physics/physics_world.hpp"
#include "render/hud.hpp"
#include "scene/animation_player.hpp"
#include "scene/particles.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"
#include "app/game_settings.hpp"
#include "app/presenter.hpp"
#include "storage/save_store.hpp"
#include "script/script_context.hpp"
#include "script/script_host.hpp"

namespace redgem {

struct EngineOptions {
  std::filesystem::path game_dir;
  // Who shows the game, which has to outlive the engine. Without one nobody does (a test, a
  // server): the game is as big as its settings say and every frame takes one fixed step.
  Presenter* presenter = nullptr;
  // Run this in-memory scene instead of loading game_dir/scene.glb (a scene being edited).
  const SceneDesc* scene = nullptr;
  // Where the game's keys, mouse and gamepad come from, which has to outlive the engine: the
  // devices of the window it is shown in, a view that hands on what happens inside it, a program
  // that plays the game. Without one nothing is pressed until scripted_input() says so.
  InputSource* input = nullptr;
  // Every frame advances the game by exactly one fixed step, however long it really took: the same
  // run gives the same game every time. Nothing else selects it.
  bool fixed_step = false;
  // Where Save keeps the game's data. Empty: the game's file in the user's RedGem folder.
  std::filesystem::path save_file;
  // What the game sounds through, which has to outlive the engine. Without one it is silent.
  AudioSystem* audio = nullptr;
};

// Wires scene, physics, scripts and input together and runs the game loop. It opens no window and
// draws nothing: each frame is handed to its presenter.
class Engine {
 public:
  // Returns the engine or an error message.
  static std::variant<std::unique_ptr<Engine>, std::string> create(const EngineOptions& options);
  ~Engine();
  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  // Runs until the presenter has nowhere left to show the game or a script quits, each frame as
  // long as the presenter says the last one took. Returns the exit code.
  int run();
  // Copies a prefab (or any object) into the running game with physics and scripts. `world`
  // overrides the copy's position/rotation. Returns the new root object.
  ObjectId spawn(ObjectId source, const std::optional<Transform>& world, bool physics = true);
  // Simulates one frame and hands it to the presenter. With fixed_step it always advances
  // exactly one fixed step.
  void frame(float frame_seconds);
  // Hands the game as it is now to the presenter again, without advancing it (a paused game
  // whose view has to be drawn anew).
  void present();

  // Whether a script quit or the presenter has nowhere left to show the game.
  bool finished() const;

  Scene& scene() { return scene_; }
  PhysicsWorld& physics() { return *physics_; }
  ScriptHost& scripts() { return *scripts_; }
  ScriptContext& script_context() { return context_; }
  // What the game reads without an `input` of its options: nothing is pressed until the code
  // that runs the engine says so.
  ScriptedInputSource& scripted_input() { return scripted_input_; }
  // Scene currently running ("main" is scene.glb).
  const std::string& scene_name() const { return context_.scene_name; }
  AudioSystem& audio();
  // Who shows the game: the presenter of its options, or the nobody it made for itself.
  Presenter& presenter() { return *presenter_; }
  AnimationPlayer& animations() { return animations_; }
  net::NetworkSession& network() { return network_; }

 private:
  explicit Engine(const EngineOptions& options);

  // Starting up, in this order.
  std::optional<std::string> initialize();
  std::optional<std::string> load_first_scene();
  void build_world();
  void open_presenter(const GameSettings& settings);
  void open_audio();
  void open_saves();
  void fill_context(const std::string& game_title);
  void connect_context();
  void start_scripts();

  void create_physics_bodies();
  std::filesystem::path scene_path(const std::string& name) const;
  // Whether a script's scene name stays inside the game's folders (a warning says so when not).
  static bool safe_scene_name(const std::string& name);
  void load_scene(const std::string& name);
  // Makes a scene that was loaded the one that runs, under `name`.
  void switch_to(const std::string& name, SceneDesc desc);
  // Puts a newly loaded scene, with its bodies, in place of the running one.
  void replace_world(SceneDesc desc);
  void create_body(ObjectId id);
  BodyDesc describe_body(ObjectId id, BodyConfig config) const;
  // An object's mesh bounds, or the default collider's when it has no mesh.
  Bounds bounds_of(const GameObject& object) const;
  std::vector<WheelDesc> collect_wheels(ObjectId vehicle) const;
  WheelDesc describe_wheel(ObjectId child, const Transform& chassis) const;

  // The parts of a frame, in the order frame() runs them.
  void read_input();
  InputSource& input_source();
  void poll_network(float frame_seconds);
  void follow_presenter();
  void advance_game(float frame_seconds);
  // Fixed steps to run this frame, with the game's time scaled by `scale`.
  int fixed_steps_due(float frame_seconds, float scale);
  void update_frame(float delta);
  void switch_scene_if_asked();

  void fixed_step(float dt);
  void push_poses_to_physics();
  void pull_poses_from_physics();
  void follow_body(ObjectId id, const Transform& pose, BodyType type);
  // Moves and turns an object to where physics has it, keeping its scale.
  void move_to(ObjectId id, const Transform& pose);
  void process_destroyed();
  void on_transform_written(ObjectId id, TransformWrite write);

  EngineOptions options_;
  std::filesystem::path images_dir_;
  std::unique_ptr<Presenter> own_presenter_;  // the nobody that shows a game given no presenter
  Presenter* presenter_ = nullptr;
  SceneDesc scene_desc_;
  Scene scene_;
  std::unique_ptr<PhysicsWorld> physics_;
  InputState input_;
  ScriptedInputSource scripted_input_;
  ScriptContext context_;
  std::unique_ptr<ScriptHost> scripts_;
  std::unique_ptr<AudioSystem> owned_audio_;
  std::unique_ptr<SaveStore> save_;
  float pending_steps_ = 0.0f;  // fixed steps owed while Time.scale slows a fixed-step run
  int scene_generation_ = kFirstSceneGeneration;
  AnimationPlayer animations_;
  ParticleSystem particles_;
  std::vector<ControlCircle> controls_;  // the touch controls of the frame being shown
  net::NetworkSession network_;
  net::LanDiscovery discovery_;
  FixedClock clock_;
};

// A touch screen's controls as they are drawn: the stick's ring first, then what lies over it
// (the knob, the buttons), each brighter while a finger is on it.
std::vector<ControlCircle> control_circles(const TouchLayout& layout);

}  // namespace redgem
