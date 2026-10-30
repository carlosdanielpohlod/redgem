#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "audio/audio.hpp"
#include "core/config.hpp"
#include "core/fixed_clock.hpp"
#include "core/stopwatch.hpp"
#include "core/frame_profiler.hpp"
#include "render/debug_draw.hpp"
#include "input/input.hpp"
#include "net/http/http_client.hpp"
#include "net/discovery/lan_discovery.hpp"
#include "net/network_session.hpp"
#include "physics/physics_world.hpp"
#include "render/hud.hpp"
#include "scene/animation/animation_player.hpp"
#include "scene/particles.hpp"
#include "scene/particle_emitters.hpp"
#include "scene/water_interactions.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"
#include "app/game_display.hpp"
#include "app/game_settings.hpp"
#include "app/game_identity.hpp"
#include "app/presenter.hpp"
#include "storage/save_store.hpp"
#include "storage/save_slots.hpp"
#include "script/script_context.hpp"
#include "script/script_host.hpp"

namespace redgem {
class SceneLoader;
class SceneLoad;
struct LoadResult;

struct EngineOptions {
  std::filesystem::path game_dir;
  // Who shows the game, which has to outlive the engine. Without one nobody does (a test, a
  // server): the game is as big as its settings say and every frame takes one fixed step.
  Presenter* presenter = nullptr;
  // Run this in-memory scene instead of loading game_dir/scene.glb (a scene being edited).
  const SceneDesc* scene = nullptr;
  // What scripts call the scene the game starts on (Game.scene_name, its <name>_scene.rb): another
  // than "main" when the editor plays one of the game's other scenes.
  std::string scene_name = config::kMainSceneName;
  // Where the game's keys, mouse and gamepad come from, which has to outlive the engine: the
  // devices of the window it is shown in, a view that hands on what happens inside it, a program
  // that plays the game. Without one nothing is pressed until scripted_input() says so.
  InputSource* input = nullptr;
  // Every frame advances the game by exactly one fixed step, however long it really took: the same
  // run gives the same game every time. Nothing else selects it.
  bool fixed_step = false;
  // A reproducible random stream, seeded before any game scripts are loaded.
  std::optional<std::uint32_t> random_seed{};
  // How the game is shown when it starts (Settings): what its game.json says when none.
  std::optional<DisplaySettings> display;
  // Where Settings.save keeps them for the next runs. None: they last for this run only, and
  // nothing a player kept is read or written (see keeps_display).
  std::optional<DisplayKeeping> display_kept_in;
  // Where Save keeps the game's data. Empty: the game's file in the user's RedGem folder.
  std::filesystem::path save_file;
  // Where the players' bindings of the game's actions are kept (Input.bind). Empty: the game's
  // file in the user's RedGem folder, or nowhere for a game with no id or a server's room.
  std::filesystem::path bindings_file;
  // What Save keeps the game's data in, kept by whoever runs the engine, which it has to outlive
  // (a dedicated server's, shared by its rooms). save_file is not looked at then.
  SaveStore* save = nullptr;
  // What the game sounds through, which has to outlive the engine. Without one it is silent.
  AudioSystem* audio = nullptr;
  // Plays the game as a room of a dedicated server (redgem-server), under this code: online from
  // the start as the game's host, with no player of its own (Network.server?), and, without a
  // `save` of the server's, what it saves kept for this match only. Empty: an ordinary game.
  std::string server_room;
  // Where the network reads the real time its timeouts count (a join that gets no answer, a
  // decision that never comes): the computer's steady clock when empty. Frames do not count: a
  // game with nobody to show it plays them faster than real time. Tests that play frames to see
  // a timeout give a reading that follows them.
  Stopwatch::Reading profile_time;  // foreground profiling clock; empty uses the steady clock
  Stopwatch::Reading network_time;
  // A borrowed reader (must outlive the engine); otherwise one process-wide worker is shared.
  SceneLoader* scene_loader = nullptr;
};

// Wires scene, physics, scripts and input together and runs the game loop. It opens no window and
// draws nothing: each frame is handed to its presenter.
class Engine {
 public:
  // Returns the engine or an error message.
  static std::variant<std::unique_ptr<Engine>, std::string> create(const EngineOptions& options);
  // The engine of a game whose first scene was already read (see read_game_scene): for whoever
  // has to know that the game can start before making what it is shown in. `options.scene` is
  // not looked at.
  static std::unique_ptr<Engine> create(const EngineOptions& options, SceneDesc scene);
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

