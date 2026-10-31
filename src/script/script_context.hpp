#pragma once

#include <nlohmann/json_fwd.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "audio/audio.hpp"
#include "input/input.hpp"
#include "physics/physics_world.hpp"
#include "render/display_settings.hpp"
#include "render/hud.hpp"
#include "script/script_config.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

class SaveStore;
namespace net {
class NetworkSession;
class LanDiscovery;
class HttpClient;
}  // namespace net
class AnimationPlayer;
class ParticleSystem;
class ParticleEmitters;

// What a script changed on a transform; physics reacts differently to each.
enum class TransformWrite { Position, Rotation };

struct TimeInfo {
  float delta = 0.0f;
  float fixed_delta = 0.0f;
  double elapsed = 0.0;
  long long frame = 0;
  // How fast game time runs (Time.scale): 0 pauses the game, 1 is normal. Menus keep working
  // while paused: behaviours are still updated, with a delta of 0.
  float scale = 1.0f;
  // The frame's seconds whatever the scale (Time.unscaled_delta): what moves while the game is
  // paused or slowed, such as an orbit the player turns, goes by it.
  float unscaled_delta = 0.0f;
};

// Everything the Ruby bindings are allowed to touch. Owned by the engine.
class FrameProfiler;
class DebugDrawBuffer;

struct ScriptContext {
  DebugDrawBuffer* debug_draw = nullptr;  // absent without a presenter; local frame commands only
  FrameProfiler* profiler = nullptr;      // borrowed from the engine; absent in standalone interpreters
  bool stats_visible = false;
  std::function<nlohmann::json(ObjectHandle)> capture_object_state;
  std::function<void(ObjectHandle, const nlohmann::json&)> restore_object_state;
  Scene* scene = nullptr;
  PhysicsWorld* physics = nullptr;
  InputState* input = nullptr;
  TimeInfo time;
  std::vector<HudItem> hud;  // cleared by the engine every rendered frame
  // The size of what the game is drawn in: the window, or the editor's Scene view while playing.
  int screen_width = 0;
  int screen_height = 0;
  // The size the world's picture is drawn at, shown stretched over the screen as large as fits
  // (see Presenter::picture_size); 0 when it is the screen's own.
  int picture_width = 0;
  int picture_height = 0;
  // Width of a text in pixels at a size (the font it is drawn with; estimated_text_width without one).
  std::function<int(const std::string&, int)> measure_text;
  std::string ui_font;  // selected game face; empty uses the built-in font
  // Called when a player's bindings changed (Input.bind, unbind, reset_bindings), so that they are
  // kept for the next run; null when nobody keeps them.
  std::function<void()> on_bindings_changed;
  // Input.mouse_locked: the pointer is hidden and kept in the window (first-person cameras).
  bool mouse_locked = false;
  // Input.touch_controls: whether a touch screen shows its stick and buttons.
  bool touch_controls = true;
  // Camera.motion_blur: how much the picture streaks away from its center, 0 to 1.
  float motion_blur = 0.0f;
  // How the player chose the game to be shown and heard (Settings): what is in use now.
  DisplaySettings display;
  // What puts a change of `display` into effect (the window, the renderer's quality, the volume),
  // keeps it for the game's next runs (false, with what went wrong, when it cannot), and lists the
  // window sizes the player may choose. Null outside a running engine: a change is only kept here.
  std::function<void(const DisplaySettings&)> apply_display;
  std::function<bool(const DisplaySettings&, std::string&)> save_display;
  std::function<std::vector<SurfaceSize>()> display_modes;
  // What the game keeps between runs (Save); null when there is nowhere to keep it.
  SaveStore* save = nullptr;
  std::string save_slot;
  std::function<void(const std::string&)> select_save_slot;
  std::function<std::vector<std::string>()> list_save_slots;
  // The scene's animation clips (Animation) and the particles flying around (Particles).
  AnimationPlayer* animations = nullptr;
  ParticleSystem* particles = nullptr;
  ParticleEmitters* particle_emitters = nullptr;
  bool renders_particles = false;  // emitters are local presentation, never simulated by a headless room
  // The multiplayer game this machine hosts or joined (Network).
  net::NetworkSession* network = nullptr;
  // Finding games on the local network, and being found (Network.search, Network.host).
  net::LanDiscovery* discovery = nullptr;
  // The game's web requests (HTTP).
  net::HttpClient* http = nullptr;
  // What the game is called (game.json): how a hosted game shows up to others unless named.
  std::string game_title;
  // Its version (game.json), for Game.version; empty when it gives none.
  std::string game_version;
  bool quit_requested = false;
  bool stop_recording_requested = false;  // Game.stop_recording: the video being recorded ends here
  // Called when a script writes a transform, so physics can follow.
  std::function<void(ObjectId, TransformWrite)> on_transform_written;
  // Creates a live copy of an object (Scene.spawn); null outside a running engine.
  // The flag says whether the copy gets physics bodies (a copy that only shows where something is
  // on another machine must not push things around here).
  std::function<ObjectId(ObjectId, const std::optional<Transform>&, bool)> spawn;
  AudioSystem* audio = nullptr;
  // Scene requests are collected at frame end; asynchronous reads keep the old world until ready.
  std::string scene_name;
  std::optional<std::string> requested_scene;
  bool scene_request_async = false;
  bool cancel_scene_load = false;
  std::string loading_scene;  // nonempty while the current world is frozen for a background read
  std::string loading_error;  // last failed read, retained until the next request
  std::function<bool(const std::string&)> scene_exists;
};

// How wide a text of that size is taken to be when there is no font to measure it with.
inline int estimated_text_width(const std::string& text, int size) {
  return static_cast<int>(static_cast<float>(text.size()) * static_cast<float>(size) * config::kEstimatedLetterWidth);
}

}  // namespace redgem
