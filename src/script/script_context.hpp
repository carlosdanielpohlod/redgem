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

// What a script changed on a transform; physics reacts differently to each.
enum class TransformWrite { Position, Rotation };

struct TimeInfo {
  float delta = 0.0f;
  float fixed_delta = 0.0f;
  double elapsed = 0.0;
  long long frame = 0;
};

struct HudText {
  std::string text;
  int x = 0;
  int y = 0;
  int size = 0;
  Color color;
};

// Everything the Ruby bindings are allowed to touch. Owned by the engine.
struct ScriptContext {
  Scene* scene = nullptr;
  PhysicsWorld* physics = nullptr;
  InputState* input = nullptr;
  ScriptedInputSource* test_input = nullptr;  // only set when running a system test
  TimeInfo time;
  std::vector<HudText> hud;                    // cleared by the engine every rendered frame
  std::vector<std::string> screenshot_requests;
  bool quit_requested = false;
  // Called when a script writes a transform, so physics can follow.
  std::function<void(ObjectId, TransformWrite)> on_transform_written;
  // Creates a live copy of an object (Scene.spawn); null outside a running engine.
  std::function<ObjectId(ObjectId, const std::optional<Transform>&)> spawn;
  AudioSystem* audio = nullptr;
  // Scene switching: Game.load_scene sets the request; the engine switches at the end of the frame.
  std::string scene_name;
  std::optional<std::string> requested_scene;
  std::function<bool(const std::string&)> scene_exists;
};

}  // namespace redgem