  // Starting up, in this order (the first scene is in place by then).
  void initialize();
  // The scene, its animations and its bodies, which collide as game.json's layers say.
  void build_world(const CollisionLayers& layers);
  void open_presenter(const GameSettings& settings);
  void open_audio();
  void open_saves();
  // The player's display settings, as they were kept; and what changes and keeps them (Settings).
  void open_display(const DisplaySettings& game_defaults);
  void connect_display();
  void follow_players_volume(float volume);
  // What the player chose of the game: its display settings, and the keys of its actions.
  void open_player_choices(const GameSettings& settings);
  // The game's actions and axes (game.json), and what its players bound them to.
  void open_bindings();
  std::filesystem::path chosen_bindings_file() const;
  // Writes the players' bindings when a script changed them since the last frame, and they are
  // not what the file already holds: once a frame at most, at the frame's end, and when the
  // game ends.
  void keep_bindings();
  // Whether the game is a room of a dedicated server (see EngineOptions::server_room).
  bool serves_a_room() const { return !options_.server_room.empty(); }
  void connect_effects();
  void connect_saves();
  void select_save_slot(const std::string& name);
  void fill_context(const std::string& game_title);
  // What game.json says the game is (its version, for Game.version).
  // What game.json says the game is, read once; its splash picture shown while the rest loads.
  GameIdentityReading begin_loading();
  void take_identity(const GameIdentityReading& identity);
  void connect_context();
  void connect_object_checkpoints();
  nlohmann::json capture_object_checkpoint(ObjectHandle object);
  void restore_object_checkpoint(ObjectHandle object, const nlohmann::json& data);
  // (With the time each call into them may take: game.json's script_budget_ms.)
  void start_scripts(std::chrono::milliseconds script_budget);
  void seed_scripts();

  static Vec3 gravity();
  // A physics world of its own for the scene in place, with a body for every object that asks for one.
  void make_bodies();

  // Makes a scene that was loaded the one that runs, under `name`.
  void switch_to(const std::string& name, SceneDesc desc);
  // Puts a newly loaded scene, with its bodies, in place of the running one.
  void replace_world(SceneDesc desc);
  void place_spawned(ObjectId id, const std::optional<Transform>& world);

  // The parts of a frame, in the order frame() runs them.
  void prepare_frame();
  void read_input();
  // The key or button a capture caught, to the block waiting for it (Input.capture_next).
  void hand_on_capture();
  InputSource& input_source();
  void poll_network();
  // The answers of the game's web requests that came, each to the block waiting for it.
  void answer_requests();
  // Whether the game is one the neighbours looking for a game are told of: hosted on this
  // machine's own port, and not as a room on a relay.
  bool found_by_neighbours() const;
  bool captures_cursor() const;
  void follow_presenter();
  void advance_game(float frame_seconds);
  // Fixed steps to run this frame, with the game's time scaled by `scale`.
  int fixed_steps_due(float frame_seconds, float scale);
  void update_frame(float delta);
  void switch_scene_if_asked();
  bool is_loading_scene() const;
  bool deterministic_scene_load() const;
  void advance_or_loading(float frame_seconds);
  void request_scene(const std::string& name);
  void begin_scene_load(const std::string& name);
  void cancel_scene_load();
  void poll_scene_load();
  void finish_scene_load(LoadResult loaded);

  void fixed_step(float dt);
  // Whether a contact is between two objects that are both still there to hear of it.
  bool both_alive(const ContactEvent& event) const;
  void process_destroyed();

  EngineOptions options_;
  std::filesystem::path images_dir_;
  std::unique_ptr<Presenter> own_presenter_;  // the nobody that shows a game given no presenter
  Presenter* presenter_ = nullptr;
  std::shared_ptr<SceneLoad> scene_load_;
  SceneDesc scene_desc_;
  Scene scene_;
  std::unique_ptr<PhysicsWorld> physics_;
  CollisionLayers collision_layers_;  // game.json's: every scene's physics world has them
  InputState input_;
  ScriptedInputSource scripted_input_;
  std::filesystem::path bindings_file_;  // empty: the players' bindings are kept nowhere
  bool bindings_changed_ = false;        // by a script, since they were last kept
  std::string kept_bindings_;            // what the file holds, as last read or written
  ScriptContext context_;
  std::unique_ptr<ScriptHost> scripts_;
  std::unique_ptr<AudioSystem> owned_audio_;
  std::unique_ptr<SaveStore> save_;
  SaveSlots save_slots_;
  float pending_steps_ = 0.0f;  // fixed steps owed while Time.scale slows a fixed-step run
  int scene_generation_ = kFirstSceneGeneration;
  AnimationPlayer animations_;
  ParticleSystem particles_;
  ParticleEmitters particle_emitters_;
  WaterInteractions water_;
  std::vector<ControlCircle> controls_;  // the touch controls of the frame being shown
  net::NetworkSession network_;
  net::LanDiscovery discovery_;
  net::HttpClient http_;  // (its requests still on their way are given up with the engine)
  FixedClock clock_;
  DebugDrawBuffer debug_draw_;
  FrameProfiler profiler_;
  Stopwatch network_watch_;  // the real time between the network's polls
  // The player's volume their settings last set (a multiplier of the game's own master volume):
  // only a change of it is passed on.
  float players_volume_ = kPlayersFullVolume;
};

}  // namespace redgem
