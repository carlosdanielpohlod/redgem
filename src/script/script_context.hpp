#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "audio/audio.hpp"
#include "input/input.hpp"
#include "physics/physics_world.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

class SaveStore;
namespace net {
class NetworkSession;
class LanDiscovery;
}
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

// One thing drawn over the game this frame (UI.text, UI.rect, UI.image), in pixels from the
// top-left corner.
struct HudItem {
  enum class Kind { Text, Rect, Image };
  Kind kind = Kind::Text;
  std::string text;  // the text, or the image's name (a file in the game's images/ folder)
  int x = 0;
  int y = 0;
  int width = 0;   // rectangles and images
  int height = 0;
  int size = 0;    // text height
  float radius = 0.0f;  // rounded corners of a rectangle, in pixels
  Color color;
};

// Everything the Ruby bindings are allowed to touch. Owned by the engine.
struct ScriptContext {
  Scene* scene = nullptr;
  PhysicsWorld* physics = nullptr;
  InputState* input = nullptr;
  ScriptedInputSource* test_input = nullptr;  // only set when running a system test
  TimeInfo time;
  std::vector<HudItem> hud;                    // cleared by the engine every rendered frame
  // The size of what the game is drawn in: the window, or the editor's Scene view while playing.
  int screen_width = 0;
  int screen_height = 0;
  // Width of a text in pixels at a size (the renderer's font; an estimate without one).
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
  std::vector<std::string> screenshot_requests;
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

}  // namespace redgem
