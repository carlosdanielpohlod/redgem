#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "audio/audio.hpp"
#include "input/input.hpp"
#include "physics/physics_world.hpp"
#include "render/hud.hpp"
#include "script/script_config.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

class SaveStore;
namespace net {
class NetworkSession;
class LanDiscovery;
}  // namespace net
class AnimationPlayer;
class ParticleSystem;

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
};

// Everything the Ruby bindings are allowed to touch. Owned by the engine.
struct ScriptContext {
  Scene* scene = nullptr;
  PhysicsWorld* physics = nullptr;
  InputState* input = nullptr;
  TimeInfo time;
  std::vector<HudItem> hud;  // cleared by the engine every rendered frame
  // The size of what the game is drawn in: the window, or the editor's Scene view while playing.
  int screen_width = 0;
  int screen_height = 0;
  // Width of a text in pixels at a size (the font it is drawn with; estimated_text_width without one).
  std::function<int(const std::string&, int)> measure_text;
  // Input.mouse_locked: the pointer is hidden and kept in the window (first-person cameras).
  bool mouse_locked = false;
  // Input.touch_controls: whether a touch screen shows its stick and buttons.
  bool touch_controls = true;
  // Camera.motion_blur: how much the picture streaks away from its center, 0 to 1.
  float motion_blur = 0.0f;
  // What the game keeps between runs (Save); null when there is nowhere to keep it.
  SaveStore* save = nullptr;
  // The scene's animation clips (Animation) and the particles flying around (Particles).
  AnimationPlayer* animations = nullptr;
  ParticleSystem* particles = nullptr;
  // The multiplayer game this machine hosts or joined (Network).
  net::NetworkSession* network = nullptr;
  // Finding games on the local network, and being found (Network.search, Network.host).
  net::LanDiscovery* discovery = nullptr;
  // What the game is called (game.json): how a hosted game shows up to others unless named.
  std::string game_title;
  bool quit_requested = false;
  // Called when a script writes a transform, so physics can follow.
  std::function<void(ObjectId, TransformWrite)> on_transform_written;
  // Creates a live copy of an object (Scene.spawn); null outside a running engine.
  // The flag says whether the copy gets physics bodies (a copy that only shows where something is
  // on another machine must not push things around here).
  std::function<ObjectId(ObjectId, const std::optional<Transform>&, bool)> spawn;
  AudioSystem* audio = nullptr;
  // Scene switching: Game.load_scene sets the request; the engine switches at the end of the frame.
  std::string scene_name;
  std::optional<std::string> requested_scene;
  std::function<bool(const std::string&)> scene_exists;
};

// How wide a text of that size is taken to be when there is no font to measure it with.
inline int estimated_text_width(const std::string& text, int size) {
  return static_cast<int>(static_cast<float>(text.size()) * static_cast<float>(size) * config::kEstimatedLetterWidth);
}

}  // namespace redgem
